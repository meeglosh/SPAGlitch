#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>
#include <stdexcept>

// Local measurement runner. Requires the user's licensed Kontakt and a state
// saved by ReferenceHost. The original NKI is never edited or distributed.
int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        if(argc!=4) throw std::runtime_error("Usage: KontaktProbe ABS_STATE ABS_PROTOCOL_JSON ABS_NEW_WAV");
        const juce::File stateFile(argv[1]),protocolFile(argv[2]),outputFile(argv[3]);
        auto protocol=juce::JSON::parse(protocolFile.loadFileAsString());
        const int rate=(int)protocol["rate"],block=(int)protocol["block"],frames=(int)protocol["frames"];
        auto events=protocol["events"];
        if(rate<8000 || rate>192000 || block<1 || block>4096 || frames<1 || frames>rate*600 || !events.isArray())
            throw std::runtime_error("Invalid measurement protocol");
        if(outputFile.exists()) throw std::runtime_error("Output must be new");
        juce::MemoryBlock state;
        if(!stateFile.loadFileAsData(state) || state.isEmpty()) throw std::runtime_error("Missing reference state");
        juce::MidiBuffer timeline;
        for(auto& event:*events.getArray())
        {
            const int frame=(int)event["frame"],key=(int)event["key"],value=(int)event["value"];
            const auto type=event["type"].toString();
            if(frame<0 || frame>=frames || key<0 || key>127 || value<0 || value>127)
                throw std::runtime_error("Invalid MIDI event");
            if(type=="on") timeline.addEvent(juce::MidiMessage::noteOn(1,key,(juce::uint8)value),frame);
            else if(type=="off") timeline.addEvent(juce::MidiMessage::noteOff(1,key),frame);
            else if(type=="cc") timeline.addEvent(juce::MidiMessage::controllerEvent(1,key,value),frame);
            else throw std::runtime_error("Unknown MIDI event type");
        }
        juce::VST3PluginFormat format;juce::OwnedArray<juce::PluginDescription> types;
        format.findAllTypesForFile(types,"/Library/Audio/Plug-Ins/VST3/Kontakt.vst3");
        if(types.isEmpty()) throw std::runtime_error("Licensed Kontakt 6 VST3 is required");
        juce::String error;auto plugin=format.createInstanceFromDescription(*types[0],rate,block,error);
        if(!plugin) throw std::runtime_error(error.toStdString());
        plugin->enableAllBuses();plugin->setNonRealtime(true);
        plugin->setRateAndBufferSizeDetails(rate,block);plugin->prepareToPlay(rate,block);
        plugin->setStateInformation(state.getData(),(int)state.getSize());
        // Loading samples is asynchronous. Pump native messages before rendering;
        // every new state must also pass an independent dry-reference comparison.
        for(int i=0;i<50;++i) juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
        const int channels=std::max(2,plugin->getTotalNumOutputChannels());
        juce::AudioBuffer<float> audio(channels,block),output(2,frames);output.clear();
        for(int offset=0;offset<frames;offset+=block)
        {
            const int n=std::min(block,frames-offset);
            audio.setSize(channels,n,false,false,true);audio.clear();
            juce::MidiBuffer midi;midi.addEvents(timeline,offset,n,-offset);
            plugin->processBlock(audio,midi);
            for(int ch=0;ch<2;++ch) output.copyFrom(ch,offset,audio,ch,0,n);
        }
        juce::WavAudioFormat wav;std::unique_ptr<juce::OutputStream> stream=outputFile.createOutputStream();
        if(!stream) throw std::runtime_error("Cannot create output");
        const bool floating=(bool)protocol["float"];
        auto options=juce::AudioFormatWriterOptions{}.withSampleRate(rate).withNumChannels(2)
            .withBitsPerSample(floating ? 32 : 24)
            .withSampleFormat(floating ? juce::AudioFormatWriterOptions::SampleFormat::floatingPoint
                                      : juce::AudioFormatWriterOptions::SampleFormat::integral);
        auto writer=wav.createWriterFor(stream,options);
        if(!writer || !writer->writeFromAudioSampleBuffer(output,0,frames)) throw std::runtime_error("Capture write failed");
        writer.reset();plugin->releaseResources();
        std::cout<<"Captured "<<frames<<" frames at "<<rate<<" Hz; peak "<<output.getMagnitude(0,frames)<<'\n';
        return 0;
    }
    catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
