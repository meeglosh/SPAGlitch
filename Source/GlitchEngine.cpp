#include "GlitchEngine.h"
#include <algorithm>
#include <cmath>

namespace glitch
{
NoteSettings forNote(const Controls& c,int note,Random& random,bool audition) noexcept
{
    NoteSettings n;
    n.category=c.category; n.pitchUnits=c.pitch*100000; n.bits=c.lofi*62500+250000;
    n.drive=c.drive; n.cutoff=c.cutoff; n.resonance=c.resonance; n.destroy=c.destroy; n.filter=c.filter;
    if(!audition && (note<12 || note>106)) { n.category=-1; return n; }
    if(c.randomness==100 && !audition)
    {
        std::array<int,9> eligible{}; int count=0;
        for(int g=0;g<9;++g) if(note<12+counts[(size_t)g]) eligible[(size_t)count++]=g;
        if(count==0) { n.category=-1; return n; }
        n.category=eligible[(size_t)random.between(0,count-1)];
    }
    const int amount=std::clamp(c.randomness,0,100);
    if(amount>0)
    {
        const int lo=random.between(-amount,amount), di=random.between(-amount,amount);
        const int cut=random.between(-amount,amount), rez=random.between(-amount,amount);
        n.pitchUnits=c.pitch*80000+random.between(-amount,amount)*10000;
        n.drive=std::clamp(c.drive+di*4000,0,1000000);
        n.bits=std::max(250000,c.lofi*62500+250000+lo*2300);
        n.cutoff=std::clamp(c.cutoff+cut*1700,0,1000000);
        n.resonance=std::clamp(c.resonance+rez/2,0,100);
        if(amount==100) { n.destroy=random.between(0,1); n.filter=random.between(0,2); }
    }
    return n;
}
void Engine::prepare(double rate) noexcept
{
    sampleRate=rate>0 ? rate : 48000;
    tube.prepare(sampleRate);
    outputGain.reset(sampleRate,0.01);
    outputGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(controls.gainDb));
    reset(); updateEffects();
}
void Engine::reset() noexcept
{
    for(auto& v:voices) v={};
    for(auto& f:filters) f={};
    heldSample.fill(0); resamplePhase.fill(1); tube.reset();
    sustain.fill(false); bend.fill(1.0); clock=0;
}
void Engine::setControls(const Controls& c) noexcept
{
    // KSP callbacks write only the addressed insert. Moving cutoff/resonance
    // while bypassed changes the randomization target, not either filter.
    if(c.category!=controls.category) selectedCategory=c.category;
    if(c.pitch!=controls.pitch) pitchKnobUsed=true;
    if(c.drive!=controls.drive) settings.drive=c.drive;
    if(c.lofi!=controls.lofi) settings.bits=c.lofi*62500+250000;
    if(c.destroy!=controls.destroy) settings.destroy=c.destroy;
    if(c.filter!=controls.filter) settings.filter=c.filter;
    if(c.randomness!=controls.randomness && c.randomness>=80 && c.randomness<100)
    {
        if(controls.randomness==100) selectedCategory=c.category;
        settings.filter=c.filter;
        if(settings.filter!=1)
            filterValues[settings.filter==2 ? 1 : 0]={lastRandomCutoff,lastRandomResonance};
    }
    if(settings.filter!=1)
    {
        auto& values=filterValues[settings.filter==2 ? 1 : 0];
        if(c.cutoff!=controls.cutoff) values[0]=c.cutoff;
        if(c.resonance!=controls.resonance) values[1]=c.resonance;
    }
    selectFilterValues();
    controls=c; outputGain.setTargetValue(juce::Decibels::decibelsToGain(c.gainDb)); updateEffects();
}
void Engine::selectFilterValues() noexcept
{
    if(settings.filter==1) return;
    const auto& values=filterValues[settings.filter==2 ? 1 : 0];
    settings.cutoff=values[0]; settings.resonance=values[1];
}
void Engine::updateEffects() noexcept
{
    // Provisional response curves: they implement the controls but are not
    // calibrated to Kontakt. Keep these separate from verified KSP arithmetic.
    const double bits=std::clamp(settings.bits/1000000.0*16.0,1.0,16.0);
    quantisation=std::pow(2.0,bits-1.0);
    tube.setParameters(settings.drive,settings.outputGainUnits());
    const double hz=std::min(20.0*std::pow(1000.0,settings.cutoff/1000000.0),sampleRate*0.45);
    filterG=std::tan(juce::MathConstants<double>::pi*hz/sampleRate);
    filterK=1.0/(0.70710678118+std::clamp(settings.resonance,0,100)*0.093);
}
void Engine::noteOn(int channel,int note,float velocity) noexcept
{
    if(!bank) return;
    // Even an unmapped key runs the original callback and can change the
    // instrument-wide effects on already sounding voices.
    auto targets=controls; targets.category=selectedCategory;
    auto selected=forNote(targets,note,random,bank->audition!=nullptr);
    if(controls.randomness>0)
    {
        settings=selected;
        lastRandomTune=selected.pitchUnits-controls.pitch*80000;
        lastRandomCutoff=selected.cutoff; lastRandomResonance=selected.resonance;
        pitchKnobUsed=false;
        if(controls.randomness==100 && selected.category>=0) selectedCategory=selected.category;
        if(settings.filter!=1)
            filterValues[settings.filter==2 ? 1 : 0]={settings.cutoff,settings.resonance};
    }
    else
    {
        settings.category=selected.category;
        settings.pitchUnits=pitchKnobUsed ? controls.pitch*100000 : lastRandomTune;
    }
    selectFilterValues(); updateEffects();
    const auto* sample=bank->get(selected.category,note);
    if(!sample) return;
    Voice* target=nullptr;
    for(auto& v:voices) if(!v.sample) { target=&v; break; }
    if(!target) target=&*std::min_element(voices.begin(),voices.end(),[](auto& a,auto& b){return a.age<b.age;});
    *target={}; target->sample=sample; target->note=note; target->channel=channel;
    target->velocity=velocityGain(velocity); target->held=true; target->age=++clock;
    double semitones=settings.pitchUnits/100000.0;
    if(bank->audition) semitones+=note-60;
    target->baseStep=sample->sampleRate/sampleRate*std::pow(2.0,semitones/12.0);
}
void Engine::noteOff(int channel,int note) noexcept
{
    for(auto& v:voices) if(v.sample && v.channel==channel && v.note==note && v.held)
    { v.held=false; if(!sustain[(size_t)channel-1]) v.releasing=true; }
}
void Engine::handle(const juce::MidiMessage& m) noexcept
{
    const int channel=m.getChannel();
    if(channel<1 || channel>16) return;
    if(m.isNoteOn()) noteOn(channel,m.getNoteNumber(),m.getFloatVelocity());
    else if(m.isNoteOff()) noteOff(channel,m.getNoteNumber());
    else if(m.isPitchWheel()) bend[(size_t)channel-1]=std::pow(2.0,((m.getPitchWheelValue()-8192)/8192.0*2.0)/12.0);
    else if(m.isController())
    {
        const int cc=m.getControllerNumber();
        if(cc==64)
        {
            sustain[(size_t)channel-1]=m.getControllerValue()>=64;
            if(!sustain[(size_t)channel-1]) for(auto& v:voices) if(v.channel==channel && !v.held) v.releasing=true;
        }
        else if(cc==120 || cc==123)
        {
            for(auto& v:voices) if(v.channel==channel) { if(cc==120) v={}; else { v.held=false; v.releasing=true; } }
        }
        else if(cc==121)
        {
            bend[(size_t)channel-1]=1.0; sustain[(size_t)channel-1]=false;
            for(auto& v:voices) if(v.channel==channel && !v.held) v.releasing=true;
        }
    }
}
float Engine::processEffect(float input,int channel) noexcept
{
    double x=input;
    if(settings.destroy==0)
    {
        // The isolated Kontakt 6 capture at 48 kHz holds for 11 frames and
        // truncates in the source-amplitude domain. Other rates and the clock's
        // start/bypass phase still need measurement; retain the old rate there.
        auto& phase=resamplePhase[(size_t)channel];
        if(phase>=1.0)
        {
            heldSample[(size_t)channel]=quantizeLoFi(x,quantisation);
            if(sampleRate==48000.0) phase=0.0;
            else phase-=std::floor(phase);
        }
        phase+=sampleRate==48000.0 ? 1.0/11.0 : 4410.0/sampleRate;
        x=heldSample[(size_t)channel];
        x=tube.process(x,channel);
    }
    if(settings.filter!=1)
    {
        // Provisional cascade. The reference mixes two/four-pole responses and
        // adapts resonance to amplitude; this topology does not yet match it.
        for(auto& s:filters[settings.filter==2 ? 1 : 0][(size_t)channel])
        {
            const double a1=1.0/(1.0+filterG*(filterG+filterK));
            const double v1=a1*(s.a+filterG*(x-s.b));
            const double v2=s.b+filterG*v1;
            s.a=2.0*v1-s.a; s.b=2.0*v2-s.b;
            x=settings.filter==2 ? v2 : x-filterK*v1-v2;
        }
    }
    return std::isfinite(x) ? (float)x : 0.0f;
}
void Engine::render(juce::AudioBuffer<float>& out,int start,int length) noexcept
{
    // Kontakt capture at 48 kHz: 479 linear fade intervals after note-off.
    // Other rates retain the inferred 10 ms duration pending reference checks.
    const float releaseStep=1.0f/(float)std::max(1.0,std::floor(sampleRate*0.01)-1.0);
    for(int i=start;i<start+length;++i)
    {
        std::array<float,2> mixed{};
        for(auto& v:voices)
        {
            if(!v.sample) continue;
            const auto& audio=v.sample->audio;
            const int frame=(int)v.position;
            if(frame>=audio.getNumSamples()) { v={}; continue; }
            const int next=std::min(frame+1,audio.getNumSamples()-1);
            const float fraction=(float)(v.position-frame), amplitude=v.velocity*v.envelope*groupGain;
            for(int ch=0;ch<2;++ch)
            {
                const auto* data=audio.getReadPointer(std::min(ch,audio.getNumChannels()-1));
                mixed[(size_t)ch]+=(data[frame]+fraction*(data[next]-data[frame]))*amplitude;
            }
            v.position+=v.baseStep*bend[(size_t)v.channel-1];
            if(v.releasing) { v.envelope=std::max(0.0f,v.envelope-releaseStep); if(v.envelope==0) v={}; }
        }
        const float gain=outputGain.getNextValue()*referenceOutputTrim;
        const float left=processEffect(mixed[0],0)*gain, right=processEffect(mixed[1],1)*gain;
        if(out.getNumChannels()==1) out.addSample(0,i,(left+right)*0.5f);
        else { out.addSample(0,i,left); out.addSample(1,i,right); }
    }
}
int Engine::activeVoices() const noexcept { return (int)std::count_if(voices.begin(),voices.end(),[](auto& v){return v.sample!=nullptr;}); }
Engine::RuntimeState Engine::runtimeState() const noexcept
{
    return {settings.bits,settings.drive,settings.cutoff,settings.resonance,settings.destroy,settings.filter,
            settings.category,settings.pitchUnits,filterValues[0][0],filterValues[0][1],filterValues[1][0],filterValues[1][1],
            selectedCategory,lastRandomTune,pitchKnobUsed?1:0,lastRandomCutoff,lastRandomResonance,
            controls.category,controls.pitch,controls.lofi,controls.drive,controls.cutoff,controls.resonance,
            controls.randomness,controls.destroy,controls.filter};
}
void Engine::restoreRuntimeState(const RuntimeState& s) noexcept
{
    settings.bits=std::clamp(s[0],250000,1000000); settings.drive=std::clamp(s[1],0,1000000);
    settings.cutoff=std::clamp(s[2],0,1000000); settings.resonance=std::clamp(s[3],0,100);
    settings.destroy=std::clamp(s[4],0,1);settings.filter=std::clamp(s[5],0,2);
    settings.category=std::clamp(s[6],-1,8);settings.pitchUnits=std::clamp(s[7],-2200000,2200000);
    filterValues[0]={std::clamp(s[8],0,1000000),std::clamp(s[9],0,100)};
    filterValues[1]={std::clamp(s[10],0,1000000),std::clamp(s[11],0,100)};
    selectedCategory=std::clamp(s[12],0,8);lastRandomTune=std::clamp(s[13],-1000000,1000000);
    pitchKnobUsed=s[14]!=0;lastRandomCutoff=std::clamp(s[15],0,1000000);lastRandomResonance=std::clamp(s[16],0,100);
    controls.category=std::clamp(s[17],0,8);controls.pitch=std::clamp(s[18],-12,12);controls.lofi=std::clamp(s[19],0,8);
    controls.drive=std::clamp(s[20],0,1000000);controls.cutoff=std::clamp(s[21],0,1000000);controls.resonance=std::clamp(s[22],0,100);
    controls.randomness=std::clamp(s[23],0,100);controls.destroy=std::clamp(s[24],0,1);controls.filter=std::clamp(s[25],0,2);
    selectFilterValues();updateEffects();
}
}
