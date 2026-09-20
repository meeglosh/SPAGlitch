#include "MidiLearnMenu.h"

namespace glitch::ui
{

MidiLearnTarget::MidiLearnTarget (MidiLearnManager& manager, juce::Component& knob,
                                  juce::String parameterID, juce::String displayName)
    : learn (manager), component (knob),
      paramID (std::move (parameterID)), name (std::move (displayName))
{
    component.addMouseListener (this, true);   // true: child text boxes too
    learn.addChangeListener (this);
}

MidiLearnTarget::~MidiLearnTarget()
{
    learn.removeChangeListener (this);
    component.removeMouseListener (this);
}

bool MidiLearnTarget::isArmed() const { return learn.getArmedParamID() == paramID; }

juce::String MidiLearnTarget::badge() const
{
    if (isArmed()) return "LEARN";
    const auto cc = learn.getAssignedCC (paramID);
    return cc >= 0 ? "CC " + juce::String (cc) : juce::String();
}

void MidiLearnTarget::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (onStateChanged) onStateChanged();
    component.repaint();
}

void MidiLearnTarget::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu()) return;

    const auto cc = learn.getAssignedCC (paramID);
    juce::PopupMenu menu;
    menu.addSectionHeader (name);

    if (isArmed())
        menu.addItem (1, "Stop listening");
    else
        menu.addItem (1, cc >= 0 ? "Re-learn MIDI CC..." : "Learn MIDI CC...");

    menu.addItem (2, cc >= 0 ? "Forget CC " + juce::String (cc) : "Not assigned", cc >= 0);
    menu.addSeparator();
    menu.addItem (3, "Clear every MIDI assignment");

    const auto armed = isArmed();
    juce::Component::SafePointer<juce::Component> safe (&component);
    auto* manager = &learn;
    const auto id = paramID;

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&component),
        [manager, id, armed, safe] (int result)
        {
            if (safe == nullptr) return;
            switch (result)
            {
                case 1: armed ? manager->cancelLearn() : manager->armLearn (id); break;
                case 2: manager->clearAssignment (id); break;
                case 3: manager->clearAll(); break;
                default: break;
            }
        });
}

} // namespace glitch::ui
