#pragma once
#include "FXParameters.h"
#include "FXTheme.h"
#include "../MidiLearnMenu.h"

namespace glitch::fx::ui
{
// Pulled in from the instrument-wide UI namespace: the FX knobs get exactly
// the same right-click MIDI learn the faceplate knobs do.
using glitch::ui::MidiLearnTarget;
using glitch::MidiLearnManager;


// A captioned rotary knob. The rotary itself is a plain juce::Slider, so it
// inherits SpaLookAndFeel::drawRotarySlider from the editor above it and looks
// identical to the faceplate's knobs.
class Knob final : public juce::Component
{
public:
    // `learn` may be null (tests build panels without a processor).
    Knob (juce::AudioProcessorValueTreeState&, const params::Def&, MidiLearnManager* learn);

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    juce::Slider slider;
    juce::Label caption;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    std::unique_ptr<MidiLearnTarget> learnTarget;
};

// A captioned combo box for choice parameters.
class Choice final : public juce::Component
{
public:
    Choice (juce::AudioProcessorValueTreeState&, const params::Def&);

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    juce::ComboBox box;
    juce::Label caption;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

// A pill toggle for bool parameters -- drawn rather than a ToggleButton so it
// reads at the small sizes the grid uses.
class Toggle final : public juce::Button
{
public:
    Toggle (juce::AudioProcessorValueTreeState&, const params::Def&);

private:
    void paintButton (juce::Graphics&, bool over, bool down) override;

    juce::String caption;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

// The lit power switch in an FX tab's header.
class PowerButton final : public juce::Button
{
public:
    PowerButton (juce::AudioProcessorValueTreeState&, const juce::String& paramID,
                 const juce::String& label);

private:
    void paintButton (juce::Graphics&, bool over, bool down) override;

    juce::String label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

// Dims (and disables) a set of components while a gate parameter says they are
// inert -- e.g. DELAY's TIME knob while SYNC is on, and its DIV box while off.
class DependentEnable final : private juce::AudioProcessorValueTreeState::Listener,
                              private juce::AsyncUpdater
{
public:
    DependentEnable (juce::AudioProcessorValueTreeState&, const juce::String& gateParamID,
                     std::function<bool (float)> shouldEnable,
                     std::vector<juce::Component*> targets);
    ~DependentEnable() override;

private:
    void parameterChanged (const juce::String&, float) override;
    void handleAsyncUpdate() override;   // parameterChanged can arrive on the audio thread
    void apply();

    juce::AudioProcessorValueTreeState& apvts;
    juce::String gate;
    std::function<bool (float)> predicate;
    std::vector<juce::Component*> components;
};

// Builds one FX tab's controls straight from the parameter table: knobs for
// floats, combo boxes for choices, pill toggles for bools. New table entries
// appear automatically, with no per-panel layout code.
class ControlGrid final : public juce::Component
{
public:
    ControlGrid (juce::AudioProcessorValueTreeState&, params::Section, MidiLearnManager* learn);

    void resized() override;

    // Height this grid needs at a given width (the grid wraps).
    int heightForWidth (int width) const;

    // Width the grid actually fills at a given available width. A tab with
    // only a few controls would otherwise stretch them across the whole band
    // and leave the row looking abandoned at one edge; the owner centres this.
    int naturalWidthFor (int width) const;

    // The component built for a parameter id, for wiring cross-parameter
    // behaviour after construction. Null if the parameter has no grid cell.
    juce::Component* componentFor (const juce::String& paramID) const;

private:
    struct Cell
    {
        juce::String paramID;
        std::unique_ptr<juce::Component> component;
        bool wide = false;   // combo boxes and toggles span two columns
    };

    // Sized so the rotary is still readable after the look-and-feel's inset,
    // and so the busiest tab (TREM/VIB, 15 column-spans) wraps to exactly two
    // rows at the band's width.
    static constexpr int cellWidth = 80;
    static constexpr int cellHeight = 84;

    int columnsFor (int width) const;

    std::vector<Cell> cells;
};

} // namespace glitch::fx::ui
