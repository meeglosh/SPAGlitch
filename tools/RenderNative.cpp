#include "../Source/Plugin.h"
#include <iostream>
#include <stdexcept>

int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    try
    {
        if(argc==7 && juce::String(argv[1])=="--filter-only")
        {
            juce::AudioFormatManager formats;formats.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(juce::File(argv[2])));
            if(!reader || reader->numChannels!=2 || reader->lengthInSamples>10000000)
                throw std::runtime_error("Expected a short stereo reference WAV");
            const juce::File outputFile(argv[3]);
            if(outputFile.exists()) throw std::runtime_error("Output must be new");
            juce::AudioBuffer<float> audio(2,(int)reader->lengthInSamples);
            if(!reader->read(&audio,0,audio.getNumSamples(),0,true,true)) throw std::runtime_error("Reference read failed");
            glitch::AdaptiveFilter filter;
            filter.setParameters(juce::String(argv[4]).getIntValue(),juce::String(argv[5]).getIntValue());
            filter.prepare(reader->sampleRate);
            const bool lowPass=juce::String(argv[6])=="lp";
            if(!lowPass && juce::String(argv[6])!="hp") throw std::runtime_error("Filter mode must be lp or hp");
            for(int i=0;i<audio.getNumSamples();++i)
            {
                auto output=filter.process({audio.getSample(0,i)/glitch::referenceOutputTrim,
                                            audio.getSample(1,i)/glitch::referenceOutputTrim},lowPass);
                for(int ch=0;ch<2;++ch) audio.setSample(ch,i,(float)(output[(size_t)ch]*glitch::referenceOutputTrim));
            }
            juce::WavAudioFormat wav;std::unique_ptr<juce::OutputStream> stream=outputFile.createOutputStream();
            if(!stream) throw std::runtime_error("Cannot open output WAV");
            auto options=juce::AudioFormatWriterOptions().withSampleRate(reader->sampleRate).withNumChannels(2)
                .withBitsPerSample(32).withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
            auto writer=wav.createWriterFor(stream,options);
            if(!writer || !writer->writeFromAudioSampleBuffer(audio,0,audio.getNumSamples())) throw std::runtime_error("Output write failed");
            return 0;
        }
        if(argc>=6 && juce::String(argv[1])=="--tube-only")
        {
            // The input is a dry reference at the instrument's normal output
            // level. Undo that trim before testing the actual production model.
            juce::AudioFormatManager formats;formats.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(juce::File(argv[2])));
            if(!reader || reader->numChannels!=2 || reader->lengthInSamples>10000000)
                throw std::runtime_error("Expected a short stereo reference WAV");
            const juce::File outputFile(argv[3]);
            if(outputFile.exists()) throw std::runtime_error("Output must be new");
            juce::AudioBuffer<float> audio(2,(int)reader->lengthInSamples);
            if(!reader->read(&audio,0,audio.getNumSamples(),0,true,true)) throw std::runtime_error("Reference read failed");
            glitch::TubeModel tube;tube.prepare(reader->sampleRate);
            tube.setParameters(juce::String(argv[4]).getIntValue(),juce::String(argv[5]).getIntValue());
            const double inputScale=argc>6 ? juce::String(argv[6]).getDoubleValue() : 1.0;
            for(int i=0;i<audio.getNumSamples();++i) for(int ch=0;ch<2;++ch)
                audio.setSample(ch,i,(float)(tube.process(audio.getSample(ch,i)*inputScale/glitch::referenceOutputTrim,ch)*glitch::referenceOutputTrim));
            juce::WavAudioFormat wav;auto stream=outputFile.createOutputStream();
            if(!stream) throw std::runtime_error("Cannot open output WAV");
            std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),reader->sampleRate,2,24,{},0));
            if(!writer || !writer->writeFromAudioSampleBuffer(audio,0,audio.getNumSamples())) throw std::runtime_error("Output write failed");
            return 0;
        }
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
            // Match ReferenceHost's MIDI CC120 drain, rather than the native
            // UI panic (which additionally resets effect state and clock phase).
            for(int i=0;i<48000;i+=block)
            {
                midi.clear();if(i==0) midi.addEvent(juce::MidiMessage::allSoundOff(1),0);
                p.processBlock(buffer,midi);
            }
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
