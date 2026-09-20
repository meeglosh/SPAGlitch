#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// RANDOMIZE ALL, ported from SPASynth's Randomizer: every randomizable
// parameter carries a window and a bias, WILDNESS opens or tightens that
// window, and lock groups hold parts of the patch across a re-roll.
namespace glitch::rnd
{

// Coarser than the UI's sections, so the lock row stays three buttons.
enum class LockGroup { sound, filter, fx, count };
inline constexpr int numLockGroups = (int) LockGroup::count;
juce::String lockGroupName (LockGroup);

// A parameter's randomization window, in normalized (0..1) units.
struct Spec
{
    float minNorm = 0.0f, maxNorm = 1.0f;
    float biasCentre = 0.5f, biasStrength = 0.0f;
};

// wildness 0 = tight around the musical centre, 0.5 = the spec's own window
// with its bias, 1 = full range, uniform.
float sampleValue (const Spec&, float wildness, juce::Random&);

// Re-rolls every randomizable parameter whose group is not in lockedMask
// (bit i = LockGroup i), then applies the musicality pass. Message thread.
void randomizeAll (juce::AudioProcessorValueTreeState&, float wildness,
                   juce::uint32 lockedMask, juce::Random&);

} // namespace glitch::rnd
