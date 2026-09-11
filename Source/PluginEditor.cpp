#include "PluginEditor.h"
#include <BinaryData.h>

namespace
{
const juce::Colour paper(0xff10241f),ink(0xfff3f5e9),muted(0xffbdcfc1),sage(0xff9abea3),electric(0xff85e8f3);
}

void SpaLookAndFeel::drawRotarySlider(juce::Graphics& g,int x,int y,int width,int height,float value,float start,float end,juce::Slider&)
{
    auto area=juce::Rectangle<float>((float)x,(float)y,(float)width,(float)height).reduced(8);
    const float radius=std::min(area.getWidth(),area.getHeight())*.5f;
    const auto centre=area.getCentre();
    const float angle=start+value*(end-start);
    juce::Path track,fill;
    track.addCentredArc(centre.x,centre.y,radius,radius,0,start,end,true);
    fill.addCentredArc(centre.x,centre.y,radius,radius,0,start,angle,true);
    g.setColour(juce::Colour(0xff426156));g.strokePath(track,juce::PathStrokeType(3));
    g.setColour(electric);g.strokePath(fill,juce::PathStrokeType(3));
    const float body=radius-7;
    g.setColour(juce::Colours::black.withAlpha(.4f));g.fillEllipse(centre.x-body,centre.y-body+2,body*2,body*2);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff527466),centre.x,centre.y-body,juce::Colour(0xff122e26),centre.x,centre.y+body,false));
    g.fillEllipse(centre.x-body,centre.y-body,body*2,body*2);
    g.setColour(sage.withAlpha(.65f));g.drawEllipse(centre.x-body,centre.y-body,body*2,body*2,1);
    const float dx=std::sin(angle),dy=-std::cos(angle);
    g.setColour(ink);g.drawLine(centre.x+dx*body*.4f,centre.y+dy*body*.4f,centre.x+dx*body*.82f,centre.y+dy*body*.82f,2.5f);
}

GlitchEditor::GlitchEditor(GlitchProcessor& p)
 :AudioProcessorEditor(p),processor(p),keyboard(p.keyboard)
{
    processor.visualPeak.store(0,std::memory_order_relaxed);
    look.setColour(juce::Label::textColourId,ink);
    look.setColour(juce::Slider::textBoxTextColourId,ink);
    look.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);
    look.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
    look.setColour(juce::ComboBox::backgroundColourId,juce::Colour(0xff203a30));
    look.setColour(juce::ComboBox::textColourId,ink);
    look.setColour(juce::ComboBox::arrowColourId,ink);
    look.setColour(juce::ComboBox::outlineColourId,sage.withAlpha(.35f));
    look.setColour(juce::PopupMenu::backgroundColourId,paper);
    look.setColour(juce::PopupMenu::textColourId,ink);
    look.setColour(juce::PopupMenu::highlightedBackgroundColourId,juce::Colour(0xff426156));
    look.setColour(juce::PopupMenu::highlightedTextColourId,ink);
    look.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff284638));
    look.setColour(juce::TextButton::textColourOffId,ink);
    look.setColour(juce::ToggleButton::textColourId,muted);
    look.setColour(juce::ToggleButton::tickColourId,ink);
    setLookAndFeel(&look);
    calmImage=juce::ImageCache::getFromMemory(BinaryData::spacalm_png,BinaryData::spacalm_pngSize);
    const char* images[]{BinaryData::spaelectric1_png,BinaryData::spaelectric2_png,BinaryData::spaelectric3_png,BinaryData::spaelectric4_png,BinaryData::spaelectric5_png};
    const int sizes[]{BinaryData::spaelectric1_pngSize,BinaryData::spaelectric2_pngSize,BinaryData::spaelectric3_pngSize,BinaryData::spaelectric4_pngSize,BinaryData::spaelectric5_pngSize};
    for(size_t i=0;i<electricImages.size();++i) electricImages[i]=juce::ImageCache::getFromMemory(images[i],sizes[i]);
    title.setText("SPA / GLITCH",juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions("Georgia",30.0f,juce::Font::plain)));
    load.setButtonText("Locate library");audition.setButtonText("Audition WAV");panic.setButtonText("All notes off");
    motion.setToggleState(true,juce::dontSendNotification);
    motion.setTooltip("Disable animated lightning, sparks and twitching while retaining the audio-reactive x-ray glow");
    addAndMakeVisible(motion);
    categoryLabel.setText("SAMPLE BANK",juce::dontSendNotification);
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
        knob.setColour(juce::Slider::textBoxTextColourId,ink);
        knob.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        knob.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);
        knob.setColour(juce::Slider::textBoxHighlightColourId,sage);
        knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setTextBoxStyle(juce::Slider::TextBoxBelow,false,112,20);
        knob.setRotaryParameters(juce::MathConstants<float>::pi*1.2f,juce::MathConstants<float>::pi*2.8f,true);
        labels[i].setFont(juce::Font(juce::FontOptions(10.5f,juce::Font::bold)));
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
    keyboard.setAvailableRange(0,127); keyboard.setLowestVisibleKey(12); keyboard.setKeyWidth(24);
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
    status.setFont(juce::Font(juce::FontOptions(11.5f)));
    status.setColour(juce::Label::textColourId,muted);
    effective.setFont(juce::Font(juce::FontOptions(11.5f)));
    for(auto* label:{&categoryLabel,&destroyLabel,&filterLabel}) label->setFont(juce::Font(juce::FontOptions(10.0f,juce::Font::bold)));
    keyboard.setColour(juce::MidiKeyboardComponent::whiteNoteColourId,juce::Colour(0xfff8f6ef));
    keyboard.setColour(juce::MidiKeyboardComponent::blackNoteColourId,paper);
    keyboard.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId,juce::Colour(0xffd4dacb));
    setSize(1120,780); startTimerHz(30); timerCallback();
}
GlitchEditor::~GlitchEditor() { stopTimer(); setLookAndFeel(nullptr); }
void GlitchEditor::resized()
{
    title.setBounds(28,18,290,40);
    load.setBounds(800,28,134,30);audition.setBounds(944,28,148,30);
    categoryLabel.setBounds(368,16,280,16);category.setBounds(368,36,330,32);
    photoBounds=getLocalBounds();
    const int knobWidth=140,rowHeight=110;
    const std::array<int,3> left{0,1,2};
    const std::array<int,4> right{3,4,5,6};
    for(size_t row=0;row<left.size();++row)
    {
        const auto i=(size_t)left[row];const int y=133+(int)row*rowHeight;
        labels[i].setBounds(28,y,knobWidth,18);knobs[i].setBounds(28,y+18,knobWidth,90);
    }
    for(size_t row=0;row<right.size();++row)
    {
        const auto i=(size_t)right[row];const int y=133+(int)row*rowHeight;
        labels[i].setBounds(952,y,knobWidth,18);knobs[i].setBounds(952,y+18,knobWidth,90);
    }
    destroyLabel.setBounds(32,487,132,18);destroy.setBounds(32,511,132,30);
    filterLabel.setBounds(32,554,132,18);filter.setBounds(32,578,132,30);
    motion.setBounds(952,586,130,26);
    effective.setBounds(196,625,500,22);panic.setBounds(784,625,140,26);
    status.setBounds(28,656,1064,24);
    keyboard.setBounds(28,694,1064,64);
}
void GlitchEditor::paint(juce::Graphics& g)
{
    {
        juce::Graphics::ScopedSaveState saved(g);
        juce::Path clip;clip.addRectangle(photoBounds.toFloat());g.reduceClipRegion(clip);
        const auto bounds=juce::RectanglePlacement(juce::RectanglePlacement::fillDestination).appliedTo(calmImage.getBounds().toFloat(),photoBounds.toFloat());
        g.drawImage(calmImage,bounds,juce::RectanglePlacement::stretchToFit);
        if(energy>.002f)
        {
            const auto& electricImage=electricImages[(size_t)std::max(0,shock.variation())];
            const float glow=std::min(1.f,energy*1.25f);
            g.setOpacity(glow);g.drawImage(electricImage,bounds,juce::RectanglePlacement::stretchToFit);
            if(motion.getToggleState())
            {
                // Exclude the subject from the moving background and lightning.
                juce::Path body;
                auto pt=[&](float x,float y){return juce::Point<float>(bounds.getX()+x*bounds.getWidth(),bounds.getY()+y*bounds.getHeight());};
                body.startNewSubPath(pt(.5f,.055f));
                body.cubicTo(pt(.28f,.055f),pt(.28f,.45f),pt(.39f,.68f));
                body.cubicTo(pt(.4f,.74f),pt(.1f,.69f),pt(.08f,1.f));
                body.lineTo(pt(.92f,1.f));
                body.cubicTo(pt(.9f,.69f),pt(.6f,.74f),pt(.61f,.68f));
                body.cubicTo(pt(.72f,.45f),pt(.72f,.055f),pt(.5f,.055f));body.closeSubPath();
                {
                    juce::Graphics::ScopedSaveState background(g);
                    juce::Path outside;outside.setUsingNonZeroWinding(false);
                    outside.addRectangle(bounds);outside.addPath(body);
                    g.reduceClipRegion(outside);
                    // A small outward drift gives the photographic energy depth.
                    // Crossfade two offset phases so the travel never snaps back.
                    for(int layer=0;layer<2;++layer)
                    {
                        const float phase=std::fmod(animationFrame/24.f+layer*.5f,1.f);
                        const float fade=std::sin(phase*juce::MathConstants<float>::pi);
                        const float scale=1.f+phase*.045f;
                        auto moving=bounds.withSizeKeepingCentre(bounds.getWidth()*scale,bounds.getHeight()*scale);
                        g.setOpacity(glow*fade*.48f);
                        g.drawImage(electricImage,moving,juce::RectanglePlacement::stretchToFit);
                    }
                    g.setOpacity(1.f);blast.draw(g,bounds,energy);
                }
                g.reduceClipRegion(body);
                const float dx=blast.displacement().x;
                const float dy=blast.displacement().y;
                g.setOpacity(glow*.8f);g.drawImage(electricImage,bounds.translated(dx,dy),juce::RectanglePlacement::stretchToFit);
            }
        }
    }

    // Stable contrast over both the warm photograph and the brightest blast.
    g.setColour(paper.withAlpha(.82f));g.fillRect(0,0,getWidth(),90);
    g.setColour(paper.withAlpha(.76f));
    g.fillRoundedRectangle(18,100,160,520,16);
    g.fillRoundedRectangle(942,100,160,520,16);
    g.setColour(sage.withAlpha(.3f));
    g.drawRoundedRectangle(18,100,160,520,16,1);
    g.drawRoundedRectangle(942,100,160,520,16,1);
    g.setColour(paper.withAlpha(.86f));g.fillRect(0,620,getWidth(),160);
    g.setColour(muted);g.setFont(juce::Font(juce::FontOptions(9.5f,juce::Font::bold)));
    g.drawText("S I L V E R P L A T T E R   A U D I O",32,64,290,18,juce::Justification::left);
    g.setColour(sage.withAlpha(.35f));g.drawHorizontalLine(90,0,getWidth());
    g.setColour(muted);g.setFont(juce::Font(juce::FontOptions(10.5f,juce::Font::bold)));
    g.drawText("01  /  SOUND",32,108,140,18,juce::Justification::left);
    g.drawText("02  /  ALTER",956,108,140,18,juce::Justification::left);
    g.setColour(paper.withAlpha(.8f));g.fillRoundedRectangle(192,102,136,26,13);
    g.setColour(energy>.03f ? electric : muted);
    g.drawText(energy>.03f ? "SIGNAL ACTIVE" : "AT REST",214,105,108,18,juce::Justification::left);
    g.fillEllipse(202,111,5,5);
    g.setColour(sage.withAlpha(.25f));g.fillRect(32,631,132,3);g.fillRect(32,639,132,3);
    g.setColour(electric);g.fillRect(32.f,631.f,132*std::min(1.f,meterLeft),3.f);
    g.fillRect(32.f,639.f,132*std::min(1.f,meterRight),3.f);

}
void GlitchEditor::timerCallback()
{
    const float peak=processor.visualPeak.exchange(0,std::memory_order_relaxed);
    shock.advance(peak,motion.getToggleState());energy=shock.energy();
    blast.advance(energy,motion.getToggleState());
    ++animationFrame;
    meterLeft=std::max(processor.leftPeak.load(),meterLeft*0.85f);
    meterRight=std::max(processor.rightPeak.load(),meterRight*0.85f);
    auto text=processor.contentStatus();
    if(processor.isLoading()) text+="  "+juce::String((int)(processor.loadProgress()*100))+"%";
    status.setText(text,juce::dontSendNotification);
    int group=processor.voiceCount.load()>0 ? processor.playingCategory.load() : category.getSelectedItemIndex();
    group=juce::jlimit(0,8,group);
    effective.setText("Playing: "+juce::String(glitch::categories[(size_t)group])+"   /   Pitch: "+juce::String(processor.playingPitch.load()/100000.0,1)+" st",juce::dontSendNotification);
    if(group!=highlighted)
    {
        highlighted=group;
        // The range matches the recovered per-category zone map.
        keyboard.setMapping(group,false,0);
    }
    keyboard.setMapping(group,knobs[5].getValue()==100,processor.playingPitch.load()/10000);
    repaint(photoBounds.expanded(3));repaint(190,100,740,22);
}
