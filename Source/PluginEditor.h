#pragma once
#include "Plugin.h"
class MappedKeyboard final : public juce::MidiKeyboardComponent
{
public:
    explicit MappedKeyboard(juce::MidiKeyboardState& state):MidiKeyboardComponent(state,horizontalKeyboard) {}
    void setMapping(int group,bool random,int tick)
    { category=group; boom=random; colourTick=tick; repaint(); }
private:
    int category=0,colourTick=0;
    bool boom=false;
    juce::Colour stripe(int note) const
    {
        if(note<12 || note>=12+glitch::counts[(size_t)category]) return juce::Colour(0xff514957);
        return boom ? juce::Colour::fromHSV((float)(((note*13+colourTick)%100+100)%100)/100.0f,0.6f,0.9f,1.0f) : juce::Colour(0xff72adf2);
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
    juce::LookAndFeel_V4 look;
    juce::Label title,status,effective,categoryLabel,destroyLabel,filterLabel;
    juce::TextButton load{"Locate library..."},audition{"Audition WAV..."},panic{"All notes off"};
    juce::ComboBox category,destroy,filter;
    std::array<juce::Slider,7> knobs;
    std::array<juce::Label,7> labels;
    MappedKeyboard keyboard;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,7> attachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> categoryAttachment,destroyAttachment,filterAttachment;
    std::unique_ptr<juce::FileChooser> chooser;
    float meterLeft=0,meterRight=0;
    int highlighted=-1;
};
