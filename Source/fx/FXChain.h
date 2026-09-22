#pragma once

#include <juce_dsp/juce_dsp.h>
#include "ModEffect.h"
#include "TremVib.h"
#include "Limiter.h"
#include "Multiband.h"
#include "PlateReverb.h"
#include "ParametricEQ.h"

namespace glitch::fx
{

// Tempo-sync divisions, shared by the delay, mod and trem/vib rate controls.
// Index order is fixed: it is stored in presets as a choice index.
const juce::StringArray& divisionNames();
float divisionBeats (int divisionChoice);

// Global stereo FX chain, processed after the glitch engine's own Kontakt-
// modelled destroy/filter stage and before the output gain. Modules run in the
// order listed in `Params::order`, which the UI reorders by dragging tabs.
//
// Ported from SPASynth, minus its Convolve module: SPAGlitch ships no impulse
// library and the IR loader's background-reshape machinery has no counterpart
// here.
class FXChain
{
public:
    FXChain() = default;

    // Append-only: module ids are serialized in the saved chain order. A new
    // module goes on the end and nowhere else; unpackOrder migrates orders
    // saved before it existed.
    enum class Module { distortion, chorus, delay, reverb, eq, mod, tremVib, limiter, multiband };
    static constexpr int numModules = 9;

    // Pack/unpack the chain order into a uint64 (4 bits/module): a single
    // atomic for the lock-free UI->audio hand-off and compact state storage.
    // Unpack validates the value is a permutation and falls back to the
    // natural order.
    static juce::uint64 packOrder (const Module* order)
    {
        juce::uint64 v = 0;
        for (int i = 0; i < numModules; ++i)
            v |= (juce::uint64) ((int) order[i] & 0xF) << (i * 4);
        return v;
    }
    static juce::uint64 defaultOrderPacked()
    {
        Module def[numModules] { Module::distortion, Module::chorus, Module::mod,
                                 Module::tremVib, Module::delay, Module::reverb,
                                 Module::eq, Module::multiband, Module::limiter };
        return packOrder (def);
    }
    static void unpackOrder (juce::uint64 packed, Module* order)
    {
        if (unpackExactly (packed, numModules, order))
            return;

        // A state or preset written before a module was appended packs one
        // entry short, leaving the unused top nibbles at zero. Read as a
        // full-length order that looks like a duplicate of module 0, which
        // would throw the user's whole saved chain away and silently reset it
        // to the default. So fall back through the shorter lengths and append
        // whatever is missing, in declaration order.
        //
        // This can never misread a valid full-length order as a short one: if
        // the first `length` nibbles really were a permutation of 0..length-1,
        // then the module at position `length` would have to be `length`
        // itself, which is non-zero, so the "rest is empty" test fails.
        for (int length = numModules - 1; length >= 1; --length)
        {
            if (! unpackExactly (packed, length, order))
                continue;

            bool present[16] = {};
            for (int i = 0; i < length; ++i)
                present[(int) order[i]] = true;

            // A module appended after the limiter would push the limiter off
            // the end of the chain, and limiter-last is the arrangement every
            // factory preset and the randomizer deliberately keep -- it is the
            // difference between a safe output and a clipped one. So anything
            // new lands in front of a trailing limiter, not behind it.
            const bool limiterLast = order[length - 1] == Module::limiter;
            int next = limiterLast ? length - 1 : length;

            for (int id = 0; id < numModules; ++id)
                if (! present[id])
                    order[next++] = (Module) id;

            if (limiterLast)
                order[next] = Module::limiter;
            return;
        }

        for (int i = 0; i < numModules; ++i)
            order[i] = (Module) i;
    }

    struct Params
    {
        bool distEnable = false;
        int distType = 0;          // Soft/Hard/Fold/Crush
        float distDrive = 0.3f;
        float distToneHz = 8000.0f;
        float distMix = 1.0f;

        bool chorusEnable = false;
        float chorusRate = 0.8f;
        float chorusDepth = 0.3f;
        float chorusFeedback = 0.0f;
        float chorusMix = 0.5f;

        bool delayEnable = false;
        bool delaySync = true;
        float delayTimeMs = 350.0f;
        int delayDivision = 6;
        float delayFeedback = 0.35f;
        bool delayPingPong = false;
        float delayMix = 0.35f;

        bool reverbEnable = false;
        // Retuned for SPAGlitch's percussive material -- see the reverb block
        // in FXParameters.cpp, which is the authoritative set.
        int reverbMode = 3;         // 0 Hall 1 Plate 2 Chamber 3 Room 4 Spring
        float reverbPreDelay = 20.0f;
        float reverbSize = 0.4f;
        float reverbDecay = 1.2f;   // RT60 seconds
        float reverbDamping = 0.5f; // HF damp
        float reverbModDepth = 0.2f;
        float reverbLowCut = 20.0f;
        float reverbHighCut = 12000.0f;
        float reverbWidth = 1.0f;
        float reverbMix = 0.25f;

        bool eqEnable = false;
        int eqCharacter = 0;   // 0 Clean 1 Modern 2 Vintage 3 Tube
        std::array<ParametricEQ::Band, ParametricEQ::numBands> eqBands {};

        double bpm = 120.0;

        bool modEnable = false;
        int modType = 0;           // 0 = Phaser, 1 = Flanger
        float modRate = 0.5f;
        bool modSync = false;
        int modDivision = 6;
        float modDepth = 0.5f;
        float modFeedback = 0.3f;
        int modStages = 6;
        float modCentreHz = 800.0f;
        float modManualMs = 3.0f;
        float modWidth = 0.5f;
        float modMix = 0.5f;

        bool tremEnable = false;
        float tremRate = 5.0f;
        bool tremSync = false;
        int tremDivision = 6;
        float tremDepth = 0.5f;
        int tremShape = 0;
        float tremStereo = 0.0f;
        float tremMix = 1.0f;

        bool vibEnable = false;
        float vibRate = 5.0f;
        bool vibSync = false;
        int vibDivision = 6;
        float vibDepth = 0.5f;
        float vibMix = 1.0f;

        bool limEnable = false;
        float limDrive = 0.0f;
        float limCeiling = -0.3f;
        float limRelease = 120.0f;
        bool limAutoRelease = false;
        int limCharacter = 0;
        float limStereoLink = 1.0f;
        bool limTruePeak = false;
        bool limLookahead = false;
        bool limAutoGain = false;

        // Three-band compressor. Defaults are a gentle, ordinary downward
        // compressor -- upward ratio 1:1, i.e. off -- so switching the module
        // on does something predictable rather than something dramatic.
        bool mbEnable = false;
        float mbMix = 1.0f;
        float mbCrossoverLow = 200.0f;
        float mbCrossoverHigh = 2000.0f;
        std::array<Multiband::Band, Multiband::numBands> mbBands {};

        // Runtime FX processing order (drag-reorderable, saved with state).
        Module order[numModules] {
            Module::distortion, Module::chorus, Module::mod, Module::tremVib,
            Module::delay, Module::reverb, Module::eq, Module::multiband, Module::limiter
        };
    };

    // Crush (bit-crusher distortion type) drive mappings, shared between the
    // DSP (processDistortion) and the UI transfer-curve staircase so they can
    // never drift apart. Both are exponential so the DRIVE knob stays useful
    // across its whole range: linear bit reduction left most of the knob's
    // travel inaudible (drive 0.5 -> ~9.5 bits).
    //   drive 0    -> 16 bits / no decimation (transparent)
    //   drive 0.5  -> ~6.9 bits / ~6.3-sample hold
    //   drive 0.8  -> ~4.2 bits / ~19-sample hold
    //   drive 1    -> 3 bits / a ~40-sample hold at 48 kHz
    static float crushBitsForDrive (float drive)
    {
        return 16.0f * std::pow (3.0f / 16.0f, drive);
    }
    static float crushHoldForDrive (float drive, double sampleRate)
    {
        return (float) (std::pow (40.0, (double) drive) * (sampleRate / 48000.0));
    }

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    void process (juce::AudioBuffer<float>& buffer, const Params& params);

    // Worst-case ring-out for AudioProcessor::getTailLengthSeconds().
    double tailSeconds (const Params& params) const;

    // Lookahead-limiter latency (reported to the host) + gain reduction meter.
    int limiterLatencySamples (const Params& p) const;
    float limiterGainReductionDb() const { return limiterEffect.gainReductionDb(); }
    float limiterOutputPeak() const { return limiterEffect.outputPeak(); }

    // Signed per-band gain, for the multiband meter: positive is upward
    // gain, negative is downward reduction.
    float multibandGainDb (int band) const { return multibandEffect.bandGainDb (band); }

private:
    // Reads exactly `length` modules and insists the rest of the word is
    // empty, so a short (pre-migration) order is distinguishable from a
    // full-length one. See unpackOrder.
    static bool unpackExactly (juce::uint64 packed, int length, Module* order)
    {
        bool seen[16] = {};
        int tmp[numModules];

        for (int i = 0; i < length; ++i)
        {
            const int id = (int) ((packed >> (i * 4)) & 0xF);
            if (id >= numModules || seen[id])
                return false;
            seen[id] = true;
            tmp[i] = id;
        }

        if ((packed >> (length * 4)) != 0)
            return false;

        for (int i = 0; i < length; ++i)
            order[i] = (Module) tmp[i];
        return true;
    }

    void processDistortion (juce::AudioBuffer<float>&, const Params&);
    void processChorus (juce::AudioBuffer<float>&, const Params&);
    void processDelay (juce::AudioBuffer<float>&, const Params&);
    void processReverb (juce::AudioBuffer<float>&, const Params&);
    void processEQ (juce::AudioBuffer<float>&, const Params&);
    void processMod (juce::AudioBuffer<float>&, const Params&);
    void processTremVib (juce::AudioBuffer<float>&, const Params&);
    void processLimiter (juce::AudioBuffer<float>&, const Params&);
    void processMultiband (juce::AudioBuffer<float>&, const Params&);

    double sampleRate = 48000.0;
    ModEffect modEffect;
    TremVib tremVibEffect;
    Limiter limiterEffect;
    Multiband multibandEffect;

    // Distortion tone filter (post-shaper lowpass), one per channel.
    std::array<juce::dsp::FirstOrderTPTFilter<float>, 2> toneFilters;

    // Crush distortion (sample-and-hold decimation) state, one per channel:
    // the currently-held output sample and a fractional phase accumulator
    // counting down the hold length. Fixed-size, no allocation.
    std::array<float, 2> crushHold {};
    std::array<float, 2> crushPhase {};

    juce::dsp::Chorus<float> chorus;

    // Delay: fixed max 4 s ring buffer per channel.
    juce::AudioBuffer<float> delayBuffer;
    int delayWritePos = 0;
    juce::SmoothedValue<float> delaySamplesSmoothed;

    PlateReverb reverb;

    // 8-band parametric EQ (hand-rolled biquads, character saturation).
    ParametricEQ eq;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FXChain)
};

} // namespace glitch::fx
