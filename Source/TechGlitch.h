#pragma once
#include <juce_core/juce_core.h>
#include <array>

namespace glitch
{

// Calm mode's second piece of movement: the tech in the scene photograph --
// the glitched mirror, the column, the disco ball, the reflections on the
// floor -- tears very slightly as notes land, so playing reads on the artwork
// and not only on the candles.
//
// This has to emphasise the notes without becoming the hazard calm mode
// exists to remove, so it is built out of the one kind of movement that is
// not a flash:
//
//   * every slice is a copy of the picture displaced sideways. The pixels are
//     the same pixels, so the luminance of the region barely changes -- it is
//     a spatial shift, not a brightness change;
//   * the regions are small and scattered, nowhere near the large contiguous
//     area that flash guidance is concerned with;
//   * the pattern only changes on a note, and never more than about six times
//     a second however fast the notes come. A free-running clock here would
//     be a strobe that happened to sit on a photograph.
class TechGlitch
{
public:
    // The glitched elements, as fractions of the photograph.
    struct Region { float x, y, w, h; };

    static constexpr int numRegions = 6;
    static constexpr int maxSlices = 7;

    static const std::array<Region, numRegions>& regions()
    {
        static const std::array<Region, numRegions> table {{
            { 0.100f, 0.035f, 0.215f, 0.375f },   // the glitched mirror
            { 0.610f, 0.000f, 0.092f, 0.470f },   // the column
            { 0.082f, 0.460f, 0.185f, 0.115f },   // the counter front
            { 0.755f, 0.625f, 0.150f, 0.200f },   // reflections on the floor
            { 0.517f, 0.388f, 0.068f, 0.110f },   // the geode
            { 0.738f, 0.050f, 0.085f, 0.145f },   // the disco ball
        }};
        return table;
    }

    struct Slice
    {
        int region;
        float y, height;   // within the region, 0..1
        float offset;      // sideways displacement, fraction of the photograph
        bool cyan;         // which way the chromatic edge leans
    };

    void advance (float peak)
    {
        const bool sounding = peak > 1.0e-7f;
        ++sinceEpoch;

        // A new pattern on a note onset, but no faster than every fifth frame
        // (about six a second at 30fps) however densely the notes arrive.
        if (sounding && ! wasSounding && sinceEpoch >= 5)
        {
            ++epoch;
            sinceEpoch = 0;
        }
        wasSounding = sounding;

        const float target = sounding ? 1.0f : 0.0f;
        // Same shape as the candles: quick to arrive, slow to leave, because
        // the slow leave is what stops repeated notes reading as a strobe.
        level += (target - level) * (target > level ? 0.5f : 0.08f);
        if (level < 1.0e-4f) level = 0.0f;
    }

    float energy() const noexcept { return level; }
    bool isActive() const noexcept { return level > 1.0e-3f; }

    // Fills `out` with the slices for this frame and returns how many. Purely
    // a function of the epoch, so it costs no state and never drifts.
    int buildSlices (Slice* out) const
    {
        const int count = juce::jlimit (0, maxSlices, (int) std::lround (level * maxSlices));

        for (int i = 0; i < count; ++i)
        {
            const auto pick = [this, i] (int salt) { return unitRandom (epoch * 977 + i * 31 + salt); };

            Slice s;
            s.region = (int) (pick (1) * numRegions) % numRegions;
            s.y      = pick (2) * 0.92f;
            s.height = 0.02f + pick (3) * 0.06f;
            // Displacement grows with level but stays tiny: a couple of dozen
            // pixels at most across the whole faceplate.
            s.offset = (pick (4) - 0.5f) * 0.030f * level;
            s.cyan   = pick (5) > 0.5f;
            out[i] = s;
        }

        return count;
    }

private:
    // A small integer hash, so a frame's pattern is reproducible and no RNG
    // state has to be carried around or kept in step with the audio thread.
    static float unitRandom (int seed)
    {
        auto x = (juce::uint32) seed;
        x ^= x >> 16; x *= 0x7feb352du;
        x ^= x >> 15; x *= 0x846ca68bu;
        x ^= x >> 16;
        return (float) (x & 0xffffffu) / (float) 0x1000000u;
    }

    float level = 0.0f;
    int epoch = 0, sinceEpoch = 0;
    bool wasSounding = false;
};

} // namespace glitch
