#pragma once
#include "Plugin.h"
#include "ShockAnimation.h"
#include "BlastAnimation.h"
#include "fx/FXSection.h"
#include "Drawer.h"
class SpaLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider&) override;
};
class MappedKeyboard final : public juce::MidiKeyboardComponent
{
public:
    explicit MappedKeyboard(juce::MidiKeyboardState& state):MidiKeyboardComponent(state,horizontalKeyboard) {}
    void setMapping(int group,bool random,int tick,bool middle)
    { category=group; boom=random; colourTick=tick; middleKeys=middle; repaint(); }
private:
    int category=0,colourTick=0;
    bool boom=false,middleKeys=true;
    juce::Colour stripe(int note) const
    {
        if(!glitch::noteInBank(category,note,middleKeys)) return juce::Colour(0xffc6ccbf);
        return boom ? juce::Colour::fromHSV((float)(((note*13+colourTick)%100+100)%100)/100.0f,0.6f,0.9f,1.0f) : juce::Colour(0xff8caa88);
    }
    void drawWhiteNote(int note,juce::Graphics& g,juce::Rectangle<float> area,bool down,bool over,juce::Colour line,juce::Colour text) override
    { MidiKeyboardComponent::drawWhiteNote(note,g,area,down,over,line,text);g.setColour(stripe(note));g.fillRect(area.removeFromBottom(5)); }
    void drawBlackNote(int note,juce::Graphics& g,juce::Rectangle<float> area,bool down,bool over,juce::Colour colour) override
    { MidiKeyboardComponent::drawBlackNote(note,g,area,down,over,colour);g.setColour(stripe(note));g.fillRect(area.removeFromBottom(4).reduced(1,0)); }
};
class GlitchEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GlitchEditor(GlitchProcessor&);
    ~GlitchEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

    // The faceplate is laid out at a fixed design size and the whole thing is
    // scaled to whatever the window is, so every pixel position below stays a
    // plain constant and the photograph can never be re-proportioned.
    static constexpr int designWidth=1120;
    // The faceplate ends immediately below the status row. It was 780 while the
    // keyboard lived inside it; once the keyboard moved to its own drawer that
    // left ~90px of photograph exposed below the status bar and nothing else,
    // so the drawers now start here instead and the whole instrument is that
    // much shorter. The artwork is 3:2, so this framing shows its full width
    // (780 cropped 50px off the sides) and trims ~29px top and bottom, most of
    // it behind the header scrim.
    static constexpr int faceplateHeight=688;
    static constexpr int keyboardHeight=64;
    // Full size is taller than a 14" laptop screen, so the window has to be
    // able to scale well below 100%.
    static constexpr float minScale=0.5f, maxScale=1.5f;
    int designHeight() const;

    // Both drawers fold to their header bar. Exposed so the processor can
    // restore them with the rest of the editor state.
    void setFxCollapsed(bool);
    void setKeyboardCollapsed(bool);
    bool isFxCollapsed() const { return fxSection.isCollapsed(); }
    bool isKeyboardCollapsed() const { return keyboardHeader.isCollapsed(); }

private:
    // Everything lives on this canvas, which is always exactly designWidth x
    // designHeight() and carries the scale transform.
    class Canvas final : public juce::Component
    {
    public:
        explicit Canvas(GlitchEditor& e):editor(e) { setInterceptsMouseClicks(false,true); }
        void paint(juce::Graphics& g) override { editor.paintCanvas(g); }
    private:
        GlitchEditor& editor;
    };

    void timerCallback() override;
    void paintCanvas(juce::Graphics&);
    void layoutCanvas();
    void applyDrawerHeights();   // re-fit the window after a drawer folds
    float scale() const { return (float)getWidth()/(float)designWidth; }

    GlitchProcessor& processor;
    Canvas canvas{*this};
    juce::ComponentBoundsConstrainer constrainer;
    SpaLookAndFeel look;
    juce::Label title,status,effective,categoryLabel,destroyLabel,filterLabel,keyRangeLabel;
    juce::TextButton load{"Locate library..."},audition{"Audition WAV..."},panic{"All notes off"};
    juce::ToggleButton motion{"Motion"};
    juce::Image calmImage;
    std::array<juce::Image,5> electricImages;
    ShockAnimation shock;
    BlastAnimation blast;
    juce::Rectangle<int> photoBounds;
    juce::ComboBox category,destroy,filter,keyRange;
    std::array<juce::Slider,7> knobs;
    std::array<juce::Label,7> labels;
    MappedKeyboard keyboard;
    glitch::fx::ui::FXSection fxSection;
    glitch::ui::DrawerHeader keyboardHeader{"04","KEYBOARD","click keys or play your MIDI controller"};
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,7> attachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> categoryAttachment,destroyAttachment,filterAttachment,keyRangeAttachment;
    std::unique_ptr<juce::FileChooser> chooser;
    float meterLeft=0,meterRight=0;
    float energy=0;
    int animationFrame=0;
    int highlighted=-1,lastKeyRange=-1;

    int keyboardStripHeight() const
    { return glitch::ui::DrawerHeader::height+(isKeyboardCollapsed() ? 0 : keyboardHeight+12); }
};
