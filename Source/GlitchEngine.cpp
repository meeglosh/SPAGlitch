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
    outputGain.reset(sampleRate,0.01);
    outputGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(controls.gainDb));
    reset(); updateEffects();
}
void Engine::reset() noexcept
{
    for(auto& v:voices) v={};
    for(auto& f:filters) f={};
    heldSample.fill(0); resamplePhase.fill(1); dcInput.fill(0); dcOutput.fill(0);
    sustain.fill(false); bend.fill(1.0); clock=0;
}
void Engine::setControls(const Controls& c) noexcept
{
    // Keep randomized effective values until a corresponding user value changes.
    if(c.drive!=controls.drive) settings.drive=c.drive;
    if(c.lofi!=controls.lofi) settings.bits=c.lofi*62500+250000;
    if(c.cutoff!=controls.cutoff) settings.cutoff=c.cutoff;
    if(c.resonance!=controls.resonance) settings.resonance=c.resonance;
    if(c.destroy!=controls.destroy) settings.destroy=c.destroy;
    if(c.filter!=controls.filter) settings.filter=c.filter;
    if(c.randomness==0 && controls.randomness!=0)
    {
        settings.drive=c.drive; settings.bits=c.lofi*62500+250000;
        settings.cutoff=c.cutoff; settings.resonance=c.resonance;
        settings.destroy=c.destroy; settings.filter=c.filter;
    }
    controls=c; outputGain.setTargetValue(juce::Decibels::decibelsToGain(c.gainDb)); updateEffects();
}
void Engine::updateEffects() noexcept
{
    // Provisional response curves: they implement the controls but are not
    // calibrated to Kontakt. Keep these separate from verified KSP arithmetic.
    const double bits=std::clamp(settings.bits/1000000.0*16.0,1.0,16.0);
    quantisation=std::pow(2.0,bits-1.0);
    driveGain=1.0+24.0*settings.drive/1000000.0;
    compensation=std::pow(10.0,-12.0*settings.drive/1000000.0/20.0);
    const double hz=std::min(20.0*std::pow(1000.0,settings.cutoff/1000000.0),sampleRate*0.45);
    filterG=std::tan(juce::MathConstants<double>::pi*hz/sampleRate);
    filterK=1.0/(0.70710678118+std::clamp(settings.resonance,0,100)*0.093);
}
void Engine::noteOn(int channel,int note,float velocity) noexcept
{
    if(!bank) return;
    auto selected=forNote(controls,note,random,bank->audition!=nullptr);
    const auto* sample=bank->get(selected.category,note);
    if(!sample) return;
    settings=selected; updateEffects();
    Voice* target=nullptr;
    for(auto& v:voices) if(!v.sample) { target=&v; break; }
    if(!target) target=&*std::min_element(voices.begin(),voices.end(),[](auto& a,auto& b){return a.age<b.age;});
    *target={}; target->sample=sample; target->note=note; target->channel=channel;
    target->velocity=velocity; target->held=true; target->age=++clock;
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
        // The original Lo-Fi module also has a fixed ~4.4 kHz sample-rate
        // reduction, independent of its scripted bit-depth control.
        auto& phase=resamplePhase[(size_t)channel];
        if(phase>=1.0)
        {
            heldSample[(size_t)channel]=std::round(x*quantisation)/quantisation;
            phase-=std::floor(phase);
        }
        phase+=4410.0/sampleRate;
        x=heldSample[(size_t)channel];
        // Asymmetric saturation for Tube mode. This is a provisional model,
        // not a reverse-engineered implementation of Kontakt's tube curve.
        x=(std::tanh(x*driveGain+0.2)-std::tanh(0.2))*compensation;
        const auto ch=(size_t)channel;
        const double dcBlocked=x-dcInput[ch]+0.995*dcOutput[ch];
        dcInput[ch]=x;dcOutput[ch]=dcBlocked;x=dcBlocked;
    }
    if(settings.filter!=1)
    {
        // Two cascaded two-pole stages provide the observed 24 dB/octave
        // topology. Kontakt's adaptive resonance response still needs calibration.
        for(auto& s:filters[(size_t)channel])
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
    const float releaseStep=1.0f/(float)(sampleRate*0.1);
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
            const float fraction=(float)(v.position-frame), amplitude=v.velocity*v.envelope*0.5011872336f; // observed group amplifier: -6 dB
            for(int ch=0;ch<2;++ch)
            {
                const auto* data=audio.getReadPointer(std::min(ch,audio.getNumChannels()-1));
                mixed[(size_t)ch]+=(data[frame]+fraction*(data[next]-data[frame]))*amplitude;
            }
            v.position+=v.baseStep*bend[(size_t)v.channel-1];
            if(v.releasing) { v.envelope=std::max(0.0f,v.envelope-releaseStep); if(v.envelope==0) v={}; }
        }
        const float gain=outputGain.getNextValue();
        const float left=processEffect(mixed[0],0)*gain, right=processEffect(mixed[1],1)*gain;
        if(out.getNumChannels()==1) out.addSample(0,i,(left+right)*0.5f);
        else { out.addSample(0,i,left); out.addSample(1,i,right); }
    }
}
int Engine::activeVoices() const noexcept { return (int)std::count_if(voices.begin(),voices.end(),[](auto& v){return v.sample!=nullptr;}); }
}
