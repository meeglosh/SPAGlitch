#pragma once
#include "MidiLearn.h"
#include "fx/FXTheme.h"

namespace glitch::ui
{

// Right-click behaviour shared by every knob in the instrument, on the
// faceplate and in the FX drawer alike: a menu to bind the knob to the next
// incoming CC, or to drop a binding it already has.
//
// Attaches to a component rather than subclassing it, so the faceplate's plain
// juce::Sliders and the FX chain's Knob wrappers get identical behaviour.
class MidiLearnTarget final : private juce::MouseListener,
                              private juce::ChangeListener
{
public:
    MidiLearnTarget (MidiLearnManager&, juce::Component& knob,
                     juce::String parameterID, juce::String displayName);
    ~MidiLearnTarget() override;

    // Drawn over the knob by its owner: "CC 74", or "LEARN..." while armed.
    juce::String badge() const;
    bool isArmed() const;

    std::function<void()> onStateChanged;

private:
    void mouseDown (const juce::MouseEvent&) override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    MidiLearnManager& learn;
    juce::Component& component;
    juce::String paramID, name;
};

} // namespace glitch::ui
