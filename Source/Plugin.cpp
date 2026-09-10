#include "Plugin.h"
GlitchProcessor::GlitchProcessor()
 : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
 parameters(*this, nullptr, "SPAGlitch", {std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"gain",1}, "Output", -60.0f, 6.0f, -6.0f)})
{
    formats.registerBasicFormats();
    for (int i=0; i<32; ++i) synth.addVoice(new juce::SamplerVoice());
}
bool GlitchProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{ return l.getMainInputChannelSet().isDisabled() && (l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo() || l.getMainOutputChannelSet()==juce::AudioChannelSet::mono()); }
void GlitchProcessor::prepareToPlay(double rate, int) { synth.setCurrentPlaybackSampleRate(rate); keyboard.reset(); }
void GlitchProcessor::processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer& m)
{
    juce::ScopedNoDenormals guard;
    b.clear();
    keyboard.processNextMidiBuffer(m,0,b.getNumSamples(),true);
    synth.renderNextBlock(b,m,0,b.getNumSamples());
    b.applyGain(juce::Decibels::decibelsToGain(parameters.getRawParameterValue("gain")->load()));
}
juce::Result GlitchProcessor::loadSample(const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (!reader) return juce::Result::fail("Cannot read sample: " + file.getFileName());
    if (reader->lengthInSamples <= 0 || reader->sampleRate <= 0 || reader->lengthInSamples / reader->sampleRate > 60.0)
        return juce::Result::fail("Use a non-empty sample of at most 60 seconds.");
    juce::BigInteger notes; notes.setRange(0,128,true);
    juce::SynthesiserSound::Ptr sound = new juce::SamplerSound(file.getFileName(),*reader,notes,60,0.0,0.1,60.0);
    const juce::ScopedLock lock(getCallbackLock());
    synth.allNotesOff(0,false); synth.clearSounds(); synth.addSound(sound);
    samplePath=file.getFullPathName();
    return juce::Result::ok();
}
void GlitchProcessor::getStateInformation(juce::MemoryBlock& out)
{
    auto state=parameters.copyState();
    { const juce::ScopedLock lock(getCallbackLock()); state.setProperty("samplePath",samplePath,nullptr); }
    if (auto xml=state.createXml()) copyXmlToBinary(*xml,out);
}
void GlitchProcessor::setStateInformation(const void* data,int size)
{
    if (auto xml=getXmlFromBinary(data,size))
        if (xml->hasTagName("SPAGlitch")) {
            auto state=juce::ValueTree::fromXml(*xml);
            parameters.replaceState(state);
            // State restores parameters only. File loading is explicit until a background
            // asset manager is implemented; hosts may restore state on the audio thread.
            const juce::ScopedLock lock(getCallbackLock());
            samplePath=state.getProperty("samplePath").toString();
        }
}
class GlitchEditor final : public juce::AudioProcessorEditor
{
public:
    explicit GlitchEditor(GlitchProcessor& p):AudioProcessorEditor(p),processor(p),keyboard(p.keyboard,juce::MidiKeyboardComponent::horizontalKeyboard),gain(p.parameters,"gain",slider)
    {
        title.setText("SPAGlitch / playback foundation",juce::dontSendNotification);
        status.setText("Load a WAV. Middle C plays its original pitch. Mapping is provisional.",juce::dontSendNotification);
        load.setButtonText("Load sample...");
        slider.setSliderStyle(juce::Slider::LinearHorizontal); slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,80,24); slider.setTextValueSuffix(" dB");
        for(auto* c:std::initializer_list<juce::Component*>{&title,&status,&load,&slider,&keyboard}) addAndMakeVisible(c);
        load.onClick=[this] {
            chooser=std::make_unique<juce::FileChooser>("Choose a Glitch WAV",juce::File{},"*.wav");
            chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
                [safe=juce::Component::SafePointer<GlitchEditor>(this)](const juce::FileChooser& fc) {
                    if(safe && fc.getResult().existsAsFile()) {
                        auto result=safe->processor.loadSample(fc.getResult());
                        safe->status.setText(result.wasOk()?fc.getResult().getFileName():result.getErrorMessage(),juce::dontSendNotification);
                    }
                });
        };
        setSize(680,260);
    }
    void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xff202329)); }
    void resized() override { auto r=getLocalBounds().reduced(20); title.setBounds(r.removeFromTop(32)); status.setBounds(r.removeFromTop(44)); load.setBounds(r.removeFromTop(32).removeFromLeft(160)); slider.setBounds(r.removeFromTop(36)); keyboard.setBounds(r); }
private:
    GlitchProcessor& processor;
    juce::Label title,status;
    juce::TextButton load;
    juce::Slider slider;
    juce::MidiKeyboardComponent keyboard;
    juce::AudioProcessorValueTreeState::SliderAttachment gain;
    std::unique_ptr<juce::FileChooser> chooser;
};
juce::AudioProcessorEditor* GlitchProcessor::createEditor() { return new GlitchEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GlitchProcessor(); }
