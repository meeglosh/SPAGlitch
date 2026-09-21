#pragma once
#include "fx/FXTheme.h"

namespace glitch::ui
{

// The clickable bar a collapsible section hangs from: a disclosure chevron, a
// numbered title in the faceplate's index style ("03 / CHAIN"), and an
// optional hint on the right. The whole bar is the hit target, not just the
// chevron, so the sections are easy to fold away.
class DrawerHeader final : public juce::Component
{
public:
    DrawerHeader (juce::String indexText, juce::String titleText, juce::String hintText = {});

    // Trimmed with the rest of the type: the bar holds a chevron and one
    // 10.5pt line.
    static constexpr int height = 26;

    void setCollapsed (bool shouldBeCollapsed);
    bool isCollapsed() const noexcept { return collapsed; }

    std::function<void()> onToggle;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    juce::String index, title, hint;
    bool collapsed = false, hovered = false;
};

} // namespace glitch::ui
