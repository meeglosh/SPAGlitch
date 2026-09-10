#pragma once
#include "GlitchEngine.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>
#include <atomic>

// Files, decoding, allocations and bank destruction belong to this worker.
// The audio thread only exchanges pointers through two atomic mailboxes.
class ContentLoader final : private juce::Thread
{
public:
    ContentLoader();
    ~ContentLoader() override;
    void request(const juce::String& path,bool singleSample=false);
    const glitch::Bank* adoptForAudio() noexcept;
    struct Location { juce::String path; bool single; };
    Location location() const;
    juce::String path() const;
    juce::String status() const;
    bool singleSample() const;
    bool ready() const noexcept { return readyGeneration.load()==generation.load(); }
    bool busy() const noexcept { return loading.load(); }
    float progress() const noexcept { return fraction.load(); }
    bool waitUntilReady(int timeoutMs) const; // non-realtime/offline callers only
private:
    void run() override;
    std::unique_ptr<glitch::Bank> load(const juce::String&,bool,uint64_t,juce::String& error);
    std::unique_ptr<glitch::Sample> read(const juce::File&,juce::String&,int64_t& memory);
    mutable juce::CriticalSection lock;
    juce::String requestedPath, message="Choose the Glitch Bundle sample folder to begin.";
    bool audition=false;
    std::atomic<uint64_t> generation{0},readyGeneration{0};
    std::atomic<bool> loading{false};
    std::atomic<float> fraction{0};
    std::atomic<glitch::Bank*> pending{nullptr},retired{nullptr};
    glitch::Bank* active=nullptr; // audio-thread owned until processor stops
    juce::AudioFormatManager formats;
};
