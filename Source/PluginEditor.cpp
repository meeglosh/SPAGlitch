#include "PluginEditor.h"

GlitchEditor::GlitchEditor(GlitchProcessor& p)
 :AudioProcessorEditor(p),processor(p),keyboard(p.keyboard)
{
    look.setColour(juce::Slider::rotarySliderFillColourId,juce::Colour(0xffc991ff));
    look.setColour(juce::Slider::rotarySliderOutlineColourId,juce::Colour(0xff34303e));
    look.setColour(juce::Slider::thumbColourId,juce::Colour(0xffe7d4ff));
    look.setColour(juce::ComboBox::backgroundColourId,juce::Colour(0xff25212e));
    look.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff34303e));
    setLookAndFeel(&look);
    title.setText("S P A G L I T C H",juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(24.0f,juce::Font::bold)));
    categoryLabel.setText("CATEGORY",juce::dontSendNotification);
    destroyLabel.setText("DESTROY",juce::dontSendNotification);
    filterLabel.setText("FILTER",juce::dontSendNotification);
    for(int i=0;i<9;++i) category.addItem(glitch::categories[(size_t)i],i+1);
    destroy.addItemList({"On","Off"},1); filter.addItemList({"High-pass","Off","Low-pass"},1);
    categoryAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters,"category",category);
    destroyAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters,"destroy",destroy);
    filterAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters,"filter",filter);
    const char* ids[]{"lofi","drive","pitch","cutoff","resonance","randomness","gain"};
    const char* names[]{"BITS","CRUNCH","PITCH","CUTOFF","RESONANCE","RANDOMNESS","OUTPUT"};
    for(size_t i=0;i<knobs.size();++i)
    {
        auto& knob=knobs[i];
        knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setTextBoxStyle(juce::Slider::TextBoxBelow,false,84,22);
        knob.setTitle(names[i]); labels[i].setText(names[i],juce::dontSendNotification);
        labels[i].setJustificationType(juce::Justification::centred);
        addAndMakeVisible(knob); addAndMakeVisible(labels[i]);
        attachments[i]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters,ids[i],knob);
    }
    for(int i:{1,3})
    {
        knobs[(size_t)i].textFromValueFunction=[](double v){return juce::String(v/10000.0,1)+" %";};
        knobs[(size_t)i].valueFromTextFunction=[](const juce::String& v){return v.getDoubleValue()*10000.0;};
    }
    knobs[1].updateText(); knobs[3].updateText();
    knobs[2].setTextValueSuffix(" st"); knobs[4].setTextValueSuffix(" %");
    knobs[5].setTextValueSuffix(" %"); knobs[6].setTextValueSuffix(" dB");
    keyboard.setAvailableRange(0,127); keyboard.setLowestVisibleKey(12); keyboard.setKeyWidth(15);
    for(auto* c:std::initializer_list<juce::Component*>{&title,&status,&effective,&categoryLabel,&destroyLabel,&filterLabel,&category,&destroy,&filter,&load,&audition,&panic,&keyboard}) addAndMakeVisible(c);
    auto choose=[this](bool single)
    {
        chooser=std::make_unique<juce::FileChooser>(single?"Choose a WAV":"Choose the Glitch Bundle or sample folder",juce::File{},single?"*.wav":"");
        const auto safe=juce::Component::SafePointer<GlitchEditor>(this);
        chooser->launchAsync(juce::FileBrowserComponent::openMode|(single?juce::FileBrowserComponent::canSelectFiles:juce::FileBrowserComponent::canSelectDirectories),
          [safe,single](const juce::FileChooser& fc)
          {
              if(!safe || fc.getResult()==juce::File{}) return;
              if(single) safe->processor.loadSample(fc.getResult()); else safe->processor.loadLibrary(fc.getResult());
          });
    };
    load.onClick=[choose]{choose(false);}; audition.onClick=[choose]{choose(true);};
    panic.onClick=[this]{processor.allNotesOff();};
    setSize(760,540); startTimerHz(20); timerCallback();
}
GlitchEditor::~GlitchEditor() { stopTimer(); setLookAndFeel(nullptr); }
void GlitchEditor::resized()
{
    title.setBounds(24,12,260,40); load.setBounds(450,18,140,28); audition.setBounds(600,18,136,28);
    categoryLabel.setBounds(24,62,100,20); category.setBounds(24,86,260,30);
    destroyLabel.setBounds(308,62,100,20); destroy.setBounds(308,86,116,30);
    filterLabel.setBounds(448,62,100,20); filter.setBounds(448,86,140,30);
    panic.setBounds(610,86,126,30);
    const std::array<juce::Rectangle<int>,7> areas{
        juce::Rectangle<int>(24,144,124,130),{24,282,124,130},{172,144,124,130},
        {172,282,124,130},{320,144,124,130},{468,154,180,210},{636,282,100,130}};
    for(size_t i=0;i<areas.size();++i)
    {
        auto r=areas[i]; labels[i].setBounds(r.removeFromTop(22)); knobs[i].setBounds(r);
    }
    effective.setBounds(320,372,308,34); status.setBounds(24,416,712,30);
    keyboard.setBounds(24,460,712, 60);
}
void GlitchEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff16131c));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff36203f),0,120,juce::Colour(0xff16131c),760,430,false));
    g.fillRoundedRectangle(12,130,736,282,10);
    g.setColour(juce::Colour(0xff594363)); g.drawLine(24,126,736,126,1);
    g.setColour(juce::Colour(0xff322a3b)); g.fillRect(308,24,118,5); g.fillRect(308,34,118,5);
    g.setColour(juce::Colour(0xff96deac));
    g.fillRect(308.0f,24.0f,118*juce::jlimit(0.0f,1.0f,meterLeft),5.0f);
    g.fillRect(308.0f,34.0f,118*juce::jlimit(0.0f,1.0f,meterRight),5.0f);
}
void GlitchEditor::timerCallback()
{
    meterLeft=std::max(processor.leftPeak.load(),meterLeft*0.85f);
    meterRight=std::max(processor.rightPeak.load(),meterRight*0.85f);
    auto text=processor.contentStatus();
    if(processor.isLoading()) text+="  "+juce::String((int)(processor.loadProgress()*100))+"%";
    status.setText(text,juce::dontSendNotification);
    int group=processor.voiceCount.load()>0 ? processor.playingCategory.load() : category.getSelectedItemIndex();
    group=juce::jlimit(0,8,group);
    effective.setText("Playing: "+juce::String(glitch::categories[(size_t)group])+"\nPitch: "+juce::String(processor.playingPitch.load()/100000.0,1)+" st",juce::dontSendNotification);
    if(group!=highlighted)
    {
        highlighted=group;
        // The range matches the recovered per-category zone map.
        keyboard.setMapping(group,false,0);
    }
    keyboard.setMapping(group,knobs[5].getValue()==100,processor.playingPitch.load()/10000);
    repaint(300,20,130,24);
}
