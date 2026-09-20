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

void drawLearnBadge (juce::Graphics& g, juce::Rectangle<float> knobBounds,
                     const juce::String& text, bool armed)
{
    if (text.isEmpty()) return;

    const juce::Font font (juce::FontOptions (8.5f, juce::Font::bold));
    g.setFont (font);
    const auto width = juce::jmax (26.0f, juce::GlyphArrangement::getStringWidth (font, text) + 8.0f);
    auto tag = knobBounds.reduced (4.0f).removeFromTop (12.0f).removeFromRight (width);

    g.setColour (armed ? electric.withAlpha (0.85f) : sage.withAlpha (0.22f));
    g.fillRoundedRectangle (tag, 3.0f);
    g.setColour (armed ? paper : muted);
    g.drawText (text, tag, juce::Justification::centred);
}

} // namespace glitch::theme
