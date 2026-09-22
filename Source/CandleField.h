#pragma once
#include <juce_graphics/juce_graphics.h>
#include <array>
#include <cmath>

namespace glitch
{

// Calm mode's only animation: the candles in the scene photograph brighten and
// waver while notes are sounding, and sit perfectly still otherwise.
//
// This exists because the normal background is a hard full-frame cut between a
// calm photograph and one of five "electric" frames, on every note. At any
// speed that is a flashing image, and flashing images cause seizures in people
// with photosensitive epilepsy -- who get no warning, because the seizure is
// how they find out. Calm mode replaces the cut with light that moves, which
// is the part of the effect that was never the hazard.
//
// The positions were found by looking for hot, orange cores sitting inside a
// pool of their own warm light, which is what separates a flame from the
// backlit mirror rim and the shelf LEDs in the same picture, and then curated
// by eye. Reflections are deliberately included: a reflection of a candle
// flickers with its candle.
class CandleField
{
public:
    struct Candle
    {
        float x, y;        // position in the photograph, 0..1
        float strength;    // relative brightness of this flame
    };

    static constexpr int numCandles = 35;

    static const std::array<Candle, numCandles>& candles()
    {
        static const std::array<Candle, numCandles> table {{
            { 0.0227f, 0.7333f, 1.00f }, { 0.1406f, 0.4623f, 1.00f },
            { 0.9665f, 0.9267f, 0.99f }, { 0.0000f, 0.7269f, 0.99f },
            { 0.0544f, 0.8300f, 0.96f }, { 0.1615f, 0.6621f, 0.95f },
            { 0.3714f, 0.4453f, 0.94f }, { 0.9516f, 0.9702f, 0.94f },
            { 0.9324f, 0.9639f, 0.91f }, { 0.1477f, 0.4230f, 0.85f },
            { 0.4976f, 0.4325f, 0.83f }, { 0.1651f, 0.4315f, 0.83f },
            { 0.0730f, 0.7715f, 0.80f }, { 0.7105f, 0.5239f, 0.80f },
            { 0.4809f, 0.4506f, 0.75f }, { 0.9994f, 0.6121f, 0.74f },
            { 0.2650f, 0.4091f, 0.74f }, { 0.2811f, 0.3858f, 0.74f },
            { 0.9659f, 0.6706f, 0.71f }, { 0.7099f, 0.6791f, 0.68f },
            { 0.8792f, 0.4761f, 0.65f }, { 0.9318f, 0.7131f, 0.64f },
            { 0.6878f, 0.5037f, 0.64f }, { 0.4217f, 0.5505f, 0.62f },
            { 0.1286f, 0.8098f, 0.62f }, { 0.9067f, 0.4846f, 0.61f },
            { 0.3475f, 0.4431f, 0.60f }, { 0.8493f, 0.6908f, 0.54f },
            { 0.1244f, 0.6780f, 0.51f }, { 0.0933f, 0.4474f, 0.45f },
            { 0.6663f, 0.5271f, 0.44f }, { 0.5114f, 0.5058f, 0.43f },
            { 0.1160f, 0.7726f, 0.37f }, { 0.1280f, 0.7269f, 0.34f },
            { 0.8720f, 0.7641f, 0.29f },
        }};
        return table;
    }

    // Called once per animation frame with the block's peak level.
    void advance (float peak)
    {
        const bool sounding = peak > 1.0e-7f;
        // Rises quickly and falls slowly. The fall is what keeps this from
        // being a flash in its own right: when a note stops, the candles
        // settle over about half a second rather than snapping back.
        target = sounding ? 1.0f : 0.0f;
        level += (target - level) * (target > level ? 0.45f : 0.06f);
        if (level < 1.0e-4f) level = 0.0f;
        ++frame;
    }

    // 0 when nothing is playing, so the photograph is genuinely static at rest.
    float energy() const noexcept { return level; }
    bool isMoving() const noexcept { return level > 1.0e-3f; }

    // Per-candle brightness, 0..1. Two detuned waves per flame with a phase
    // taken from its position, so no two candles waver together and the whole
    // field never pulses as one -- a field flashing in unison would be the
    // very thing this mode exists to avoid.
    float brightness (int index) const
    {
        const auto& candle = candles()[(size_t) index];
        const auto phase = (float) index * 2.399963f;          // golden angle
        const auto t = (float) frame;

        const auto slow = std::sin (phase + t * 0.113f);
        const auto fast = std::sin (phase * 1.7f + t * 0.291f);
        const auto waver = 0.5f + 0.5f * (0.62f * slow + 0.38f * fast);

        return level * candle.strength * (0.35f + 0.65f * waver);
    }

private:
    float level = 0.0f, target = 0.0f;
    int frame = 0;
};

} // namespace glitch
