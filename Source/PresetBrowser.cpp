#include "PresetBrowser.h"
#include "Plugin.h"

namespace glitch::ui
{
using namespace glitch::theme;

namespace
{
constexpr int rowHeight = 32;   // room for the 14.5pt name
const char* chipLabels[] { "ALL", "FACTORY", "USER" };
const char* chipCategories[] { "", "Factory", "User" };
} // namespace

std::vector<int> PresetBrowser::filterIndices (const std::vector<PresetManager::Info>& presets,
                                               const Filter& f, const juce::StringArray& favouriteKeys)
{
    std::vector<int> out;
    for (size_t i = 0; i < presets.size(); ++i)
    {
        const auto& p = presets[i];
        if (f.category.isNotEmpty() && p.category != f.category) continue;
        if (f.favouritesOnly && ! favouriteKeys.contains (PresetManager::favouriteKey (p))) continue;
        if (f.search.isNotEmpty() && ! p.name.containsIgnoreCase (f.search)) continue;
        out.push_back ((int) i);
    }
    return out;
}

PresetBrowser::PresetBrowser (GlitchProcessor& p, std::function<void()> close)
    : processor (p), onClose (std::move (close))
{
    setOpaque (false);
    setWantsKeyboardFocus (true);

    closeButton.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (closeButton);

    searchBox.setTextToShowWhenEmpty ("Search presets", muted.withAlpha (0.5f));
    searchBox.setColour (juce::TextEditor::backgroundColourId, paper.withAlpha (0.8f));
    searchBox.setColour (juce::TextEditor::outlineColourId, sage.withAlpha (0.3f));
    searchBox.setColour (juce::TextEditor::focusedOutlineColourId, electric.withAlpha (0.6f));
    searchBox.setColour (juce::TextEditor::textColourId, ink);
    searchBox.setEscapeAndReturnKeysConsumed (false);   // Esc bubbles up = close
    searchBox.onTextChange = [this] { filter.search = searchBox.getText(); applyFilter(); };
    addAndMakeVisible (searchBox);

    for (size_t i = 0; i < categoryChips.size(); ++i)
    {
        auto& chip = categoryChips[i];
        chip.setButtonText (chipLabels[i]);
        chip.setClickingTogglesState (false);
        chip.setMouseClickGrabsKeyboardFocus (false);
        chip.onClick = [this, i] { filter.category = chipCategories[i]; applyFilter(); };
        addAndMakeVisible (chip);
    }

    favouritesChip.setMouseClickGrabsKeyboardFocus (false);
    favouritesChip.onClick = [this] { filter.favouritesOnly = ! filter.favouritesOnly; applyFilter(); };
    addAndMakeVisible (favouritesChip);

    list.setRowHeight (rowHeight);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    list.setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (list);

    countLabel.setFont (juce::Font (juce::FontOptions (10.0f)));
    countLabel.setColour (juce::Label::textColourId, muted.withAlpha (0.7f));
    addAndMakeVisible (countLabel);

    saveButton.setMouseClickGrabsKeyboardFocus (false);
    saveButton.onClick = [this] { promptSave(); };
    addAndMakeVisible (saveButton);

    deleteButton.setMouseClickGrabsKeyboardFocus (false);
    deleteButton.onClick = [this]
    {
        const auto row = list.getSelectedRow();
        if (row < 0 || row >= (int) filtered.size()) return;
        const auto& info = processor.presets.all()[(size_t) filtered[(size_t) row]];
        if (info.isUser) processor.presets.remove (info);
    };
    addAndMakeVisible (deleteButton);

    processor.presets.addChangeListener (this);
    refresh();
}

PresetBrowser::~PresetBrowser() { processor.presets.removeChangeListener (this); }

void PresetBrowser::changeListenerCallback (juce::ChangeBroadcaster*) { refresh(); }

void PresetBrowser::refresh() { applyFilter(); }

void PresetBrowser::applyFilter()
{
    filtered = filterIndices (processor.presets.all(), filter, processor.presets.favourites());
    list.updateContent();
    list.repaint();

    countLabel.setText (juce::String (filtered.size()) + " of "
                        + juce::String (processor.presets.all().size()), juce::dontSendNotification);

    for (size_t i = 0; i < categoryChips.size(); ++i)
        categoryChips[i].setColour (juce::TextButton::buttonColourId,
                                    filter.category == chipCategories[i] ? juce::Colour (0xff2c5c52)
                                                                         : juce::Colour (0xff1b332c));
    favouritesChip.setColour (juce::TextButton::buttonColourId,
                              filter.favouritesOnly ? juce::Colour (0xff2c5c52) : juce::Colour (0xff1b332c));
    repaint();
}

int PresetBrowser::getNumRows() { return (int) filtered.size(); }

void PresetBrowser::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (! juce::isPositiveAndBelow (row, (int) filtered.size())) return;
    const auto& info = processor.presets.all()[(size_t) filtered[(size_t) row]];
    const bool loaded = info.name == processor.presets.currentName();

    auto bounds = juce::Rectangle<int> (0, 0, w, h).reduced (2, 1);
    if (selected || loaded)
    {
        g.setColour (loaded ? electric.withAlpha (0.16f) : sage.withAlpha (0.10f));
        g.fillRoundedRectangle (bounds.toFloat(), 4.0f);
    }

    // The star is the hit target for favouriting; the rest of the row loads.
    auto starArea = bounds.removeFromLeft (22);
    const bool fav = processor.presets.isFavourite (info);
    g.setColour (fav ? electric : muted.withAlpha (0.35f));
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText (juce::String::fromUTF8 ("\xe2\x98\x85"), starArea, juce::Justification::centred);

    g.setFont (juce::Font (juce::FontOptions (14.5f, loaded ? juce::Font::bold : juce::Font::plain)));
    drawGlitchText (g, *this, info.name, bounds.withTrimmedRight (54),
                    juce::Justification::centredLeft, loaded ? ink : muted);

    g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
    drawGlitchText (g, *this, info.category.toUpperCase(), bounds.removeFromRight (52),
                    juce::Justification::centredRight, muted.withAlpha (0.45f));
}

void PresetBrowser::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (! juce::isPositiveAndBelow (row, (int) filtered.size())) return;
    const auto& info = processor.presets.all()[(size_t) filtered[(size_t) row]];

    if (e.x < 24) { processor.presets.toggleFavourite (info); return; }
    processor.presets.load (info);
}

void PresetBrowser::promptSave()
{
    saveDialog = std::make_unique<juce::AlertWindow> ("Save preset",
        "Name this patch. Saving over an existing user preset replaces it.",
        juce::MessageBoxIconType::NoIcon);
    saveDialog->addTextEditor ("name", processor.presets.currentName(), "Name");
    saveDialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<PresetBrowser> safe (this);
    saveDialog->enterModalState (true, juce::ModalCallbackFunction::create ([safe] (int result)
    {
        if (safe == nullptr) return;
        if (result == 1)
            safe->processor.presets.save (safe->saveDialog->getTextEditorContents ("name"));
        safe->saveDialog.reset();
    }), false);
}

bool PresetBrowser::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey) { if (onClose) onClose(); return true; }
    return false;
}

void PresetBrowser::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    auto shadow = bounds.removeFromRight (shadowWidth);

    g.setColour (paper);
    g.fillRect (bounds);
    g.setColour (sage.withAlpha (0.35f));
    g.drawVerticalLine (bounds.getRight() - 1, (float) bounds.getY(), (float) bounds.getBottom());

    // Cast onto whatever sits to the right, so the column reads as in front.
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.28f),
                                             (float) shadow.getX(), 0.0f,
                                             juce::Colours::transparentBlack,
                                             (float) shadow.getRight(), 0.0f, false));
    g.fillRect (shadow);

    g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
    drawGlitchText (g, *this, "00  /  PRESETS", titleArea, juce::Justification::centredLeft, muted);

    g.setColour (well.withAlpha (0.6f));
    g.fillRoundedRectangle (listWell.toFloat(), 6.0f);
    g.setColour (sage.withAlpha (0.18f));
    g.drawRoundedRectangle (listWell.toFloat().reduced (0.5f), 6.0f, 1.0f);
}

void PresetBrowser::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromRight (shadowWidth);
    bounds.reduce (10, 10);

    auto header = bounds.removeFromTop (22);
    closeButton.setBounds (header.removeFromRight (22));
    titleArea = header;

    bounds.removeFromTop (8);
    searchBox.setBounds (bounds.removeFromTop (26));

    bounds.removeFromTop (6);
    auto chipRow = bounds.removeFromTop (22);
    favouritesChip.setBounds (chipRow.removeFromRight (28));
    chipRow.removeFromRight (4);
    const auto chipWidth = (chipRow.getWidth() - 3 * (int) categoryChips.size() + 3)
                               / (int) categoryChips.size();
    for (auto& chip : categoryChips)
    {
        chip.setBounds (chipRow.removeFromLeft (chipWidth));
        chipRow.removeFromLeft (3);
    }

    auto footer = bounds.removeFromBottom (24);
    deleteButton.setBounds (footer.removeFromRight (64));
    footer.removeFromRight (4);
    saveButton.setBounds (footer.removeFromRight (72));
    countLabel.setBounds (footer);

    bounds.removeFromBottom (8);
    bounds.removeFromTop (8);
    listWell = bounds;
    list.setBounds (bounds.reduced (1));
}

} // namespace glitch::ui
