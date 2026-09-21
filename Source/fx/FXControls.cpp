#include "FXControls.h"

namespace glitch::fx::ui
{
using namespace glitch::theme;

namespace
{
juce::Font captionFont() { return juce::Font (juce::FontOptions (9.5f, juce::Font::bold)); }

void styleCaption (juce::Label& l, const juce::String& text)
{
    l.setText (text.toUpperCase(), juce::dontSendNotification);
    l.setFont (captionFont());
    l.setColour (juce::Label::textColourId, muted);
    l.setJustificationType (juce::Justification::centred);
    l.setInterceptsMouseClicks (false, false);
}
} // namespace

// ---------------------------------------------------------------- Knob -----

Knob::Knob (juce::AudioProcessorValueTreeState& apvts, const params::Def& def,
            MidiLearnManager* learn)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 15);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                juce::MathConstants<float>::pi * 2.8f, true);
    slider.setColour (juce::Slider::textBoxTextColourId, ink);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxHighlightColourId, sage);
    slider.setTitle (def.name);
    // No suffix or text function here: the SliderAttachment below installs the
    // parameter's own formatting (see params::addToLayout), which is where all
    // readout formatting is defined.

    addAndMakeVisible (slider);
    styleCaption (caption, def.name);
    addAndMakeVisible (caption);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, def.id, slider);
    slider.updateText();

    if (learn != nullptr)
    {
        learnTarget = std::make_unique<MidiLearnTarget> (*learn, *this, def.id, def.name);
        learnTarget->onStateChanged = [this] { repaint(); };
    }
}

void Knob::resized()
{
    auto area = getLocalBounds();
    caption.setBounds (area.removeFromTop (13));
    slider.setBounds (area);
}

void Knob::paint (juce::Graphics& g)
{
    card (g, getLocalBounds().toFloat());
    // The readout sits in its own faint well, as on the faceplate.
    auto value = slider.getBounds().toFloat().removeFromBottom (15.0f).reduced (10.0f, 0.0f);
    g.setColour (sage.withAlpha (0.10f));
    g.fillRoundedRectangle (value, 4.0f);

    if (learnTarget != nullptr)
        drawLearnBadge (g, *this, getLocalBounds().toFloat(), learnTarget->badge(), learnTarget->isArmed());
}

// -------------------------------------------------------------- Choice -----

Choice::Choice (juce::AudioProcessorValueTreeState& apvts, const params::Def& def)
{
    box.addItemList (def.choices, 1);
    box.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (box);
    styleCaption (caption, def.name);
    addAndMakeVisible (caption);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, def.id, box);
}

void Choice::resized()
{
    // Caption + box as one centred group: anchoring to the top left an empty
    // half-cell below the box once the grid cells grew.
    constexpr int captionHeight = 13, boxHeight = 26, gap = 2;
    auto area = getLocalBounds().reduced (5, 0);
    auto group = area.withSizeKeepingCentre (area.getWidth(), captionHeight + gap + boxHeight);
    caption.setBounds (group.removeFromTop (captionHeight));
    group.removeFromTop (gap);
    box.setBounds (group.removeFromTop (boxHeight));
}

void Choice::paint (juce::Graphics& g)
{
    card (g, getLocalBounds().toFloat());
}

// -------------------------------------------------------------- Toggle -----

Toggle::Toggle (juce::AudioProcessorValueTreeState& apvts, const params::Def& def)
    : juce::Button (def.name), caption (juce::String (def.name).toUpperCase())
{
    setClickingTogglesState (true);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, def.id, *this);
}

void Toggle::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto on = getToggleState();

    // A fixed pill centred under the caption. Deriving it from the cell by
    // insetting made it round out into a blob once the grid cells grew.
    card (g, getLocalBounds().toFloat());

    auto area = getLocalBounds().toFloat();
    area.removeFromTop (13.0f);   // caption row, drawn below
    auto body = area.withSizeKeepingCentre (juce::jmin (48.0f, area.getWidth() - 8.0f), 20.0f);
    const auto corner = body.getHeight() * 0.5f;

    g.setColour (on ? electric.withAlpha (0.22f) : paper.withAlpha (0.72f));
    g.fillRoundedRectangle (body, corner);
    g.setColour (on ? electric.withAlpha (0.85f)
                    : sage.withAlpha (over || down ? 0.55f : 0.30f));
    g.drawRoundedRectangle (body, corner, 1.0f);

    // Travelling dot, left when off and right when on.
    const auto d = body.getHeight() - 6.0f;
    const auto cx = on ? body.getRight() - 3.0f - d : body.getX() + 3.0f;
    g.setColour (on ? electric : muted.withAlpha (0.65f));
    g.fillEllipse (cx, body.getY() + 3.0f, d, d);

    g.setFont (captionFont());
    drawGlitchText (g, *this, caption, getLocalBounds().removeFromTop (13),
                    juce::Justification::centred, on ? ink : muted);
}

// --------------------------------------------------------- PowerButton -----

PowerButton::PowerButton (juce::AudioProcessorValueTreeState& apvts,
                          const juce::String& paramID, const juce::String& text)
    : juce::Button (text), label (text.toUpperCase())
{
    setClickingTogglesState (true);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, paramID, *this);
}

void PowerButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto on = getToggleState();
    auto body = getLocalBounds().toFloat().reduced (1.0f);
    const auto corner = body.getHeight() * 0.5f;

    g.setColour (on ? electric.withAlpha (0.18f) : paper.withAlpha (0.8f));
    g.fillRoundedRectangle (body, corner);
    g.setColour (on ? electric.withAlpha (0.9f)
                    : sage.withAlpha (over || down ? 0.5f : 0.28f));
    g.drawRoundedRectangle (body, corner, 1.0f);

    const auto r = 4.0f;
    const auto cy = body.getCentreY();
    g.setColour (on ? electric : muted.withAlpha (0.55f));
    g.fillEllipse (body.getX() + 8.0f, cy - r, r * 2.0f, r * 2.0f);

    g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
    drawGlitchText (g, *this, label, body.withTrimmedLeft (22.0f).withTrimmedRight (6.0f),
                    juce::Justification::centredLeft, on ? ink : muted);
}

// ------------------------------------------------------ DependentEnable -----

DependentEnable::DependentEnable (juce::AudioProcessorValueTreeState& state,
                                  const juce::String& gateParamID,
                                  std::function<bool (float)> shouldEnable,
                                  std::vector<juce::Component*> targets)
    : apvts (state), gate (gateParamID), predicate (std::move (shouldEnable)),
      components (std::move (targets))
{
    apvts.addParameterListener (gate, this);
    apply();
}

DependentEnable::~DependentEnable()
{
    apvts.removeParameterListener (gate, this);
    cancelPendingUpdate();
}

void DependentEnable::parameterChanged (const juce::String&, float)
{
    // Automation delivers this on the audio thread; touching components there
    // is not allowed, so bounce to the message thread.
    triggerAsyncUpdate();
}

void DependentEnable::handleAsyncUpdate() { apply(); }

void DependentEnable::apply()
{
    auto* raw = apvts.getRawParameterValue (gate);
    const auto on = raw != nullptr && predicate (raw->load());
    for (auto* c : components)
        if (c != nullptr)
        {
            c->setEnabled (on);
            c->setAlpha (on ? 1.0f : 0.35f);
        }
}

// ---------------------------------------------------------- ControlGrid ----

ControlGrid::ControlGrid (juce::AudioProcessorValueTreeState& apvts, params::Section section,
                          MidiLearnManager* learn)
{
    for (const auto* def : params::forSection (section))
    {
        Cell cell;
        cell.paramID = def->id;

        switch (def->kind)
        {
            case params::Kind::floatParam:
                cell.component = std::make_unique<Knob> (apvts, *def, learn);
                break;
            case params::Kind::choiceParam:
                cell.component = std::make_unique<Choice> (apvts, *def);
                cell.wide = true;
                break;
            case params::Kind::boolParam:
                cell.component = std::make_unique<Toggle> (apvts, *def);
                break;
        }

        addAndMakeVisible (*cell.component);
        cells.push_back (std::move (cell));
    }
}

int ControlGrid::columnsFor (int width) const
{
    return juce::jmax (1, width / cellWidth);
}

int ControlGrid::heightForWidth (int width) const
{
    const auto columns = columnsFor (width);
    int col = 0, rows = 1;
    for (const auto& cell : cells)
    {
        const auto span = juce::jmin (columns, cell.wide ? 2 : 1);
        if (col + span > columns) { ++rows; col = 0; }
        col += span;
    }
    return rows * cellHeight;
}

int ControlGrid::naturalWidthFor (int width) const
{
    const auto columns = columnsFor (width);
    int spans = 0;
    for (const auto& cell : cells)
        spans += juce::jmin (columns, cell.wide ? 2 : 1);
    return juce::jmin (columns, spans) * cellWidth;
}

void ControlGrid::resized()
{
    const auto columns = columnsFor (getWidth());
    // Distribute the remainder rather than leaving a ragged right edge.
    const auto colWidth = (float) getWidth() / (float) columns;

    int col = 0, row = 0;
    for (auto& cell : cells)
    {
        const auto span = juce::jmin (columns, cell.wide ? 2 : 1);
        if (col + span > columns) { ++row; col = 0; }

        const auto x = juce::roundToInt ((float) col * colWidth);
        const auto right = juce::roundToInt ((float) (col + span) * colWidth);
        cell.component->setBounds (juce::Rectangle<int> (x, row * cellHeight,
                                                         right - x, cellHeight).reduced (2, 2));
        col += span;
    }
}

juce::Component* ControlGrid::componentFor (const juce::String& paramID) const
{
    for (const auto& cell : cells)
        if (cell.paramID == paramID)
            return cell.component.get();
    return nullptr;
}

} // namespace glitch::fx::ui
