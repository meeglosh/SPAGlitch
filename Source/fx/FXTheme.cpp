#include "FXTheme.h"

namespace glitch::theme
{

void glowStroke (juce::Graphics& g, const juce::Path& path, juce::Colour colour, float thickness)
{
    // Three feathered passes, widest and faintest first.
    g.setColour (colour.withAlpha (0.10f));
    g.strokePath (path, juce::PathStrokeType (thickness * 6.0f,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
    g.setColour (colour.withAlpha (0.28f));
    g.strokePath (path, juce::PathStrokeType (thickness * 2.6f,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
    g.setColour (colour.brighter (0.05f));
    g.strokePath (path, juce::PathStrokeType (thickness,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
}

void card (juce::Graphics& g, juce::Rectangle<float> bounds, float corner, float fillAlpha)
{
    g.setColour (paper.withAlpha (fillAlpha));
    g.fillRoundedRectangle (bounds, corner);
    g.setColour (sage.withAlpha (0.28f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), corner, 1.0f);
}

const GlitchSource* GlitchSource::find (const juce::Component& component)
{
    return dynamic_cast<const GlitchSource*> (&component.getLookAndFeel());
}

namespace
{
// Printable ASCII only: a box-drawing glyph would be a tofu box in whatever
// font the host happens to resolve.
const juce::String glitchGlyphs ("#%/\\|_*+=<>");

juce::String corrupt (const juce::String& text, float amount, juce::Random& rng)
{
    if (amount <= 0.0f) return text;

    juce::String out;
    for (auto character : text)
    {
        if (character != ' ' && rng.nextFloat() < amount)
            out += glitchGlyphs[rng.nextInt (glitchGlyphs.length())];
        else
            out += juce::String::charToString (character);
    }
    return out;
}

template <typename RectangleType>
void drawGlitched (juce::Graphics& g, const juce::Component& component, const juce::String& text,
                   RectangleType area, juce::Justification justification, juce::Colour colour)
{
    const auto* source = GlitchSource::find (component);
    const auto energy = source != nullptr ? source->glitchEnergy() : 0.0f;

    if (energy <= 0.01f || text.isEmpty())
    {
        g.setColour (colour);
        g.drawText (text, area, justification);
        return;
    }

    const auto amount = juce::jlimit (0.0f, 1.0f, energy);
    // Seeded from the text and the frame, so a string holds still within a
    // frame however many times it is repainted, and every string tears
    // differently from its neighbours.
    juce::Random rng ((juce::int64) text.hashCode() * 2654435761LL
                      + (juce::int64) source->glitchFrame() * 40503LL);

    const auto split = 1.0f + amount * 2.0f;
    const auto jitterX = (float) (rng.nextInt (3) - 1);
    const auto jitterY = (float) (rng.nextInt (3) - 1);
    const auto shown = corrupt (text, amount * 0.14f, rng);

    // Chromatic split either side, then the text itself over the top.
    g.setColour (electric.withAlpha (0.55f * amount));
    g.drawText (shown, area.translated (-split + jitterX, jitterY), justification);
    g.setColour (juce::Colour (0xffff5fa8).withAlpha (0.4f * amount));
    g.drawText (shown, area.translated (split + jitterX, jitterY), justification);
    g.setColour (colour);
    g.drawText (shown, area.translated (jitterX, jitterY), justification);
}
} // namespace

void drawGlitchText (juce::Graphics& g, const juce::Component& c, const juce::String& text,
                     juce::Rectangle<int> area, juce::Justification j, juce::Colour colour)
{
    drawGlitched (g, c, text, area.toFloat(), j, colour);
}

void drawGlitchText (juce::Graphics& g, const juce::Component& c, const juce::String& text,
                     juce::Rectangle<float> area, juce::Justification j, juce::Colour colour)
{
    drawGlitched (g, c, text, area, j, colour);
}

void drawLearnBadge (juce::Graphics& g, const juce::Component& component,
                     juce::Rectangle<float> knobBounds,
                     const juce::String& text, bool armed)
{
    if (text.isEmpty()) return;

    const juce::Font font (juce::FontOptions (8.5f, juce::Font::bold));
    g.setFont (font);
    const auto width = juce::jmax (26.0f, juce::GlyphArrangement::getStringWidth (font, text) + 8.0f);
    auto tag = knobBounds.reduced (4.0f).removeFromTop (12.0f).removeFromRight (width);

    g.setColour (armed ? electric.withAlpha (0.85f) : sage.withAlpha (0.22f));
    g.fillRoundedRectangle (tag, 3.0f);
    drawGlitchText (g, component, text, tag, juce::Justification::centred, armed ? paper : muted);
}

} // namespace glitch::theme
