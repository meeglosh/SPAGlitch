#include "FXChain.h"

namespace glitch::fx
{

const juce::StringArray& divisionNames()
{
    static const juce::StringArray names {
        "8/1", "4/1", "2/1", "1/1",
        "1/2", "1/2T", "1/4", "1/4.", "1/4T",
        "1/8", "1/8.", "1/8T", "1/16", "1/16T", "1/32",
    };
    return names;
}

float divisionBeats (int divisionChoice)
{
    static constexpr float beats[] = {
        32.0f, 16.0f, 8.0f, 4.0f,
        2.0f, 4.0f / 3.0f, 1.0f, 1.5f, 2.0f / 3.0f,
        0.5f, 0.75f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f, 0.125f,
    };
    static_assert (std::size (beats) == 15);
    return beats[juce::jlimit (0, (int) std::size (beats) - 1, divisionChoice)];
}

void FXChain::prepare (double newSampleRate, int maxBlockSize)
{
    sampleRate = newSampleRate;

    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, 2 };

    for (auto& f : toneFilters)
    {
        f.prepare ({ sampleRate, (juce::uint32) maxBlockSize, 1 });
        f.setType (juce::dsp::FirstOrderTPTFilterType::lowpass);
    }

    chorus.prepare (spec);
    modEffect.prepare (sampleRate, maxBlockSize);
    tremVibEffect.prepare (sampleRate, maxBlockSize);
    limiterEffect.prepare (sampleRate, maxBlockSize);
    ottEffect.prepare (sampleRate, maxBlockSize);

    delayBuffer.setSize (2, (int) (sampleRate * 4.0) + 8);
    delayBuffer.clear();
    delayWritePos = 0;
    delaySamplesSmoothed.reset (sampleRate, 0.1);

    reverb.prepare (sampleRate, maxBlockSize);

    eq.prepare (sampleRate, maxBlockSize);

    reset();
}

void FXChain::reset()
{
    for (auto& f : toneFilters)
        f.reset();
    crushHold.fill (0.0f);
    crushPhase.fill (0.0f);
    chorus.reset();
    modEffect.reset();
    tremVibEffect.reset();
    limiterEffect.reset();
    ottEffect.reset();
    delayBuffer.clear();
    reverb.reset();
    eq.reset();
}

double FXChain::tailSeconds (const Params& p) const
{
    double tail = 0.0;

    if (p.delayEnable)
    {
        const auto time = p.delaySync
                        ? divisionBeats (p.delayDivision) * 60.0 / p.bpm
                        : (double) p.delayTimeMs * 0.001;
        // Feedback ring-out to roughly -60 dB.
        const auto repeats = p.delayFeedback > 0.01f
                           ? std::log (0.001) / std::log ((double) p.delayFeedback)
                           : 1.0;
        tail = juce::jlimit (0.0, 12.0, time * repeats);
    }

    if (p.reverbEnable)
        tail = juce::jmax (tail, 0.5 + (double) p.reverbDecay);

    return tail;
}

void FXChain::process (juce::AudioBuffer<float>& buffer, const Params& params)
{
    for (const auto module : params.order)
    {
        switch (module)
        {
            case Module::distortion: if (params.distEnable)   processDistortion (buffer, params); break;
            case Module::chorus:     if (params.chorusEnable) processChorus (buffer, params); break;
            case Module::delay:      if (params.delayEnable)  processDelay (buffer, params); break;
            case Module::reverb:     if (params.reverbEnable) processReverb (buffer, params); break;
            case Module::eq:         if (params.eqEnable)     processEQ (buffer, params); break;
            // Always invoke (rather than gating on modEnable like the other
            // modules) so ModEffect's own enable-edge tracking sees every
            // disable; that's what lets it clear its trapped allpass/
            // feedback/delay state on re-enable instead of ringing it back
            // out (see ModEffect::process). The disabled path is a cheap
            // early-out, not a real per-sample cost.
            case Module::mod:        processMod (buffer, params); break;
            case Module::tremVib:    if (params.tremEnable || params.vibEnable)
                                                            { processTremVib (buffer, params); } break;
            case Module::limiter:    if (params.limEnable)    processLimiter (buffer, params); break;
            case Module::ott:        if (params.ottEnable)    processOTT (buffer, params); break;
        }
    }
}

void FXChain::processDistortion (juce::AudioBuffer<float>& buffer, const Params& p)
{
    const auto driveGain = 1.0f + 15.0f * p.distDrive;

    for (auto& f : toneFilters)
        f.setCutoffFrequency (p.distToneHz);

    // Crush (bit-depth + sample-rate reduction) params, driven entirely by
    // DRIVE via the exponential mappings shared with the UI curve (see
    // crushBitsForDrive/crushHoldForDrive) so the knob stays useful across
    // its whole range instead of spending half its travel inaudible.
    const auto crushLevels = std::pow (2.0f, crushBitsForDrive (p.distDrive));
    const auto crushHoldLen = crushHoldForDrive (p.distDrive, sampleRate);

    for (int ch = 0; ch < juce::jmin (2, buffer.getNumChannels()); ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto& tone = toneFilters[(size_t) ch];

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto dry = data[i];

            float wet;
            if (p.distType == 3)
            {
                // Bit-depth quantise the unscaled input (no drive boost --
                // that would just clip everything at high bit-crush amounts).
                const auto quantised = std::round (dry * crushLevels) / crushLevels;

                // Sample-and-hold decimation: advance the phase each sample;
                // only latch a new held value once the accumulated hold
                // length has been reached, else repeat the last one.
                auto& hold = crushHold[(size_t) ch];
                auto& phase = crushPhase[(size_t) ch];
                if (phase <= 0.0f)
                {
                    hold = quantised;
                    phase = crushHoldLen;
                }
                phase -= 1.0f;

                wet = tone.processSample (0, hold);
            }
            else
            {
                const auto x = dry * driveGain;
                switch (p.distType)
                {
                    case 1:  wet = juce::jlimit (-1.0f, 1.0f, x); break;              // Hard
                    case 2:  wet = std::sin (x * 1.2f); break;                        // Fold
                    default: wet = std::tanh (x); break;                              // Soft
                }
                wet = tone.processSample (0, wet / std::sqrt (driveGain));
            }

            data[i] = dry + (wet - dry) * p.distMix;
        }
    }
}

void FXChain::processChorus (juce::AudioBuffer<float>& buffer, const Params& p)
{
    chorus.setRate (p.chorusRate);
    chorus.setDepth (p.chorusDepth);
    chorus.setCentreDelay (7.0f);
    chorus.setFeedback (p.chorusFeedback);
    chorus.setMix (p.chorusMix);

    juce::dsp::AudioBlock<float> block (buffer);
    chorus.process (juce::dsp::ProcessContextReplacing<float> (block));
}

void FXChain::processDelay (juce::AudioBuffer<float>& buffer, const Params& p)
{
    const auto timeSeconds = p.delaySync
                           ? divisionBeats (p.delayDivision) * 60.0 / p.bpm
                           : (double) p.delayTimeMs * 0.001;
    const auto targetSamples = (float) juce::jlimit (
        32.0, (double) delayBuffer.getNumSamples() - 8.0, timeSeconds * sampleRate);
    delaySamplesSmoothed.setTargetValue (targetSamples);

    const auto bufLen = delayBuffer.getNumSamples();
    auto* bufL = delayBuffer.getWritePointer (0);
    auto* bufR = delayBuffer.getWritePointer (1);
    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : left;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto delaySamples = delaySamplesSmoothed.getNextValue();

        auto readPos = (double) delayWritePos - (double) delaySamples;
        while (readPos < 0.0)
            readPos += (double) bufLen;

        auto r0 = (int) readPos;
        const auto frac = (float) (readPos - (double) r0);
        while (r0 >= bufLen) r0 -= bufLen;   // wrap can round to exactly bufLen -- see FDNReverb.h
        const auto r1 = (r0 + 1) % bufLen;

        const auto outL = bufL[r0] + frac * (bufL[r1] - bufL[r0]);
        const auto outR = bufR[r0] + frac * (bufR[r1] - bufR[r0]);

        // Ping-pong crosses the feedback paths.
        bufL[delayWritePos] = left[i] + (p.delayPingPong ? outR : outL) * p.delayFeedback;
        bufR[delayWritePos] = right[i] + (p.delayPingPong ? outL : outR) * p.delayFeedback;

        left[i] += outL * p.delayMix;
        right[i] += outR * p.delayMix;

        delayWritePos = (delayWritePos + 1) % bufLen;
    }
}

void FXChain::processReverb (juce::AudioBuffer<float>& buffer, const Params& p)
{
    // Dattorro-plate-derived engine (PlateReverb) with mode voicings. MIX is
    // a LINEAR dry/wet dial handled inside the engine (dry = 1-mix, wet =
    // mix): 0% is untouched dry, 100% is pure wet, 50% is exactly half of
    // each. (1.0.14 and earlier used an equal-power sin/cos crossfade on the
    // old FDN engine, which made the knob feel oversensitive near 0 -- see
    // CHANGELOG 1.0.15.)
    PlateReverb::Params rp;
    rp.mode = p.reverbMode;
    rp.preDelayMs = p.reverbPreDelay;
    rp.size = p.reverbSize;
    rp.decaySec = p.reverbDecay;
    rp.hfDamp = p.reverbDamping;
    rp.modDepth = p.reverbModDepth;
    rp.lowCutHz = p.reverbLowCut;
    rp.highCutHz = p.reverbHighCut;
    rp.width = p.reverbWidth;
    rp.mix = p.reverbMix;
    reverb.process (buffer, rp);
}

void FXChain::processMod (juce::AudioBuffer<float>& buffer, const Params& p)
{
    ModEffect::Params mp;
    mp.enable  = p.modEnable;
    mp.type    = p.modType == 1 ? ModEffect::Type::flanger : ModEffect::Type::phaser;
    mp.rateHz  = p.modSync
               ? (float) (p.bpm / 60.0
                          / juce::jmax (0.01, (double) divisionBeats (p.modDivision)))
               : p.modRate;
    mp.depth    = p.modDepth;
    mp.feedback = p.modFeedback;
    mp.stages   = p.modStages;
    mp.centreHz = p.modCentreHz;
    mp.manualMs = p.modManualMs;
    mp.spread   = p.modWidth;
    mp.mix      = p.modMix;
    modEffect.process (buffer, mp);
}

void FXChain::processTremVib (juce::AudioBuffer<float>& buffer, const Params& p)
{
    const auto syncHz = [&] (int div)
    {
        return (float) (p.bpm / 60.0
                        / juce::jmax (0.01, (double) divisionBeats (div)));
    };

    TremVib::Params tp;
    tp.tremOn     = p.tremEnable;
    tp.tremRateHz = p.tremSync ? syncHz (p.tremDivision) : p.tremRate;
    tp.tremDepth  = p.tremDepth;
    tp.tremShape  = p.tremShape;
    tp.tremStereo = p.tremStereo;
    tp.tremMix    = p.tremMix;
    tp.vibOn      = p.vibEnable;
    tp.vibRateHz  = p.vibSync ? syncHz (p.vibDivision) : p.vibRate;
    tp.vibDepth   = p.vibDepth;
    tp.vibMix     = p.vibMix;
    tremVibEffect.process (buffer, tp);
}

void FXChain::processLimiter (juce::AudioBuffer<float>& buffer, const Params& p)
{
    Limiter::Params lp;
    lp.enable      = p.limEnable;
    lp.driveDb     = p.limDrive;
    lp.ceilingDb   = p.limCeiling;
    lp.releaseMs   = p.limRelease;
    lp.autoRelease = p.limAutoRelease;
    lp.character   = p.limCharacter;
    lp.stereoLink  = p.limStereoLink;
    lp.truePeak    = p.limTruePeak;
    lp.lookahead   = p.limLookahead;
    lp.autoGain    = p.limAutoGain;
    limiterEffect.process (buffer, lp);
}

void FXChain::processOTT (juce::AudioBuffer<float>& buffer, const Params& p)
{
    OTT::Params op;
    op.enable           = p.ottEnable;
    op.depth            = p.ottDepth;
    op.timePercent      = p.ottTime;
    op.inGainDb         = p.ottInGain;
    op.outGainDb        = p.ottOutGain;
    op.crossoverLowHz   = p.ottCrossoverLow;
    op.crossoverHighHz  = p.ottCrossoverHigh;
    op.bands[0] = { p.ottLowUp,  p.ottLowDown,  p.ottLowGain };
    op.bands[1] = { p.ottMidUp,  p.ottMidDown,  p.ottMidGain };
    op.bands[2] = { p.ottHighUp, p.ottHighDown, p.ottHighGain };
    ottEffect.process (buffer, op);
}

int FXChain::limiterLatencySamples (const Params& p) const
{
    Limiter::Params lp;
    lp.enable    = p.limEnable;
    lp.lookahead = p.limLookahead;
    return limiterEffect.latencySamples (lp);
}

void FXChain::processEQ (juce::AudioBuffer<float>& buffer, const Params& p)
{
    eq.setCharacter (p.eqCharacter);
    eq.updateBands (p.eqBands);
    eq.process (buffer);
}

} // namespace glitch::fx
