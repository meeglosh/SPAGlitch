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

void PanicButton::paintButton(juce::Graphics& g,bool over,bool down)
{
    const juce::Colour ink(0xfff3f5e9),muted(0xffbdcfc1);
    // The mark is a quarter of the button's old size. The component stays
    // bigger than the mark so it is still comfortably clickable -- shrinking
    // the hit area to match would leave a ~10px target.
    const auto c=getLocalBounds().toFloat().getCentre();
    const float r=markDiameter*.5f;
    g.setColour(over||down ? ink : muted.withAlpha(.75f));
    g.drawEllipse(c.x-r,c.y-r,r*2.f,r*2.f,1.2f);
    const float d=r*0.707f;
    g.drawLine(c.x-d,c.y+d,c.x+d,c.y-d,1.2f);
}
GlitchEditor::GlitchEditor(GlitchProcessor& p)
 :AudioProcessorEditor(p),processor(p),keyboard(p.keyboard),
  fxSection(p.parameters,
            [&p](float* dest,int n){ return p.readScope(dest,n); },
            [&p]{ return p.getSampleRate(); },
            [&p]{ return p.limiterGainReductionDb(); },
            [&p]{ return p.limiterOutputPeak(); },
            &p.midiLearn)
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
    motion.setToggleState(true,juce::dontSendNotification);
    motion.setTooltip("Disable animated lightning, sparks and twitching while retaining the audio-reactive x-ray glow");
    faceplate.addAndMakeVisible(motion);
    categoryLabel.setText("SAMPLE BANK",juce::dontSendNotification);
    filterLabel.setText("FILTER",juce::dontSendNotification);
    for(int i=0;i<9;++i) category.addItem(glitch::categories[(size_t)i],i+1);
    filter.addItemList({"Low Pass","High Pass","Band Pass","Notch","Peak"},1);
    categoryAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters,"category",category);
    filterAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters,"filterType",filter);
    filterPower=std::make_unique<glitch::fx::ui::PowerButton>(p.parameters,"filterEnable","ON");
    faceplate.addAndMakeVisible(*filterPower);
    const char* ids[]{"pitch","randomness","gain","cutoff","resonance"};
    const char* names[]{"PITCH","RANDOMNESS","OUTPUT","CUTOFF","RESONANCE"};
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
        faceplate.addAndMakeVisible(knob); faceplate.addAndMakeVisible(labels[i]);
        attachments[i]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters,ids[i],knob);
        learnTargets[i]=std::make_unique<glitch::ui::MidiLearnTarget>(p.midiLearn,knob,ids[i],names[i]);
        learnTargets[i]->onStateChanged=[this]{ faceplate.repaint(); };
    }
    // CUTOFF is stored in KSP units (0..1000000) but reads as a percentage.
    knobs[3].textFromValueFunction=[](double v){return juce::String(v/10000.0,1)+" %";};
    knobs[3].valueFromTextFunction=[](const juce::String& v){return v.getDoubleValue()*10000.0;};
    knobs[3].updateText();
    knobs[0].setTextValueSuffix(" st"); knobs[1].setTextValueSuffix(" %");
    knobs[2].setTextValueSuffix(" dB"); knobs[4].setTextValueSuffix(" %");
    keyboard.setAvailableRange(0,127); keyboard.setLowestVisibleKey(48); keyboard.setKeyWidth(24);
    for(auto* c:std::initializer_list<juce::Component*>{&title,&status,&effective,&categoryLabel,&filterLabel,&category,&filter,&panic,&keyboard}) faceplate.addAndMakeVisible(c);
    panic.onClick=[this]{processor.allNotesOff();};
    status.setFont(juce::Font(juce::FontOptions(11.5f)));
    status.setColour(juce::Label::textColourId,muted);
    status.setJustificationType(juce::Justification::centredRight);
    effective.setFont(juce::Font(juce::FontOptions(11.5f)));
    effective.setJustificationType(juce::Justification::centredRight);
    for(auto* label:{&categoryLabel,&filterLabel}) label->setFont(juce::Font(juce::FontOptions(10.0f,juce::Font::bold)));
    keyboard.setColour(juce::MidiKeyboardComponent::whiteNoteColourId,juce::Colour(0xfff8f6ef));
    keyboard.setColour(juce::MidiKeyboardComponent::blackNoteColourId,paper);
    keyboard.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId,juce::Colour(0xffd4dacb));
    fxSection.applyOrder(processor.getFxOrder());
    fxSection.onOrderChanged=[this](const juce::Array<int>& order){ processor.setFxOrder(order); };
    fxSection.onCollapsedChanged=[this]
    { processor.fxCollapsed=fxSection.isCollapsed(); applyDrawerHeights(); };
    faceplate.addAndMakeVisible(fxSection);

    keyboardHeader.onToggle=[this]{ setKeyboardCollapsed(!isKeyboardCollapsed()); };
    faceplate.addAndMakeVisible(keyboardHeader);

    // Restore however the drawers were left, before the first sizing pass.
    fxSection.setCollapsed(processor.fxCollapsed);
    keyboardHeader.setCollapsed(processor.keyboardCollapsed);
    keyboard.setVisible(!processor.keyboardCollapsed);

    canvas.addAndMakeVisible(faceplate);
    presetsButton.setMouseClickGrabsKeyboardFocus(false);
    presetsButton.onClick=[this]{ setPresetBrowserOpen(!presetBrowserOpen); };
    faceplate.addAndMakeVisible(presetsButton);

    rollButton.setTooltip("Randomize every unlocked group");
    rollButton.setMouseClickGrabsKeyboardFocus(false);
    rollButton.onClick=[this]{ processor.randomizeAll(); };
    faceplate.addAndMakeVisible(rollButton);

    wildness.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    wildness.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
    wildness.setRange(0.0,1.0,0.01);
    wildness.setValue(processor.randomWildness(),juce::dontSendNotification);
    wildness.setTooltip("How far a roll may stray: tight and musical, through to full range");
    wildness.setMouseClickGrabsKeyboardFocus(false);
    wildness.onValueChange=[this]{ processor.setRandomWildness((float)wildness.getValue()); };
    faceplate.addAndMakeVisible(wildness);

    for(int g=0;g<glitch::rnd::numLockGroups;++g)
    {
        auto& button=lockButtons[(size_t)g];
        button.setButtonText(glitch::rnd::lockGroupName((glitch::rnd::LockGroup)g));
        button.setClickingTogglesState(true);
        button.setToggleState(processor.isGroupLocked(g),juce::dontSendNotification);
        button.setTooltip("Hold this group across a roll");
        button.setMouseClickGrabsKeyboardFocus(false);
        button.onClick=[this,g]{ processor.setGroupLocked(g,lockButtons[(size_t)g].getToggleState()); };
        button.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff2c5c52));
        faceplate.addAndMakeVisible(button);
    }

    presetBrowser=std::make_unique<glitch::ui::PresetBrowser>(p,[this]{ setPresetBrowserOpen(false); });
    canvas.addChildComponent(*presetBrowser);

    canvas.addAndMakeVisible(faceplate);
    addAndMakeVisible(canvas);

    // A pure scale: the constrainer pins the aspect ratio so the faceplate can
    // never be stretched, and the window is only ever a zoom of the design.
    setResizable(true,true);
    constrainer.setFixedAspectRatio((double)designWidth()/(double)designHeight());
    constrainer.setSizeLimits((int)(designWidth()*minScale),(int)(designHeight()*minScale),
                              (int)(designWidth()*maxScale),(int)(designHeight()*maxScale));
    setConstrainer(&constrainer);

    // Open at 100% when the display can take it, otherwise at the largest
    // whole-instrument scale that fits -- at full size this is taller than a
    // 14" laptop's screen.
    float initial=1.0f;
    if(auto* display=juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto area=display->userArea;
        initial=juce::jlimit(minScale,1.0f,
                             std::min((float)area.getWidth()*0.95f/(float)designWidth(),
                                      (float)area.getHeight()*0.92f/(float)designHeight()));
    }
    setSize((int)(designWidth()*initial),(int)(designHeight()*initial));
    startTimerHz(30); timerCallback();
}
GlitchEditor::~GlitchEditor() { stopTimer(); setLookAndFeel(nullptr); }
int GlitchEditor::designHeight() const
{
    return faceplateHeight+fxSection.preferredHeight()+keyboardStripHeight();
}
void GlitchEditor::setFxCollapsed(bool shouldBeCollapsed)
{
    fxSection.setCollapsed(shouldBeCollapsed);   // fires onCollapsedChanged
}
void GlitchEditor::setKeyboardCollapsed(bool shouldBeCollapsed)
{
    if(isKeyboardCollapsed()==shouldBeCollapsed) return;
    keyboardHeader.setCollapsed(shouldBeCollapsed);
    keyboard.setVisible(!shouldBeCollapsed);
    processor.keyboardCollapsed=shouldBeCollapsed;
    applyDrawerHeights();
}
void GlitchEditor::setPresetBrowserOpen(bool shouldBeOpen)
{
    if(presetBrowserOpen==shouldBeOpen) return;
    presetBrowserOpen=shouldBeOpen;
    presetsButton.setToggleState(shouldBeOpen,juce::dontSendNotification);
    // ChangeBroadcaster delivers asynchronously, so pull the current list
    // rather than trusting a message that may not have arrived yet.
    if(shouldBeOpen && presetBrowser!=nullptr) presetBrowser->refresh();
    applyDrawerHeights();
    if(shouldBeOpen && presetBrowser!=nullptr) presetBrowser->grabKeyboardFocus();
}
void GlitchEditor::applyDrawerHeights()
{
    // Folding a drawer, or opening the preset column, changes the design's
    // aspect ratio, so the constrainer has to be told before the window is
    // re-fitted. The scale is preserved across the change: the faceplate must
    // not shrink just because the drawer took a column beside it.
    const auto held=scale();
    const auto width=designWidth(),height=designHeight();
    constrainer.setFixedAspectRatio((double)width/(double)height);
    constrainer.setSizeLimits((int)(width*minScale),(int)(height*minScale),
                              (int)(width*maxScale),(int)(height*maxScale));
    setSize(juce::roundToInt((float)width*held),juce::roundToInt((float)height*held));
    resized();
}
void GlitchEditor::resized()
{
    // Width drives the scale; the constrainer keeps the height in step.
    canvas.setTransform(juce::AffineTransform::scale(scale()));
    canvas.setBounds(0,0,designWidth(),designHeight());
    layoutCanvas();
}
void GlitchEditor::paint(juce::Graphics& g)
{
    // Only ever visible as a hairline from rounding the scaled canvas.
    g.fillAll(juce::Colour(0xff10241f));
}
void GlitchEditor::layoutCanvas()
{
    // The faceplate keeps its own coordinate system; the preset column simply
    // takes the space to its left.
    const int x0=designWidth()-faceplateWidth;
    if(presetBrowser!=nullptr)
    {
        presetBrowser->setVisible(presetBrowserOpen);
        if(presetBrowserOpen)
            presetBrowser->setBounds(0,0,glitch::ui::PresetBrowser::width,designHeight());
    }
    faceplate.setBounds(x0,0,faceplateWidth,designHeight());
    title.setBounds(28,18,290,40);

    // Header: title left, the one remaining menu centred, and everything that
    // reports state -- what is playing, the meters, the load status and panic
    // -- gathered top right where the transport controls used to be.
    categoryLabel.setBounds((faceplateWidth-248)/2,16,248,16);
    category.setBounds((faceplateWidth-248)/2,36,248,32);
    panic.setBounds(1058,32,28,28);
    effective.setBounds(646,12,382,18);
    status.setBounds(646,56,382,18);
    photoBounds=juce::Rectangle<int>(0,0,faceplateWidth,faceplateHeight);
    const int knobWidth=140,rowHeight=112;
    // Two even columns: PITCH + RANDOMNESS and the FILTER menu on the left,
    // CUTOFF + RESONANCE + OUTPUT on the right.
    const std::array<int,3> left{0,1,2};
    const std::array<int,2> right{3,4};
    for(size_t row=0;row<left.size();++row)
    {
        const auto i=(size_t)left[row];const int y=133+(int)row*rowHeight;
        labels[i].setBounds(28,y+4,knobWidth,18);knobs[i].setBounds(28,y+22,knobWidth,80);
    }
    // The right column starts lower, under the filter menu.
    for(size_t row=0;row<right.size();++row)
    {
        const auto i=(size_t)right[row];const int y=194+(int)row*rowHeight;
        labels[i].setBounds(952,y+4,knobWidth,18);knobs[i].setBounds(952,y+22,knobWidth,80);
    }
    // The filter menu and its bypass switch sit directly above CUTOFF and
    // RESONANCE, the two knobs they drive.
    filterLabel.setBounds(956,128,60,18);
    filterPower->setBounds(1022,126,62,22);
    filter.setBounds(956,152,132,30);
    motion.setBounds(956,420,132,26);

    // PRESETS fills the header gap between the title block and the centred
    // SAMPLE BANK menu, on the menu's own row.
    presetsButton.setBounds(330,36,96,32);

    // The randomize cluster takes the strip of photograph between the cards.
    auto cluster=juce::Rectangle<int>(randomStripX+14,508,randomStripW-28,46);
    rollButton.setBounds(cluster.removeFromLeft(104).withSizeKeepingCentre(104,30));
    cluster.removeFromLeft(8);
    wildness.setBounds(cluster.removeFromLeft(46));
    cluster.removeFromLeft(10);
    for(auto& button:lockButtons)
    {
        button.setBounds(cluster.removeFromLeft(66).withSizeKeepingCentre(66,30));
        cluster.removeFromLeft(3);
    }

    fxSection.setBounds(0,faceplateHeight,faceplateWidth,fxSection.preferredHeight());

    auto strip=juce::Rectangle<int>(0,fxSection.getBottom(),faceplateWidth,keyboardStripHeight());
    keyboardHeader.setBounds(strip.removeFromTop(glitch::ui::DrawerHeader::height));
    if(!isKeyboardCollapsed())
        keyboard.setBounds(strip.reduced(28,0).withTrimmedBottom(12));
}
void GlitchEditor::paintCanvas(juce::Graphics& g)
{
    {
        juce::Graphics::ScopedSaveState saved(g);
        juce::Path clip;clip.addRectangle(photoBounds.toFloat());g.reduceClipRegion(clip);
        const auto bounds=juce::RectanglePlacement(juce::RectanglePlacement::fillDestination).appliedTo(calmImage.getBounds().toFloat(),photoBounds.toFloat());
        g.drawImage(calmImage,bounds,juce::RectanglePlacement::stretchToFit);
        if(energy>.002f)
        {
            const auto& electricImage=electricImages[(size_t)std::max(0,shock.variation())];
            const float glow=1.f; // Hard cut: the calm image never bleeds through the zap.
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
                        const float drift=1.f+phase*.045f;
                        auto moving=bounds.withSizeKeepingCentre(bounds.getWidth()*drift,bounds.getHeight()*drift);
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
    g.setColour(paper.withAlpha(.82f));g.fillRect(0,0,faceplateWidth,90);
    g.setColour(paper.withAlpha(.76f));
    g.fillRoundedRectangle(18,100,160,370,16);
    g.fillRoundedRectangle(942,100,160,370,16);
    g.setColour(sage.withAlpha(.3f));
    g.drawRoundedRectangle(18,100,160,370,16,1);
    g.drawRoundedRectangle(942,100,160,370,16,1);
    // Enclose each label, dial and readout in one visual group.
    for(size_t i=0;i<knobs.size();++i)
    {
        auto card=labels[i].getBounds().getUnion(knobs[i].getBounds()).toFloat();
        card.setY(card.getY()-4);card.setHeight(106);
        g.setColour(paper.withAlpha(.72f));g.fillRoundedRectangle(card,9);
        g.setColour(sage.withAlpha(.28f));g.drawRoundedRectangle(card,9,1);
        const auto value=knobs[i].getBounds().toFloat().removeFromBottom(20).reduced(21,0);
        g.setColour(sage.withAlpha(.10f));g.fillRoundedRectangle(value,5);
        if(learnTargets[i]!=nullptr)
            glitch::theme::drawLearnBadge(g,card,learnTargets[i]->badge(),learnTargets[i]->isArmed());
    }
    // The scrim stops at the status row: the keyboard has moved to its own
    // drawer below, so the bottom of the photograph is no longer covered.
    g.setColour(muted);g.setFont(juce::Font(juce::FontOptions(9.5f,juce::Font::bold)));
    g.drawText("S I L V E R P L A T T E R   A U D I O",32,64,290,18,juce::Justification::left);
    g.setColour(sage.withAlpha(.35f));g.drawHorizontalLine(90,0,(float)faceplateWidth);
    g.setColour(muted);g.setFont(juce::Font(juce::FontOptions(10.5f,juce::Font::bold)));
    g.drawText("01  /  SOUND",32,108,140,18,juce::Justification::left);
    g.drawText("02  /  ALTER",956,108,140,18,juce::Justification::left);
    g.setColour(paper.withAlpha(.8f));g.fillRoundedRectangle(192,102,136,26,13);
    g.setColour(energy>.03f ? electric : muted);
    g.drawText(energy>.03f ? "SIGNAL ACTIVE" : "AT REST",214,105,108,18,juce::Justification::left);
    g.fillEllipse(202,111,5,5);
    // Backing for the randomize cluster, matching the control cards.
    {
        auto strip=juce::Rectangle<float>((float)randomStripX,494.f,(float)randomStripW,74.f);
        g.setColour(paper.withAlpha(.72f));g.fillRoundedRectangle(strip,9.f);
        g.setColour(sage.withAlpha(.28f));g.drawRoundedRectangle(strip,9.f,1.f);
        g.setColour(muted);g.setFont(juce::Font(juce::FontOptions(9.5f,juce::Font::bold)));
        g.drawText("RANDOMIZE",strip.withTrimmedLeft(14.f).withHeight(16.f),juce::Justification::left);
        g.drawText("WILD",juce::Rectangle<float>((float)(randomStripX+14+112),strip.getY()+2.f,46.f,14.f),
                   juce::Justification::centred);
        g.drawText("LOCK",juce::Rectangle<float>((float)(randomStripX+14+176),strip.getY()+2.f,66.f,14.f),
                   juce::Justification::left);
    }

    // The keyboard drawer sits on the bare faceplate colour, so it needs the
    // same hairline the FX drawer paints for itself.
    g.setColour(sage.withAlpha(.35f));
    g.drawHorizontalLine(fxSection.getBottom(),0.f,(float)faceplateWidth);

    // Output meters, right-aligned in the header between the "playing" line
    // and the load status.
    constexpr float meterX=896.f,meterW=132.f;
    g.setColour(sage.withAlpha(.25f));
    g.fillRect(meterX,38.f,meterW,3.f);g.fillRect(meterX,46.f,meterW,3.f);
    g.setColour(electric);
    g.fillRect(meterX,38.f,meterW*std::min(1.f,meterLeft),3.f);
    g.fillRect(meterX,46.f,meterW*std::min(1.f,meterRight),3.f);

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
    // The key-range menu is gone; the instrument always uses the middle-key
    // mapping, which is what that menu defaulted to.
    if(lastKeyRange!=1) { lastKeyRange=1;keyboard.setLowestVisibleKey(48); }
    if(group!=highlighted)
    {
        highlighted=group;
        // The range matches the recovered per-category zone map.
        keyboard.setMapping(group,false,0,true);
    }
    keyboard.setMapping(group,knobs[1].getValue()==100,processor.playingPitch.load()/10000,true);
    faceplate.repaint(photoBounds.expanded(3));faceplate.repaint(190,100,740,22);
}
