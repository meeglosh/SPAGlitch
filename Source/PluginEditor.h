#pragma once
#include "Plugin.h"
#include "ShockAnimation.h"
#include "BlastAnimation.h"
#include "fx/FXSection.h"
#include "Drawer.h"
#include "MidiLearnMenu.h"
#include "PresetBrowser.h"
#include "Randomizer.h"
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
// Icon-only MIDI panic: a slashed circle, the same "kill everything" glyph a
// mute button uses. Labelled by tooltip, since the header has no room for text.
class PanicButton final : public juce::Button
{
public:
    static constexpr float markDiameter=11.f;   // a quarter of the original 42px button
    PanicButton():juce::Button("All notes off")
    { setTooltip("All notes off"); setMouseClickGrabsKeyboardFocus(false); }
private:
    void paintButton(juce::Graphics&,bool over,bool down) override;
};
// RANDOMIZE ALL's trigger: a die that lands on a new face every time it is
// clicked, so a roll that happens to change little still reads as a roll.
class DiceButton final : public juce::Button
{
public:
    DiceButton();
    void roll();
private:
    void paintButton(juce::Graphics&,bool over,bool down) override;
    juce::Random random;
    int frontFace=5,backFace=2;
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
    static constexpr int faceplateWidth=1120;
    // The preset drawer opens in its own column to the LEFT: the window widens
    // by exactly its width, so it never covers any of the instrument.
    int designWidth() const
    { return faceplateWidth+(isPresetBrowserOpen() ? glitch::ui::PresetBrowser::width : 0); }
    // The faceplate ends where the control cards do. It was 780 with the
    // keyboard inside it, then 688 once that moved to its own drawer; the
    // status/meter/panic strip that occupied 620..688 has since moved into the
    // header, so the drawers start here and the instrument is shorter again.
    static constexpr int faceplateHeight=620;
    static constexpr int keyboardHeight=64;
    // Full size is taller than a 14" laptop screen, so the window has to be
    // able to scale well below 100%.
    static constexpr float minScale=0.5f, maxScale=1.5f;
    int designHeight() const;

    // Both drawers fold to their header bar. Exposed so the processor can
    // restore them with the rest of the editor state.
    void setFxCollapsed(bool);
    void setKeyboardCollapsed(bool);
    void setPresetBrowserOpen(bool);
    bool isPresetBrowserOpen() const { return presetBrowserOpen; }
    bool isFxCollapsed() const { return fxSection.isCollapsed(); }
    bool isKeyboardCollapsed() const { return keyboardHeader.isCollapsed(); }

private:
    // Everything lives on this canvas, which is always exactly designWidth x
    // designHeight() and carries the scale transform.
    class Canvas final : public juce::Component
    {
    public:
        Canvas() { setInterceptsMouseClicks(false,true); }
    };
    // The instrument itself, always faceplateWidth wide. It sits at the right
    // of the canvas so the preset column can own the space to its left, which
    // keeps every coordinate in layoutCanvas()/paintCanvas() faceplate-local.
    class Faceplate final : public juce::Component
    {
    public:
        explicit Faceplate(GlitchEditor& e):editor(e) { setInterceptsMouseClicks(false,true); }
        void paint(juce::Graphics& g) override { editor.paintCanvas(g); }
    private:
        GlitchEditor& editor;
    };

    void timerCallback() override;
    void paintCanvas(juce::Graphics&);
    void layoutCanvas();
    void applyDrawerHeights();   // re-fit the window after a drawer folds
    float scale() const { return (float)getWidth()/(float)designWidth(); }

    GlitchProcessor& processor;
    Canvas canvas;
    Faceplate faceplate{*this};
    juce::ComponentBoundsConstrainer constrainer;
    SpaLookAndFeel look;
    juce::Label title,status,effective,categoryLabel,filterLabel;
    PanicButton panic;
    juce::TooltipWindow tooltips{this,600};
    juce::ToggleButton motion{"Motion"};
    juce::Image calmImage;
    std::array<juce::Image,5> electricImages;
    ShockAnimation shock;
    BlastAnimation blast;
    juce::Rectangle<int> photoBounds;
    juce::ComboBox category,filter;
    std::unique_ptr<glitch::fx::ui::PowerButton> filterPower;
    // PITCH, RANDOMNESS, OUTPUT | CUTOFF, RESONANCE. The filter menu and its
    // bypass switch sit above the two knobs they drive.
    static constexpr int numKnobs=5;
    std::array<juce::Slider,numKnobs> knobs;
    std::array<juce::Label,numKnobs> labels;
    MappedKeyboard keyboard;
    glitch::fx::ui::FXSection fxSection;
    glitch::ui::DrawerHeader keyboardHeader{"04","KEYBOARD","click keys or play your MIDI controller"};
    std::unique_ptr<glitch::ui::PresetBrowser> presetBrowser;
    juce::TextButton presetsButton{"PRESETS"};
    DiceButton rollButton;
    juce::Slider wildness;
    std::array<juce::TextButton,glitch::rnd::numLockGroups> lockButtons;
    bool presetBrowserOpen=false;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,numKnobs> attachments;
    std::array<std::unique_ptr<glitch::ui::MidiLearnTarget>,numKnobs> learnTargets;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> categoryAttachment,filterAttachment;
    float meterLeft=0,meterRight=0;
    float energy=0;
    int animationFrame=0;
    int highlighted=-1,lastKeyRange=-1;
    // The chain order can change without the strip being touched -- a roll or
    // a preset load both rewrite it -- so the editor follows the processor
    // rather than assuming it only ever changes by dragging a tab.
    juce::Array<int> shownFxOrder;

    // The randomize cluster: the dice, WILD, and one lock per group, sitting
    // low over the photograph just above the FX drawer.
    //
    // Three columns, each wide enough for its own caption, so every caption
    // can be centred over what it labels and they line up as a row. The dice
    // column is sized by the word RANDOMIZE rather than by the dice, which
    // are narrower -- captioning the dice at their own width is what left
    // RANDOMIZE running into WILD.
    static constexpr int randomPad=14,randomColGap=12;
    static constexpr int diceCol=72,wildCol=46,lockCol=204;   // lockCol: 3*66 + 2*3
    static constexpr int diceSize=40,lockButtonW=66,lockGap=3;
    static constexpr int randomRowH=46,randomCaptionH=12;

    static constexpr int randomStripW=randomPad*2+diceCol+randomColGap+wildCol+randomColGap+lockCol;
    static constexpr int randomStripH=8+randomCaptionH+4+randomRowH+8;
    static constexpr int randomStripX=(faceplateWidth-randomStripW)/2;
    static constexpr int randomStripY=faceplateHeight-randomStripH-12;

    // Column left edges, shared by the layout and the captions above it.
    static constexpr int diceColX=randomStripX+randomPad;
    static constexpr int wildColX=diceColX+diceCol+randomColGap;
    static constexpr int lockColX=wildColX+wildCol+randomColGap;
    static constexpr int randomCaptionY=randomStripY+8;
    static constexpr int randomRowY=randomCaptionY+randomCaptionH+4;

    int keyboardStripHeight() const
    { return glitch::ui::DrawerHeader::height+(isKeyboardCollapsed() ? 0 : keyboardHeight+12); }
};
