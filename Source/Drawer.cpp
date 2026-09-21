#include "Drawer.h"

namespace glitch::ui
{
using namespace glitch::theme;

DrawerHeader::DrawerHeader (juce::String indexText, juce::String titleText, juce::String hintText)
    : index (std::move (indexText)), title (std::move (titleText)), hint (std::move (hintText))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    // The header is a click target inside a plugin editor that also hosts an
    // on-screen keyboard; taking focus here would steal its QWERTY note input.
    setMouseClickGrabsKeyboardFocus (false);
    setWantsKeyboardFocus (false);
}

void DrawerHeader::setCollapsed (bool shouldBeCollapsed)
{
    if (collapsed == shouldBeCollapsed) return;
    collapsed = shouldBeCollapsed;
    repaint();
}

void DrawerHeader::mouseDown (const juce::MouseEvent&)
{
    if (onToggle) onToggle();
}

void DrawerHeader::mouseEnter (const juce::MouseEvent&) { hovered = true;  repaint(); }
void DrawerHeader::mouseExit  (const juce::MouseEvent&) { hovered = false; repaint(); }

void DrawerHeader::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    if (hovered)
    {
        g.setColour (sage.withAlpha (0.07f));
        g.fillRect (bounds);
    }

    // Disclosure chevron: pointing right when folded away, down when open.
    const auto cx = 36.0f, cy = bounds.getCentreY();
    constexpr float arm = 4.0f;
    juce::Path chevron;
    if (collapsed)
    {
        chevron.startNewSubPath (cx - arm * 0.5f, cy - arm);
        chevron.lineTo (cx + arm * 0.7f, cy);
        chevron.lineTo (cx - arm * 0.5f, cy + arm);
    }
    else
    {
        chevron.startNewSubPath (cx - arm, cy - arm * 0.5f);
        chevron.lineTo (cx, cy + arm * 0.7f);
        chevron.lineTo (cx + arm, cy - arm * 0.5f);
    }
    g.setColour (hovered ? ink : muted);
    g.strokePath (chevron, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

    g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
    drawGlitchText (g, *this, index + "  /  " + title, bounds.withTrimmedLeft (52.0f),
                    juce::Justification::centredLeft, hovered ? ink : muted);

    if (hint.isNotEmpty() && ! collapsed)
    {
        g.setFont (juce::Font (juce::FontOptions (9.5f)));
        drawGlitchText (g, *this, hint, bounds.withTrimmedRight (32.0f),
                        juce::Justification::centredRight, muted.withAlpha (0.5f));
    }
}

} // namespace glitch::ui
