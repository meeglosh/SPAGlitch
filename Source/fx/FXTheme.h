#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// SPAGlitch's faceplate palette, shared by the main editor and the FX band so
// the two can never drift apart.
namespace glitch::theme
{
inline constexpr juce::uint32 paperARGB    = 0xff10241f;
inline constexpr juce::uint32 inkARGB      = 0xfff3f5e9;
inline constexpr juce::uint32 mutedARGB    = 0xffbdcfc1;
inline constexpr juce::uint32 sageARGB     = 0xff9abea3;
inline constexpr juce::uint32 electricARGB = 0xff85e8f3;

inline const juce::Colour paper    { paperARGB };
inline const juce::Colour ink      { inkARGB };
inline const juce::Colour muted    { mutedARGB };
inline const juce::Colour sage     { sageARGB };
inline const juce::Colour electric { electricARGB };

// Panel interiors sit a shade below the faceplate so the FX band reads as a
// recessed channel rather than a second faceplate.
inline const juce::Colour well     { 0xff0b1a16 };
inline const juce::Colour outline  { 0xff426156 };

// Three feathered passes give scope traces the same neon glow the knob arcs
// have, so the FX displays sit in the same visual language as the faceplate.
void glowStroke (juce::Graphics&, const juce::Path&, juce::Colour, float thickness = 1.8f);

// The rounded, hairline-outlined card every FX control group and display is
// drawn into -- the same treatment PluginEditor gives its knob cards.
void card (juce::Graphics&, juce::Rectangle<float>, float corner = 9.0f,
           float fillAlpha = 0.72f);

// The "CC 74" / "LEARN" tag a knob wears once it is bound to a hardware
// controller. Nothing is drawn for an unbound knob.
void drawLearnBadge (juce::Graphics&, juce::Rectangle<float> knobBounds,
                     const juce::String& text, bool armed);

} // namespace glitch::theme
