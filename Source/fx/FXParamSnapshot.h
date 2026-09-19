#pragma once
#include "FXParameters.h"
#include "FXChain.h"

namespace glitch::fx::params
{

// Cached raw-parameter pointers for the audio thread. bind() runs once on the
// message thread; read() is called per block and does nothing but atomic loads,
// so no parameter lookup ever happens while rendering.
//
// The EQ's 48 band parameters are read separately by readEqBands(), since they
// live in a fixed-size array rather than named fields.
struct Snapshot
{
    void bind (juce::AudioProcessorValueTreeState& apvts);

    // `packedOrder` is the chain order the UI last published (see
    // FXChain::packOrder); `bpm` comes from the host playhead.
    void read (FXChain::Params& p, double bpm, juce::uint64 packedOrder) const;

private:
    void readEqBands (FXChain::Params& p) const;

    static float load (const std::atomic<float>* v) { return v != nullptr ? v->load() : 0.0f; }

    std::atomic<float>* distEnable = nullptr;
    std::atomic<float>* distType = nullptr;
    std::atomic<float>* distDrive = nullptr;
    std::atomic<float>* distTone = nullptr;
    std::atomic<float>* distMix = nullptr;
    std::atomic<float>* chorusEnable = nullptr;
    std::atomic<float>* chorusRate = nullptr;
    std::atomic<float>* chorusDepth = nullptr;
    std::atomic<float>* chorusFeedback = nullptr;
    std::atomic<float>* chorusMix = nullptr;
    std::atomic<float>* delayEnable = nullptr;
    std::atomic<float>* delaySync = nullptr;
    std::atomic<float>* delayTime = nullptr;
    std::atomic<float>* delayDivision = nullptr;
    std::atomic<float>* delayFeedback = nullptr;
    std::atomic<float>* delayPingPong = nullptr;
    std::atomic<float>* delayMix = nullptr;
    std::atomic<float>* reverbEnable = nullptr;
    std::atomic<float>* reverbMode = nullptr;
    std::atomic<float>* reverbPreDelay = nullptr;
    std::atomic<float>* reverbSize = nullptr;
    std::atomic<float>* reverbDecay = nullptr;
    std::atomic<float>* reverbDamping = nullptr;
    std::atomic<float>* reverbModDepth = nullptr;
    std::atomic<float>* reverbLowCut = nullptr;
    std::atomic<float>* reverbHighCut = nullptr;
    std::atomic<float>* reverbWidth = nullptr;
    std::atomic<float>* reverbMix = nullptr;
    std::atomic<float>* eqEnable = nullptr;
    std::atomic<float>* eqCharacter = nullptr;
    std::atomic<float>* modEnable = nullptr;
    std::atomic<float>* modType = nullptr;
    std::atomic<float>* modRate = nullptr;
    std::atomic<float>* modSync = nullptr;
    std::atomic<float>* modDivision = nullptr;
    std::atomic<float>* modDepth = nullptr;
    std::atomic<float>* modFeedback = nullptr;
    std::atomic<float>* modStages = nullptr;
    std::atomic<float>* modCentre = nullptr;
    std::atomic<float>* modManual = nullptr;
    std::atomic<float>* modWidth = nullptr;
    std::atomic<float>* modMix = nullptr;
    std::atomic<float>* tremEnable = nullptr;
    std::atomic<float>* tremRate = nullptr;
    std::atomic<float>* tremSync = nullptr;
    std::atomic<float>* tremDivision = nullptr;
    std::atomic<float>* tremDepth = nullptr;
    std::atomic<float>* tremShape = nullptr;
    std::atomic<float>* tremStereo = nullptr;
    std::atomic<float>* tremMix = nullptr;
    std::atomic<float>* vibEnable = nullptr;
    std::atomic<float>* vibRate = nullptr;
    std::atomic<float>* vibSync = nullptr;
    std::atomic<float>* vibDivision = nullptr;
    std::atomic<float>* vibDepth = nullptr;
    std::atomic<float>* vibMix = nullptr;
    std::atomic<float>* limEnable = nullptr;
    std::atomic<float>* limDrive = nullptr;
    std::atomic<float>* limCeiling = nullptr;
    std::atomic<float>* limRelease = nullptr;
    std::atomic<float>* limAutoRelease = nullptr;
    std::atomic<float>* limCharacter = nullptr;
    std::atomic<float>* limStereoLink = nullptr;
    std::atomic<float>* limTruePeak = nullptr;
    std::atomic<float>* limLookahead = nullptr;
    std::atomic<float>* limAutoGain = nullptr;

    // [band][enable, type, slope, freq, gain, q]
    std::array<std::array<std::atomic<float>*, 6>, ParametricEQ::numBands> eqBandValues {};
};

} // namespace glitch::fx::params
