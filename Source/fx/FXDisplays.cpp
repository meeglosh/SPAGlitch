#include "FXDisplays.h"
#include "FXChain.h"

namespace glitch::fx::ui
{
using namespace glitch::theme;

// ----------------------------------------------------- DisplayComponent ----

DisplayComponent::DisplayComponent (juce::AudioProcessorValueTreeState& state,
                                    juce::StringArray paramIDs)
    : apvts (state), watched (std::move (paramIDs))
{
    setInterceptsMouseClicks (false, false);
    for (const auto& id : watched)
        apvts.addParameterListener (id, this);
    startTimerHz (24);
}

DisplayComponent::~DisplayComponent()
{
    stopTimer();
    for (const auto& id : watched)
        apvts.removeParameterListener (id, this);
}

void DisplayComponent::timerCallback()
{
    // Only the selected tab's content is on screen; the other seven stay alive
    // and would otherwise repaint every time their parameters moved.
    if (dirty.exchange (false) && isShowing())
        repaint();
}

float DisplayComponent::value (const juce::String& paramID) const
{
    if (auto* p = apvts.getParameter (paramID))
        return p->convertFrom0to1 (p->getValue());
    return 0.0f;
}

void DisplayComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (well.withAlpha (0.85f));
    g.fillRoundedRectangle (bounds, 7.0f);
    g.setColour (sage.withAlpha (0.22f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 7.0f, 1.0f);

    paintDisplay (g, bounds.reduced (9.0f, 8.0f));
}

// ------------------------------------------------------------- FXScope ----

juce::StringArray FXScope::watchedFor (Kind kind, params::Section section)
{
    namespace i = params::id;

    // MOD and TREM/VIB render as chorus scopes but must follow their own
    // parameters, so the watch list keys off the section, not the kind.
    if (section == params::Section::mod)
        return { i::modEnable, i::modDepth, i::modMix, i::modRate, i::modFeedback };
    if (section == params::Section::tremVib)
        return { i::tremEnable, i::tremDepth, i::tremMix, i::tremRate,
                 i::vibEnable, i::vibDepth, i::vibMix };

    switch (kind)
    {
        case Kind::distortion: return { i::distEnable, i::distType, i::distDrive, i::distMix };
        case Kind::chorus:     return { i::chorusEnable, i::chorusRate, i::chorusDepth, i::chorusMix };
        case Kind::delay:      return { i::delayEnable, i::delayFeedback, i::delayPingPong, i::delayMix };
        case Kind::reverb:     return { i::reverbEnable, i::reverbSize, i::reverbDamping, i::reverbMix };
        case Kind::eq:
        {
            juce::StringArray ids { i::eqEnable, i::eqCharacter };
            for (int b = 0; b < ParametricEQ::numBands; ++b)
            {
                ids.add (params::id::eqBand (b, i::eqband::enable));
                ids.add (params::id::eqBand (b, i::eqband::type));
                ids.add (params::id::eqBand (b, i::eqband::freq));
                ids.add (params::id::eqBand (b, i::eqband::gain));
                ids.add (params::id::eqBand (b, i::eqband::q));
            }
            return ids;
        }
    }
    return {};
}

FXScope::FXScope (juce::AudioProcessorValueTreeState& state, Kind k, params::Section s)
    : DisplayComponent (state, watchedFor (k, s)), kind (k), section (s)
{
}

void FXScope::paintDisplay (juce::Graphics& g, juce::Rectangle<float> area)
{
    namespace i = params::id;

    const auto* enableID = params::enableID (section);
    const auto lit = enableID != nullptr && value (enableID) >= 0.5f;
    const auto colour = lit ? electric : muted.withAlpha (0.45f);

    switch (kind)
    {
        case Kind::distortion:
        {
            // Transfer curve, input -1..1 -> output, over the dry diagonal.
            const auto drive = 1.0f + 15.0f * value (i::distDrive);
            const auto type = (int) value (i::distType);
            const auto mix = value (i::distMix);

            g.setColour (outline);
            g.drawLine (area.getX(), area.getBottom(), area.getRight(), area.getY(), 1.0f);

            juce::Path curve;

            if (type == 3)
            {
                // Bit-crush staircase: fewer, wider steps as DRIVE (bit-depth
                // reduction) increases, so the display reads as a quantiser
                // rather than a smooth shaper.
                const auto crushLevels = std::pow (2.0f, FXChain::crushBitsForDrive (value (i::distDrive)));
                const auto numSteps = (float) juce::jlimit (3, 64, (int) crushLevels);

                for (int s = 0; s <= (int) numSteps; ++s)
                {
                    const auto in = -1.0f + 2.0f * (float) s / numSteps;
                    const auto quantised = std::round (in * crushLevels) / crushLevels;
                    const auto out = in + (quantised - in) * mix;

                    const auto px = area.getX() + area.getWidth() * (float) s / numSteps;
                    const auto py = area.getCentreY() - out * area.getHeight() * 0.46f;
                    if (s == 0) curve.startNewSubPath (px, py);
                    else        curve.lineTo (px, py);

                    if ((float) s < numSteps)
                        curve.lineTo (area.getX() + area.getWidth() * (float) (s + 1) / numSteps, py);
                }
            }
            else
            {
                constexpr int steps = 96;
                for (int s = 0; s <= steps; ++s)
                {
                    const auto in = -1.0f + 2.0f * (float) s / steps;
                    const auto x = in * drive;
                    float wet;
                    switch (type)
                    {
                        case 1:  wet = juce::jlimit (-1.0f, 1.0f, x); break;   // Hard
                        case 2:  wet = std::sin (x * 1.2f); break;             // Fold
                        default: wet = std::tanh (x); break;                   // Soft
                    }
                    wet /= std::sqrt (drive);
                    const auto out = in + (wet - in) * mix;

                    const auto px = area.getX() + area.getWidth() * (float) s / steps;
                    const auto py = area.getCentreY() - out * area.getHeight() * 0.46f;
                    if (s == 0) curve.startNewSubPath (px, py);
                    else        curve.lineTo (px, py);
                }
            }
            glowStroke (g, curve, colour, 1.6f);
            break;
        }

        case Kind::chorus:
        {
            // Two detuned voices weaving around the dry centre line. MOD and
            // TREM/VIB feed their own depth/mix in here.
            const auto depth = section == params::Section::mod     ? value (i::modDepth)
                             : section == params::Section::tremVib ? juce::jmax (value (i::tremDepth),
                                                                                value (i::vibDepth))
                                                                   : value (i::chorusDepth);
            const auto mix = section == params::Section::mod     ? value (i::modMix)
                           : section == params::Section::tremVib ? juce::jmax (value (i::tremMix),
                                                                              value (i::vibMix))
                                                                 : value (i::chorusMix);

            for (int voice = 0; voice < 2; ++voice)
            {
                juce::Path curve;
                constexpr int steps = 120;
                for (int s = 0; s <= steps; ++s)
                {
                    const auto ph = (float) s / steps * juce::MathConstants<float>::twoPi * 2.0f
                                  + (voice == 0 ? 0.0f : juce::MathConstants<float>::pi * 0.6f);
                    const auto v = std::sin (ph) * depth * (0.25f + 0.75f * mix);
                    const auto x = area.getX() + area.getWidth() * (float) s / steps;
                    const auto y = area.getCentreY() - v * area.getHeight() * 0.42f;
                    if (s == 0) curve.startNewSubPath (x, y);
                    else        curve.lineTo (x, y);
                }
                glowStroke (g, curve, voice == 0 ? colour : colour.withAlpha (0.55f), 1.4f);
            }
            break;
        }

        case Kind::delay:
        {
            // Echo taps decaying by feedback; ping-pong alternates sides.
            const auto feedback = value (i::delayFeedback);
            const auto pingpong = value (i::delayPingPong) >= 0.5f;
            const auto mix = value (i::delayMix);

            const auto baseX = area.getX() + 6.0f;
            const auto spacing = (area.getWidth() - 12.0f) / 6.0f;

            g.setColour (ink.withAlpha (0.8f));
            g.fillRect (juce::Rectangle<float> (baseX - 1.5f,
                                                area.getCentreY() - area.getHeight() * 0.45f,
                                                3.0f, area.getHeight() * 0.9f));

            float gain = mix;
            for (int tap = 1; tap <= 6 && gain > 0.02f; ++tap)
            {
                const auto h = area.getHeight() * 0.45f * gain;
                const auto x = baseX + spacing * (float) tap;
                const auto up = ! pingpong || (tap % 2 == 1);
                g.setColour (colour);
                g.fillRect (juce::Rectangle<float> (x - 1.5f,
                                                    up ? area.getCentreY() - h : area.getCentreY(),
                                                    3.0f, h));
                gain *= feedback;
            }

            g.setColour (outline.withAlpha (0.7f));
            g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());
            break;
        }

        case Kind::reverb:
        {
            // Decay envelope; size stretches it, damping bows it down.
            const auto size = value (i::reverbSize);
            const auto damping = value (i::reverbDamping);
            const auto mix = value (i::reverbMix);

            juce::Path curve;
            constexpr int steps = 100;
            for (int s = 0; s <= steps; ++s)
            {
                const auto x01 = (float) s / steps;
                const auto decay = 1.2f + (1.0f - size) * 6.0f + damping * 2.0f;
                const auto v = (0.2f + 0.8f * mix) * std::exp (-decay * x01);
                const auto x = area.getX() + area.getWidth() * x01;
                const auto y = area.getBottom() - v * area.getHeight() * 0.92f;
                if (s == 0) curve.startNewSubPath (x, y);
                else        curve.lineTo (x, y);
            }

            auto fill = curve;
            fill.lineTo (area.getRight(), area.getBottom());
            fill.lineTo (area.getX(), area.getBottom());
            fill.closeSubPath();
            g.setColour (colour.withAlpha (0.18f));
            g.fillPath (fill);
            glowStroke (g, curve, colour, 1.6f);
            break;
        }

        case Kind::eq:
        {
            // Composite response of the 8 bands, +/-24 dB, computed by the
            // same function the DSP uses.
            std::array<ParametricEQ::Band, ParametricEQ::numBands> bands;
            for (int b = 0; b < ParametricEQ::numBands; ++b)
            {
                auto& bd = bands[(size_t) b];
                bd.enabled = value (params::id::eqBand (b, i::eqband::enable)) >= 0.5f;
                bd.type    = (int) value (params::id::eqBand (b, i::eqband::type));
                bd.slope   = (int) value (params::id::eqBand (b, i::eqband::slope));
                bd.freq    = value (params::id::eqBand (b, i::eqband::freq));
                bd.gainDb  = value (params::id::eqBand (b, i::eqband::gain));
                bd.q       = value (params::id::eqBand (b, i::eqband::q));
            }

            g.setColour (outline.withAlpha (0.7f));
            g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());

            juce::Path curve;
            constexpr int steps = 160;
            for (int s = 0; s <= steps; ++s)
            {
                const auto x01 = (float) s / steps;
                const auto hz = 20.0f * std::pow (1000.0f, x01);
                const auto db = ParametricEQ::magnitudeDb (bands, hz, 48000.0);
                const auto x = area.getX() + area.getWidth() * x01;
                const auto y = area.getCentreY() - juce::jlimit (-24.0f, 24.0f, db)
                                                 / 24.0f * area.getHeight() * 0.46f;
                if (s == 0) curve.startNewSubPath (x, y);
                else        curve.lineTo (x, y);
            }
            glowStroke (g, curve, colour, 1.6f);
            break;
        }
    }
}

// ------------------------------------------------------ LimiterDisplay ----

LimiterDisplay::LimiterDisplay (juce::AudioProcessorValueTreeState& state,
                                std::function<float()> gainReductionDb,
                                std::function<float()> outputPeak)
    : apvts (state), readGainReduction (std::move (gainReductionDb)),
      readOutputPeak (std::move (outputPeak))
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (30);
}

void LimiterDisplay::timerCallback()
{
    // Limiter::gainReductionDb() reports the gain applied, so it is <= 0 dB;
    // the meter wants reduction as a positive magnitude.
    const auto newGr = readGainReduction ? juce::jmax (0.0f, -readGainReduction()) : 0.0f;
    const auto newOut = readOutputPeak ? readOutputPeak() : 0.0f;

    // Fast attack on the bar, slow release, with a peak-hold tick that decays
    // more slowly still -- the usual GR-meter ballistics.
    gr = juce::jmax (newGr, gr * 0.82f);
    grPeak = juce::jmax (gr, grPeak - 0.35f);
    out = juce::jmax (newOut, out * 0.85f);

    if (isShowing())
        repaint();
}

void LimiterDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (well.withAlpha (0.85f));
    g.fillRoundedRectangle (bounds, 7.0f);
    g.setColour (sage.withAlpha (0.22f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 7.0f, 1.0f);

    auto area = bounds.reduced (9.0f, 8.0f);
    auto meterArea = area.removeFromBottom (26.0f);
    area.removeFromBottom (6.0f);

    auto paramValue = [this] (const juce::String& id)
    {
        auto* p = apvts.getParameter (id);
        return p != nullptr ? p->convertFrom0to1 (p->getValue()) : 0.0f;
    };

    const auto on = paramValue (params::id::limEnable) >= 0.5f;
    const auto colour = on ? electric : muted.withAlpha (0.45f);
    const auto ceilingDb = paramValue (params::id::limCeiling);
    const auto driveDb = paramValue (params::id::limDrive);

    // Transfer curve: input dBFS (-48..0) against limited output, with the
    // ceiling drawn as the plateau the curve flattens onto.
    constexpr float floorDb = -48.0f;
    auto dbToX = [&] (float db) { return area.getX() + area.getWidth() * (db - floorDb) / -floorDb; };
    auto dbToY = [&] (float db) { return area.getBottom() - area.getHeight() * (db - floorDb) / -floorDb; };

    g.setColour (outline);
    g.drawLine (dbToX (floorDb), dbToY (floorDb), dbToX (0.0f), dbToY (0.0f), 1.0f);

    g.setColour (sage.withAlpha (0.35f));
    const auto ceilingY = dbToY (ceilingDb);
    g.drawHorizontalLine ((int) ceilingY, area.getX(), area.getRight());

    juce::Path curve;
    constexpr int steps = 96;
    for (int s = 0; s <= steps; ++s)
    {
        const auto inDb = floorDb + (float) s / steps * -floorDb;
        const auto outDb = juce::jmin (ceilingDb, inDb + driveDb);
        if (s == 0) curve.startNewSubPath (dbToX (inDb), dbToY (outDb));
        else        curve.lineTo (dbToX (inDb), dbToY (outDb));
    }
    glowStroke (g, curve, colour, 1.6f);

    // Gain-reduction bar, 0..-12 dB left to right.
    constexpr float grRange = 12.0f;
    g.setColour (paper.withAlpha (0.85f));
    g.fillRoundedRectangle (meterArea, 4.0f);

    const auto grFrac = juce::jlimit (0.0f, 1.0f, gr / grRange);
    auto bar = meterArea.reduced (3.0f);
    g.setColour (colour.withAlpha (0.85f));
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * grFrac), 3.0f);

    const auto peakFrac = juce::jlimit (0.0f, 1.0f, grPeak / grRange);
    if (peakFrac > 0.005f)
    {
        g.setColour (ink.withAlpha (0.8f));
        g.fillRect (bar.getX() + bar.getWidth() * peakFrac - 1.0f, bar.getY(), 2.0f, bar.getHeight());
    }

    g.setColour (muted);
    g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
    g.drawText ("GR " + juce::String (gr, 1) + " dB", meterArea.reduced (6.0f, 0.0f),
                juce::Justification::centredLeft);
    g.drawText ("OUT " + juce::String (juce::Decibels::gainToDecibels (out, -60.0f), 1),
                meterArea.reduced (6.0f, 0.0f), juce::Justification::centredRight);
}


} // namespace glitch::fx::ui
