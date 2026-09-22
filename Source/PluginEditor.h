#pragma once
#include "Plugin.h"
#include "ShockAnimation.h"
#include "CandleField.h"
#include "TechGlitch.h"
#include "VisualSettings.h"
#include "BlastAnimation.h"
#include "fx/FXTheme.h"
#include "KeyboardFocus.h"
#include "fx/FXSection.h"
#include "Drawer.h"
#include "MidiLearnMenu.h"
#include "PresetBrowser.h"
#include "Randomizer.h"
// Also the instrument's glitch source: every Label, button and combo box in
// the editor resolves to this one look-and-feel, so routing their text through
// it glitches the whole UI from a single place.
class SpaLookAndFeel final : public juce::LookAndFeel_V4,
                             public glitch::theme::GlitchSource
{
public:
    void drawRotarySlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider&) override;
    void drawLabel(juce::Graphics&,juce::Label&) override;
    // A ComboBox rebuilds this Label whenever the look-and-feel changes, so
    // the flag has to be cleared here rather than only in the editor's sweep.
    juce::Label* createComboBoxTextBox(juce::ComboBox&) override;

    // JUCE's defaults (15pt buttons, 17pt menu items) are set for a stock UI.
    // This faceplate runs on 9.5-11.5pt captions, so the stock sizes read as
    // oversized next to everything around them.
    // Only for the focus flag; drawLabel() sizes the readout -- see there.
    juce::Label* createSliderTextBox(juce::Slider&) override;
    juce::Font getTextButtonFont(juce::TextButton&,int buttonHeight) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    // JUCE derives a tab's width from the BAR's depth, not from the font the
    // tab is painted in, so trimming the bar clipped "CHORUS". DraggableTabs
    // paints at a fixed size, so measure that instead.
    int getTabButtonBestWidth(juce::TabBarButton&,int tabDepth) override;
    void drawButtonText(juce::Graphics&,juce::TextButton&,bool,bool) override;

    void setGlitch(float energy,int frame) { energyValue=energy; frameValue=frame; }
    float glitchEnergy() const override { return energyValue; }
    int glitchFrame() const override { return frameValue; }
private:
    float energyValue=0; int frameValue=0;
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
// Calm mode's switch. Not an APVTS parameter: it is an accessibility setting
// that belongs to the person at the machine, not to the patch, so it must not
// travel in a preset or get automated by a host.
class CalmButton final : public juce::Button
{
public:
    CalmButton():juce::Button("Calm mode")
    {
        setTooltip("Calm mode: replaces the flashing background with a still scene whose candles "
                   "flicker as you play. Recommended if you are sensitive to flashing light.");
        setClickingTogglesState(true);
        setMouseClickGrabsKeyboardFocus(false);
    }
private:
    void paintButton(juce::Graphics&,bool over,bool down) override;
};

// Shown once per machine, before anyone has played a note: the background
// flashes, and the people most at risk from that cannot find out safely by
// trying it.
class FlashNotice final : public juce::Component
{
public:
    FlashNotice();
    std::function<void(bool enableCalm)> onDismiss;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    juce::TextButton calmChoice{"USE CALM MODE"},keepChoice{"KEEP FLASHING"};
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
    // 90 was set for 15pt buttons and a 15pt readout; at 12.5 and 11.5 the
    // rows pack tighter, so the band comes in with them.
    static constexpr int headerHeight=74,headerMargin=24;
    // Everything on the faceplate hangs off the header rather than from
    // absolute pixels, so changing the band's height moves the lot.
    static constexpr int contentTop=headerHeight+10;
    static constexpr int meterY=32;
    static constexpr int faceplateHeight=contentTop+520;
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
    // Re-fits the window after a drawer folds or the preset column opens.
    // The scale MUST be sampled before the state that designWidth() reads
    // changes, so it is passed in rather than read here.
    void refitWindow(float heldScale);
    // Keeps QWERTY note entry alive: nothing in the instrument takes focus on
    // a click, and anything that does lose it hands it straight back.
    void restoreKeyboardFocus();
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
    CalmButton calmButton;
    std::unique_ptr<FlashNotice> flashNotice;
    juce::Image calmImage,sceneImage,candleGlow;
    std::array<juce::Image,5> electricImages;
    ShockAnimation shock;
    BlastAnimation blast;
    glitch::CandleField candles;
    glitch::TechGlitch techGlitch;
    bool calmMode=false;
    void setCalmMode(bool on,bool store);
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
    // What the instrument is doing, as opposed to what the artwork is doing.
    // Calm mode holds `energy` at zero on purpose, so the SIGNAL ACTIVE
    // readout needs its own source or it would never light again.
    float signalEnergy=0;
    int animationFrame=0;
    int highlighted=-1,lastKeyRange=-1;
    // The chain order can change without the strip being touched -- a roll or
    // a preset load both rewrite it -- so the editor follows the processor
    // rather than assuming it only ever changes by dragging a tab.
    juce::Array<int> shownFxOrder;
    int glitchTick=0,glitchFrame=0;
    bool wasGlitching=false,wasCandleMoving=false;
    // Invalidates a pending close-completion if the drawer is reopened first.
    int browserAnimSeq=0;

    // The randomize cluster: the dice, WILD, and one lock per group, sitting
    // low over the photograph just above the FX drawer.
    //
    // Three columns, each wide enough for its own caption, so every caption
    // can be centred over what it labels and they line up as a row. The dice
    // column is sized by the word RANDOMIZE rather than by the dice, which
    // are narrower -- captioning the dice at their own width is what left
    // RANDOMIZE running into WILD.
    // Header right cluster: the readouts, the meters and panic, as one block
    // ending on a common margin rather than an icon stranded in the corner.
    static constexpr int panicSize=24,panicGap=14,meterW=108;
    static constexpr int headerRight=faceplateWidth-headerMargin;
    static constexpr int panicX=headerRight-panicSize;
    static constexpr int readoutRight=panicX-panicGap;
    static constexpr int readoutX=700;
    static constexpr int meterX=readoutRight-meterW;
    // Sits in the meter's row, between the two readout lines, so it needs no
    // extra header height -- which Mike has twice asked to keep down.
    static constexpr int calmW=100,calmH=20,calmGap=10;
    static constexpr int calmX=meterX-calmGap-calmW;

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
