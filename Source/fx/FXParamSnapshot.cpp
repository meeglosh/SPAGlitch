#include "FXParamSnapshot.h"

namespace glitch::fx::params
{

void Snapshot::bind (juce::AudioProcessorValueTreeState& apvts)
{
    distEnable = apvts.getRawParameterValue (id::distEnable);
    distType = apvts.getRawParameterValue (id::distType);
    distDrive = apvts.getRawParameterValue (id::distDrive);
    distTone = apvts.getRawParameterValue (id::distTone);
    distMix = apvts.getRawParameterValue (id::distMix);
    chorusEnable = apvts.getRawParameterValue (id::chorusEnable);
    chorusRate = apvts.getRawParameterValue (id::chorusRate);
    chorusDepth = apvts.getRawParameterValue (id::chorusDepth);
    chorusFeedback = apvts.getRawParameterValue (id::chorusFeedback);
    chorusMix = apvts.getRawParameterValue (id::chorusMix);
    delayEnable = apvts.getRawParameterValue (id::delayEnable);
    delaySync = apvts.getRawParameterValue (id::delaySync);
    delayTime = apvts.getRawParameterValue (id::delayTime);
    delayDivision = apvts.getRawParameterValue (id::delayDivision);
    delayFeedback = apvts.getRawParameterValue (id::delayFeedback);
    delayPingPong = apvts.getRawParameterValue (id::delayPingPong);
    delayMix = apvts.getRawParameterValue (id::delayMix);
    reverbEnable = apvts.getRawParameterValue (id::reverbEnable);
    reverbMode = apvts.getRawParameterValue (id::reverbMode);
    reverbPreDelay = apvts.getRawParameterValue (id::reverbPreDelay);
    reverbSize = apvts.getRawParameterValue (id::reverbSize);
    reverbDecay = apvts.getRawParameterValue (id::reverbDecay);
    reverbDamping = apvts.getRawParameterValue (id::reverbDamping);
    reverbModDepth = apvts.getRawParameterValue (id::reverbModDepth);
    reverbLowCut = apvts.getRawParameterValue (id::reverbLowCut);
    reverbHighCut = apvts.getRawParameterValue (id::reverbHighCut);
    reverbWidth = apvts.getRawParameterValue (id::reverbWidth);
    reverbMix = apvts.getRawParameterValue (id::reverbMix);
    eqEnable = apvts.getRawParameterValue (id::eqEnable);
    eqCharacter = apvts.getRawParameterValue (id::eqCharacter);
    modEnable = apvts.getRawParameterValue (id::modEnable);
    modType = apvts.getRawParameterValue (id::modType);
    modRate = apvts.getRawParameterValue (id::modRate);
    modSync = apvts.getRawParameterValue (id::modSync);
    modDivision = apvts.getRawParameterValue (id::modDivision);
    modDepth = apvts.getRawParameterValue (id::modDepth);
    modFeedback = apvts.getRawParameterValue (id::modFeedback);
    modStages = apvts.getRawParameterValue (id::modStages);
    modCentre = apvts.getRawParameterValue (id::modCentre);
    modManual = apvts.getRawParameterValue (id::modManual);
    modWidth = apvts.getRawParameterValue (id::modWidth);
    modMix = apvts.getRawParameterValue (id::modMix);
    tremEnable = apvts.getRawParameterValue (id::tremEnable);
    tremRate = apvts.getRawParameterValue (id::tremRate);
    tremSync = apvts.getRawParameterValue (id::tremSync);
    tremDivision = apvts.getRawParameterValue (id::tremDivision);
    tremDepth = apvts.getRawParameterValue (id::tremDepth);
    tremShape = apvts.getRawParameterValue (id::tremShape);
    tremStereo = apvts.getRawParameterValue (id::tremStereo);
    tremMix = apvts.getRawParameterValue (id::tremMix);
    vibEnable = apvts.getRawParameterValue (id::vibEnable);
    vibRate = apvts.getRawParameterValue (id::vibRate);
    vibSync = apvts.getRawParameterValue (id::vibSync);
    vibDivision = apvts.getRawParameterValue (id::vibDivision);
    vibDepth = apvts.getRawParameterValue (id::vibDepth);
    vibMix = apvts.getRawParameterValue (id::vibMix);
    limEnable = apvts.getRawParameterValue (id::limEnable);
    limDrive = apvts.getRawParameterValue (id::limDrive);
    limCeiling = apvts.getRawParameterValue (id::limCeiling);
    limRelease = apvts.getRawParameterValue (id::limRelease);
    limAutoRelease = apvts.getRawParameterValue (id::limAutoRelease);
    limCharacter = apvts.getRawParameterValue (id::limCharacter);
    limStereoLink = apvts.getRawParameterValue (id::limStereoLink);
    limTruePeak = apvts.getRawParameterValue (id::limTruePeak);
    limLookahead = apvts.getRawParameterValue (id::limLookahead);
    limAutoGain = apvts.getRawParameterValue (id::limAutoGain);
    mbEnable = apvts.getRawParameterValue (id::mbEnable);
    mbMix = apvts.getRawParameterValue (id::mbMix);
    mbXoverLow = apvts.getRawParameterValue (id::mbXoverLow);
    mbXoverHigh = apvts.getRawParameterValue (id::mbXoverHigh);

    for (int b = 0; b < Multiband::numBands; ++b)
    {
        const char* mbKeys[6] { id::mbband::threshold, id::mbband::ratio, id::mbband::upRatio,
                                id::mbband::attack, id::mbband::release, id::mbband::gain };
        for (int k = 0; k < 6; ++k)
            mbBandValues[(size_t) b][(size_t) k] = apvts.getRawParameterValue (id::mbBand (b, mbKeys[k]));
    }

    for (int b = 0; b < ParametricEQ::numBands; ++b)
    {
        const char* keys[6] { id::eqband::enable, id::eqband::type, id::eqband::slope,
                              id::eqband::freq, id::eqband::gain, id::eqband::q };
        for (int k = 0; k < 6; ++k)
            eqBandValues[(size_t) b][(size_t) k] = apvts.getRawParameterValue (id::eqBand (b, keys[k]));
    }
}

void Snapshot::readEqBands (FXChain::Params& p) const
{
    for (int b = 0; b < ParametricEQ::numBands; ++b)
    {
        const auto& v = eqBandValues[(size_t) b];
        auto& band = p.eqBands[(size_t) b];
        band.enabled = load (v[0]) >= 0.5f;
        band.type    = (int) load (v[1]);
        band.slope   = (int) load (v[2]);
        band.freq    = load (v[3]);
        band.gainDb  = load (v[4]);
        band.q       = load (v[5]);
    }
}

void Snapshot::readMbBands (FXChain::Params& p) const
{
    for (int b = 0; b < Multiband::numBands; ++b)
    {
        const auto& v = mbBandValues[(size_t) b];
        auto& band = p.mbBands[(size_t) b];
        band.thresholdDb = load (v[0]);
        band.ratio       = load (v[1]);
        band.upRatio     = load (v[2]);
        band.attackMs    = load (v[3]);
        band.releaseMs   = load (v[4]);
        band.gainDb      = load (v[5]);
    }
}

void Snapshot::read (FXChain::Params& p, double bpm, juce::uint64 packedOrder) const
{
    p.distEnable = load (distEnable) >= 0.5f;
    p.distType = (int) load (distType);
    p.distDrive = load (distDrive);
    p.distToneHz = load (distTone);
    p.distMix = load (distMix);
    p.chorusEnable = load (chorusEnable) >= 0.5f;
    p.chorusRate = load (chorusRate);
    p.chorusDepth = load (chorusDepth);
    p.chorusFeedback = load (chorusFeedback);
    p.chorusMix = load (chorusMix);
    p.delayEnable = load (delayEnable) >= 0.5f;
    p.delaySync = load (delaySync) >= 0.5f;
    p.delayTimeMs = load (delayTime);
    p.delayDivision = (int) load (delayDivision);
    p.delayFeedback = load (delayFeedback);
    p.delayPingPong = load (delayPingPong) >= 0.5f;
    p.delayMix = load (delayMix);
    p.reverbEnable = load (reverbEnable) >= 0.5f;
    p.reverbMode = (int) load (reverbMode);
    p.reverbPreDelay = load (reverbPreDelay);
    p.reverbSize = load (reverbSize);
    p.reverbDecay = load (reverbDecay);
    p.reverbDamping = load (reverbDamping);
    p.reverbModDepth = load (reverbModDepth);
    p.reverbLowCut = load (reverbLowCut);
    p.reverbHighCut = load (reverbHighCut);
    p.reverbWidth = load (reverbWidth);
    p.reverbMix = load (reverbMix);
    p.eqEnable = load (eqEnable) >= 0.5f;
    p.eqCharacter = (int) load (eqCharacter);
    p.modEnable = load (modEnable) >= 0.5f;
    p.modType = (int) load (modType);
    p.modRate = load (modRate);
    p.modSync = load (modSync) >= 0.5f;
    p.modDivision = (int) load (modDivision);
    p.modDepth = load (modDepth);
    p.modFeedback = load (modFeedback);
    p.modStages = (int) load (modStages);
    p.modCentreHz = load (modCentre);
    p.modManualMs = load (modManual);
    p.modWidth = load (modWidth);
    p.modMix = load (modMix);
    p.tremEnable = load (tremEnable) >= 0.5f;
    p.tremRate = load (tremRate);
    p.tremSync = load (tremSync) >= 0.5f;
    p.tremDivision = (int) load (tremDivision);
    p.tremDepth = load (tremDepth);
    p.tremShape = (int) load (tremShape);
    p.tremStereo = load (tremStereo);
    p.tremMix = load (tremMix);
    p.vibEnable = load (vibEnable) >= 0.5f;
    p.vibRate = load (vibRate);
    p.vibSync = load (vibSync) >= 0.5f;
    p.vibDivision = (int) load (vibDivision);
    p.vibDepth = load (vibDepth);
    p.vibMix = load (vibMix);
    p.limEnable = load (limEnable) >= 0.5f;
    p.limDrive = load (limDrive);
    p.limCeiling = load (limCeiling);
    p.limRelease = load (limRelease);
    p.limAutoRelease = load (limAutoRelease) >= 0.5f;
    p.limCharacter = (int) load (limCharacter);
    p.limStereoLink = load (limStereoLink);
    p.limTruePeak = load (limTruePeak) >= 0.5f;
    p.limLookahead = load (limLookahead) >= 0.5f;
    p.limAutoGain = load (limAutoGain) >= 0.5f;
    p.mbEnable = load (mbEnable) >= 0.5f;
    p.mbMix = load (mbMix);
    p.mbCrossoverLow = load (mbXoverLow);
    p.mbCrossoverHigh = load (mbXoverHigh);
    readMbBands (p);

    readEqBands (p);
    p.bpm = bpm > 0.0 ? bpm : 120.0;
    FXChain::unpackOrder (packedOrder, p.order);
}

} // namespace glitch::fx::params
