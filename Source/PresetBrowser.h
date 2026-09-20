#pragma once
#include "PresetManager.h"
#include "fx/FXTheme.h"
#include "KeyboardFocus.h"

class GlitchProcessor;

namespace glitch::ui
{

// Preset drawer, after SPASynth's: a column that opens to the LEFT of the
// instrument (the editor widens to make room, so it covers nothing), with live
// search, Factory/User/favourite chips, a favourites star per row, and save /
// delete for user patches. Esc or the X closes it.
class PresetBrowser final : public juce::Component,
                            private juce::ChangeListener,
                            private juce::ListBoxModel,
                            private juce::ComponentListener
{
public:
    static constexpr int width = 320;

    PresetBrowser (GlitchProcessor&, std::function<void()> onClose);
    ~PresetBrowser() override;

    // Pure filtering, so the rules are testable without a UI.
    struct Filter
    {
        juce::String search;      // case-insensitive substring of the name
        juce::String category;    // "" = all, else "Factory" / "User"
        bool favouritesOnly = false;
    };
    static std::vector<int> filterIndices (const std::vector<PresetManager::Info>&,
                                           const Filter&, const juce::StringArray& favouriteKeys);

    void refresh();

    // The search box is the one thing here that SHOULD take focus on a click;
    // the editor's tree-wide sweep clears that, so it is restored afterwards.
    void restoreSearchFocusGrab() { searchBox.setMouseClickGrabsKeyboardFocus (true); }

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    // ListBox builds its rows lazily while scrolling, so fresh RowComponents
    // keep arriving after construction and have to be swept as they appear.
    void componentChildrenChanged (juce::Component&) override;
    void applyFilter();
    void promptSave();

    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;

    GlitchProcessor& processor;
    std::function<void()> onClose;

    juce::TextButton closeButton { juce::String::fromUTF8 ("\xc3\x97") };
    juce::TextEditor searchBox;
    std::array<juce::TextButton, 3> categoryChips;   // ALL / FACTORY / USER
    juce::TextButton favouritesChip { juce::String::fromUTF8 ("\xe2\x98\x85") };
    juce::ListBox list { {}, this };
    juce::Label countLabel;
    juce::TextButton saveButton { "SAVE..." }, deleteButton { "DELETE" };

    std::vector<int> filtered;
    Filter filter;
    juce::Rectangle<int> titleArea, listWell;
    std::unique_ptr<juce::AlertWindow> saveDialog;

    static constexpr int shadowWidth = 10;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBrowser)
};

} // namespace glitch::ui
