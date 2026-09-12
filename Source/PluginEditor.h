#pragma once
#include "Plugin.h"
#include "ShockAnimation.h"
#include "BlastAnimation.h"
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
private:
    void timerCallback() override;
    GlitchProcessor& processor;
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
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,7> attachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> categoryAttachment,destroyAttachment,filterAttachment,keyRangeAttachment;
    std::unique_ptr<juce::FileChooser> chooser;
    float meterLeft=0,meterRight=0;
    float energy=0;
    int animationFrame=0;
    int highlighted=-1,lastKeyRange=-1;
};
