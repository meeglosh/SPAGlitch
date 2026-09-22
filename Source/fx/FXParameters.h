#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include "FXChain.h"

// Parameter registry for the ported SPASynth FX chain.
//
// One descriptor table drives three things that must never disagree: the APVTS
// layout, the auto-built control grid in each FX tab, and the snapshot handed
// to FXChain::process on the audio thread. Adding a parameter is a single
// table entry.
namespace glitch::fx::params
{

enum class Section { dist, chorus, delay, reverb, eq, mod, tremVib, limiter, multiband, count };
inline constexpr int numSections = (int) Section::count;

// Tab labels, indexed by Section AND by FXChain::Module -- the two enums are
// deliberately kept in the same order so a tab index is a module id.
const juce::StringArray& sectionTabNames();
const juce::StringArray& sectionTitles();

enum class Kind { floatParam, choiceParam, boolParam };

struct Def
{
    const char* id;
    const char* name;
    Section section;
    Kind kind;
    juce::NormalisableRange<float> range;
    float defaultValue;
    const char* unit;
    juce::StringArray choices;
    // Excluded from the auto-built grid: the tab's own power switch (drawn in
    // the tab header) and every EQ band parameter (the curve editor owns them).
    bool hidden;
};

const std::vector<Def>& all();
const Def* find (const juce::String& paramID);

// Parameters a given section contributes to the grid, in declaration order.
std::vector<const Def*> forSection (Section);

void addToLayout (juce::AudioProcessorValueTreeState::ParameterLayout&);

// The on/off parameter for a section. TREM/VIB has two; `second` is the vib
// switch, null elsewhere.
const char* enableID (Section);
const char* secondEnableID (Section);

namespace id
{
    inline constexpr const char* distEnable = "fxDist.enable";
    inline constexpr const char* distType   = "fxDist.type";
    inline constexpr const char* distDrive  = "fxDist.drive";
    inline constexpr const char* distTone   = "fxDist.tone";
    inline constexpr const char* distMix    = "fxDist.mix";

    inline constexpr const char* chorusEnable   = "fxChorus.enable";
    inline constexpr const char* chorusRate     = "fxChorus.rate";
    inline constexpr const char* chorusDepth    = "fxChorus.depth";
    inline constexpr const char* chorusFeedback = "fxChorus.feedback";
    inline constexpr const char* chorusMix      = "fxChorus.mix";

    inline constexpr const char* delayEnable   = "fxDelay.enable";
    inline constexpr const char* delaySync     = "fxDelay.sync";
    inline constexpr const char* delayTime     = "fxDelay.time";
    inline constexpr const char* delayDivision = "fxDelay.division";
    inline constexpr const char* delayFeedback = "fxDelay.feedback";
    inline constexpr const char* delayPingPong = "fxDelay.pingpong";
    inline constexpr const char* delayMix      = "fxDelay.mix";

    inline constexpr const char* reverbEnable   = "fxReverb.enable";
    inline constexpr const char* reverbMode     = "fxReverb.mode";
    inline constexpr const char* reverbPreDelay = "fxReverb.predelay";
    inline constexpr const char* reverbSize     = "fxReverb.size";
    inline constexpr const char* reverbDecay    = "fxReverb.decay";
    inline constexpr const char* reverbDamping  = "fxReverb.damping";
    inline constexpr const char* reverbModDepth = "fxReverb.mod";
    inline constexpr const char* reverbLowCut   = "fxReverb.lowcut";
    inline constexpr const char* reverbHighCut  = "fxReverb.highcut";
    inline constexpr const char* reverbWidth    = "fxReverb.width";
    inline constexpr const char* reverbMix      = "fxReverb.mix";

    inline constexpr const char* eqEnable    = "fxEQ.enable";
    inline constexpr const char* eqCharacter = "fxEQ.character";

    namespace eqband
    {
        inline constexpr const char* enable = "enable";
        inline constexpr const char* type   = "type";
        inline constexpr const char* slope  = "slope";
        inline constexpr const char* freq   = "freq";
        inline constexpr const char* gain   = "gain";
        inline constexpr const char* q      = "q";
    }
    // "fxEQ.band0.freq" and friends.
    juce::String eqBand (int band, const juce::String& key);

    inline constexpr const char* modEnable   = "fxMod.enable";
    inline constexpr const char* modType     = "fxMod.type";
    inline constexpr const char* modRate     = "fxMod.rate";
    inline constexpr const char* modSync     = "fxMod.sync";
    inline constexpr const char* modDivision = "fxMod.division";
    inline constexpr const char* modDepth    = "fxMod.depth";
    inline constexpr const char* modFeedback = "fxMod.feedback";
    inline constexpr const char* modStages   = "fxMod.stages";
    inline constexpr const char* modCentre   = "fxMod.centre";
    inline constexpr const char* modManual   = "fxMod.manual";
    inline constexpr const char* modWidth    = "fxMod.width";
    inline constexpr const char* modMix      = "fxMod.mix";

    inline constexpr const char* tremEnable   = "fxTrem.enable";
    inline constexpr const char* tremRate     = "fxTrem.rate";
    inline constexpr const char* tremSync     = "fxTrem.sync";
    inline constexpr const char* tremDivision = "fxTrem.division";
    inline constexpr const char* tremDepth    = "fxTrem.depth";
    inline constexpr const char* tremShape    = "fxTrem.shape";
    inline constexpr const char* tremStereo   = "fxTrem.stereo";
    inline constexpr const char* tremMix      = "fxTrem.mix";

    inline constexpr const char* vibEnable   = "fxVib.enable";
    inline constexpr const char* vibRate     = "fxVib.rate";
    inline constexpr const char* vibSync     = "fxVib.sync";
    inline constexpr const char* vibDivision = "fxVib.division";
    inline constexpr const char* vibDepth    = "fxVib.depth";
    inline constexpr const char* vibMix      = "fxVib.mix";

    inline constexpr const char* limEnable      = "fxLim.enable";
    inline constexpr const char* limDrive       = "fxLim.drive";
    inline constexpr const char* limCeiling     = "fxLim.ceiling";
    inline constexpr const char* limRelease     = "fxLim.release";
    inline constexpr const char* limAutoRelease = "fxLim.autorelease";
    inline constexpr const char* limCharacter   = "fxLim.character";
    inline constexpr const char* limStereoLink  = "fxLim.link";
    inline constexpr const char* limTruePeak    = "fxLim.truepeak";
    inline constexpr const char* limLookahead   = "fxLim.lookahead";
    inline constexpr const char* limAutoGain    = "fxLim.autogain";

    // Three-band compressor. The per-band controls are generated ids, like the
    // EQ's bands: band 0 is low, 1 is mid, 2 is high.
    inline constexpr const char* mbEnable     = "fxMB.enable";
    inline constexpr const char* mbMix        = "fxMB.mix";
    inline constexpr const char* mbXoverLow   = "fxMB.xoverlow";
    inline constexpr const char* mbXoverHigh  = "fxMB.xoverhigh";

    namespace mbband
    {
        inline constexpr const char* threshold = "thresh";
        inline constexpr const char* ratio     = "ratio";
        inline constexpr const char* upRatio   = "upratio";
        inline constexpr const char* attack    = "attack";
        inline constexpr const char* release   = "release";
        inline constexpr const char* gain      = "gain";
    }
    // "fxMB.band0.thresh" and friends.
    juce::String mbBand (int band, const juce::String& key);

    // Band names as the UI shows them, indexed by band.
    const juce::StringArray& mbBandNames();
}

} // namespace glitch::fx::params
