#pragma once
#include "FXControls.h"
#include "FXParameters.h"
#include "FXTheme.h"
#include <array>

namespace glitch::fx::ui
{

// The crossover graph: a log frequency axis with the two crossover points
// drawn as draggable vertical handles, the three bands tinted between them,
// and each band's live gain reduction filled from its centre line. Clicking a
// band selects it; dragging a handle moves that crossover.
class CrossoverGraph final : public juce::Component,
                             private juce::Timer
{
public:
    CrossoverGraph (juce::AudioProcessorValueTreeState&, std::function<float (int)> bandGainDb);

    void setSelectedBand (int band);
    std::function<void (int)> onBandSelected;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    juce::Rectangle<float> plotArea() const;
    float xForHz (float hz) const;
    float hzForX (float x) const;
    float crossover (int which) const;              // 0 = low|mid, 1 = mid|high
    void setCrossover (int which, float hz);
    int handleNear (float x) const;                 // -1 when nothing is close
    int bandAt (float x) const;

    juce::AudioProcessorValueTreeState& apvts;
    std::function<float (int)> readBandGain;
    std::array<float, 3> shown {};
    int selected = 0;
    int dragging = -1;
    int hovered = -1;
};

// The multiband tab's whole control surface: the graph on the left, and on the
// right the controls for whichever band is selected.
//
// Three bands times six controls will not fit in the FX band at once, and a
// compressor is edited one band at a time anyway, so only the selected band's
// controls are shown -- every band's gain reduction stays visible in the graph
// meanwhile. All eighteen knobs are built up front and hidden rather than
// rebuilt on selection, so no parameter attachment is ever torn down while the
// audio thread is reading it.
class MultibandEditor final : public juce::Component
{
public:
    MultibandEditor (juce::AudioProcessorValueTreeState&,
                     std::function<float (int)> bandGainDb,
                     MidiLearnManager* learn);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void selectBand (int band);

    juce::AudioProcessorValueTreeState& apvts;
    CrossoverGraph graph;
    std::array<std::unique_ptr<juce::Button>, 3> bandButtons;
    std::array<std::array<std::unique_ptr<Knob>, 6>, 3> knobs;
    int selected = 0;
};

} // namespace glitch::fx::ui
