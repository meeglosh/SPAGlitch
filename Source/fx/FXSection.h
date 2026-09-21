#pragma once
#include "FXParameters.h"
#include "FXControls.h"
#include "FXDisplays.h"
#include "EqEditor.h"
#include "../Drawer.h"

namespace glitch::fx::ui
{

// A tab button that draws a grip-dots handle and reorders its bar when
// dragged. The move is delegated to the owning DraggableTabs so the button AND
// its content component move together -- the bar's own moveTab would reorder
// only the buttons, desyncing them from the content array.
class DraggableTabButton final : public juce::TabBarButton
{
public:
    DraggableTabButton (const juce::String& name, juce::TabbedButtonBar& bar,
                        std::function<void (int from, int to)> mover);

    void mouseDrag (const juce::MouseEvent&) override;
    void paintButton (juce::Graphics&, bool over, bool down) override;

    // Lit when the effect this tab holds is switched on.
    std::function<bool (const juce::String& tabName)> isEngaged;

private:
    std::function<void (int from, int to)> onMove;
};

// TabbedComponent whose tabs can be dragged to reorder. Tabs are identified by
// name; setModuleNames() maps each name (by index) to a stable module id so the
// current order and a saved order can be read and applied as module ids.
class DraggableTabs final : public juce::TabbedComponent
{
public:
    DraggableTabs();

    std::function<void()> onOrderChanged;                          // after a drag reorder
    std::function<bool (const juce::String& tabName)> isTabEngaged; // drives the lit tab state

    void setModuleNames (juce::StringArray namesByModuleId) { moduleNames = std::move (namesByModuleId); }

    juce::Array<int> currentOrder() const;
    void applyOrder (const juce::Array<int>& ids);

    juce::TabBarButton* createTabButton (const juce::String& name, int index) override;

private:
    juce::StringArray moduleNames;   // index = module id
};

// One FX tab's contents: the power switch and title, the scope, and the
// auto-built control grid.
class FXTab final : public juce::Component
{
public:
    FXTab (juce::AudioProcessorValueTreeState&, params::Section, FXScope::Kind, MidiLearnManager*);

    // EQ and LIMIT supply their own display instead of an FXScope.
    FXTab (juce::AudioProcessorValueTreeState&, params::Section,
           std::unique_ptr<juce::Component> customDisplay, MidiLearnManager*);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void buildHeader (juce::AudioProcessorValueTreeState&);

    params::Section section;
    juce::String title;
    // The EQ curve editor is the tab's control surface, not a read-out beside
    // one, so it takes most of the width; the scopes sit beside their knobs.
    float displayWidthFraction = 0.34f;
    std::unique_ptr<PowerButton> power, secondPower;
    std::unique_ptr<juce::Component> display;
    ControlGrid grid;

    // DELAY dims TIME when synced and DIV when free-running; MOD and TREM/VIB
    // do the same for their own rate pairs.
    std::vector<std::unique_ptr<DependentEnable>> dependencies;
};

// The whole FX band along the bottom of the editor: a header rule, the
// draggable tab strip, and the selected effect's panel.
class FXSection final : public juce::Component,
                       private juce::AudioProcessorValueTreeState::Listener,
                       private juce::AsyncUpdater
{
public:
    FXSection (juce::AudioProcessorValueTreeState&,
               EqEditor::ScopeReader, std::function<double()> sampleRateFn,
               std::function<float()> limiterGainReduction,
               std::function<float()> limiterOutputPeak,
               std::function<float (int)> ottBandGain,
               MidiLearnManager* learn = nullptr);
    ~FXSection() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Folded away, only the header bar remains; the chain keeps processing.
    static constexpr int collapsedHeight = glitch::ui::DrawerHeader::height;
    // Sized to what the tallest tab actually needs -- a two-row control grid
    // under the tab strip -- rather than left at a round number.
    static constexpr int expandedHeight = 260;
    int preferredHeight() const { return header.isCollapsed() ? collapsedHeight : expandedHeight; }

    void setCollapsed (bool);
    bool isCollapsed() const { return header.isCollapsed(); }
    std::function<void()> onCollapsedChanged;

    // The chain order as module ids, and a setter for restoring saved state.
    juce::Array<int> currentOrder() const { return tabs.currentOrder(); }
    void applyOrder (const juce::Array<int>& ids) { tabs.applyOrder (ids); }

    std::function<void (const juce::Array<int>&)> onOrderChanged;

private:
    // A tab lights up when its effect is on, so the strip has to be repainted
    // when an enable parameter moves -- including from host automation, which
    // arrives on the audio thread.
    void parameterChanged (const juce::String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override;

    juce::AudioProcessorValueTreeState& apvts;
    glitch::ui::DrawerHeader header { "03", "CHAIN", "drag tabs to reorder the chain" };
    DraggableTabs tabs;
    juce::StringArray watchedEnables;
};

} // namespace glitch::fx::ui
