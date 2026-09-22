#include "MultibandEditor.h"
#include "Multiband.h"

namespace glitch::fx::ui
{
using namespace glitch::theme;

namespace
{
constexpr float minHz = 20.0f;
constexpr float maxHz = 20000.0f;
constexpr float meterRangeDb = 18.0f;

// How close the pointer has to get before a drag grabs a crossover.
constexpr float grabRadius = 9.0f;

float parameterValue (juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    auto* p = apvts.getParameter (id);
    return p != nullptr ? p->convertFrom0to1 (p->getValue()) : 0.0f;
}

juce::String frequencyText (float hz)
{
    return hz >= 1000.0f ? juce::String (hz / 1000.0f, hz >= 10000.0f ? 0 : 1) + " kHz"
                         : juce::String (juce::roundToInt (hz)) + " Hz";
}

// The pill a band is selected with, painted like the chain's own tabs so the
// two read as the same kind of control.
class BandButton final : public juce::Button
{
public:
    explicit BandButton (const juce::String& name) : juce::Button (name)
    {
        setClickingTogglesState (false);
        setMouseClickGrabsKeyboardFocus (false);
        setWantsKeyboardFocus (false);
    }

    void setSelected (bool shouldBeSelected)
    {
        if (selected == shouldBeSelected) return;
        selected = shouldBeSelected;
        repaint();
    }

private:
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (selected ? sage.withAlpha (0.22f) : well.withAlpha (0.6f));
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (selected ? electric.withAlpha (0.7f) : outline.withAlpha (0.6f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);

        g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
        drawGlitchText (g, *this, getButtonText(), getLocalBounds(),
                        juce::Justification::centred,
                        selected ? ink : (over || down ? ink.withAlpha (0.8f) : muted));
    }

    bool selected = false;
};
} // namespace

// ---------------------------------------------------------- CrossoverGraph --

CrossoverGraph::CrossoverGraph (juce::AudioProcessorValueTreeState& state,
                                std::function<float (int)> bandGainDb)
    : apvts (state), readBandGain (std::move (bandGainDb))
{
    // The graph is a click target inside an editor that also hosts an
    // on-screen keyboard; taking focus here would steal its QWERTY notes.
    setMouseClickGrabsKeyboardFocus (false);
    setWantsKeyboardFocus (false);
    startTimerHz (30);
}

void CrossoverGraph::setSelectedBand (int band)
{
    if (selected == band) return;
    selected = band;
    repaint();
}

void CrossoverGraph::timerCallback()
{
    for (int b = 0; b < (int) shown.size(); ++b)
    {
        const auto target = readBandGain ? readBandGain (b) : 0.0f;
        // Snap toward a bigger excursion and ease back from it, so the fill
        // reads as movement rather than flicker at 30 fps.
        auto& value = shown[(size_t) b];
        value = std::abs (target) > std::abs (value) ? target : value * 0.84f;
    }

    if (isShowing())
        repaint();
}

juce::Rectangle<float> CrossoverGraph::plotArea() const
{
    return getLocalBounds().toFloat().reduced (2.0f).withTrimmedTop (14.0f).withTrimmedBottom (12.0f);
}

float CrossoverGraph::xForHz (float hz) const
{
    const auto r = plotArea();
    const auto t = std::log (juce::jlimit (minHz, maxHz, hz) / minHz) / std::log (maxHz / minHz);
    return r.getX() + r.getWidth() * t;
}

float CrossoverGraph::hzForX (float x) const
{
    const auto r = plotArea();
    const auto t = juce::jlimit (0.0f, 1.0f, (x - r.getX()) / juce::jmax (1.0f, r.getWidth()));
    return minHz * std::pow (maxHz / minHz, t);
}

float CrossoverGraph::crossover (int which) const
{
    return parameterValue (apvts, which == 0 ? params::id::mbXoverLow : params::id::mbXoverHigh);
}

void CrossoverGraph::setCrossover (int which, float hz)
{
    // The two must not cross, or the mid band inverts. A octave-and-a-bit of
    // separation also keeps the middle band wide enough to be worth having.
    constexpr float separation = 1.25f;
    if (which == 0) hz = juce::jmin (hz, crossover (1) / separation);
    else            hz = juce::jmax (hz, crossover (0) * separation);

    const auto* id = which == 0 ? params::id::mbXoverLow : params::id::mbXoverHigh;
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (hz));
}

int CrossoverGraph::handleNear (float x) const
{
    for (int i = 0; i < 2; ++i)
        if (std::abs (x - xForHz (crossover (i))) <= grabRadius)
            return i;
    return -1;
}

int CrossoverGraph::bandAt (float x) const
{
    if (x < xForHz (crossover (0))) return 0;
    if (x < xForHz (crossover (1))) return 1;
    return 2;
}

void CrossoverGraph::mouseDown (const juce::MouseEvent& e)
{
    dragging = handleNear ((float) e.x);

    if (dragging >= 0)
    {
        const auto* id = dragging == 0 ? params::id::mbXoverLow : params::id::mbXoverHigh;
        if (auto* p = apvts.getParameter (id))
            p->beginChangeGesture();
        return;
    }

    const auto band = bandAt ((float) e.x);
    setSelectedBand (band);
    if (onBandSelected) onBandSelected (band);
}

void CrossoverGraph::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0) return;
    setCrossover (dragging, hzForX ((float) e.x));
    repaint();
}

void CrossoverGraph::mouseUp (const juce::MouseEvent&)
{
    if (dragging < 0) return;
    const auto* id = dragging == 0 ? params::id::mbXoverLow : params::id::mbXoverHigh;
    if (auto* p = apvts.getParameter (id))
        p->endChangeGesture();
    dragging = -1;
}

void CrossoverGraph::mouseMove (const juce::MouseEvent& e)
{
    const auto near = handleNear ((float) e.x);
    if (near == hovered) return;
    hovered = near;
    setMouseCursor (near >= 0 ? juce::MouseCursor::LeftRightResizeCursor
                              : juce::MouseCursor::PointingHandCursor);
    repaint();
}

void CrossoverGraph::mouseExit (const juce::MouseEvent&)
{
    if (hovered < 0) return;
    hovered = -1;
    repaint();
}

void CrossoverGraph::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (well.withAlpha (0.85f));
    g.fillRoundedRectangle (bounds, 7.0f);
    g.setColour (sage.withAlpha (0.22f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 7.0f, 1.0f);

    const auto area = plotArea();
    const auto on = parameterValue (apvts, params::id::mbEnable) >= 0.5f;

    // Decade gridlines, so a dragged crossover can be read against something.
    g.setFont (juce::Font (juce::FontOptions (8.0f)));
    for (const float hz : { 100.0f, 1000.0f, 10000.0f })
    {
        const auto x = xForHz (hz);
        g.setColour (outline.withAlpha (0.35f));
        g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
        g.setColour (muted.withAlpha (0.3f));
        g.drawText (hz >= 1000.0f ? juce::String (hz / 1000.0f, 0) + "k"
                                  : juce::String ((int) hz),
                    juce::Rectangle<float> (x + 2.0f, area.getBottom(), 30.0f, 11.0f),
                    juce::Justification::centredLeft);
    }

    const auto centreY = area.getCentreY();
    const auto halfHeight = area.getHeight() * 0.5f;

    // The vertical axis is gain applied, not level, so it needs saying: a bar
    // above the line is a band being lifted and below it a band being held
    // down, and without a scale neither is a number.
    for (const int markDb : { 12 })
    {
        const auto offset = halfHeight * (float) markDb / meterRangeDb;
        g.setColour (outline.withAlpha (0.4f));
        g.drawHorizontalLine ((int) (centreY - offset), area.getX(), area.getRight());
        g.drawHorizontalLine ((int) (centreY + offset), area.getX(), area.getRight());

        g.setColour (muted.withAlpha (0.3f));
        g.setFont (juce::Font (juce::FontOptions (8.0f)));
        g.drawText ("+" + juce::String (markDb) + " dB",
                    juce::Rectangle<float> (area.getX() + 3.0f, centreY - offset - 9.0f, 40.0f, 10.0f),
                    juce::Justification::centredLeft);
        g.drawText ("-" + juce::String (markDb) + " dB",
                    juce::Rectangle<float> (area.getX() + 3.0f, centreY + offset - 1.0f, 40.0f, 10.0f),
                    juce::Justification::centredLeft);
    }

    const float edges[4] { area.getX(), xForHz (crossover (0)), xForHz (crossover (1)),
                           area.getRight() };
    const juce::String names[3] { "LOW", "MID", "HIGH" };

    for (int b = 0; b < 3; ++b)
    {
        const juce::Rectangle<float> region (edges[b], area.getY(),
                                             juce::jmax (1.0f, edges[b + 1] - edges[b]),
                                             area.getHeight());

        g.setColour (sage.withAlpha (b == selected ? 0.13f : 0.05f));
        g.fillRect (region);

        // Gain reduction, filled from the centre line: up is gain, down is
        // reduction. Both directions matter here, so a one-sided meter would
        // hide half of what the band is doing.
        const auto db = juce::jlimit (-meterRangeDb, meterRangeDb, shown[(size_t) b]);
        const auto extent = halfHeight * std::abs (db) / meterRangeDb;
        if (on && extent > 0.5f)
        {
            const auto fill = db >= 0.0f
                ? juce::Rectangle<float> (region.getX(), centreY - extent, region.getWidth(), extent)
                : juce::Rectangle<float> (region.getX(), centreY, region.getWidth(), extent);
            g.setColour ((db >= 0.0f ? electric : sage).withAlpha (0.4f));
            g.fillRect (fill.reduced (1.0f, 0.0f));
        }

        g.setColour (muted.withAlpha (b == selected ? 0.85f : 0.4f));
        g.setFont (juce::Font (juce::FontOptions (8.5f, juce::Font::bold)));
        g.drawText (names[b], region.withHeight (11.0f).translated (0.0f, 2.0f),
                    juce::Justification::centred);
    }

    g.setColour (outline);
    g.drawHorizontalLine ((int) centreY, area.getX(), area.getRight());

    // The crossover handles last, so they sit over the band fills.
    for (int i = 0; i < 2; ++i)
    {
        const auto x = xForHz (crossover (i));
        const auto active = dragging == i || hovered == i;
        g.setColour (active ? ink : electric.withAlpha (0.75f));
        g.drawVerticalLine ((int) x, area.getY(), area.getBottom());

        const juce::Rectangle<float> grip (x - 3.0f, area.getY() - 1.0f, 6.0f, 9.0f);
        g.fillRoundedRectangle (grip, 2.0f);

        g.setColour (active ? ink : muted.withAlpha (0.65f));
        g.setFont (juce::Font (juce::FontOptions (8.5f)));
        // Keep the readout inside the graph at both extremes rather than
        // letting it run off the edge.
        const auto width = 54.0f;
        const auto textX = juce::jlimit (area.getX(), area.getRight() - width, x - width * 0.5f);
        g.drawText (frequencyText (crossover (i)),
                    juce::Rectangle<float> (textX, bounds.getY() + 1.0f, width, 11.0f),
                    juce::Justification::centred);
    }
}

// --------------------------------------------------------- MultibandEditor --

MultibandEditor::MultibandEditor (juce::AudioProcessorValueTreeState& state,
                                  std::function<float (int)> bandGainDb,
                                  MidiLearnManager* learn)
    : apvts (state), graph (state, std::move (bandGainDb))
{
    addAndMakeVisible (graph);
    graph.onBandSelected = [this] (int band) { selectBand (band); };

    static const char* keys[6] { params::id::mbband::threshold, params::id::mbband::ratio,
                                 params::id::mbband::upRatio,   params::id::mbband::attack,
                                 params::id::mbband::release,   params::id::mbband::gain };

    for (int b = 0; b < 3; ++b)
    {
        auto button = std::make_unique<BandButton> (params::id::mbBandNames()[b]);
        button->onClick = [this, b] { selectBand (b); };
        addAndMakeVisible (*button);
        bandButtons[(size_t) b] = std::move (button);

        for (int k = 0; k < 6; ++k)
        {
            const auto id = params::id::mbBand (b, keys[k]);
            if (const auto* def = params::find (id))
            {
                auto knob = std::make_unique<Knob> (apvts, *def, learn);
                addChildComponent (*knob);
                knobs[(size_t) b][(size_t) k] = std::move (knob);
            }
        }
    }

    selectBand (0);
}

void MultibandEditor::selectBand (int band)
{
    selected = juce::jlimit (0, 2, band);
    graph.setSelectedBand (selected);

    for (int b = 0; b < 3; ++b)
    {
        if (auto* button = dynamic_cast<BandButton*> (bandButtons[(size_t) b].get()))
            button->setSelected (b == selected);

        for (auto& knob : knobs[(size_t) b])
            if (knob != nullptr)
                knob->setVisible (b == selected);
    }

    resized();
}

void MultibandEditor::paint (juce::Graphics&) {}

void MultibandEditor::resized()
{
    auto area = getLocalBounds();

    // The graph takes the larger share: it is the part you aim at, and the
    // controls only ever show one band's worth.
    const auto controlsWidth = juce::jlimit (240, 400, juce::roundToInt (area.getWidth() * 0.40f));
    auto controls = area.removeFromRight (controlsWidth);
    area.removeFromRight (10);
    graph.setBounds (area);

    auto selector = controls.removeFromTop (20);
    const auto buttonWidth = selector.getWidth() / 3;
    for (int b = 0; b < 3; ++b)
        bandButtons[(size_t) b]->setBounds (selector.removeFromLeft (
            b == 2 ? selector.getWidth() : buttonWidth).reduced (2, 0));

    controls.removeFromTop (4);

    // Two rows of three: six controls at a readable width, in the height the
    // FX band actually has.
    const auto cellWidth = controls.getWidth() / 3;
    const auto cellHeight = controls.getHeight() / 2;

    for (int k = 0; k < 6; ++k)
    {
        const auto row = k / 3, column = k % 3;
        const juce::Rectangle<int> cell (controls.getX() + column * cellWidth,
                                         controls.getY() + row * cellHeight,
                                         cellWidth, cellHeight);
        if (auto& knob = knobs[(size_t) selected][(size_t) k])
            knob->setBounds (cell.reduced (2));
    }
}

} // namespace glitch::fx::ui
