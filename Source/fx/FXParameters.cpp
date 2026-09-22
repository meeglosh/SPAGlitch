#include "FXParameters.h"
#include "Multiband.h"

namespace glitch::fx::params
{
namespace
{
juce::NormalisableRange<float> frequencyRange (float min, float max)
{
    juce::NormalisableRange<float> r (min, max);
    r.setSkewForCentre (std::sqrt (min * max)); // log-ish response, geometric centre
    return r;
}

juce::NormalisableRange<float> skewedRange (float min, float max, float centre)
{
    juce::NormalisableRange<float> r (min, max);
    r.setSkewForCentre (centre);
    return r;
}

juce::StringArray divisions() { return divisionNames(); }

void addFloat (std::vector<Def>& p, const char* pid, const char* name, Section s,
               juce::NormalisableRange<float> range, float def, const char* unit = "",
               bool hidden = false)
{
    p.push_back ({ pid, name, s, Kind::floatParam, range, def, unit, {}, hidden });
}

void addChoice (std::vector<Def>& p, const char* pid, const char* name, Section s,
                juce::StringArray choices, float def, bool hidden = false)
{
    p.push_back ({ pid, name, s, Kind::choiceParam, { 0.0f, (float) juce::jmax (1, choices.size() - 1), 1.0f },
                   def, "", std::move (choices), hidden });
}

void addBool (std::vector<Def>& p, const char* pid, const char* name, Section s,
              float def, bool hidden = false)
{
    p.push_back ({ pid, name, s, Kind::boolParam, { 0.0f, 1.0f, 1.0f }, def, "", {}, hidden });
}

std::vector<Def> build()
{
    std::vector<Def> p;
    namespace i = id;

    // --- Distortion ------------------------------------------------------
    addBool   (p, i::distEnable, "On", Section::dist, 0.0f, true);
    addChoice (p, i::distType, "Type", Section::dist, { "Soft", "Hard", "Fold", "Crush" }, 0.0f);
    addFloat  (p, i::distDrive, "Drive", Section::dist, { 0.0f, 1.0f }, 0.3f);
    addFloat  (p, i::distTone, "Tone", Section::dist, frequencyRange (500.0f, 20000.0f), 8000.0f, "Hz");
    addFloat  (p, i::distMix, "Mix", Section::dist, { 0.0f, 1.0f }, 1.0f);

    // --- Chorus ----------------------------------------------------------
    addBool  (p, i::chorusEnable, "On", Section::chorus, 0.0f, true);
    addFloat (p, i::chorusRate, "Rate", Section::chorus, frequencyRange (0.05f, 5.0f), 0.8f, "Hz");
    addFloat (p, i::chorusDepth, "Depth", Section::chorus, { 0.0f, 1.0f }, 0.3f);
    addFloat (p, i::chorusFeedback, "Fdbk", Section::chorus, { -0.9f, 0.9f }, 0.0f);
    addFloat (p, i::chorusMix, "Mix", Section::chorus, { 0.0f, 1.0f }, 0.5f);

    // --- Delay -----------------------------------------------------------
    addBool   (p, i::delayEnable, "On", Section::delay, 0.0f, true);
    addBool   (p, i::delaySync, "Sync", Section::delay, 1.0f);
    addFloat  (p, i::delayTime, "Time", Section::delay, { 1.0f, 2000.0f, 0.0f, 0.4f }, 350.0f, "ms");
    addChoice (p, i::delayDivision, "Div", Section::delay, divisions(), 6.0f /* 1/4 */);
    addFloat  (p, i::delayFeedback, "Fdbk", Section::delay, { 0.0f, 0.95f }, 0.35f);
    addBool   (p, i::delayPingPong, "Ping", Section::delay, 0.0f);
    addFloat  (p, i::delayMix, "Mix", Section::delay, { 0.0f, 1.0f }, 0.35f);

    // --- Reverb ----------------------------------------------------------
    addBool   (p, i::reverbEnable, "On", Section::reverb, 0.0f, true);
    // Room rather than SPASynth's Hall, and shorter/smaller/drier with it.
    // SPASynth's defaults are almost never heard -- a preset sets the reverb
    // before you get to it -- and on SPAGlitch's short percussive hits a 2 s
    // hall at 30% wet washes the instrument out: measured, it peaks 505 ms
    // after the transient and rings for 1.53 s. Room at 1.2 s peaks at 287 ms
    // and rings for 0.59 s, which sits behind the hits instead of over them.
    addChoice (p, i::reverbMode, "Mode", Section::reverb,
               { "Hall", "Plate", "Chamber", "Room", "Spring" }, 3.0f);
    addFloat  (p, i::reverbPreDelay, "Pre", Section::reverb, { 0.0f, 200.0f }, 20.0f, "ms");
    addFloat  (p, i::reverbSize, "Size", Section::reverb, { 0.0f, 1.0f }, 0.4f);
    // Top end held at 8 s: at max Decay + Hall's decay multiplier a wider range
    // produces an effective RT60 near 17 s, which reads as runaway feedback
    // rather than a long tail.
    addFloat  (p, i::reverbDecay, "Decay", Section::reverb, skewedRange (0.2f, 8.0f, 2.5f), 1.2f, "s");
    addFloat  (p, i::reverbDamping, "Damp", Section::reverb, { 0.0f, 1.0f }, 0.5f);
    addFloat  (p, i::reverbModDepth, "Mod", Section::reverb, { 0.0f, 1.0f }, 0.2f);
    addFloat  (p, i::reverbLowCut, "LoCut", Section::reverb, frequencyRange (20.0f, 2000.0f), 20.0f, "Hz");
    addFloat  (p, i::reverbHighCut, "HiCut", Section::reverb, frequencyRange (1000.0f, 20000.0f), 12000.0f, "Hz");
    addFloat  (p, i::reverbWidth, "Width", Section::reverb, { 0.0f, 1.0f }, 1.0f);
    addFloat  (p, i::reverbMix, "Mix", Section::reverb, { 0.0f, 1.0f }, 0.25f);

    // --- EQ --------------------------------------------------------------
    // Every band parameter is hidden from the grid: the curve editor is the
    // control surface for them. Only CHARACTER gets a grid cell.
    addBool   (p, i::eqEnable, "On", Section::eq, 0.0f, true);
    addChoice (p, i::eqCharacter, "Char", Section::eq, { "Clean", "Modern", "Vintage", "Tube" }, 0.0f);
    {
        struct BandDef { float freq; int type; };
        const BandDef defs[ParametricEQ::numBands] = {
            {    80.0f, 1 /* Low Shelf */ }, {   200.0f, 0 }, {  500.0f, 0 },
            {  1200.0f, 0 }, {  3000.0f, 0 }, {  6000.0f, 0 },
            { 10000.0f, 2 /* High Shelf */ }, { 15000.0f, 0 } };

        // The table stores `const char*`, so each generated band id needs an
        // address that outlives build(). These are leaked once, deliberately.
        static std::vector<std::unique_ptr<juce::String>> ownedIDs;
        auto keep = [] (juce::String s)
        {
            ownedIDs.push_back (std::make_unique<juce::String> (std::move (s)));
            return ownedIDs.back()->toRawUTF8();
        };

        for (int b = 0; b < ParametricEQ::numBands; ++b)
        {
            addBool   (p, keep (id::eqBand (b, id::eqband::enable)), "On", Section::eq, 0.0f, true);
            addChoice (p, keep (id::eqBand (b, id::eqband::type)), "Type", Section::eq,
                       // Append-only (ParametricEQ::Type indices).
                       { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut",
                         "Notch", "Band Pass", "Tilt Shelf" }, (float) defs[b].type, true);
            addChoice (p, keep (id::eqBand (b, id::eqband::slope)), "Slope", Section::eq,
                       // Only meaningful for Low Cut / High Cut bands.
                       { "6 dB", "12 dB", "18 dB", "24 dB", "36 dB", "48 dB" }, 1.0f, true);
            addFloat  (p, keep (id::eqBand (b, id::eqband::freq)), "Freq", Section::eq,
                       frequencyRange (20.0f, 20000.0f), defs[b].freq, "Hz", true);
            addFloat  (p, keep (id::eqBand (b, id::eqband::gain)), "Gain", Section::eq,
                       { -24.0f, 24.0f, 0.1f }, 0.0f, "dB", true);
            addFloat  (p, keep (id::eqBand (b, id::eqband::q)), "Q", Section::eq,
                       skewedRange (0.1f, 18.0f, 1.0f), 0.707f, "", true);
        }
    }

    // --- Mod (Phaser / Flanger) ------------------------------------------
    addBool   (p, i::modEnable, "On", Section::mod, 0.0f, true);
    addChoice (p, i::modType, "Type", Section::mod, { "Phaser", "Flanger" }, 0.0f);
    addFloat  (p, i::modRate, "Rate", Section::mod, frequencyRange (0.02f, 8.0f), 0.5f, "Hz");
    addBool   (p, i::modSync, "Sync", Section::mod, 0.0f);
    addChoice (p, i::modDivision, "Div", Section::mod, divisions(), 6.0f);
    addFloat  (p, i::modDepth, "Depth", Section::mod, { 0.0f, 1.0f }, 0.5f);
    addFloat  (p, i::modFeedback, "Fdbk", Section::mod, { -0.95f, 0.95f }, 0.3f);
    addChoice (p, i::modStages, "Stages", Section::mod, { "2", "4", "6", "8", "12" }, 2.0f);
    addFloat  (p, i::modCentre, "Centre", Section::mod, frequencyRange (100.0f, 6000.0f), 800.0f, "Hz");
    addFloat  (p, i::modManual, "Delay", Section::mod, { 0.1f, 20.0f, 0.01f }, 3.0f, "ms");
    addFloat  (p, i::modWidth, "Width", Section::mod, { 0.0f, 1.0f }, 0.5f);
    addFloat  (p, i::modMix, "Mix", Section::mod, { 0.0f, 1.0f }, 0.5f);

    // --- Tremolo / Vibrato (independent, one tab) ------------------------
    addBool   (p, i::tremEnable, "Trem", Section::tremVib, 0.0f, true);
    addFloat  (p, i::tremRate, "T Rate", Section::tremVib, frequencyRange (0.05f, 20.0f), 5.0f, "Hz");
    addBool   (p, i::tremSync, "T Sync", Section::tremVib, 0.0f);
    addChoice (p, i::tremDivision, "T Div", Section::tremVib, divisions(), 6.0f);
    addFloat  (p, i::tremDepth, "T Depth", Section::tremVib, { 0.0f, 1.0f }, 0.5f);
    addChoice (p, i::tremShape, "T Shape", Section::tremVib, { "Sine", "Triangle", "Square", "Saw" }, 0.0f);
    addFloat  (p, i::tremStereo, "T Stereo", Section::tremVib, { 0.0f, 1.0f }, 0.0f);
    addFloat  (p, i::tremMix, "T Mix", Section::tremVib, { 0.0f, 1.0f }, 1.0f);
    addBool   (p, i::vibEnable, "Vib", Section::tremVib, 0.0f, true);
    addFloat  (p, i::vibRate, "V Rate", Section::tremVib, frequencyRange (0.05f, 14.0f), 5.0f, "Hz");
    addBool   (p, i::vibSync, "V Sync", Section::tremVib, 0.0f);
    addChoice (p, i::vibDivision, "V Div", Section::tremVib, divisions(), 6.0f);
    addFloat  (p, i::vibDepth, "V Depth", Section::tremVib, { 0.0f, 1.0f }, 0.4f);
    addFloat  (p, i::vibMix, "V Mix", Section::tremVib, { 0.0f, 1.0f }, 1.0f);

    // --- Limiter / Maximizer ---------------------------------------------
    addBool   (p, i::limEnable, "On", Section::limiter, 0.0f, true);
    addFloat  (p, i::limDrive, "Drive", Section::limiter, { 0.0f, 24.0f, 0.1f }, 0.0f, "dB");
    addFloat  (p, i::limCeiling, "Ceil", Section::limiter, { -12.0f, 0.0f, 0.1f }, -0.3f, "dB");
    addFloat  (p, i::limRelease, "Rel", Section::limiter, { 1.0f, 1000.0f, 0.0f, 0.4f }, 120.0f, "ms");
    addBool   (p, i::limAutoRelease, "Auto Rel", Section::limiter, 0.0f);
    addChoice (p, i::limCharacter, "Char", Section::limiter, { "Clean", "Punchy", "Aggressive" }, 0.0f);
    addFloat  (p, i::limStereoLink, "Link", Section::limiter, { 0.0f, 1.0f }, 1.0f);
    addBool   (p, i::limTruePeak, "True Pk", Section::limiter, 0.0f);
    addBool   (p, i::limLookahead, "Look", Section::limiter, 0.0f);
    addBool   (p, i::limAutoGain, "Auto Gain", Section::limiter, 0.0f);

    // --- Multiband compressor --------------------------------------------
    // Crossovers and the per-band controls are owned by the tab's editor (you
    // drag the crossovers on the graph and the band controls follow the
    // selected band), so they are hidden from the auto-built grid the way the
    // EQ's bands are. MIX is the one control that belongs to the module rather
    // than to a band, so it is the only one the grid draws.
    addBool  (p, i::mbEnable, "On", Section::multiband, 0.0f, true);
    addFloat (p, i::mbMix, "Mix", Section::multiband, { 0.0f, 1.0f }, 1.0f);
    addFloat (p, i::mbXoverLow, "Xover L", Section::multiband,
              frequencyRange (20.0f, 2000.0f), 200.0f, "Hz", true);
    addFloat (p, i::mbXoverHigh, "Xover H", Section::multiband,
              frequencyRange (200.0f, 18000.0f), 2000.0f, "Hz", true);
    {
        static std::vector<std::unique_ptr<juce::String>> ownedMbIDs;
        auto keepMb = [] (juce::String s)
        {
            ownedMbIDs.push_back (std::make_unique<juce::String> (std::move (s)));
            return ownedMbIDs.back()->toRawUTF8();
        };

        // Low bands need slower attacks than high ones: a 60 Hz cycle is 16 ms
        // long, and a detector faster than that follows the waveform instead
        // of the envelope, which is distortion rather than compression.
        const struct { float attack, release; } timing[] {
            { 30.0f, 200.0f }, { 12.0f, 120.0f }, { 5.0f, 80.0f } };

        for (int b = 0; b < Multiband::numBands; ++b)
        {
            addFloat (p, keepMb (i::mbBand (b, i::mbband::threshold)), "Thresh", Section::multiband,
                      { -60.0f, 0.0f, 0.1f }, -24.0f, "dB", true);
            addFloat (p, keepMb (i::mbBand (b, i::mbband::ratio)), "Ratio", Section::multiband,
                      skewedRange (1.0f, 20.0f, 4.0f), 4.0f, ":1", true);
            // 1:1 is off, so the band is an ordinary downward compressor until
            // this is deliberately turned up.
            addFloat (p, keepMb (i::mbBand (b, i::mbband::upRatio)), "Up Ratio", Section::multiband,
                      skewedRange (1.0f, 10.0f, 2.0f), 1.0f, ":1", true);
            addFloat (p, keepMb (i::mbBand (b, i::mbband::attack)), "Attack", Section::multiband,
                      skewedRange (0.1f, 300.0f, 20.0f), timing[b].attack, "ms", true);
            addFloat (p, keepMb (i::mbBand (b, i::mbband::release)), "Release", Section::multiband,
                      skewedRange (5.0f, 2000.0f, 150.0f), timing[b].release, "ms", true);
            addFloat (p, keepMb (i::mbBand (b, i::mbband::gain)), "Gain", Section::multiband,
                      { -24.0f, 24.0f, 0.1f }, 0.0f, "dB", true);
        }
    }

    return p;
}
} // namespace

juce::String id::eqBand (int band, const juce::String& key)
{
    return "fxEQ.band" + juce::String (band) + "." + key;
}

juce::String id::mbBand (int band, const juce::String& key)
{
    return "fxMB.band" + juce::String (band) + "." + key;
}

const juce::StringArray& id::mbBandNames()
{
    static const juce::StringArray names { "LOW", "MID", "HIGH" };
    return names;
}

const juce::StringArray& sectionTabNames()
{
    static const juce::StringArray names { "DIST", "CHORUS", "DELAY", "REVERB",
                                           "EQ", "MOD", "TREM/VIB", "LIMIT", "MBAND" };
    return names;
}

const juce::StringArray& sectionTitles()
{
    static const juce::StringArray names { "Distortion", "Chorus", "Delay", "Reverb",
                                           "Equaliser", "Modulation", "Trem / Vib", "Limiter",
                                           "Multiband Compressor" };
    return names;
}

const std::vector<Def>& all()
{
    static const std::vector<Def> defs = build();
    return defs;
}

const Def* find (const juce::String& paramID)
{
    for (const auto& d : all())
        if (paramID == d.id)
            return &d;
    return nullptr;
}

std::vector<const Def*> forSection (Section s)
{
    std::vector<const Def*> out;
    for (const auto& d : all())
        if (d.section == s && ! d.hidden)
            out.push_back (&d);
    return out;
}

const char* enableID (Section s)
{
    switch (s)
    {
        case Section::dist:    return id::distEnable;
        case Section::chorus:  return id::chorusEnable;
        case Section::delay:   return id::delayEnable;
        case Section::reverb:  return id::reverbEnable;
        case Section::eq:      return id::eqEnable;
        case Section::mod:     return id::modEnable;
        case Section::tremVib: return id::tremEnable;
        case Section::limiter: return id::limEnable;
        case Section::multiband: return id::mbEnable;
        default:               return nullptr;
    }
}

const char* secondEnableID (Section s)
{
    return s == Section::tremVib ? id::vibEnable : nullptr;
}

void addToLayout (juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    for (const auto& d : all())
    {
        const juce::ParameterID pid { d.id, 1 };
        const juce::String name (d.name);

        switch (d.kind)
        {
            case Kind::boolParam:
                layout.add (std::make_unique<juce::AudioParameterBool> (
                    pid, name, d.defaultValue >= 0.5f));
                break;

            case Kind::choiceParam:
                layout.add (std::make_unique<juce::AudioParameterChoice> (
                    pid, name, d.choices,
                    juce::jlimit (0, d.choices.size() - 1, (int) d.defaultValue)));
                break;

            case Kind::floatParam:
            {
                // Formatting lives on the parameter, not the slider: an
                // APVTS SliderAttachment replaces the slider's own text
                // functions with the parameter's, so anything set there would
                // be silently overwritten and readouts would fall back to raw
                // float precision ("7999.9995").
                const juce::String unit (d.unit);
                auto toText = [unit] (float v, int) -> juce::String
                {
                    if (unit == "Hz")
                    {
                        // LFO-rate parameters live under 10 Hz, where rounding
                        // to whole hertz turned 0.5 into "0 Hz".
                        if (v >= 1000.0f) return juce::String (v / 1000.0f, 2) + " kHz";
                        if (v >= 100.0f)  return juce::String (juce::roundToInt (v)) + " Hz";
                        return juce::String (v, v >= 10.0f ? 1 : 2) + " Hz";
                    }
                    if (unit == "ms")
                        return juce::String (v, v >= 100.0f ? 0 : 1) + " ms";
                    if (unit == "dB")
                        return juce::String (v, 1) + " dB";
                    if (unit == "s")
                        return juce::String (v, 2) + " s";
                    if (unit == "%")
                        return juce::String (juce::roundToInt (v)) + " %";
                    if (unit == ":1")
                        return juce::String (v, v >= 10.0f ? 0 : 1) + ":1";
                    return juce::String (v, 2);
                };
                auto fromText = [unit] (const juce::String& t)
                {
                    const auto v = (float) t.getDoubleValue();
                    return unit == "Hz" && t.containsIgnoreCase ("k") ? v * 1000.0f : v;
                };

                layout.add (std::make_unique<juce::AudioParameterFloat> (
                    pid, name, d.range, d.defaultValue,
                    juce::AudioParameterFloatAttributes()
                        .withLabel (d.unit)
                        .withStringFromValueFunction (toText)
                        .withValueFromStringFunction (fromText)));
                break;
            }
        }
    }
}

} // namespace glitch::fx::params
