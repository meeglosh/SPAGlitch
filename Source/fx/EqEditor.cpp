#include "EqEditor.h"

namespace glitch::fx::ui
{
using namespace glitch::theme;
namespace pid = params::id;

EqEditor::EqEditor (juce::AudioProcessorValueTreeState& state, ScopeReader scopeReader,
                    std::function<double()> sampleRateFn)
    : apvts (state), readScope (std::move (scopeReader)),
      getSampleRate (std::move (sampleRateFn))
{
    // CHARACTER lives in the tab's control grid alongside every other FX
    // parameter; the editor owns only the curve.
    refreshSampleRate();
    setWantsKeyboardFocus (false);
    // setWantsKeyboardFocus alone doesn't stop a click from grabbing focus,
    // which would steal it from the on-screen keyboard's note input.
    setMouseClickGrabsKeyboardFocus (false);
    modName = (juce::SystemStats::getOperatingSystemType() & juce::SystemStats::MacOSX)
                  ? "Cmd" : "Ctrl";
    setTooltip ("Double-click empty space to add a Bell (near the left/right edges: a "
                "Low/High Cut). Double-click a node to remove it. Drag a node to move it; "
                + modName + "-drag vertically or use the mouse wheel to set its Q. "
                "Right-click a node to change its type or (for cuts) its slope.");
    startTimerHz (30);
}

EqEditor::~EqEditor() { stopTimer(); }

void EqEditor::resized()
{
    graph = getLocalBounds().reduced (8, 6).toFloat();
}

void EqEditor::timerCallback()
{
    refreshSampleRate();
    if (! isShowing()) return;
    computeSpectrum();
    repaint();
}

// ------------------------------------------------------------ parameters ---

float EqEditor::value (const juce::String& id) const
{
    if (auto* v = apvts.getRawParameterValue (id)) return v->load();
    return 0.0f;
}

float EqEditor::rawBand (int b, const char* key) const
{
    return value (pid::eqBand (b, key));
}

bool EqEditor::bandEnabled (int b) const
{
    return rawBand (b, pid::eqband::enable) >= 0.5f;
}

void EqEditor::setBand (int b, const char* key, float realValue)
{
    if (auto* p = apvts.getParameter (pid::eqBand (b, key)))
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
}

std::array<ParametricEQ::Band, EqEditor::numBands> EqEditor::readBands() const
{
    std::array<ParametricEQ::Band, numBands> bands {};
    for (int b = 0; b < numBands; ++b)
    {
        auto& bd = bands[(size_t) b];
        bd.enabled = bandEnabled (b);
        bd.type    = (int) rawBand (b, pid::eqband::type);
        bd.slope   = (int) rawBand (b, pid::eqband::slope);
        bd.freq    = rawBand (b, pid::eqband::freq);
        bd.gainDb  = rawBand (b, pid::eqband::gain);
        bd.q       = rawBand (b, pid::eqband::q);
    }
    return bands;
}

// -------------------------------------------------------------- geometry ---

float EqEditor::freqToX (float f) const
{
    return graph.getX() + graph.getWidth() * std::log (f / minF) / std::log (maxF / minF);
}

float EqEditor::xToFreq (float x) const
{
    return minF * std::pow (maxF / minF, (x - graph.getX()) / juce::jmax (1.0f, graph.getWidth()));
}

float EqEditor::dbToY (float db) const
{
    return graph.getY() + graph.getHeight() * (0.5f - db / (2.0f * dbRange));
}

float EqEditor::yToDb (float y) const
{
    return (0.5f - (y - graph.getY()) / juce::jmax (1.0f, graph.getHeight())) * 2.0f * dbRange;
}

// Bell / Low Shelf / High Shelf / Tilt Shelf carry a gain; cuts, Notch and
// Band Pass don't -- their node sits on the 0 dB line.
bool EqEditor::isGainType (int type)
{
    using T = ParametricEQ::Type;
    return (T) type == T::bell || (T) type == T::lowShelf
        || (T) type == T::highShelf || (T) type == T::tiltShelf;
}

juce::Point<float> EqEditor::nodeCentre (int b) const
{
    const int type = (int) rawBand (b, pid::eqband::type);
    const float gain = isGainType (type) ? rawBand (b, pid::eqband::gain) : 0.0f;
    return { freqToX (rawBand (b, pid::eqband::freq)),
             dbToY (juce::jlimit (-dbRange, dbRange, gain)) };
}

int EqEditor::bandAt (juce::Point<float> p) const
{
    for (int b = 0; b < numBands; ++b)
    {
        if (! bandEnabled (b)) continue;
        if (nodeCentre (b).getDistanceFrom (p) <= nodeRadius + 4.0f) return b;
    }
    return -1;
}

// ------------------------------------------------------------------ paint ---

juce::String EqEditor::badgeText (int type, int slope)
{
    using T = ParametricEQ::Type;
    switch ((T) type)
    {
        case T::bell:      return "BELL";
        case T::lowShelf:  return "LS";
        case T::highShelf: return "HS";
        case T::notch:     return "NOTCH";
        case T::bandPass:  return "BP";
        case T::tiltShelf: return "TILT";
        case T::lowCut:    return "LC " + juce::String (ParametricEQ::slopeDbPerOct (slope));
        case T::highCut:   return "HC " + juce::String (ParametricEQ::slopeDbPerOct (slope));
    }
    return {};
}

void EqEditor::drawGrid (juce::Graphics& g) const
{
    g.setColour (outline.withAlpha (0.6f));
    for (float f : { 100.0f, 1000.0f, 10000.0f })
        g.drawVerticalLine ((int) freqToX (f), graph.getY(), graph.getBottom());

    for (float db : { 12.0f, 0.0f, -12.0f })
    {
        g.setColour (outline.withAlpha (db == 0.0f ? 0.8f : 0.4f));
        g.drawHorizontalLine ((int) dbToY (db), graph.getX(), graph.getRight());
    }

    g.setColour (muted.withAlpha (0.6f));
    g.setFont (juce::Font (juce::FontOptions (9.0f)));
    auto tick = [&] (const char* text, float f)
    {
        g.drawText (text, juce::Rectangle<float> (freqToX (f) - 14.0f, graph.getBottom() - 11.0f,
                                                  28.0f, 10.0f), juce::Justification::centred);
    };
    tick ("100", 100.0f); tick ("1k", 1000.0f); tick ("10k", 10000.0f);
}

void EqEditor::drawSpectrum (juce::Graphics& g) const
{
    juce::Path p;
    bool started = false;
    const int bins = scopeSize / 2;
    for (int i = 1; i < bins; ++i)
    {
        const float freq = (float) i * (float) sampleRate / (float) scopeSize;
        if (freq < minF || freq > maxF) continue;
        const float x = freqToX (freq);
        // -80..0 dB mapped into the graph height.
        const float db = juce::jlimit (-80.0f, 0.0f, spectrum[(size_t) i]);
        const float y = graph.getBottom() - (db + 80.0f) / 80.0f * graph.getHeight();
        if (! started) { p.startNewSubPath (x, graph.getBottom()); p.lineTo (x, y); started = true; }
        else p.lineTo (x, y);
    }
    if (started)
    {
        p.lineTo (graph.getRight(), graph.getBottom());
        p.closeSubPath();
        g.setColour (sage.withAlpha (0.16f));
        g.fillPath (p);
    }
}

void EqEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (well.withAlpha (0.85f));
    g.fillRoundedRectangle (bounds, 7.0f);
    g.setColour (sage.withAlpha (0.22f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 7.0f, 1.0f);

    const bool on = value (pid::eqEnable) >= 0.5f;

    drawGrid (g);
    drawSpectrum (g);

    const auto bands = readBands();
    const auto colour = on ? electric : muted.withAlpha (0.5f);

    juce::Path curve;
    constexpr int steps = 220;
    for (int i = 0; i <= steps; ++i)
    {
        const float x = graph.getX() + graph.getWidth() * (float) i / steps;
        const float db = ParametricEQ::magnitudeDb (bands, xToFreq (x), sampleRate);
        const float y = dbToY (juce::jlimit (-dbRange, dbRange, db));
        if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
    }
    auto fill = curve;
    fill.lineTo (graph.getRight(), dbToY (0.0f));
    fill.lineTo (graph.getX(), dbToY (0.0f));
    fill.closeSubPath();
    g.setColour (colour.withAlpha (0.14f));
    g.fillPath (fill);
    glowStroke (g, curve, colour, 1.8f);

    for (int b = 0; b < numBands; ++b)
    {
        if (! bandEnabled (b)) continue;
        const auto c = nodeCentre (b);
        const bool hot = (b == dragBand || b == hoverBand || b == selectedBand);
        const float rad = hot ? nodeRadius + 2.0f : nodeRadius;

        g.setColour (well.withAlpha (0.9f));
        g.fillEllipse (c.x - rad - 1.0f, c.y - rad - 1.0f, (rad + 1.0f) * 2.0f, (rad + 1.0f) * 2.0f);
        g.setColour (on ? electric : muted);
        g.fillEllipse (c.x - rad, c.y - rad, rad * 2.0f, rad * 2.0f);

        if (b == selectedBand)
        {
            g.setColour (ink);
            g.drawEllipse (c.x - rad - 2.0f, c.y - rad - 2.0f,
                           (rad + 2.0f) * 2.0f, (rad + 2.0f) * 2.0f, 1.5f);
        }

        g.setColour (paper);
        g.setFont (juce::Font (juce::FontOptions (10.0f)));
        g.drawText (juce::String (b + 1),
                    juce::Rectangle<float> (c.x - rad, c.y - rad, rad * 2.0f, rad * 2.0f),
                    juce::Justification::centred);

        // Type badge beside hot nodes, so a cut's slope is visible without
        // opening the menu.
        if (hot)
        {
            g.setColour (ink.withAlpha (0.85f));
            g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
            g.drawText (badgeText ((int) rawBand (b, pid::eqband::type),
                                   (int) rawBand (b, pid::eqband::slope)),
                        juce::Rectangle<float> (c.x + rad + 4.0f, c.y - 7.0f, 60.0f, 14.0f),
                        juce::Justification::centredLeft);
        }
    }

    // Readout for the hovered (else selected) node, so the Q wheel has a
    // visible target.
    const int info = hoverBand >= 0 ? hoverBand : selectedBand;
    if (info >= 0 && bandEnabled (info))
    {
        const float f = rawBand (info, pid::eqband::freq);
        const float gainDb = rawBand (info, pid::eqband::gain);
        const float q = rawBand (info, pid::eqband::q);
        const int type = (int) rawBand (info, pid::eqband::type);
        const int slope = (int) rawBand (info, pid::eqband::slope);

        juce::String txt = "B" + juce::String (info + 1) + "  " + badgeText (type, slope) + "   "
                         + (f >= 1000.0f ? juce::String (f / 1000.0f, 2) + " kHz"
                                         : juce::String (juce::roundToInt (f)) + " Hz");
        if (isGainType (type)) txt += "   " + juce::String (gainDb, 1) + " dB";
        txt += "   Q " + juce::String (q, 2);

        g.setColour (muted);
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText (txt, graph.reduced (8.0f, 5.0f).removeFromTop (14.0f),
                    juce::Justification::topLeft);
    }

    g.setColour (muted.withAlpha (0.45f));
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText ("double-click: add / remove    right-click: type    " + modName + "-drag or wheel: Q",
                graph.reduced (8.0f, 5.0f).removeFromTop (13.0f), juce::Justification::topRight);
}

// ------------------------------------------------------------ interaction ---

void EqEditor::mouseDown (const juce::MouseEvent& e)
{
    const int b = bandAt (e.position);
    if (e.mods.isPopupMenu())   // right-click / Ctrl-click: type + slope menu
    {
        if (b >= 0)
        {
            selectedBand = b;
            repaint();
            showTypeMenu (b);
        }
        return;
    }
    // Click selects and grabs the node under the pointer (no accidental
    // creation); clicking empty space deselects. Bands are added and removed
    // by double-click.
    selectedBand = b;
    dragBand = b;
    qDragActive = false;
    repaint();
}

void EqEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragBand < 0) return;

    if (e.mods.isCommandDown())   // Cmd/Ctrl-drag = Q, vertical
    {
        if (! qDragActive)        // anchor when the Q gesture begins
        {
            qDragActive = true;
            qRefY = e.position.y;
            qRefQ = rawBand (dragBand, pid::eqband::q);
        }
        const float dy = qRefY - e.position.y;   // up = narrower (higher Q)
        setBand (dragBand, pid::eqband::q, juce::jlimit (0.1f, 18.0f, qRefQ * std::exp (dy * 0.012f)));
    }
    else
    {
        qDragActive = false;
        applyDrag (e.position);   // freq (x) + gain (y)
    }
    repaint();
}

void EqEditor::mouseUp (const juce::MouseEvent&)
{
    dragBand = -1;
    qDragActive = false;
    repaint();
}

void EqEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    // Double-click a node to remove it, or empty graph space to add one: near
    // the left edge -> Low Cut, near the right edge -> High Cut, else a Bell.
    const int b = bandAt (e.position);
    if (b >= 0)
    {
        setBand (b, pid::eqband::enable, 0.0f);
        if (selectedBand == b) selectedBand = -1;
    }
    else
    {
        const float frac = graph.getWidth() > 0.0f
                         ? (e.position.x - graph.getX()) / graph.getWidth() : 0.5f;
        const int edgeType = frac < 0.12f ? (int) ParametricEQ::Type::lowCut
                           : frac > 0.88f ? (int) ParametricEQ::Type::highCut
                                          : (int) ParametricEQ::Type::bell;
        const int n = createBandAt (e.position, edgeType);
        if (n >= 0) selectedBand = n;
    }
    repaint();
}

void EqEditor::mouseMove (const juce::MouseEvent& e)
{
    const int b = bandAt (e.position);
    if (b != hoverBand) { hoverBand = b; repaint(); }
    setMouseCursor (b >= 0 ? juce::MouseCursor::DraggingHandCursor
                           : juce::MouseCursor::NormalCursor);
}

void EqEditor::mouseExit (const juce::MouseEvent&) { hoverBand = -1; repaint(); }

void EqEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    // Wheel sets Q: the hovered node if the pointer is over one, else the
    // selected node, so Q can be dialled after picking a node.
    int b = bandAt (e.position);
    if (b < 0) b = selectedBand;
    if (b < 0 || ! bandEnabled (b)) return;

    const float q = rawBand (b, pid::eqband::q);
    setBand (b, pid::eqband::q, juce::jlimit (0.1f, 18.0f, q * (1.0f + w.deltaY * 0.6f)));
    repaint();
}

void EqEditor::applyDrag (juce::Point<float> p)
{
    setBand (dragBand, pid::eqband::freq, juce::jlimit (minF, maxF, xToFreq (p.x)));
    if (isGainType ((int) rawBand (dragBand, pid::eqband::type)))
        setBand (dragBand, pid::eqband::gain, juce::jlimit (-dbRange, dbRange, yToDb (p.y)));
}

// Enable the first free band at the click point with the given type, or -1 if
// all 8 are in use or the click is outside the graph. Cuts, Notch and Band
// Pass ignore the click's vertical position -- their node sits at 0 dB.
int EqEditor::createBandAt (juce::Point<float> p, int type)
{
    if (! graph.contains (p)) return -1;
    for (int i = 0; i < numBands; ++i)
        if (! bandEnabled (i))
        {
            setBand (i, pid::eqband::type, (float) type);
            setBand (i, pid::eqband::freq, juce::jlimit (minF, maxF, xToFreq (p.x)));
            if (isGainType (type))
                setBand (i, pid::eqband::gain, juce::jlimit (-dbRange, dbRange, yToDb (p.y)));
            setBand (i, pid::eqband::enable, 1.0f);
            return i;
        }
    return -1;
}

void EqEditor::showTypeMenu (int b)
{
    using T = ParametricEQ::Type;
    const int curType = (int) rawBand (b, pid::eqband::type);
    const int curSlope = (int) rawBand (b, pid::eqband::slope);

    juce::PopupMenu menu;
    const struct { const char* name; T type; } types[] = {
        { "Bell", T::bell }, { "Low Shelf", T::lowShelf }, { "High Shelf", T::highShelf },
        { "Low Cut", T::lowCut }, { "High Cut", T::highCut }, { "Notch", T::notch },
        { "Band Pass", T::bandPass }, { "Tilt Shelf", T::tiltShelf },
    };

    for (auto& entry : types)
    {
        const int typeIdx = (int) entry.type;
        if (entry.type == T::lowCut || entry.type == T::highCut)
        {
            juce::PopupMenu slopeMenu;
            const char* slopeNames[6] = { "6 dB/oct", "12 dB/oct", "18 dB/oct",
                                          "24 dB/oct", "36 dB/oct", "48 dB/oct" };
            for (int s = 0; s < 6; ++s)
                slopeMenu.addItem (10000 + typeIdx * 100 + s, slopeNames[s], true,
                                   curType == typeIdx && curSlope == s);
            menu.addSubMenu (entry.name, slopeMenu, true, nullptr, curType == typeIdx);
        }
        else
        {
            menu.addItem (typeIdx + 1, entry.name, true, curType == typeIdx);
        }
    }

    const auto safe = juce::Component::SafePointer<EqEditor> (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
        [safe, b] (int result)
        {
            if (safe == nullptr || result <= 0) return;
            if (result >= 10000)
            {
                const int r = result - 10000;
                safe->setTypeAndSlope (b, r / 100, r % 100);
            }
            else
            {
                safe->setTypeAndSlope (b, result - 1, -1);
            }
        });
}

// The one path the menu uses to change a band's type (and, for cuts, its
// slope), so the two can never drift apart.
void EqEditor::setTypeAndSlope (int b, int type, int slope)
{
    setBand (b, pid::eqband::type, (float) type);
    if (slope >= 0)
        setBand (b, pid::eqband::slope, (float) slope);
    repaint();
}

// ------------------------------------------------------------- spectrum ----

void EqEditor::computeSpectrum()
{
    if (! readScope || ! readScope (scopeBuffer.data(), scopeSize))
    {
        // No audio available: let the analyser fall away rather than freeze.
        for (auto& s : spectrum) s = s * 0.85f - 12.0f;
        return;
    }

    for (int i = 0; i < scopeSize; ++i)
    {
        const float win = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi
                                                  * (float) i / (float) (scopeSize - 1));
        fftData[(size_t) i] = scopeBuffer[(size_t) i] * win;
    }
    std::fill (fftData.begin() + scopeSize, fftData.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    const int bins = scopeSize / 2;
    const float norm = 2.0f / (float) scopeSize;
    for (int i = 0; i < bins; ++i)
    {
        const float mag = fftData[(size_t) i] * norm;
        const float db = juce::Decibels::gainToDecibels (mag + 1.0e-9f);
        // Temporal smoothing; fast attack, slower release for readability.
        float& s = spectrum[(size_t) i];
        s = db > s ? db : s * 0.85f + db * 0.15f;
    }
}

void EqEditor::refreshSampleRate()
{
    const double sr = getSampleRate ? getSampleRate() : 0.0;
    if (sr > 0.0) sampleRate = sr;
}

} // namespace glitch::fx::ui
