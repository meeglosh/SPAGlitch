#pragma once
#include "FXParameters.h"
#include "FXTheme.h"
#include <array>

namespace glitch::fx::ui
{

// Base for the FX scopes: watches a set of parameters and repaints, throttled,
// when any of them move. Parameter changes arrive on the audio thread, so the
// listener only sets a flag that the timer picks up.
class DisplayComponent : public juce::Component,
                         private juce::Timer,
                         private juce::AudioProcessorValueTreeState::Listener
{
public:
    DisplayComponent (juce::AudioProcessorValueTreeState&, juce::StringArray paramIDs);
    ~DisplayComponent() override;

    void paint (juce::Graphics&) final;
    void markDirty() { dirty.store (true); }

protected:
    virtual void paintDisplay (juce::Graphics&, juce::Rectangle<float>) = 0;

    float value (const juce::String& paramID) const;   // real-world value

    juce::AudioProcessorValueTreeState& apvts;

private:
    void parameterChanged (const juce::String&, float) override { dirty.store (true); }
    void timerCallback() override;

    juce::StringArray watched;
    std::atomic<bool> dirty { true };
};

// The FX scopes: one class, five characters. MOD and TREM/VIB borrow the
// chorus scope, as they do in SPASynth.
class FXScope final : public DisplayComponent
{
public:
    enum class Kind { distortion, chorus, delay, reverb, eq };

    FXScope (juce::AudioProcessorValueTreeState&, Kind, params::Section);

private:
    void paintDisplay (juce::Graphics&, juce::Rectangle<float>) override;
    static juce::StringArray watchedFor (Kind, params::Section);

    const Kind kind;
    const params::Section section;
};

// Limiter scope: the input/output transfer curve with the ceiling marked, plus
// a live gain-reduction bar. Unlike the other scopes this animates off meter
// telemetry, so it repaints on its own clock.
class LimiterDisplay final : public juce::Component,
                             private juce::Timer
{
public:
    // The meters are read from the processor's FX chain each frame.
    LimiterDisplay (juce::AudioProcessorValueTreeState&,
                    std::function<float()> gainReductionDb,
                    std::function<float()> outputPeak);

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    juce::AudioProcessorValueTreeState& apvts;
    std::function<float()> readGainReduction, readOutputPeak;
    float gr = 0.0f, grPeak = 0.0f, out = 0.0f;
};

// OTT scope: one bidirectional meter per band. The bar grows up from the
// centre line when that band is being lifted and down when it is being held
// back, which is the only way to see at a glance that both halves of the
// effect are working -- a single reduction meter would show upward
// compression as nothing at all.
class OttDisplay final : public juce::Component,
                         private juce::Timer
{
public:
    // `bandGainDb` is read from the processor's FX chain each frame: signed,
    // positive for boost.
    OttDisplay (juce::AudioProcessorValueTreeState&,
                std::function<float (int)> bandGainDb);

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    juce::AudioProcessorValueTreeState& apvts;
    std::function<float (int)> readBandGain;
    std::array<float, 3> shown {};
};

} // namespace glitch::fx::ui
