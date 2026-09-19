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

} // namespace glitch::theme
