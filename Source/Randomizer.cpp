#include "Randomizer.h"
#include "fx/FXParameters.h"
#include "fx/Multiband.h"
#include "GlitchEngine.h"

namespace glitch::rnd
{
namespace fxid = fx::params::id;
namespace
{
struct Entry { juce::String id; LockGroup group; Spec spec; };

// The one list of what RANDOMIZE ALL touches. A parameter absent from it is
// never rolled -- OUTPUT most importantly, because rolling the master level
// works directly against "never silent, never too loud" and buys no sound
// design. Filter type/slope on the EQ bands are likewise left alone (a rolled
// EQ shape is enough without rolling what kind of shape it is).
const std::vector<Entry>& table()
{
    static const std::vector<Entry> entries {
    { "category", LockGroup::sound, { 0.0f, 1.0f, 0.5f, 0.0f } },
    { "pitch", LockGroup::sound, { 0.0f, 1.0f, 0.5f, 0.55f } },
    { "randomness", LockGroup::sound, { 0.0f, 0.35f, 0.1f, 0.5f } },
    { "filterEnable", LockGroup::filter, { 0.0f, 1.0f, 0.6f, 0.3f } },
    { "filterType", LockGroup::filter, { 0.0f, 1.0f, 0.5f, 0.0f } },
    { "cutoff", LockGroup::filter, { 0.15f, 0.85f, 0.5f, 0.25f } },
    { "resonance", LockGroup::filter, { 0.0f, 0.8f, 0.4f, 0.3f } },

    // --- FX chain (windows carried over from SPASynth) ------------------
    { fxid::distEnable, LockGroup::fx, { 0.0f, 1.0f, 0.3f, 0.3f } },
    { fxid::distType, LockGroup::fx, { 0.0f, 1.0f, 0.5f, 0.0f } },
    { fxid::distDrive, LockGroup::fx, { 0.0f, 0.8f, 0.3f, 0.3f } },
    { fxid::distTone, LockGroup::fx, { 0.3f, 1.0f, 0.5f, 0.0f } },
    { fxid::distMix, LockGroup::fx, { 0.3f, 1.0f, 0.5f, 0.0f } },
    { fxid::chorusEnable, LockGroup::fx, { 0.0f, 1.0f, 0.4f, 0.3f } },
    { fxid::chorusRate, LockGroup::fx, { 0.0f, 0.7f, 0.5f, 0.0f } },
    { fxid::chorusDepth, LockGroup::fx, { 0.0f, 1.0f, 0.35f, 0.3f } },
    { fxid::chorusFeedback, LockGroup::fx, { 0.0f, 1.0f, 0.5f, 0.6f } },
    { fxid::chorusMix, LockGroup::fx, { 0.0f, 1.0f, 0.5f, 0.3f } },
    { fxid::delayEnable, LockGroup::fx, { 0.0f, 1.0f, 0.4f, 0.3f } },
    { fxid::delaySync, LockGroup::fx, { 0.0f, 1.0f, 0.8f, 0.5f } },
    { fxid::delayTime, LockGroup::fx, { 0.2f, 0.8f, 0.5f, 0.0f } },
    { fxid::delayDivision, LockGroup::fx, { 0.3f, 0.9f, 0.5f, 0.0f } },
    { fxid::delayFeedback, LockGroup::fx, { 0.0f, 0.75f, 0.4f, 0.3f } },
    { fxid::delayPingPong, LockGroup::fx, { 0.0f, 1.0f, 0.5f, 0.0f } },
    { fxid::delayMix, LockGroup::fx, { 0.0f, 0.8f, 0.35f, 0.3f } },
    { fxid::reverbEnable, LockGroup::fx, { 0.0f, 1.0f, 0.6f, 0.3f } },
    { fxid::reverbMode, LockGroup::fx, { 0.0f, 1.0f, 0.5f, 0.0f } },
    { fxid::reverbPreDelay, LockGroup::fx, { 0.0f, 0.4f, 0.5f, 0.0f } },
    { fxid::reverbSize, LockGroup::fx, { 0.0f, 1.0f, 0.5f, 0.0f } },
    { fxid::reverbDecay, LockGroup::fx, { 0.0f, 0.6f, 0.5f, 0.0f } },
    { fxid::reverbDamping, LockGroup::fx, { 0.0f, 1.0f, 0.5f, 0.0f } },
    { fxid::reverbModDepth, LockGroup::fx, { 0.0f, 0.6f, 0.5f, 0.0f } },
    { fxid::reverbLowCut, LockGroup::fx, { 0.0f, 0.5f, 0.5f, 0.0f } },
    { fxid::reverbHighCut, LockGroup::fx, { 0.4f, 1.0f, 0.5f, 0.0f } },
    { fxid::reverbWidth, LockGroup::fx, { 0.4f, 1.0f, 0.5f, 0.0f } },
    { fxid::reverbMix, LockGroup::fx, { 0.0f, 0.8f, 0.3f, 0.3f } },
    { fxid::eqEnable, LockGroup::fx, { 0.0f, 1.0f, 0.3f, 0.4f } },
    { fxid::eqCharacter, LockGroup::fx, { 0.0f, 1.0f, 0.5f, 0.0f } },
    { fxid::modEnable, LockGroup::fx, { 0.0f, 1.0f, 0.4f, 0.3f } },
    { fxid::modRate, LockGroup::fx, { 0.0f, 0.6f, 0.5f, 0.0f } },
    { fxid::modSync, LockGroup::fx, { 0.0f, 1.0f, 0.5f, 0.0f } },
    { fxid::modDepth, LockGroup::fx, { 0.0f, 1.0f, 0.5f, 0.3f } },
    { fxid::modFeedback, LockGroup::fx, { 0.0f, 1.0f, 0.5f, 0.5f } },
    { fxid::modCentre, LockGroup::fx, { 0.0f, 1.0f, 0.5f, 0.0f } },
    { fxid::modManual, LockGroup::fx, { 0.0f, 1.0f, 0.5f, 0.0f } },
    { fxid::modWidth, LockGroup::fx, { 0.0f, 1.0f, 0.5f, 0.0f } },
    { fxid::modMix, LockGroup::fx, { 0.0f, 1.0f, 0.5f, 0.3f } },
    { fxid::tremEnable, LockGroup::fx, { 0.0f, 1.0f, 0.5f, 0.0f } },
    { fxid::tremRate, LockGroup::fx, { 0.0f, 0.6f, 0.5f, 0.0f } },
    { fxid::tremSync, LockGroup::fx, { 0.0f, 1.0f, 0.5f, 0.0f } },
    { fxid::tremDepth, LockGroup::fx, { 0.0f, 1.0f, 0.5f, 0.0f } },
    { fxid::tremStereo, LockGroup::fx, { 0.0f, 1.0f, 0.5f, 0.0f } },
    { fxid::tremMix, LockGroup::fx, { 0.0f, 1.0f, 0.5f, 0.0f } },
    { fxid::vibEnable, LockGroup::fx, { 0.0f, 1.0f, 0.5f, 0.0f } },
    { fxid::vibRate, LockGroup::fx, { 0.0f, 0.6f, 0.5f, 0.0f } },
    { fxid::vibSync, LockGroup::fx, { 0.0f, 1.0f, 0.5f, 0.0f } },
    { fxid::vibDepth, LockGroup::fx, { 0.0f, 1.0f, 0.5f, 0.0f } },
    { fxid::vibMix, LockGroup::fx, { 0.0f, 1.0f, 0.5f, 0.0f } },
    { fxid::limEnable, LockGroup::fx, { 0.0f, 1.0f, 0.5f, 0.0f } },
    { fxid::limDrive, LockGroup::fx, { 0.0f, 0.5f, 0.5f, 0.0f } },
    { fxid::limRelease, LockGroup::fx, { 0.0f, 1.0f, 0.5f, 0.0f } },
    { fxid::limAutoRelease, LockGroup::fx, { 0.0f, 1.0f, 0.5f, 0.0f } },
    // Multiband compressor. The crossovers and MIX belong to the module; the
    // per-band controls are generated ids and live in multibandTable() below.
    { fxid::mbEnable, LockGroup::fx, { 0.0f, 1.0f, 0.5f, 0.0f } },
    { fxid::mbMix, LockGroup::fx, { 0.5f, 1.0f, 0.85f, 0.4f } },
    { fxid::mbXoverLow, LockGroup::fx, { 0.25f, 0.75f, 0.5f, 0.3f } },
    { fxid::mbXoverHigh, LockGroup::fx, { 0.25f, 0.75f, 0.5f, 0.3f } },
    };
    return entries;
}

// The EQ's eight bands are generated ids, so they are appended rather than
// listed. Type and slope stay fixed, as they do in SPASynth.
const std::vector<Entry>& eqBandTable()
{
    static const std::vector<Entry> entries = []
    {
        std::vector<Entry> v;
        for (int b = 0; b < fx::ParametricEQ::numBands; ++b)
        {
            v.push_back ({ fxid::eqBand (b, fxid::eqband::enable), LockGroup::fx, { 0.0f, 1.0f, 0.2f, 0.5f } });
            v.push_back ({ fxid::eqBand (b, fxid::eqband::freq),   LockGroup::fx, { 0.0f, 1.0f, 0.5f, 0.0f } });
            v.push_back ({ fxid::eqBand (b, fxid::eqband::gain),   LockGroup::fx, { 0.0f, 1.0f, 0.5f, 0.6f } });
            v.push_back ({ fxid::eqBand (b, fxid::eqband::q),      LockGroup::fx, { 0.0f, 1.0f, 0.3f, 0.0f } });
        }
        return v;
    }();
    return entries;
}
// The compressor's three bands are generated ids too. The windows here are
// what keeps a rolled patch usable: GAIN is held inside +/-6 dB of the +/-24 dB
// it offers, because one band 24 dB up is not a patch, and UP RATIO is biased
// hard toward 1:1 (off) so upward compression is something a roll reaches for
// occasionally rather than every time.
const std::vector<Entry>& multibandTable()
{
    static const std::vector<Entry> entries = []
    {
        std::vector<Entry> v;
        for (int b = 0; b < fx::Multiband::numBands; ++b)
        {
            v.push_back ({ fxid::mbBand (b, fxid::mbband::threshold), LockGroup::fx, { 0.35f, 0.85f, 0.6f, 0.3f } });
            v.push_back ({ fxid::mbBand (b, fxid::mbband::ratio),     LockGroup::fx, { 0.0f, 0.7f, 0.35f, 0.3f } });
            // Wide, because the parameter's own skew already puts 1:1 to 2:1
            // across the bottom half of the range: a window of 0..0.5 here
            // produced 1.0:1 -- upward compression switched off -- in every
            // roll of the first batch. This spans off to about 7:1.
            v.push_back ({ fxid::mbBand (b, fxid::mbband::upRatio),   LockGroup::fx, { 0.2f, 1.0f, 0.45f, 0.25f } });
            v.push_back ({ fxid::mbBand (b, fxid::mbband::attack),    LockGroup::fx, { 0.1f, 0.9f, 0.5f, 0.2f } });
            v.push_back ({ fxid::mbBand (b, fxid::mbband::release),   LockGroup::fx, { 0.1f, 0.9f, 0.5f, 0.2f } });
            v.push_back ({ fxid::mbBand (b, fxid::mbband::gain),      LockGroup::fx, { 0.375f, 0.625f, 0.5f, 0.4f } });
        }
        return v;
    }();
    return entries;
}
} // namespace

juce::String lockGroupName (LockGroup g)
{
    switch (g)
    {
        case LockGroup::sound:  return "SOUND";
        case LockGroup::filter: return "FILTER";
        case LockGroup::fx:     return "FX";
        case LockGroup::count:  break;
    }
    return {};
}

juce::Random seededGenerator (int seed)
{
    juce::Random rng (seed);
    // Sixteen steps is enough for the multiplier to spread a small seed
    // difference across the high bits JUCE actually reads: measured over the
    // factory batch's seeds, the first draw goes from a 0.61-0.74 huddle to
    // spanning 0.02-0.98.
    for (int i = 0; i < 16; ++i) rng.nextInt();
    return rng;
}

float sampleValue (const Spec& spec, float wildness, juce::Random& rng)
{
    auto lo = spec.minNorm, hi = spec.maxNorm;

    if (wildness > 0.5f)
    {
        // Open the window toward the full range.
        const auto t = (wildness - 0.5f) * 2.0f;
        lo = juce::jmap (t, lo, 0.0f);
        hi = juce::jmap (t, hi, 1.0f);
    }
    else
    {
        // Shrink the window toward the musical centre.
        const auto t = (0.5f - wildness) * 2.0f;
        lo = lo + (spec.biasCentre - lo) * t * 0.8f;
        hi = hi - (hi - spec.biasCentre) * t * 0.8f;
    }

    if (hi < lo) std::swap (lo, hi);
    auto v = lo + rng.nextFloat() * (hi - lo);

    // Bias fades as wildness rises: a full-wild roll is uniform.
    const auto strength = juce::jlimit (0.0f, 1.0f, spec.biasStrength * (1.5f - wildness));
    v += (spec.biasCentre - v) * strength;

    return juce::jlimit (0.0f, 1.0f, v);
}

juce::uint64 chainOrder (juce::uint64 current, juce::uint32 lockedMask, juce::Random& rng)
{
    using Module = fx::FXChain::Module;
    if ((lockedMask & (1u << (int) LockGroup::fx)) != 0)
        return current;

    std::array<Module, fx::FXChain::numModules> order {};
    int count = 0;
    for (int i = 0; i < fx::FXChain::numModules; ++i)
        if ((Module) i != Module::limiter)
            order[(size_t) count++] = (Module) i;

    for (int i = count - 1; i > 0; --i)
        std::swap (order[(size_t) i], order[(size_t) rng.nextInt (i + 1)]);

    order[(size_t) count] = Module::limiter;   // see the header: always last
    return fx::FXChain::packOrder (order.data());
}

void randomizeAll (juce::AudioProcessorValueTreeState& apvts, float wildness,
                   juce::uint32 lockedMask, juce::Random& rng)
{
    const auto locked = [lockedMask] (LockGroup g)
    { return (lockedMask & (1u << (int) g)) != 0; };

    const auto roll = [&] (const std::vector<Entry>& entries)
    {
        for (const auto& entry : entries)
        {
            if (locked (entry.group)) continue;
            if (auto* param = apvts.getParameter (entry.id))
            {
                param->beginChangeGesture();
                param->setValueNotifyingHost (sampleValue (entry.spec, wildness, rng));
                param->endChangeGesture();
            }
        }
    };
    roll (table());
    roll (eqBandTable());
    roll (multibandTable());

    // --- Musicality pass ----------------------------------------------------
    // sampleValue() opens every window toward the full range as WILDNESS rises
    // past 0.5, so a spec's maxNorm is a bias, not a ceiling. Anything that has
    // to hold at ANY wildness is clamped here instead.
    const auto setNorm = [&apvts] (const juce::String& id, float norm)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
    };
    const auto realValue = [&apvts] (const juce::String& id)
    {
        auto* param = apvts.getParameter (id);
        return param != nullptr ? param->convertFrom0to1 (param->getValue()) : 0.0f;
    };

    if (! locked (LockGroup::fx))
    {
        // The loudness ceiling, and the whole reason a roll can be trusted:
        // the limiter is always on afterwards, at its default -0.3 dBFS
        // ceiling, whatever the rest of the chain rolled. Drive is what pushes
        // level INTO that ceiling, so it is capped hard rather than by bias.
        setNorm (fxid::limEnable, 1.0f);
        if (auto* ceiling = apvts.getParameter (fxid::limCeiling))
            ceiling->setValueNotifyingHost (ceiling->getDefaultValue());
        if (realValue (fxid::limDrive) > 9.0f)
            setNorm (fxid::limDrive, 9.0f / 24.0f);

        // A delay near unity feedback never decays; the reverb's own top end
        // is already long for percussive material.
        if (realValue (fxid::delayFeedback) > 0.8f)
            setNorm (fxid::delayFeedback, 0.8f / 0.95f);
    }

    if (! locked (LockGroup::filter))
    {
        // High-pass or band-pass parked above the material's energy is the one
        // way this instrument's filter can mute a patch outright.
        const auto type = (int) realValue ("filterType");
        const bool subtractive = type == 1 /* High Pass */ || type == 2 /* Band Pass */;
        if (subtractive && realValue ("cutoff") > 700000.0f)
            setNorm ("cutoff", 0.4f + rng.nextFloat() * 0.25f);
    }
}

} // namespace glitch::rnd
