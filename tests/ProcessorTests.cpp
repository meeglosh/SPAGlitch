#include "../Source/Plugin.h"
#include "../Source/ShockAnimation.h"
#include "../Source/BlastAnimation.h"
#include "../Source/fx/FXParameters.h"
#include "../Source/fx/FXSection.h"
#include "../Source/PluginEditor.h"
#include <cmath>
#include <complex>
#include <iostream>
#include <stdexcept>

static void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
static void shockTests()
{
    BlastAnimation blast;
    auto visiblePixels=[&]()
    {
        juce::Image image(juce::Image::ARGB,320,200,true);
        { juce::Graphics g(image);blast.draw(g,image.getBounds().toFloat(),1.f); }
        int count=0;
        for(int y=0;y<200;++y) for(int x=0;x<320;++x) count+=image.getPixelAt(x,y).getAlpha()>0;
        return count;
    };
    blast.advance(1.f,true);require(visiblePixels()>0,"Active blast must render");
    blast.advance(1.f,false);require(visiblePixels()==0,"Motion off must clear blast geometry");
    blast.advance(1.f,true);blast.advance(0.f,true);
    require(visiblePixels()==0,"Silent blast must clear geometry");
    ShockAnimation shock;
    shock.advance(0,true);
    require(shock.energy()==0 && shock.variation()==-1,"Silent animation must stay idle");
    int previous=-1;
    for(int group=0;group<20;++group)
    {
        int seen=0;
        for(int i=0;i<5;++i)
        {
            shock.advance(.7f,true);
            const int selected=shock.variation();
            require(selected>=0 && selected<5 && selected!=previous,"Shock must avoid immediate repeats");
            require((seen & (1<<selected))==0,"Shock bag repeated before all five appeared");
            seen|=1<<selected;previous=selected;shock.advance(0,true);
        }
        require(seen==31,"Shock bag must include all five expressions");
    }
    shock.advance(.7f,false);const int held=shock.variation();
    for(int i=0;i<60;++i) shock.advance(.7f,false);
    require(shock.variation()==held,"Motion off must hold the expression");
    bool changed=false;
    for(int i=0;i<16;++i) { shock.advance(.7f,true);changed|=shock.variation()!=held; }
    require(changed,"Sustained audio must cycle expressions");
    shock.advance(0,true);
    require(shock.energy()==0,"Zap must stop on the first silent frame without fading");
    shock.advance(.000001f,true);
    require(shock.energy()==1,"Even quiet audio must switch to a fully opaque zap");
    shock.advance(0,false);
    require(shock.energy()==0,"Motion off must retain the instant stop");
}
static void parameter(GlitchProcessor& p,const juce::String& id,float value)
{
    auto* param=p.parameters.getParameter(id);
    require(param!=nullptr,("No such parameter: "+id).toRawUTF8());
    param->setValueNotifyingHost(param->convertTo0to1(value));
}
static void writeWave(const juce::File& file,float amplitude=0.25f,int frames=4800)
{
    juce::WavAudioFormat wav; auto stream=file.createOutputStream();
    require(stream!=nullptr,"Cannot create fixture");
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,1,16,{},0));
    require(writer!=nullptr,"Cannot create WAV writer");
    juce::AudioBuffer<float> source(1,frames);
    for(int i=0;i<frames;++i) source.setSample(0,i,amplitude*std::sin((float)i*0.1f));
    require(writer->writeFromAudioSampleBuffer(source,0,frames),"Cannot write WAV");
}
static void note(GlitchProcessor& p,juce::AudioBuffer<float>& b,int key=60,int offset=0)
{
    juce::MidiBuffer midi; midi.addEvent(juce::MidiMessage::noteOn(1,key,1.0f),offset); p.processBlock(b,midi);
}
static void render(GlitchProcessor& p,juce::AudioBuffer<float>& b)
{ juce::MidiBuffer midi; p.processBlock(b,midi); }
static void finite(const juce::AudioBuffer<float>& b)
{
    for(int ch=0;ch<b.getNumChannels();++ch)
        for(int i=0;i<b.getNumSamples();++i) require(std::isfinite(b.getSample(ch,i)),"Non-finite output");
}
struct Scratch
{
    juce::File root=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("spaglitch-tests","");
    Scratch() { require(root.createDirectory().wasOk(),"Cannot create fixture folder"); }
    ~Scratch() { root.deleteRecursively(); }
};
static void processorTests(const juce::File& root)
{
    auto file=root.getChildFile("sample.wav");
    GlitchProcessor p(juce::File{}); p.prepareToPlay(48000,512); juce::AudioBuffer<float> b(2,512);
    render(p,b); require(b.getMagnitude(0,512)==0,"Empty bank should be silent");
    p.loadSample(file); require(!p.waitForContent(),"Missing WAV must fail");
    writeWave(file);
    p.loadSample(file); require(p.waitForContent(),"WAV should load");
    note(p,b,60,128);
    require(b.getMagnitude(0,128)==0 && b.getMagnitude(128,384)>0,"Sample-accurate note onset");
    const float visualHit=p.visualPeak.load();
    require(visualHit>0,"Audio must publish visual activity");
    p.allNotesOff();render(p,b);
    require(p.visualPeak.exchange(0)>=visualHit,"A short hit must survive until the next editor frame");
    render(p,b);require(p.visualPeak.load()==0,"Silence must not retrigger the visual");
    parameter(p,"gain",-18); parameter(p,"pitch",7);
    juce::MemoryBlock state; p.getStateInformation(state);
    GlitchProcessor restored(juce::File{}); restored.prepareToPlay(48000,512);
    restored.setStateInformation(state.getData(),(int)state.getSize());
    require(restored.waitForContent(),"State must reload its WAV");
    require(std::abs(restored.parameters.getRawParameterValue("gain")->load()+18)<0.001f,"Gain state recall");
    require(restored.parameters.getRawParameterValue("pitch")->load()==7,"Pitch state recall");
    note(restored,b); require(b.getMagnitude(0,512)>0,"Restored sample must play");
    require(restored.playingPitch.load()==700000,"A pitch edit saved before processing must survive runtime restore");
    restored.allNotesOff(); render(restored,b); require(b.getMagnitude(0,512)==0,"Panic must stop MIDI voices");
    // Missing content must clear old audio and remain recoverable.
    file.deleteFile(); restored.setStateInformation(state.getData(),(int)state.getSize());
    require(!restored.waitForContent(),"Missing restored content must report failure");
    note(restored,b); require(b.getMagnitude(0,512)==0,"Do not play stale sample after failed restore");
    writeWave(file); restored.loadSample(file); require(restored.waitForContent(),"Relocation/reload recovery");
    note(restored,b); require(b.getMagnitude(0,512)>0,"Recovered sample should play");
    // Newest request wins even when a preceding decode was still running.
    restored.loadSample(root.getChildFile("missing.wav")); restored.loadSample(file);
    require(restored.waitForContent(),"Newest load request must win");
    note(restored,b); require(b.getMagnitude(0,512)>0,"Newest bank should be active");
    parameter(restored,"filter",2);render(restored,b);
    parameter(restored,"cutoff",200000);render(restored,b);
    parameter(restored,"filter",0);render(restored,b);
    parameter(restored,"cutoff",800000);render(restored,b);
    restored.getStateInformation(state);
    GlitchProcessor recalled(juce::File{});recalled.prepareToPlay(48000,512);recalled.setStateInformation(state.getData(),(int)state.getSize());
    // Saving again before the first callback must preserve the pending runtime.
    juce::MemoryBlock pending;recalled.getStateInformation(pending);
    auto saved=juce::AudioProcessor::getXmlFromBinary(pending.getData(),(int)pending.getSize());
    require(saved && saved->getIntAttribute("runtime8")==800000 && saved->getIntAttribute("runtime10")==200000,"Pending state must preserve independent filters");
    require(recalled.waitForContent(),"Runtime state content reload");render(recalled,b);
    recalled.getStateInformation(pending);saved=juce::AudioProcessor::getXmlFromBinary(pending.getData(),(int)pending.getSize());
    require(saved && saved->getIntAttribute("runtime8")==800000 && saved->getIntAttribute("runtime10")==200000,"Applied state must preserve independent filters");
    std::cout<<"PASS: WAV playback, MIDI timing, restore, panic, missing-content recovery\n";
}
static void libraryTests(const juce::File& root)
{
    auto folder=root.getChildFile("library"); require(folder.createDirectory().wasOk(),"Library directory");
    for(int group=0;group<9;++group)
        for(int i=1;i<=glitch::counts[(size_t)group];++i)
            writeWave(folder.getChildFile(juce::String(glitch::categories[(size_t)group])+" "+juce::String(i).paddedLeft('0',2)+".wav"),0.05f*(group+1),960);
    GlitchProcessor p(juce::File{}); p.prepareToPlay(48000,512); parameter(p,"keyRange",0); p.loadLibrary(folder);
    require(p.waitForContent(),"All 479 category samples must load");
    juce::AudioBuffer<float> b(2,512);
    for(int group=0;group<9;++group)
    {
        p.allNotesOff(); parameter(p,"category",(float)group); note(p,b,12);
        require(b.getMagnitude(0,512)>0,"First key in each group must play");
        p.allNotesOff(); note(p,b,11+glitch::counts[(size_t)group]);
        require(b.getMagnitude(0,512)>0,"Last key in each group must play");
        p.allNotesOff(); note(p,b,12+glitch::counts[(size_t)group]);
        require(b.getMagnitude(0,512)==0,"Key beyond selected group must be silent");
    }
    parameter(p,"randomness",100); p.allNotesOff(); note(p,b,127);
    require(b.getMagnitude(0,512)==0,"Boom mode must terminate on unmapped high notes");
    for(int i=0;i<100;++i) { p.allNotesOff(); note(p,b,106); finite(b); require(p.playingCategory.load()==2,"High note must select the only eligible category"); }
    parameter(p,"randomness",0); parameter(p,"category",0); parameter(p,"filter",1);
    juce::MemoryBlock state; p.getStateInformation(state);
    GlitchProcessor restored(juce::File{}); restored.prepareToPlay(48000,512); restored.setNonRealtime(true);
    restored.setStateInformation(state.getData(),(int)state.getSize());
    note(restored,b,12); require(b.getMagnitude(0,512)>0,"Offline first block must await content restore");
    parameter(p,"keyRange",1);
    glitch::Bank mappingBank;
    for(int group=0;group<9;++group)
    {
        const int start=glitch::bankFirstNote(group,true);
        require(start>=24 && start+glitch::counts[(size_t)group]<=128,"Middle mapping must fit MIDI range");
        for(int i=0;i<glitch::counts[(size_t)group];++i)
        {
            mappingBank.samples[(size_t)group][(size_t)i]=std::make_unique<glitch::Sample>();
            require(mappingBank.get(group,start+i,true)==mappingBank.get(group,12+i,false),"Remapping must preserve every sample's identity and order");
        }
        p.allNotesOff();parameter(p,"category",(float)group);note(p,b,start);
        require(b.getMagnitude(0,512)>0,"Middle first key must play");
        p.allNotesOff();note(p,b,start+glitch::counts[(size_t)group]-1);
        require(b.getMagnitude(0,512)>0,"Middle last key must play");
        p.allNotesOff();note(p,b,start-1);require(b.getMagnitude(0,512)==0,"Below middle mapping must be silent");
    }
    glitch::Controls middle;middle.middleKeys=true;middle.randomness=100;glitch::Random rng;
    int seen=0;
    for(int i=0;i<2000;++i) { auto selected=glitch::forNote(middle,60,rng);require(glitch::noteInBank(selected.category,60,true),"Middle Boom must pick mapped banks");seen|=1<<selected.category; }
    require(seen==511,"Middle C must reach all nine banks in Boom mode");
    p.getStateInformation(state);restored.setStateInformation(state.getData(),(int)state.getSize());
    require(restored.parameters.getRawParameterValue("keyRange")->load()==1,"New projects must retain middle mapping");
    auto old=juce::AudioProcessor::getXmlFromBinary(state.getData(),(int)state.getSize());old->setAttribute("stateVersion",3);
    for(auto* child=old->getFirstChildElement();child!=nullptr;)
    { auto* next=child->getNextElement();if(child->getStringAttribute("id")=="keyRange")old->removeChildElement(child,true);child=next; }
    juce::AudioProcessor::copyXmlToBinary(*old,state);restored.setStateInformation(state.getData(),(int)state.getSize());
    require(restored.parameters.getRawParameterValue("keyRange")->load()==0,"Old projects must retain Kontakt mapping");
    std::cout<<"PASS: 479 mappings, nine categories, unmapped notes, Boom safety, offline restore\n";
}
static void engineTests()
{
    glitch::Bank bank; auto sample=std::make_unique<glitch::Sample>(); sample->audio.setSize(1,48000);
    for(int i=0;i<48000;++i) sample->audio.setSample(0,i,std::sin((float)i*0.05f)*0.2f);
    bank.samples[0][0]=std::move(sample);
    {
        glitch::Engine pitchEngine;
        glitch::Controls c; c.pitch=12; c.gainDb=0;
        pitchEngine.setControls(c); pitchEngine.setBank(&bank); pitchEngine.prepare(48000);
        pitchEngine.handle(juce::MidiMessage::noteOn(1,12,1.0f));
        juce::AudioBuffer<float> pitched(2,8);pitched.clear();pitchEngine.render(pitched,0,8);
        const float expected=bank.samples[0][0]->audio.getSample(0,2)*0.4916505699f;
        require(std::abs(pitched.getSample(0,1)-expected)<0.00001f,"Octave pitch must advance two source frames per output frame");
    }
    for(double rate:{44100.0,48000.0,96000.0})
    {
        glitch::Engine engine; engine.setBank(&bank); engine.prepare(rate);
        glitch::Controls c; engine.setControls(c);
        juce::AudioBuffer<float> b(2,256);
        engine.handle(juce::MidiMessage::noteOn(1,12,1.0f));
        engine.handle(juce::MidiMessage::controllerEvent(1,64,127)); engine.handle(juce::MidiMessage::noteOff(1,12));
        for(int i=0;i<50;++i) { b.clear();engine.render(b,0,256);finite(b); }
        require(engine.activeVoices()==1,"Sustain must hold released note");
        engine.handle(juce::MidiMessage::controllerEvent(1,64,0));
        for(int i=0;i<50;++i) { b.clear();engine.render(b,0,256);finite(b); }
        require(engine.activeVoices()==0,"Sustain release must finish envelope");
        for(int i=0;i<80;++i) engine.handle(juce::MidiMessage::noteOn(1,12,1.0f));
        require(engine.activeVoices()==32,"Voice stealing must bound polyphony");
        engine.handle(juce::MidiMessage::controllerEvent(1,120,0)); require(engine.activeVoices()==0,"All sound off");
        c.destroy=0; c.drive=1000000; c.resonance=100;
        for(int mode:{0,1,2}) for(int cutoff:{0,1000000})
        {
            c.filter=mode;c.cutoff=cutoff;engine.setControls(c);
            engine.handle(juce::MidiMessage::noteOn(1,12,1.0f));
            for(int i=0;i<10;++i) { b.clear();engine.render(b,0,256);finite(b); }
        }
    }
    glitch::Controls c; c.randomness=100;c.pitch=12;
    glitch::Random a(42),b(42);
    for(int i=0;i<10000;++i)
    {
        auto x=glitch::forNote(c,60,a),y=glitch::forNote(c,60,b);
        require(x.category==y.category && x.pitchUnits==y.pitchUnits && x.drive==y.drive,"Seeded modulation must reproduce");
        require(60<12+glitch::counts[(size_t)x.category],"Random group must cover note");
        require(x.pitchUnits>=-40000 && x.pitchUnits<=1960000,"KSP pitch arithmetic bounds");
        require(x.drive>=0 && x.drive<=1000000 && x.cutoff>=0 && x.cutoff<=1000000,"KSP drive/cutoff bounds");
        require(x.resonance>=0 && x.resonance<=100,"Resonance bounds");
    }
    std::cout<<"PASS: sustain, release, polyphony, rates, effects stability, seeded KSP arithmetic\n";
}
static void callbackParityTests()
{
    glitch::Bank bank;
    for(int g=0;g<9;++g)
    {
        auto sample=std::make_unique<glitch::Sample>(); sample->audio.setSize(1,16); sample->audio.clear();
        bank.samples[(size_t)g][0]=std::move(sample);
    }
    glitch::Engine engine; engine.setBank(&bank); engine.prepare(48000);
    glitch::Controls c;
    c.filter=2; engine.setControls(c);
    c.cutoff=200000; c.resonance=15; engine.setControls(c);
    c.filter=0; engine.setControls(c);
    c.cutoff=800000; c.resonance=75; engine.setControls(c);
    c.filter=1; engine.setControls(c);
    c.cutoff=500000; c.resonance=50; engine.setControls(c);
    c.filter=2; engine.setControls(c);
    require(engine.effective().cutoff==200000 && engine.effective().resonance==15,"LP must remember its own values; bypass edits do not write it");
    engine.handle(juce::MidiMessage::noteOn(1,12,1.0f));
    require(engine.effective().cutoff==200000,"Dry note must not overwrite latent filter settings");
    c.filter=0; engine.setControls(c);
    require(engine.effective().cutoff==800000 && engine.effective().resonance==75,"HP must remember independent values");
    c.pitch=7;c.randomness=100;engine.setControls(c);engine.setSeed(42);
    engine.handle(juce::MidiMessage::noteOn(1,12,1.0f));
    auto last=engine.effective();
    c.randomness=0;engine.setControls(c);
    engine.handle(juce::MidiMessage::noteOn(1,12,1.0f));
    auto stopped=engine.effective();
    require(stopped.category==last.category,"Boom to zero retains last selected category");
    require(stopped.drive==last.drive && stopped.bits==last.bits && stopped.filter==last.filter && stopped.destroy==last.destroy,"Randomness zero retains effective inserts");
    require(stopped.pitchUnits==last.pitchUnits-7*80000,"Randomness zero uses last random pitch offset, as KSP does");
    c.pitch=8;engine.setControls(c);engine.handle(juce::MidiMessage::noteOn(1,12,1.0f));
    require(engine.effective().pitchUnits==800000,"Pitch gesture restores manual tuning");
    c.randomness=100;engine.setControls(c);engine.handle(juce::MidiMessage::noteOn(1,12,1.0f));
    c.randomness=90;engine.setControls(c);engine.handle(juce::MidiMessage::noteOn(1,12,1.0f));
    require(engine.effective().category==c.category,"Boom to 80..99 restores user category");
    auto runtime=engine.runtimeState();
    glitch::Engine recalled;recalled.setBank(&bank);recalled.prepare(48000);recalled.restoreRuntimeState(runtime);
    require(recalled.runtimeState()==runtime,"Script state and both filter memories must round trip");
    c.randomness=0;engine.setControls(c);recalled.setControls(c);
    engine.handle(juce::MidiMessage::noteOn(1,12,1.0f));recalled.handle(juce::MidiMessage::noteOn(1,12,1.0f));
    require(engine.runtimeState()==recalled.runtimeState(),"Recalled random-to-manual transition must match uninterrupted playback");
    std::cout<<"PASS: independent filter memories and KSP random-to-manual transitions\n";
}
static void measuredReleaseTest()
{
    require(std::abs(glitch::velocityGain(64.0f/127)-0.2469635219f)<1e-6f,"Measured velocity 64 response");
    require(std::abs(glitch::velocityGain(32.0f/127)-0.0841095666f)<1e-6f,"Measured velocity 32 response");
    require(std::abs(glitch::velocityGain(1.0f/127)-0.0165420987f)<1e-6f,"Measured velocity 1 response");
    require(glitch::velocityGain(1.0f)==1.0f && glitch::velocityGain(0.0f)==0.0f,"Velocity endpoints");
    glitch::Bank bank;auto sample=std::make_unique<glitch::Sample>();sample->sampleRate=48000;
    sample->audio.setSize(1,2000);for(int i=0;i<2000;++i)sample->audio.setSample(0,i,0.25f);
    bank.samples[0][0]=std::move(sample);
    glitch::Engine engine;engine.setBank(&bank);engine.prepare(48000);
    engine.handle(juce::MidiMessage::noteOn(1,12,1.0f));engine.handle(juce::MidiMessage::noteOff(1,12));
    juce::AudioBuffer<float> audio(2,600);audio.clear();engine.render(audio,0,600);
    const float peak=0.25f*0.4916505699f;
    for(int i=0;i<600;++i)
    {
        const float expected=peak*std::max(0.0f,1.0f-i/479.0f);
        // Float envelope accumulation contributes < 9e-7 at this fixture level.
        require(std::abs(audio.getSample(0,i)-expected)<1e-6f,"48 kHz release must match measured 479-interval linear fade");
    }
    require(engine.activeVoices()==0,"Measured release terminates the voice");
    std::cout<<"PASS: reference-calibrated dry level and 48 kHz release\n";
}
static void loFiClockTest()
{
    // Phases independently measured from continuous Kontakt timelines, with
    // note-offs and natural sample ends. No per-note phase fitting in the model.
    constexpr int starts[]{1024,51025,99333,148001,196003,244017,292099,340103};
    constexpr int gates[]{2400,24000,30000,4800,2400,24000,30000,4800};
    constexpr int sizes[]{32,64,256,512};
    constexpr int phases[][8]{{10,7,5,1,1,2,2,1},{10,8,7,5,8,0,2,2},
                             {10,0,5,4,6,4,1,6},{10,5,4,0,4,4,3,2}};
    for(int setting=0;setting<4;++setting)
    {
        glitch::LoFiModel model;model.reset();model.setEnabled(true);model.setLevels(1e9);
        std::array<int,8> first;first.fill(-1);
        const int block=sizes[setting];
        for(int start=0;start<400000;start+=block)
        {
            bool active=false;
            for(int j=0;j<8;++j)
                active|=starts[j]<start+block && starts[j]+std::min(gates[j]+480,25397)>start;
            model.beginBlock(active);
            for(int frame=start;frame<std::min(start+block,400000);++frame)
            {
                int note=-1;
                for(int j=0;j<8;++j) if(frame>=starts[j] && frame<starts[j]+std::min(gates[j]+480,25397)) note=j;
                const double output=model.process(note>=0 ? 0.25 : 0,0);
                if(note>=0 && first[(size_t)note]<0 && output!=0) first[(size_t)note]=frame-starts[note];
            }
        }
        for(int j=0;j<8;++j) require(first[(size_t)j]==phases[setting][j],"Lo-Fi clock must match measured buffer-dependent note phase");
        model.setEnabled(false);model.setEnabled(true);model.beginBlock(true);
        for(int i=0;i<10;++i) require(model.process(.25,0)==0,"Re-enabling Lo-Fi resets its ten-frame countdown");
        require(model.process(.25,0)==.25,"Lo-Fi captures on frame ten after bypass reset");
    }
    std::cout<<"PASS: 32 measured Lo-Fi phases across four buffer sizes and bypass reset\n";
}
static void tubeStabilityTest()
{
    // Exercise long filter tails, stereo isolation, overload and live control
    // changes at every supported common rate. Reference accuracy is measured
    // separately by the compiled renderer; these are numerical safety checks.
    for(double rate:{44100.0,48000.0,88200.0,96000.0,192000.0})
    {
        glitch::TubeModel tube;tube.prepare(rate);tube.setParameters(0,338989);
        double tail=0;
        for(int i=0;i<(int)rate*2;++i)
        {
            if(i%997==0) tube.setParameters((i*37)%1000001,338989);
            const double input=i<4096 ? 4.0*std::sin(i*0.173) : 0.0;
            const double y=tube.process(input,0);
            require(std::isfinite(y) && std::abs(y)<16,"Tube overload must remain finite and bounded");
            require(tube.process(0,1)==0,"Tube channels must not share filter history");
            if(i>(int)rate) tail=std::max(tail,std::abs(y));
        }
        require(tail<1e-8,"Tube tail must decay to silence");
        tube.reset();require(tube.process(0,0)==0,"Tube reset must clear the tail");
        tube.setParameters(1000000,0);
        require(tube.process(1,0)==0,"Zero Tube output must mute");
    }
    std::cout<<"PASS: Tube stability, tails, stereo isolation and reset at five rates\n";
}
static void adaptiveFilterTests()
{
    for(double rate:{44100.,48000.,88200.,96000.,192000.})
        for(bool lowPass:{false,true})
        {
            glitch::AdaptiveFilter filter;filter.prepare(rate);
            for(int cutoff:{0,500000,1000000})
            {
                filter.setParameters(cutoff,100);
                for(int i=0;i<12000;++i)
                {
                    auto y=filter.process({i<6000 ? 4*std::sin(i*.35) : 0.,0.},lowPass);
                    require(std::isfinite(y[0]) && std::abs(y[0])<100,"Adaptive filter overload and parameter changes must remain bounded");
                    require(y[1]==0,"Linked resonance must not leak audio into a silent channel");
                }
            }
            filter.reset();auto y=filter.process({0,0},lowPass);
            require(y[0]==0 && y[1]==0,"Adaptive filter reset must clear both stages");
        }
    std::cout<<"PASS: adaptive filter overload, stereo isolation, reset and five rates\n";
}

// --- FX chain -------------------------------------------------------------
// Every module defaults to off, so these switch one on at a time, confirm it
// actually changes the audio, and confirm the chain order is honoured, saved
// and restored.
static void fxChainTests(const juce::File& root)
{
    namespace fxp=glitch::fx::params;
    using Chain=glitch::fx::FXChain;

    auto file=root.getChildFile("fx-source.wav");
    writeWave(file);

    auto renderNote=[&file](std::function<void(GlitchProcessor&)> configure)
    {
        GlitchProcessor p(juce::File{});p.prepareToPlay(48000,512);
        p.loadSample(file);require(p.waitForContent(),"FX fixture must load");
        configure(p);
        juce::AudioBuffer<float> out(2,512);
        note(p,out);
        finite(out);
        return out;
    };

    const auto dry=renderNote([](GlitchProcessor&){});
    require(dry.getMagnitude(0,512)>0,"FX baseline must sound");

    auto differsFromDry=[&dry](const juce::AudioBuffer<float>& wet)
    {
        for(int ch=0;ch<2;++ch)
            for(int i=0;i<512;++i)
                if(std::abs(wet.getSample(ch,i)-dry.getSample(ch,i))>1e-6f) return true;
        return false;
    };

    // Each module, switched on alone, must audibly change the output. A few
    // need help to do anything inside a single 512-sample block: at the
    // default synced 1/4 the first echo lands ~24000 samples away, and the
    // limiter is transparent until it has something to clamp.
    using Setup=std::function<void(GlitchProcessor&)>;
    const struct { const char* enableID; const char* name; Setup extra; } modules[]{
        {fxp::id::distEnable,"Distortion",{}},
        {fxp::id::chorusEnable,"Chorus",{}},
        {fxp::id::delayEnable,"Delay",[](GlitchProcessor& p)
            { parameter(p,fxp::id::delaySync,0.f);parameter(p,fxp::id::delayTime,4.f);
              parameter(p,fxp::id::delayMix,0.9f); }},
        {fxp::id::reverbEnable,"Reverb",{}},
        {fxp::id::modEnable,"Mod",{}},
        {fxp::id::tremEnable,"Tremolo",{}},
        {fxp::id::vibEnable,"Vibrato",{}},
        {fxp::id::limEnable,"Limiter",[](GlitchProcessor& p)
            { parameter(p,fxp::id::limDrive,18.f);parameter(p,fxp::id::limCeiling,-6.f); }}};
    for(const auto& module:modules)
    {
        const auto wet=renderNote([&module](GlitchProcessor& p)
        {
            parameter(p,module.enableID,1.f);
            if(module.extra) module.extra(p);
        });
        require(differsFromDry(wet),(juce::String(module.name)+" must change the output when enabled").toRawUTF8());
    }

    // EQ only does anything once a band is enabled, so it is checked apart
    // from the tab toggles above.
    {
        const auto wet=renderNote([](GlitchProcessor& p)
        {
            parameter(p,fxp::id::eqEnable,1.f);
            parameter(p,fxp::id::eqBand(0,fxp::id::eqband::enable),1.f);
            parameter(p,fxp::id::eqBand(0,fxp::id::eqband::gain),18.f);
        });
        require(differsFromDry(wet),"EQ must change the output when a band is enabled");
    }

    // Order matters: distortion into a delay is not the same signal as a delay
    // into distortion, so reordering must reach the audio thread.
    {
        auto configure=[](GlitchProcessor& p)
        {
            parameter(p,fxp::id::distEnable,1.f);parameter(p,fxp::id::distDrive,0.9f);
            parameter(p,fxp::id::delayEnable,1.f);parameter(p,fxp::id::delaySync,0.f);
            parameter(p,fxp::id::delayTime,4.f);parameter(p,fxp::id::delayFeedback,0.7f);
            parameter(p,fxp::id::delayMix,0.9f);
        };
        const auto distFirst=renderNote([&configure](GlitchProcessor& p)
        {
            configure(p);
            p.setFxOrder({(int)Chain::Module::distortion,(int)Chain::Module::delay,
                          (int)Chain::Module::chorus,(int)Chain::Module::reverb,
                          (int)Chain::Module::eq,(int)Chain::Module::mod,
                          (int)Chain::Module::tremVib,(int)Chain::Module::limiter});
        });
        const auto delayFirst=renderNote([&configure](GlitchProcessor& p)
        {
            configure(p);
            p.setFxOrder({(int)Chain::Module::delay,(int)Chain::Module::distortion,
                          (int)Chain::Module::chorus,(int)Chain::Module::reverb,
                          (int)Chain::Module::eq,(int)Chain::Module::mod,
                          (int)Chain::Module::tremVib,(int)Chain::Module::limiter});
        });
        bool different=false;
        for(int i=0;i<512 && !different;++i)
            different=std::abs(distFirst.getSample(0,i)-delayFirst.getSample(0,i))>1e-6f;
        require(different,"Reordering the chain must change the rendered audio");
    }

    // An invalid permutation must be rejected rather than leaving the audio
    // thread with a chain that skips or repeats a module.
    {
        GlitchProcessor p(juce::File{});p.prepareToPlay(48000,512);
        const auto before=p.getFxOrder();
        p.setFxOrder({0,0,0,0,0,0,0,0});                       // duplicates
        require(p.getFxOrder()==before,"A duplicate chain order must be rejected");
        p.setFxOrder({0,1,2,3,4,5,6});                         // too short
        require(p.getFxOrder()==before,"A short chain order must be rejected");
        p.setFxOrder({0,1,2,3,4,5,6,99});                      // out of range
        require(p.getFxOrder()==before,"An out-of-range chain order must be rejected");
    }

    // Chain order and FX parameters survive a save/restore round trip.
    {
        GlitchProcessor p(juce::File{});p.prepareToPlay(48000,512);
        const juce::Array<int> custom{(int)Chain::Module::limiter,(int)Chain::Module::eq,
                                      (int)Chain::Module::tremVib,(int)Chain::Module::mod,
                                      (int)Chain::Module::reverb,(int)Chain::Module::delay,
                                      (int)Chain::Module::chorus,(int)Chain::Module::distortion};
        p.setFxOrder(custom);
        parameter(p,fxp::id::reverbMix,0.77f);
        juce::MemoryBlock state;p.getStateInformation(state);

        GlitchProcessor restored(juce::File{});restored.prepareToPlay(48000,512);
        restored.setStateInformation(state.getData(),(int)state.getSize());
        require(restored.getFxOrder()==custom,"Chain order must survive a state round trip");
        require(std::abs(restored.parameters.getRawParameterValue(fxp::id::reverbMix)->load()-0.77f)<1e-4f,
                "FX parameters must survive a state round trip");
    }

    // A pre-v5 project has no stored order and must fall back to the default.
    {
        GlitchProcessor p(juce::File{});p.prepareToPlay(48000,512);
        auto legacy=juce::ValueTree("SPAGlitch");legacy.setProperty("stateVersion",4,nullptr);
        juce::MemoryBlock state;juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(),state);
        p.setStateInformation(state.getData(),(int)state.getSize());
        juce::Array<int> expected;
        glitch::fx::FXChain::Module order[glitch::fx::FXChain::numModules];
        glitch::fx::FXChain::unpackOrder(glitch::fx::FXChain::defaultOrderPacked(),order);
        for(auto module:order) expected.add((int)module);
        require(p.getFxOrder()==expected,"A pre-v5 project must fall back to the default chain order");
    }

    // Lookahead limiting is the only latency the chain introduces, and the
    // host has to be told about it.
    {
        GlitchProcessor p(juce::File{});p.prepareToPlay(48000,512);
        juce::AudioBuffer<float> b(2,512);render(p,b);
        // processBlock only publishes the figure; the processor's timer is what
        // calls setLatencySamples (doing that from a render callback would
        // notify the host from the audio thread).
        auto pumpTimers=[]{ juce::Thread::sleep(250);juce::Timer::callPendingTimersSynchronously(); };
        pumpTimers();
        require(p.getLatencySamples()==0,"An idle chain must report no latency");
        parameter(p,fxp::id::limEnable,1.f);parameter(p,fxp::id::limLookahead,1.f);
        render(p,b);
        require(p.getLatencySamples()==0,"Latency must not be applied from the audio thread");
        pumpTimers();
        require(p.getLatencySamples()>0,"Lookahead limiting must report its latency to the host");
    }

    // The EQ tab's spectrum analyser reads the post-FX output ring.
    {
        GlitchProcessor p(juce::File{});p.prepareToPlay(48000,512);
        p.loadSample(file);require(p.waitForContent(),"Analyser fixture must load");
        std::vector<float> samples((size_t)GlitchProcessor::scopeSize,0.f);
        require(!p.readScope(samples.data(),GlitchProcessor::scopeSize),
                "The analyser must report no data before anything is rendered");
        require(!p.readScope(samples.data(),64),"A wrong-sized analyser read must be refused");

        juce::AudioBuffer<float> b(2,512);
        note(p,b);
        for(int i=0;i<8;++i) render(p,b);
        require(p.readScope(samples.data(),GlitchProcessor::scopeSize),"The analyser must see rendered audio");
        float peak=0;for(auto v:samples) peak=std::max(peak,std::abs(v));
        require(peak>0,"The analyser ring must carry the rendered signal");
    }

    std::cout<<"PASS: eight FX modules, EQ bands, chain reordering, order validation, state recall, limiter latency and the analyser feed\n";
}

// The x-ray zap is tied to the instrument's notes, not to whatever is coming
// out of it. ShockAnimation is a hard gate, so metering the post-FX buffer let
// a reverb tail hold the zap on for seconds after the last note.
static void xrayFollowsNotesTest(const juce::File& root)
{
    namespace fxp=glitch::fx::params;
    auto file=root.getChildFile("xray.wav");writeWave(file);
    GlitchProcessor p(juce::File{});p.prepareToPlay(48000,512);
    p.loadSample(file);require(p.waitForContent(),"X-ray fixture must load");

    parameter(p,fxp::id::reverbEnable,1.f);
    parameter(p,fxp::id::reverbDecay,8.f);
    parameter(p,fxp::id::reverbMix,1.f);

    juce::AudioBuffer<float> b(2,512);
    note(p,b);
    require(p.visualPeak.exchange(0)>0,"A note must light the x-ray");

    // Run well past the end of the 100 ms fixture, consuming the flag each
    // block the way the editor's timer does.
    for(int i=0;i<60;++i){ render(p,b);p.visualPeak.exchange(0); }
    require(p.voiceCount.load()==0,"The note must have finished");

    render(p,b);
    require(b.getMagnitude(0,512)>1e-4f,"The reverb must still be ringing for this test to mean anything");
    require(p.visualPeak.load()==0,"An FX tail must not keep the x-ray lit");

    // A fresh note lights it again.
    note(p,b);
    require(p.visualPeak.exchange(0)>0,"A new note must light the x-ray again");

    std::cout<<"PASS: the x-ray follows notes and ignores FX tails\n";
}

// The Kontakt lo-fi + tube stage (BITS / CRUNCH, gated by DESTROY) has been
// retired from the front panel now that the FX chain carries the distortion.
// The DSP stays in the engine as a calibration asset, but nothing may switch
// it on behind the user's back.
static void destroyStageRetirementTests()
{
    GlitchProcessor p(juce::File{});p.prepareToPlay(48000,512);
    for(const char* gone:{"destroy","lofi","drive"})
        require(p.parameters.getParameter(gone)==nullptr,
                (juce::String("Retired parameter still present: ")+gone).toRawUTF8());

    // Boom randomises every insert; it must no longer reach the destroy stage.
    {
        glitch::Controls c;c.randomness=100;
        glitch::Random random(42);
        for(int i=0;i<2000;++i)
            require(glitch::forNote(c,60,random).destroy==1,
                    "Boom must not switch the destroy stage on");
    }

    // A project saved while the stage was on must not resurrect it.
    {
        glitch::Engine seed;glitch::Controls on;on.destroy=0;seed.setControls(on);
        const auto enabled=seed.runtimeState();
        require(enabled[4]==0 && enabled[24]==0,"Fixture must have the stage switched on");

        auto legacy=juce::ValueTree("SPAGlitch");legacy.setProperty("stateVersion",5,nullptr);
        for(size_t i=0;i<enabled.size();++i)
            legacy.setProperty("runtime"+juce::String((int)i),enabled[i],nullptr);
        juce::MemoryBlock block;juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(),block);
        p.setStateInformation(block.getData(),(int)block.getSize());

        juce::AudioBuffer<float> b(2,512);render(p,b);
        juce::MemoryBlock out;p.getStateInformation(out);
        auto xml=juce::AudioProcessor::getXmlFromBinary(out.getData(),(int)out.getSize());
        require(xml!=nullptr,"State must round trip");
        auto tree=juce::ValueTree::fromXml(*xml);
        require((int)tree.getProperty("runtime4")==1 && (int)tree.getProperty("runtime24")==1,
                "An old project must not restore the retired destroy stage");
    }

    std::cout<<"PASS: the destroy stage is retired, Boom cannot reach it and old projects cannot restore it\n";
}

// Finds the FX tab strip inside the editor. The band is built from plain
// components with no IDs, so the search is by type.
static juce::TabbedButtonBar* findTabBar(juce::Component& parent)
{
    for(auto* child:parent.getChildren())
    {
        if(auto* bar=dynamic_cast<juce::TabbedButtonBar*>(child)) return bar;
        if(auto* found=findTabBar(*child)) return found;
    }
    return nullptr;
}

// The end-to-end drag: a real mouseDrag on a tab button must reorder the strip
// AND publish the new order to the processor. The unit tests above cover
// setFxOrder itself; this covers the wiring between the two.
static void fxDragReorderTest()
{
    GlitchProcessor p(juce::File{});
    auto* editor=dynamic_cast<GlitchEditor*>(p.createEditor());
    require(editor!=nullptr,"The processor must build its own editor");
    std::unique_ptr<juce::AudioProcessorEditor> owned(editor);
    editor->setSize(GlitchEditor::designWidth,editor->designHeight());   // 1:1 scale

    auto* bar=findTabBar(*editor);
    require(bar!=nullptr,"The editor must contain the FX tab strip");
    require(bar->getNumTabs()==glitch::fx::FXChain::numModules,"The strip must hold every FX module");

    const auto before=p.getFxOrder();
    const auto firstName=bar->getTabNames()[0];

    // Drag tab 0 far enough right to land inside tab 2's slot.
    // The cast also asserts the strip is built from draggable buttons.
    auto* dragged=dynamic_cast<glitch::fx::ui::DraggableTabButton*>(bar->getTabButton(0));
    auto* target=bar->getTabButton(2);
    require(dragged!=nullptr,"Tabs must use the draggable button");
    require(target!=nullptr,"Tabs must have buttons");
    const auto drop=dragged->getLocalPoint(bar,juce::Point<int>(target->getBounds().getCentreX(),
                                                               target->getBounds().getCentreY()));
    const juce::MouseEvent drag(juce::Desktop::getInstance().getMainMouseSource(),
                                drop.toFloat(),juce::ModifierKeys::leftButtonModifier,
                                1.0f,0.0f,0.0f,0.0f,0.0f,dragged,dragged,
                                juce::Time::getCurrentTime(),drop.toFloat(),
                                juce::Time::getCurrentTime(),1,true);
    dragged->mouseDrag(drag);

    require(bar->getTabNames()[0]!=firstName,"Dragging a tab must reorder the strip");
    const auto after=p.getFxOrder();
    require(after!=before,"A tab drag must publish the new chain order to the processor");

    // What the strip shows and what the audio thread runs must agree.
    juce::Array<int> shown;
    for(const auto& name:bar->getTabNames())
        shown.add(glitch::fx::params::sectionTabNames().indexOf(name));
    require(shown==after,"The visible tab order must match the processor's chain order");

    std::cout<<"PASS: tab drag reorders the strip and publishes the chain order\n";
}

static MappedKeyboard* findKeyboard(juce::Component& parent)
{
    for(auto* child:parent.getChildren())
    {
        if(auto* keys=dynamic_cast<MappedKeyboard*>(child)) return keys;
        if(auto* found=findKeyboard(*child)) return found;
    }
    return nullptr;
}

// The editor is a fixed design scaled to the window, with two drawers that
// fold to their header bars and a keyboard that must stay at the very bottom.
static void editorLayoutTests(GlitchProcessor& p)
{
    auto* editor=dynamic_cast<GlitchEditor*>(p.createEditor());
    require(editor!=nullptr,"The processor must build its own editor");
    std::unique_ptr<juce::AudioProcessorEditor> owned(editor);
    editor->setSize(GlitchEditor::designWidth,editor->designHeight());

    auto* bar=findTabBar(*editor);require(bar!=nullptr,"No FX tab strip");
    auto* keys=findKeyboard(*editor);require(keys!=nullptr,"No on-screen keyboard");

    auto topIn=[editor](juce::Component& c){ return editor->getLocalPoint(&c,juce::Point<int>(0,0)).y; };

    // The keyboard belongs below the FX drawer, not inside the faceplate.
    require(topIn(*keys)>topIn(*bar),"The keyboard must sit below the FX drawer");
    require(topIn(*keys)>GlitchEditor::faceplateHeight,"The keyboard must sit below the faceplate");
    require(keys->getBottom()<=editor->designHeight(),"The keyboard must fit inside the window");

    // Resizable, and pinned to the design's aspect ratio so a host (or a drag
    // on the corner) can only ever zoom the instrument, never stretch it.
    require(editor->isResizable(),"The editor must be resizable");
    auto* constrainer=editor->getConstrainer();
    require(constrainer!=nullptr,"A resizable editor needs a constrainer");
    const int expanded=editor->designHeight();
    require(std::abs(constrainer->getFixedAspectRatio()
                     -(double)GlitchEditor::designWidth/(double)expanded)<1e-9,
            "The aspect ratio must be pinned to the design size");

    editor->setFxCollapsed(true);
    const int withoutFx=editor->designHeight();
    require(editor->isFxCollapsed(),"The FX drawer must report itself collapsed");
    require(withoutFx<expanded,"Collapsing the FX drawer must shorten the window");
    require(editor->getHeight()==withoutFx,"Folding a drawer must re-fit the window at the same scale");
    require(std::abs(constrainer->getFixedAspectRatio()
                     -(double)GlitchEditor::designWidth/(double)withoutFx)<1e-9,
            "Folding a drawer must update the pinned aspect ratio");
    // The bar's own visible flag stays set; it is its parent TabbedComponent
    // that the drawer hides.
    auto* tabs=dynamic_cast<juce::TabbedComponent*>(bar->getParentComponent());
    require(tabs!=nullptr,"The tab strip must live in a TabbedComponent");
    require(!tabs->isVisible(),"A collapsed FX drawer must hide its tabs");
    require(findKeyboard(*editor)!=nullptr && keys->isVisible(),
            "Collapsing the FX drawer must not touch the keyboard");

    editor->setKeyboardCollapsed(true);
    const int both=editor->designHeight();
    require(both<withoutFx,"Collapsing the keyboard must shorten the window further");
    require(!keys->isVisible(),"A collapsed keyboard drawer must hide its keys");
    // Both folded away leaves the faceplate plus the two header bars.
    require(both==GlitchEditor::faceplateHeight+2*glitch::ui::DrawerHeader::height,
            "Both drawers folded must leave exactly the faceplate and two headers");

    editor->setFxCollapsed(false);editor->setKeyboardCollapsed(false);
    require(editor->designHeight()==expanded,"Re-opening both drawers must restore the height");
    require(keys->isVisible(),"Re-opening the keyboard drawer must show its keys");

    // Resizing is a pure scale: children keep their position as a fraction of
    // the window, so the faceplate can never be stretched out of proportion.
    const auto fullHeight=editor->designHeight();
    const auto keysAtFull=topIn(*keys);
    editor->setSize(GlitchEditor::designWidth/2,fullHeight/2);
    require(std::abs(topIn(*keys)-keysAtFull/2)<=2,"Halving the window must halve child positions");
    editor->setSize(GlitchEditor::designWidth,fullHeight);
    require(std::abs(topIn(*keys)-keysAtFull)<=2,"Restoring the size must restore the layout");

    std::cout<<"PASS: keyboard below the FX drawer, both drawers fold, and the window scales\n";
}

// Drawer states are editor-only, but they travel with the saved state.
static void drawerStateTests()
{
    GlitchProcessor p(juce::File{});p.prepareToPlay(48000,512);
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        dynamic_cast<GlitchEditor*>(editor.get())->setFxCollapsed(true);
    }
    require(p.fxCollapsed.load(),"Folding a drawer must reach the processor");

    juce::MemoryBlock state;p.getStateInformation(state);
    GlitchProcessor restored(juce::File{});restored.prepareToPlay(48000,512);
    restored.setStateInformation(state.getData(),(int)state.getSize());
    require(restored.fxCollapsed.load(),"Drawer state must survive a state round trip");
    require(!restored.keyboardCollapsed.load(),"An un-folded drawer must stay un-folded");

    std::unique_ptr<juce::AudioProcessorEditor> editor(restored.createEditor());
    auto* glitchEditor=dynamic_cast<GlitchEditor*>(editor.get());
    require(glitchEditor->isFxCollapsed(),"A reopened editor must restore its drawers");

    std::cout<<"PASS: drawer state reaches the processor, survives a round trip and is restored\n";
}

int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    try
    {
        if(argc==3 && juce::String(argv[1])=="--filter-reference")
        {
            auto document=juce::JSON::parse(juce::File(argv[2]).loadFileAsString());
            auto rows=document["measurements"];
            require(rows.isArray() && rows.size()==5,"Filter transfer measurements must exist");
            using Complex=std::complex<double>;const Complex imaginary(0,1);
            for(auto& row:*rows.getArray()) for(bool lowPass:{false,true}) for(double frequency:{100.,500.,5000.})
            {
                const int resonance=(int)row["resonance"];
                const double g=(double)row["g"],k=(double)row["k"],r=resonance/100.;
                const double omega=2*juce::MathConstants<double>::pi*frequency/48000.;
                const Complex z=std::exp(-imaginary*omega),q=1.-z,t=g*(1.+z),den=t*t+k*t*q+q*q;
                const Complex expected=std::pow(10.,-2.4*r/20.)*
                    ((lowPass ? t*t : q*q)*den+2*r*t*t*q*q)/(den*den);
                glitch::AdaptiveFilter filter;filter.setParameters(500000,resonance);filter.prepare(48000);
                Complex measured=0;
                for(int i=0;i<96000;++i)
                {
                    const double phase=omega*i;
                    const auto output=filter.process({1e-5*std::sin(phase),0.},lowPass);
                    if(i>=48000) measured+=output[0]*std::exp(-imaginary*phase);
                }
                measured*=2.*imaginary/(48000.*1e-5);
                require(std::abs(measured-expected)<1e-4*std::max(1.,std::abs(expected)),"Filter complex response must match measured Kontakt transfer");
            }
            std::cout<<"PASS: 30 complex filter responses against measured Kontakt coefficients\n";return 0;
        }
        if(argc==3 && juce::String(argv[1])=="--lofi-reference")
        {
            auto document=juce::JSON::parse(juce::File(argv[2]).loadFileAsString());
            auto measurements=document["measurements"];
            require(measurements.isArray() && measurements.size()==200,"Lo-Fi empirical fixtures must exist");
            for(auto& row:*measurements.getArray())
            {
                const double input=(double)row["source_amplitude"]*glitch::groupGain;
                const double output=glitch::quantizeLoFi(input,std::pow(2.0,(int)row["bits"]-1))*glitch::referenceOutputTrim;
                require(std::abs(output-(double)row["measured_output"])<=(double)document["tolerance"],"Lo-Fi transfer must agree with Kontakt measurements");
            }
            std::cout<<"PASS: 200 measured Lo-Fi transfer points\n";return 0;
        }
        if(argc==3 && juce::String(argv[1])=="--velocity-reference")
        {
            auto document=juce::JSON::parse(juce::File(argv[2]).loadFileAsString());
            auto measurements=document["measurements"];
            require(measurements.isArray(),"Velocity reference data must exist");
            int verified=0;
            for(auto& row:*measurements.getArray()) if((bool)row["used_for_fit"])
            {
                const int velocity=(int)row["velocity"];
                require(std::abs(glitch::velocityGain(velocity/127.0f)-(double)row["gain"])<1e-6,"Velocity must agree with clean Kontakt measurements");
                ++verified;
            }
            require(verified==127,"Verify every clean original velocity measurement");
            std::cout<<"PASS: 127 clean Kontakt velocity measurements\n";return 0;
        }
        if((argc==2 && juce::String(argv[1])=="--installed-library") || (argc==3 && juce::String(argv[1])=="--factory-library"))
        {
            GlitchProcessor p(argc==3 ? juce::File(argv[2]) : GlitchProcessor::installedLibrary());p.prepareToPlay(48000,512);
            require(p.waitForContent(30000),"Factory library did not auto-load");
            juce::AudioBuffer<float> b(2,512);
            for(int g=0;g<9;++g) { p.allNotesOff();parameter(p,"category",(float)g);note(p,b,48);require(b.getMagnitude(0,512)>0,"Factory category silent"); }
            auto empty=juce::ValueTree("SPAGlitch");empty.setProperty("contentPath","/missing/old/library",nullptr);
            juce::MemoryBlock state;juce::AudioProcessor::copyXmlToBinary(*empty.createXml(),state);
            p.setStateInformation(state.getData(),(int)state.getSize());
            require(p.waitForContent(30000),"Old project must fall back to installed factory library");
            note(p,b,12);require(b.getMagnitude(0,512)>0,"Restored factory library silent");
            std::cout<<"PASS: automatic factory loading, all nine categories, and stale project path recovery\n";return 0;
        }
        if(argc==3 && juce::String(argv[1])=="--library")
        {
            GlitchProcessor p(juce::File{});p.prepareToPlay(48000,512);p.loadLibrary(juce::File(argv[2]));
            require(p.waitForContent(180000),p.contentStatus().toRawUTF8());
            juce::AudioBuffer<float> b(2,512);
            for(int g=0;g<9;++g) { p.allNotesOff();parameter(p,"category",(float)g);note(p,b,48);finite(b);require(b.getMagnitude(0,512)>0,"Real library category should sound"); }
            std::cout<<"PASS: original 479-sample library loaded and all nine categories render\n";return 0;
        }
        if(argc==3 && juce::String(argv[1])=="--animation-preview")
        {
            GlitchProcessor p(juce::File{});std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
            juce::File directory(argv[2]);require(directory.createDirectory().wasOk(),"Preview directory failed");
            for(int frame=0;frame<90;++frame)
            {
                p.visualPeak.store(frame<12 || frame>70 ? 0.f : .15f+.55f*std::abs(std::sin(frame*.23f)));
                juce::Thread::sleep(34);juce::Timer::callPendingTimersSynchronously();
                auto shot=editor->createComponentSnapshot(editor->getLocalBounds());
                juce::PNGImageFormat format;
                auto output=directory.getChildFile(juce::String(frame).paddedLeft('0',3)+".png").createOutputStream();
                require(output!=nullptr,"Preview frame open failed");output->setPosition(0);output->truncate();
                require(format.writeImageToStream(shot,*output),"Preview frame write failed");
            }
            return 0;
        }
        // Renders each FX tab with its effect switched on, for eyeballing the
        // band's layout and its lit-tab state without a host.
        // Renders the whole instrument in each drawer state, at 1:1.
        if(argc==3 && juce::String(argv[1])=="--layout-screenshots")
        {
            GlitchProcessor p(juce::File{});
            juce::File directory(argv[2]);require(directory.createDirectory().wasOk(),"Layout dir failed");
            const struct { bool fx,keys; const char* name; } states[]{
                {false,false,"expanded"},{true,false,"fx-collapsed"},
                {false,true,"keys-collapsed"},{true,true,"both-collapsed"}};
            for(const auto& want:states)
            {
                auto* editor=dynamic_cast<GlitchEditor*>(p.createEditor());
                require(editor!=nullptr,"No editor");
                std::unique_ptr<juce::AudioProcessorEditor> owned(editor);
                editor->setSize(GlitchEditor::designWidth,editor->designHeight());
                editor->setFxCollapsed(want.fx);
                editor->setKeyboardCollapsed(want.keys);
                juce::Timer::callPendingTimersSynchronously();
                auto shot=editor->createComponentSnapshot(editor->getLocalBounds());
                juce::PNGImageFormat format;
                auto output=directory.getChildFile(juce::String(want.name)+".png").createOutputStream();
                require(output!=nullptr,"Layout shot open failed");output->setPosition(0);output->truncate();
                require(format.writeImageToStream(shot,*output),"Layout shot write failed");
            }
            return 0;
        }
        if(argc==2 && juce::String(argv[1])=="--reverb-report")
        {
            const char* modeNames[]{"Hall","Plate","Chamber","Room","Spring"};
            const struct { int mode; float decay,size,mix; } cands[]{
                {0,2.0f,0.5f,0.30f},   // current defaults
                {1,1.5f,0.45f,0.25f},
                {3,1.2f,0.40f,0.25f},
                {1,1.0f,0.35f,0.22f}};
            for(double sr:{48000.0})
              for(const auto& cand:cands)
            {
                glitch::fx::FXChain chain;chain.prepare(sr,512);
                glitch::fx::FXChain::Params fx;fx.reverbEnable=true;
                fx.reverbMode=cand.mode;fx.reverbDecay=cand.decay;
                fx.reverbSize=cand.size;fx.reverbMix=cand.mix;
                const float decay=cand.decay;
                juce::AudioBuffer<float> b(2,512);
                double wetPeak=0,dry=0;int peakAt=-1,tailAt=-1;
                const int total=(int)(sr*10);
                for(int block=0;block*512<total;++block)
                {
                    b.clear();
                    if(block==0){b.setSample(0,0,1.f);b.setSample(1,0,1.f);}
                    chain.process(b,fx);
                    for(int i=0;i<512;++i)
                    {
                        const int idx=block*512+i;const double m=std::abs(b.getSample(0,i));
                        if(idx==0){dry=m;continue;}
                        if(m>wetPeak){wetPeak=m;peakAt=idx;}
                        if(m>1e-3) tailAt=idx;
                    }
                }
                std::cout<<juce::String(modeNames[cand.mode]).paddedRight(' ',8)
                         <<" decay "<<decay<<"s size "<<cand.size<<" mix "<<cand.mix
                         <<" -> dry "<<dry<<"  wet peak "<<wetPeak
                         <<" at "<<(int)(1000.0*peakAt/sr)<<" ms"
                         <<"  tail "<<((double)tailAt/sr)<<" s\n";
            }
            return 0;
        }
        if(argc==3 && juce::String(argv[1])=="--fx-screenshots")
        {
            namespace fxp=glitch::fx::params;
            GlitchProcessor p(juce::File{});
            juce::File directory(argv[2]);require(directory.createDirectory().wasOk(),"FX shot directory failed");
            auto* editor=dynamic_cast<GlitchEditor*>(p.createEditor());
            require(editor!=nullptr,"The processor must build its own editor");
            std::unique_ptr<juce::AudioProcessorEditor> owned(editor);
            editor->setSize(GlitchEditor::designWidth,editor->designHeight());   // 1:1 scale

            auto* bar=findTabBar(*editor);require(bar!=nullptr,"No FX tab strip");
            for(int i=0;i<fxp::numSections;++i)
            {
                const auto section=(fxp::Section)i;
                if(const auto* on=fxp::enableID(section)) parameter(p,on,1.f);
                if(const auto* second=fxp::secondEnableID(section)) parameter(p,second,1.f);
            }
            // EQ is flat until a band exists; give it a visible curve.
            parameter(p,fxp::id::eqBand(0,fxp::id::eqband::enable),1.f);
            parameter(p,fxp::id::eqBand(0,fxp::id::eqband::gain),12.f);
            parameter(p,fxp::id::eqBand(3,fxp::id::eqband::enable),1.f);
            parameter(p,fxp::id::eqBand(3,fxp::id::eqband::gain),-9.f);

            for(int tab=0;tab<bar->getNumTabs();++tab)
            {
                bar->setCurrentTabIndex(tab);
                juce::Timer::callPendingTimersSynchronously();
                auto shot=editor->createComponentSnapshot(
                    editor->getLocalBounds().withTrimmedTop(GlitchEditor::faceplateHeight));
                juce::PNGImageFormat format;
                auto output=directory.getChildFile(bar->getTabNames()[tab].replace("/","-")+".png").createOutputStream();
                require(output!=nullptr,"FX shot open failed");output->setPosition(0);output->truncate();
                require(format.writeImageToStream(shot,*output),"FX shot write failed");
            }
            return 0;
        }
        if(argc==3 && (juce::String(argv[1])=="--screenshot" || juce::String(argv[1])=="--screenshot-active"))
        {
            GlitchProcessor p(juce::File{}); std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
            if(juce::String(argv[1])=="--screenshot-active")
            {
                p.visualPeak.store(.7f);
                for(int i=0;i<40 && p.visualPeak.load()>0;++i)
                { juce::Thread::sleep(25);juce::Timer::callPendingTimersSynchronously(); }
                require(p.visualPeak.load()==0,"Editor did not consume visual audio peak");
            }
            auto shot=editor->createComponentSnapshot(editor->getLocalBounds());
            juce::PNGImageFormat format;auto output=juce::File(argv[2]).createOutputStream();
            require(output!=nullptr,"Screenshot open failed");
            output->setPosition(0); output->truncate();
            require(format.writeImageToStream(shot,*output),"Screenshot write failed");return 0;
        }
        shockTests();Scratch scratch;processorTests(scratch.root);libraryTests(scratch.root);engineTests();callbackParityTests();measuredReleaseTest();tubeStabilityTest();loFiClockTest();adaptiveFilterTests();fxChainTests(scratch.root);fxDragReorderTest();{GlitchProcessor lp(juce::File{});editorLayoutTests(lp);}drawerStateTests();destroyStageRetirementTests();xrayFollowsNotesTest(scratch.root);
        return 0;
    }
    catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n';return 1; }
}
