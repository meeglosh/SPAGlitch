#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "ContentLoader.h"
#include "fx/FXChain.h"
#include "MidiLearn.h"
#include "Randomizer.h"
#include "PresetManager.h"
#include "fx/FXParamSnapshot.h"
class GlitchProcessor final : public juce::AudioProcessor,
                             private juce::Timer
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
    // The engine's own 0.1 s release plus whatever the FX chain is ringing
    // out, so hosts don't truncate a reverb or delay tail on bounce.
    double getTailLengthSeconds() const override
    { return 0.1 + fxTailSeconds.load(std::memory_order_relaxed); }
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

    // --- FX chain -------------------------------------------------------
    // The chain order is a single packed uint64 so the UI can publish a
    // reorder with one atomic store and the audio thread never sees a
    // half-written permutation (see FXChain::packOrder).
    void setFxOrder (const juce::Array<int>& moduleIds);
    juce::Array<int> getFxOrder() const;

    float limiterGainReductionDb() const { return fxChain.limiterGainReductionDb(); }
    float limiterOutputPeak() const { return fxChain.limiterOutputPeak(); }

    // Fills `dest` with the most recent `numSamples` output samples, oldest
    // first, for the EQ tab's spectrum analyser. False when nothing has been
    // rendered yet. Message thread; the ring is written on the audio thread.
    static constexpr int scopeSize = 2048;
    bool readScope (float* dest, int numSamples) const;

    // Whether each editor drawer is folded away. Editor-only state, so these
    // are plain flags rather than parameters, but they travel with the saved
    // state so a reopened project looks the way it was left.
    std::atomic<bool> fxCollapsed { false }, keyboardCollapsed { false };

    // Right-click any knob to bind it to a hardware CC. Constructed after the
    // APVTS so it can index the finished parameter list.
    glitch::MidiLearnManager midiLearn { parameters };

    // RANDOMIZE ALL. Wildness and the lock mask live in the state tree rather
    // than as parameters: they steer the roll, they are not part of the sound.
    void randomizeAll();
    // Deterministic overload for the factory-preset generator and the seeded
    // sweep test; reseeding the shared system Random is not allowed.
    void randomizeAll (juce::Random&);

    // A patch is the parameter tree plus the FX chain order. Deliberately not
    // the sample-content path, the MIDI map, the drawer states or the
    // randomizer settings -- those belong to the instance, not the sound.
    juce::ValueTree capturePreset();
    void applyPreset (const juce::ValueTree&);
    glitch::PresetManager presets { [this] { return capturePreset(); },
                                    [this] (const juce::ValueTree& t) { applyPreset (t); } };
    float randomWildness() const;
    void setRandomWildness (float);
    bool isGroupLocked (int group) const;
    void setGroupLocked (int group, bool);
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
    glitch::fx::FXChain fxChain;
    glitch::fx::params::Snapshot fxSnapshot;
    std::atomic<juce::uint64> fxOrderPacked { glitch::fx::FXChain::defaultOrderPacked() };
    int reportedLatency = 0;

    // Post-FX output ring feeding the EQ analyser. Single audio-thread writer,
    // message-thread reader; a torn read only costs one analyser frame.
    std::array<std::atomic<float>, scopeSize> scope {};
    std::atomic<int> scopeWrite { 0 };
    std::atomic<bool> scopeFilled { false };
    std::atomic<double> fxTailSeconds { 0.0 };

    // Lookahead-limiter latency: computed on the audio thread, applied via
    // setLatencySamples on the timer. setLatencySamples notifies the host and
    // its listeners, which is not safe to do from a render callback.
    std::atomic<int> desiredLatency { 0 };
    void timerCallback() override;

    void publishTailAndLatency(const glitch::fx::FXChain::Params&);
    void pushScope(const juce::AudioBuffer<float>&);
    const glitch::Bank* currentBank=nullptr;
    std::array<std::atomic<float>*,8> values{};   // see controls()
    std::atomic<bool> panicRequested{false};
    std::atomic<uint32_t> restoredSeed{0},currentSeed{0x47544348u};
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    glitch::Controls controls() const noexcept;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlitchProcessor)
};
