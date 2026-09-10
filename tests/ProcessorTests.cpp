#include "../Source/Plugin.h"
#include <iostream>
int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    GlitchProcessor p; p.prepareToPlay(48000,512);
    juce::AudioBuffer<float> b(2,512); juce::MidiBuffer midi;
    b.clear(); p.processBlock(b,midi);
    if(b.getMagnitude(0,512)!=0) return 1;
    if(p.loadSample(juce::File("/nonexistent.wav")).wasOk()) return 2;
    auto file=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("spaglitch-test",".wav");
    {
        juce::WavAudioFormat wav;
        auto stream=file.createOutputStream();
        std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,1,16,{},0));
        if(!writer) return 3;
        juce::AudioBuffer<float> source(1,4800);
        for(int i=0;i<4800;++i) source.setSample(0,i,0.25f*std::sin(float(i)*0.1f));
        writer->writeFromAudioSampleBuffer(source,0,4800);
    }
    auto result=p.loadSample(file); file.deleteFile(); if(result.failed()) return 4;
    midi.addEvent(juce::MidiMessage::noteOn(1,60,1.0f),128); p.processBlock(b,midi);
    if(b.getMagnitude(0,128)!=0 || b.getMagnitude(128,384)<=0) return 5;
    auto* gain = p.parameters.getParameter("gain");
    gain->setValueNotifyingHost(gain->convertTo0to1(-18.0f));
    juce::MemoryBlock state; p.getStateInformation(state);
    GlitchProcessor restored; restored.setStateInformation(state.getData(),int(state.getSize()));
    if(std::abs(restored.parameters.getRawParameterValue("gain")->load()+18.0f)>0.001f) return 6;
    std::cout<<"PASS: silence, missing sample, WAV decode, MIDI onset, parameter state\n";
}
