#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>

namespace glitch::fx
{

// Three-band upward/downward compressor in the OTT mould: split the signal at
// two crossovers and, in every band, both lift what is quiet and hold down what
// is loud, so each band comes out dense and level rather than dynamic.
//
// Upward compression is the half that gives the effect its character and its
// danger. It raises anything below the threshold, which includes noise, room
// tone and the tail of a sound that was meant to end, so the boost here is both
// clamped (never more than the band's UP amount is worth) and tapered to
// nothing below a noise floor. Without that taper a quiet passage turns into a
// hiss swell, and RANDOMIZE would eventually find that patch.
//
// The crossover is a pair of 4th-order Linkwitz-Riley filters, with the low
// band passed through an allpass at the upper crossover so all three bands
// carry the same phase shift and sum flat when nothing is compressing. That
// flat sum is an invariant the tests assert rather than a claim.
class OTT
{
public:
    static constexpr int numBands = 3;

    struct Band
    {
        float up = 0.0f;        // 0..1, how much quiet material is lifted
        float down = 0.0f;      // 0..1, how much loud material is held down
        float gainDb = 0.0f;    // post-compression band trim
    };

    struct Params
    {
        bool enable = false;
        float depth = 1.0f;             // wet/dry for the module as a whole
        float timePercent = 100.0f;     // scales every attack and release
        float inGainDb = 0.0f;
        float outGainDb = 0.0f;
        float crossoverLowHz = 90.0f;   // low | mid
        float crossoverHighHz = 2500.0f;// mid | high
        std::array<Band, numBands> bands {};
    };

    void prepare (double sr, int maxBlock)
    {
        sampleRate = juce::jmax (8000.0, sr);
        const juce::dsp::ProcessSpec spec { sampleRate,
                                            (juce::uint32) juce::jmax (1, maxBlock),
                                            2 };
        splitLow.prepare (spec);
        splitHigh.prepare (spec);
        alignLow.prepare (spec);
        alignLow.setType (juce::dsp::LinkwitzRileyFilterType::allpass);
        reset();
    }

    void reset()
    {
        splitLow.reset();
        splitHigh.reset();
        alignLow.reset();
        for (auto& b : bands)
            b = {};
    }

    // Signed, so a meter can show both directions: positive is upward boost,
    // negative is downward reduction.
    float bandGainDb (int band) const
    {
        return juce::isPositiveAndBelow (band, numBands) ? bands[(size_t) band].meterDb : 0.0f;
    }

    void process (juce::AudioBuffer<float>& buffer, const Params& p)
    {
        const int n = buffer.getNumSamples();
        const int numCh = juce::jmin (2, buffer.getNumChannels());
        if (n <= 0 || numCh <= 0)
            return;

        // The upper crossover has to stay above the lower one or the mid band
        // inverts and the sum stops being flat. Clamping here rather than
        // trusting the parameter ranges keeps a preset written by an older
        // build from producing a broken split.
        const float lowHz  = juce::jlimit (30.0f, 1000.0f, p.crossoverLowHz);
        const float highHz = juce::jlimit (lowHz * 1.5f, 16000.0f, p.crossoverHighHz);
        splitLow.setCutoffFrequency (lowHz);
        splitHigh.setCutoffFrequency (highHz);
        alignLow.setCutoffFrequency (highHz);

        const float inGain  = juce::Decibels::decibelsToGain (p.inGainDb);
        const float outGain = juce::Decibels::decibelsToGain (p.outGainDb);
        const float depth   = juce::jlimit (0.0f, 1.0f, p.depth);
        const float timeScale = juce::jlimit (0.1f, 5.0f, p.timePercent * 0.01f);

        // Per-band constants, hoisted out of the sample loop.
        struct Settings { float attack, release, downSlope, upSlope, maxUpDb; };
        std::array<Settings, numBands> settings {};
        std::array<float, numBands> bandTrim {};

        for (int b = 0; b < numBands; ++b)
        {
            const auto& src = p.bands[(size_t) b];
            const float up   = juce::jlimit (0.0f, 1.0f, src.up);
            const float down = juce::jlimit (0.0f, 1.0f, src.down);

            // Low bands move slower than high ones, as they must: a 60 Hz cycle
            // is 16 ms long, and a detector faster than that follows the
            // waveform instead of the envelope and turns into distortion.
            const float baseAttack  = baseAttackMs[(size_t) b]  * timeScale;
            const float baseRelease = baseReleaseMs[(size_t) b] * timeScale;

            settings[(size_t) b] = {
                coefficientFor (baseAttack),
                coefficientFor (baseRelease),
                // Amount 0..1 maps to a 1:1..1:6 ratio, expressed as the slope
                // of the dB-in/dB-out line so the per-sample maths is a
                // multiply rather than a divide.
                1.0f - 1.0f / (1.0f + 5.0f * down),
                1.0f - 1.0f / (1.0f + 5.0f * up),
                maxUpwardDb * up
            };
            bandTrim[(size_t) b] = juce::Decibels::decibelsToGain (src.gainDb);
        }

        std::array<float, numBands> peakGainDb {};
        std::array<bool, numBands> sawPeak {};

        auto* left  = buffer.getWritePointer (0);
        auto* right = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

        for (int i = 0; i < n; ++i)
        {
            const float dryL = left[i];
            const float dryR = right != nullptr ? right[i] : dryL;

            const float inL = dryL * inGain;
            const float inR = dryR * inGain;

            // Split: low | rest, then rest into mid | high. The low band then
            // goes through the allpass so it picks up the same phase shift the
            // upper split imposed on the other two.
            float lowL, restL, lowR, restR;
            splitLow.processSample (0, inL, lowL, restL);
            splitLow.processSample (1, inR, lowR, restR);

            float midL, highL, midR, highR;
            splitHigh.processSample (0, restL, midL, highL);
            splitHigh.processSample (1, restR, midR, highR);

            lowL = alignLow.processSample (0, lowL);
            lowR = alignLow.processSample (1, lowR);

            const std::array<float, numBands> bandL { lowL, midL, highL };
            const std::array<float, numBands> bandR { lowR, midR, highR };

            float wetL = 0.0f, wetR = 0.0f;

            for (int b = 0; b < numBands; ++b)
            {
                auto& state = bands[(size_t) b];
                const auto& set = settings[(size_t) b];

                // One detector for both channels, so compression never pulls
                // the stereo image to one side.
                const float detector = juce::jmax (std::abs (bandL[(size_t) b]),
                                                   std::abs (bandR[(size_t) b]));
                const float coeff = detector > state.env ? set.attack : set.release;
                state.env = detector + coeff * (state.env - detector);

                const float levelDb = juce::Decibels::gainToDecibels (state.env, silenceDb);

                float gainDb = 0.0f;
                if (levelDb > downThresholdDb)
                    gainDb -= (levelDb - downThresholdDb) * set.downSlope;

                if (levelDb < upThresholdDb)
                {
                    // Taper the lift away as the band approaches silence, so
                    // the noise floor is never what gets compressed upward.
                    const float taper = juce::jlimit (0.0f, 1.0f,
                                                      (levelDb - noiseFloorDb) / taperRangeDb + 1.0f);
                    gainDb += (upThresholdDb - levelDb) * set.upSlope * taper;
                }

                gainDb = juce::jlimit (-maxDownwardDb, set.maxUpDb, gainDb);
                state.meterDb = gainDb;

                if (! sawPeak[(size_t) b] || std::abs (gainDb) > std::abs (peakGainDb[(size_t) b]))
                {
                    peakGainDb[(size_t) b] = gainDb;
                    sawPeak[(size_t) b] = true;
                }

                const float gain = juce::Decibels::decibelsToGain (gainDb) * bandTrim[(size_t) b];
                wetL += bandL[(size_t) b] * gain;
                wetR += bandR[(size_t) b] * gain;
            }

            wetL *= outGain;
            wetR *= outGain;

            left[i] = dryL + depth * (wetL - dryL);
            if (right != nullptr)
                right[i] = dryR + depth * (wetR - dryR);
        }

        splitLow.snapToZero();
        splitHigh.snapToZero();
        alignLow.snapToZero();

        // The meter shows the block's extreme rather than its last sample,
        // which would otherwise be whatever the envelope happened to be on at
        // the block boundary and would flicker.
        for (int b = 0; b < numBands; ++b)
            if (sawPeak[(size_t) b])
                bands[(size_t) b].meterDb = peakGainDb[(size_t) b];
    }

private:
    float coefficientFor (float milliseconds) const
    {
        const float seconds = juce::jmax (1.0e-5f, milliseconds * 0.001f);
        return std::exp (-1.0f / ((float) sampleRate * seconds));
    }

    struct BandState
    {
        float env = 0.0f;
        float meterDb = 0.0f;
    };

    // Thresholds are fixed rather than exposed: OTT's whole interface premise
    // is that you set how much of each direction you want, not where each one
    // starts. These sit low enough that ordinary material is always touching
    // both the upward and the downward side, which is what makes it dense.
    static constexpr float downThresholdDb = -28.0f;
    static constexpr float upThresholdDb   = -32.0f;
    static constexpr float maxUpwardDb     = 24.0f;
    static constexpr float maxDownwardDb   = 36.0f;
    static constexpr float noiseFloorDb    = -60.0f;
    static constexpr float taperRangeDb    = 18.0f;
    static constexpr float silenceDb       = -100.0f;

    static constexpr std::array<float, numBands> baseAttackMs  { 15.0f, 8.0f, 4.0f };
    static constexpr std::array<float, numBands> baseReleaseMs { 150.0f, 80.0f, 50.0f };

    double sampleRate = 48000.0;
    juce::dsp::LinkwitzRileyFilter<float> splitLow, splitHigh, alignLow;
    std::array<BandState, numBands> bands {};

    JUCE_LEAK_DETECTOR (OTT)
};

} // namespace glitch::fx
