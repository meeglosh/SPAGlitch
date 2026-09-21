#pragma once
#include <juce_dsp/juce_dsp.h>
#include "FXParameters.h"
#include "FXTheme.h"

namespace glitch::fx::ui
{

// Interactive EQ: a log-frequency response graph with a live spectrum analyser
// behind it, 8 draggable band nodes (X = freq, Y = gain, wheel = Q),
// double-click to add or remove a band, and a right-click type/slope menu.
// Nodes write straight to the APVTS band parameters, and the curve is drawn by
// the same magnitude function the DSP uses, so it can never lie.
//
// Ported from SPASynth; the spectrum is fed by the processor's scope ring
// rather than SPASynth's telemetry block.
class EqEditor final : public juce::Component,
                       public juce::SettableTooltipClient,
                       private juce::Timer
{
public:
    // scopeReader fills the supplied buffer with the most recent `scopeSize`
    // output samples, oldest first, and returns false when no audio is
    // available (the analyser then fades out rather than freezing).
    static constexpr int scopeSize = 2048;
    using ScopeReader = std::function<bool (float* dest, int numSamples)>;

    EqEditor (juce::AudioProcessorValueTreeState&, ScopeReader, std::function<double()> sampleRateFn);
    ~EqEditor() override;

    void resized() override;
    void paint (juce::Graphics&) override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static juce::String badgeText (int type, int slope);

private:
    static constexpr int numBands = ParametricEQ::numBands;
    static constexpr float minF = 20.0f, maxF = 20000.0f, dbRange = 24.0f, nodeRadius = 6.0f;

    void timerCallback() override;

    float value (const juce::String& id) const;
    float rawBand (int b, const char* key) const;
    bool bandEnabled (int b) const;
    void setBand (int b, const char* key, float realValue);
    std::array<ParametricEQ::Band, numBands> readBands() const;

    float freqToX (float f) const;
    float xToFreq (float x) const;
    float dbToY (float db) const;
    float yToDb (float y) const;
    static bool isGainType (int type);

    juce::Point<float> nodeCentre (int b) const;
    int bandAt (juce::Point<float>) const;
    void applyDrag (juce::Point<float>);
    int createBandAt (juce::Point<float>, int type);
    void showTypeMenu (int b);
    void setTypeAndSlope (int b, int type, int slope);

    void drawGrid (juce::Graphics&) const;
    void drawSpectrum (juce::Graphics&) const;
    void computeSpectrum();
    void refreshSampleRate();

    juce::AudioProcessorValueTreeState& apvts;
    ScopeReader readScope;
    std::function<double()> getSampleRate;
    double sampleRate = 48000.0;

    juce::Rectangle<float> graph;
    int dragBand = -1, hoverBand = -1, selectedBand = -1;
    bool qDragActive = false;      // Cmd/Ctrl-drag Q gesture in progress
    float qRefY = 0.0f, qRefQ = 1.0f;
    juce::String modName { "Ctrl" };

    juce::dsp::FFT fft { 11 };   // 2^11 = 2048 = scopeSize
    std::array<float, scopeSize> scopeBuffer {};
    std::array<float, scopeSize * 2> fftData {};
    std::array<float, scopeSize / 2> spectrum {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqEditor)
};

} // namespace glitch::fx::ui
