#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
class GlitchProcessor final : public juce::AudioProcessor
{
public:
    GlitchProcessor();
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "SPAGlitch"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::Result loadSample(const juce::File&); // control thread only
    juce::AudioProcessorValueTreeState parameters;
    juce::MidiKeyboardState keyboard;
private:
    juce::Synthesiser synth;
    juce::AudioFormatManager formats;
    juce::String samplePath;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlitchProcessor)
};
