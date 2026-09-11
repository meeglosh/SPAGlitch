#include "../Source/Plugin.h"
#include "../Source/ShockAnimation.h"
#include "../Source/BlastAnimation.h"
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
        juce::Image image(juce::Image::ARGB,320,200,true);juce::Graphics g(image);
        blast.draw(g,image.getBounds().toFloat(),1.f);
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
    for(int i=0;i<60;++i) shock.advance(0,true);
    require(shock.energy()<.002f,"Animation must settle after audio stops");
}
static void parameter(GlitchProcessor& p,const char* id,float value)
{
    auto* param=p.parameters.getParameter(id);
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
    GlitchProcessor p; p.prepareToPlay(48000,512); juce::AudioBuffer<float> b(2,512);
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
    GlitchProcessor restored; restored.prepareToPlay(48000,512);
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
    GlitchProcessor recalled;recalled.prepareToPlay(48000,512);recalled.setStateInformation(state.getData(),(int)state.getSize());
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
    GlitchProcessor p; p.prepareToPlay(48000,512); p.loadLibrary(folder);
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
    parameter(p,"randomness",0); parameter(p,"category",0); parameter(p,"destroy",1); parameter(p,"filter",1);
    juce::MemoryBlock state; p.getStateInformation(state);
    GlitchProcessor restored; restored.prepareToPlay(48000,512); restored.setNonRealtime(true);
    restored.setStateInformation(state.getData(),(int)state.getSize());
    note(restored,b,12); require(b.getMagnitude(0,512)>0,"Offline first block must await content restore");
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
        if(argc==3 && juce::String(argv[1])=="--library")
        {
            GlitchProcessor p;p.prepareToPlay(48000,512);p.loadLibrary(juce::File(argv[2]));
            require(p.waitForContent(180000),p.contentStatus().toRawUTF8());
            juce::AudioBuffer<float> b(2,512);
            for(int g=0;g<9;++g) { p.allNotesOff();parameter(p,"category",(float)g);note(p,b,12);finite(b);require(b.getMagnitude(0,512)>0,"Real library category should sound"); }
            std::cout<<"PASS: original 479-sample library loaded and all nine categories render\n";return 0;
        }
        if(argc==3 && juce::String(argv[1])=="--animation-preview")
        {
            GlitchProcessor p;std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
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
        if(argc==3 && (juce::String(argv[1])=="--screenshot" || juce::String(argv[1])=="--screenshot-active"))
        {
            GlitchProcessor p; std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
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
        shockTests();Scratch scratch;processorTests(scratch.root);libraryTests(scratch.root);engineTests();callbackParityTests();measuredReleaseTest();tubeStabilityTest();loFiClockTest();adaptiveFilterTests();
        return 0;
    }
    catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n';return 1; }
}
