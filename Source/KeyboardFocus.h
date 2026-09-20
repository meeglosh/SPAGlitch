#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace glitch::ui
{

// Clears setMouseClickGrabsKeyboardFocus on a component and every current
// descendant.
//
// setWantsKeyboardFocus(false) alone does NOT stop a click from taking focus.
// JUCE grabs keyboard focus on every mouse press unconditionally
// (Component::internalMouseDown -> grabKeyboardFocusInternal), walking up the
// parent chain and re-checking each ancestor's OWN dontFocusOnMouseClickFlag
// until it finds one that either wants focus and takes it, or blocks the
// attempt. setMouseClickGrabsKeyboardFocus(false) is the flag checked first at
// each step, and the one that short-circuits the walk.
//
// It sweeps recursively because the leaves that matter are not ours: a
// ComboBox's internal text Label, a ListBox's RowComponents, viewport and
// scrollbars. Those also appear AFTER construction -- rows are built lazily
// while scrolling, and a ComboBox rebuilds its Label whenever the
// look-and-feel changes -- so a sweep has to be repeatable, not a one-off.
inline void disableMouseClickFocusGrab (juce::Component& component)
{
    component.setMouseClickGrabsKeyboardFocus (false);

    for (int i = 0; i < component.getNumChildComponents(); ++i)
        if (auto* child = component.getChildComponent (i))
            disableMouseClickFocusGrab (*child);
}

} // namespace glitch::ui
