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

// juce::Random is an LCG, so seeds close together hand out nearly identical
// FIRST draws -- and the first thing a roll draws is the sample bank. Seeding
// directly for a reproducible roll therefore gives every patch in a batch the
// same bank. Warm the generator first; this returns one ready to use.
juce::Random seededGenerator (int seed);

// wildness 0 = tight around the musical centre, 0.5 = the spec's own window
// with its bias, 1 = full range, uniform.
float sampleValue (const Spec&, float wildness, juce::Random&);

// A shuffled FX chain order, packed for FXChain::unpackOrder. Returns
// `current` untouched when the FX group is locked.
//
// The limiter never moves: it always lands last. That is not a stylistic
// preference -- randomizeAll()'s loudness guarantee is the limiter forced on
// at its -0.3 dBFS ceiling, and a ceiling is only a ceiling if nothing runs
// after it. A reverb or a distortion placed downstream would put the level
// straight back over the top.
juce::uint64 chainOrder (juce::uint64 current, juce::uint32 lockedMask, juce::Random&);

// Re-rolls every randomizable parameter whose group is not in lockedMask
// (bit i = LockGroup i), then applies the musicality pass. Message thread.
void randomizeAll (juce::AudioProcessorValueTreeState&, float wildness,
                   juce::uint32 lockedMask, juce::Random&);

} // namespace glitch::rnd
