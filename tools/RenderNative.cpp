#include "../Source/Plugin.h"
#include <iostream>
#include <stdexcept>

int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    try
    {
        if(argc<3) throw std::runtime_error("Usage: SPAGlitchRender LIBRARY NEW_OUTPUT_DIRECTORY [parameter=value ...]");
        const juce::File library(argv[1]),directory(argv[2]);
        if(directory.exists() || directory.createDirectory().failed()) throw std::runtime_error("Output directory must be new and writable");
        GlitchProcessor p;p.prepareToPlay(48000,256);p.loadLibrary(library);
        if(!p.waitForContent(60000)) throw std::runtime_error(p.contentStatus().toStdString());
        for(int i=3;i<argc;++i)
        {
            juce::String item(argv[i]);auto id=item.upToFirstOccurrenceOf("=",false,false);
            auto* parameter=p.parameters.getParameter(id);
            if(!item.contains("=") || !parameter) throw std::runtime_error("Unknown parameter assignment: "+item.toStdString());
            parameter->setValueNotifyingHost(parameter->convertTo0to1(item.fromFirstOccurrenceOf("=",false,false).getFloatValue()));
        }
        juce::String report="note,velocity,gate_frames,rate,peak\n";
        for(int velocity:{127,64,32}) for(int gate:{2400,24000,144000})
        {
            constexpr int frames=192000,block=256;
            juce::AudioBuffer<float> buffer(2,block),output(2,frames);output.clear();juce::MidiBuffer midi;
            p.allNotesOff();
            for(int i=0;i<48000;i+=block) { midi.clear();p.processBlock(buffer,midi); }
            for(int i=0;i<frames;i+=block)
            {
                midi.clear();
                if(i==0) midi.addEvent(juce::MidiMessage::noteOn(1,12,(juce::uint8)velocity),0);
                if(gate>=i && gate<i+block) midi.addEvent(juce::MidiMessage::noteOff(1,12),gate-i);
                p.processBlock(buffer,midi);
                for(int ch=0;ch<2;++ch) output.copyFrom(ch,i,buffer,ch,0,juce::jmin(block,frames-i));
            }
            auto file=directory.getChildFile("n12-v"+juce::String(velocity)+"-g"+juce::String(gate)+".wav");
            juce::WavAudioFormat format;auto stream=file.createOutputStream();
            if(!stream) throw std::runtime_error("Cannot open capture");
            std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(),48000,2,24,{},0));
            if(!writer || !writer->writeFromAudioSampleBuffer(output,0,frames)) throw std::runtime_error("Cannot write capture");
            report+="12,"+juce::String(velocity)+","+juce::String(gate)+",48000,"+juce::String(output.getMagnitude(0,frames),9)+"\n";
        }
        juce::MemoryBlock state;p.getStateInformation(state);
        directory.getChildFile("native.state").replaceWithData(state.getData(),state.getSize());
        directory.getChildFile("capture.csv").replaceWithText(report);
        std::cout<<report;return 0;
    }
    catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
