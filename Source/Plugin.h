#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "ContentLoader.h"
class GlitchProcessor final : public juce::AudioProcessor
{
public:
    static juce::File installedLibrary();
    explicit GlitchProcessor(juce::File factory=installedLibrary());
    void loadInstalledLibrary() { content.request(factoryLibrary.getFullPathName()); }
    ~GlitchProcessor() override;
    void prepareToPlay(double,int) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
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
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;
    void loadLibrary(const juce::File& file) { content.request(file.getFullPathName()); }
    void loadSample(const juce::File& file) { content.request(file.getFullPathName(),true); }
    void allNotesOff() { panicRequested=true; }
    bool waitForContent(int timeout=10000) const { return content.waitUntilReady(timeout); }
    juce::String contentStatus() const { return content.status(); }
    bool isLoading() const { return content.busy(); }
    float loadProgress() const { return content.progress(); }
    juce::AudioProcessorValueTreeState parameters;
    juce::MidiKeyboardState keyboard;
    std::atomic<float> leftPeak{0},rightPeak{0};
    // Held until the editor reads it, so short audio hits aren't missed between UI frames.
    std::atomic<float> visualPeak{0};
    std::atomic<int> playingCategory{0},playingPitch{0},voiceCount{0};
private:
    juce::File factoryLibrary;
    // Each mailbox has one writer. Atomics make a rejected read race-free;
    // the audio thread tries once and defers a concurrent restore to next block.
    struct RuntimeMailbox
    {
        static_assert(std::atomic<int>::is_always_lock_free && std::atomic<uint64_t>::is_always_lock_free);
        std::atomic<uint64_t> revision{0};
        std::array<std::atomic<int>,26> values{};
        void write(const glitch::Engine::RuntimeState& state) noexcept
        {
            revision.fetch_add(1);
            for(size_t i=0;i<values.size();++i) values[i].store(state[i]);
            revision.fetch_add(1);
        }
        bool read(glitch::Engine::RuntimeState& state,uint64_t& version) const noexcept
        {
            version=revision.load(); if(version&1) return false;
            for(size_t i=0;i<values.size();++i) state[i]=values[i].load();
            return version==revision.load();
        }
    };
    RuntimeMailbox publishedRuntime,pendingRuntime;
    std::atomic<uint64_t> appliedRuntime{0};
    ContentLoader content;
    glitch::Engine engine;
    const glitch::Bank* currentBank=nullptr;
    std::array<std::atomic<float>*,11> values{};
    std::atomic<bool> panicRequested{false};
    std::atomic<uint32_t> restoredSeed{0},currentSeed{0x47544348u};
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    glitch::Controls controls() const noexcept;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlitchProcessor)
};
