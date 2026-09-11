#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cstdint>
#include <memory>

namespace glitch
{
inline constexpr std::array<const char*,9> categories {
    "Glitch Digital", "Glitch Digital Long", "Glitch Digital Short", "Glitch Heavy",
    "Glitch Heavy Long", "Glitch Heavy Short", "Glitch Rapid Modulation", "Glitch Squelchy", "Glitch Percussive" };
inline constexpr std::array<int,9> counts {46,14,95,76,24,65,32,50,77};
inline constexpr int firstNote=12;
// Measured full-velocity dry level, relative to the original PCM. Keep the
// small residual trim after effects until the internal gain staging is isolated.
inline constexpr float groupGain=0.5011872336f;
inline constexpr float referenceOutputTrim=0.49165056986916567f/groupGain;

struct Sample
{
    juce::AudioBuffer<float> audio;
    double sampleRate=48000;
};
struct Bank
{
    std::array<std::array<std::unique_ptr<Sample>,95>,9> samples;
    std::unique_ptr<Sample> audition;
    uint64_t generation=0;
    int size=0;
    const Sample* get(int group,int note) const noexcept
    {
        if(audition) return audition.get();
        if(group<0 || group>=9 || note<firstNote || note>=firstNote+counts[(size_t)group]) return nullptr;
        return samples[(size_t)group][(size_t)(note-firstNote)].get();
    }
};
struct Controls
{
    int category=0, pitch=0, lofi=4, drive=488095, cutoff=476191, resonance=49;
    int randomness=0, destroy=1, filter=1;
    float gainDb=0;
};
struct NoteSettings
{
    int category=0, pitchUnits=0, bits=500000, drive=488095, cutoff=476191, resonance=49;
    int destroy=1, filter=1;
    int outputGainUnits() const noexcept { return 400000-drive/8; }
};
class Random
{
public:
    explicit Random(uint32_t seed=0x47544348u):state(seed ? seed : 1) {}
    uint32_t next() noexcept { auto x=state; x^=x<<13; x^=x>>17; x^=x<<5; return state=x; }
    int between(int low,int high) noexcept { return low+int((uint64_t(next())*uint64_t(high-low+1))>>32); }
    uint32_t state;
};
// KSP arithmetic is preserved in integer parameter units. Physical DSP response
// is isolated below, since Kontakt's effect implementations are not available.
NoteSettings forNote(const Controls&,int note,Random&,bool audition=false) noexcept;

class Engine
{
public:
    using RuntimeState=std::array<int,26>;
    RuntimeState runtimeState() const noexcept;
    void restoreRuntimeState(const RuntimeState&) noexcept;
    void prepare(double rate) noexcept;
    void reset() noexcept;
    void setBank(const Bank* b) noexcept { reset(); bank=b; }
    void setControls(const Controls&) noexcept;
    void handle(const juce::MidiMessage&) noexcept;
    void render(juce::AudioBuffer<float>&,int start,int length) noexcept;
    void setSeed(uint32_t seed) noexcept { random.state=seed ? seed : 1; }
    uint32_t seed() const noexcept { return random.state; }
    const NoteSettings& effective() const noexcept { return settings; }
    int activeVoices() const noexcept;
    int filterCutoff(int mode) const noexcept { return filterValues[mode==2 ? 1 : 0][0]; }
    int filterResonance(int mode) const noexcept { return filterValues[mode==2 ? 1 : 0][1]; }
private:
    struct Voice
    {
        const Sample* sample=nullptr;
        double position=0, baseStep=1;
        float velocity=0, envelope=1;
        int note=0,channel=1;
        bool held=false, releasing=false;
        uint64_t age=0;
    };
    struct FilterState { double a=0,b=0; };
    std::array<Voice,32> voices;
    std::array<bool,16> sustain{};
    std::array<double,16> bend{};
    std::array<std::array<std::array<FilterState,2>,2>,2> filters;
    std::array<double,2> heldSample{},resamplePhase{},dcInput{},dcOutput{};
    const Bank* bank=nullptr;
    Controls controls;
    NoteSettings settings;
    // Separate insert parameter memories. Initial physical Kontakt values still
    // require calibration; these preserve subsequent KSP callback routing.
    std::array<std::array<int,2>,2> filterValues{{{476191,49},{476191,49}}};
    int selectedCategory=0, lastRandomTune=0, lastRandomCutoff=0, lastRandomResonance=0;
    bool pitchKnobUsed=false;
    Random random;
    double sampleRate=48000, filterG=0, filterK=1.41421356237;
    double quantisation=256, driveGain=1, compensation=1;
    juce::SmoothedValue<float> outputGain;
    uint64_t clock=0;
    void updateEffects() noexcept;
    void selectFilterValues() noexcept;
    void noteOn(int channel,int note,float velocity) noexcept;
    void noteOff(int channel,int note) noexcept;
    float processEffect(float,int channel) noexcept;
};
}
