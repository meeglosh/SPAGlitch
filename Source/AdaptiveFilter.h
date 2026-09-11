#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace glitch
{
// Empirical AR LP/HP model. See docs/FILTER-CALIBRATION.md for measured
// coverage and the remaining initialization/nonlinear residuals.
class AdaptiveFilter
{
public:
    void prepare(double rate) noexcept
    {
        sampleRate=std::max(8000.0,rate);
        smoothing=1-std::exp(-48000.0/(22.3373503*sampleRate));
        decay=std::exp(-48000.0/(2264.82240*sampleRate));
        setParameters(cutoff,resonance);
        reset();
    }
    void reset() noexcept { stages={}; envelope=0; damping=baseDamping; }
    void setParameters(int cutoffUnits,int resonancePercent) noexcept
    {
        cutoff=std::clamp(cutoffUnits,0,1000000);
        resonance=std::clamp(resonancePercent,0,100);
        const double frequency=8.17579891564*std::exp2(145.0*cutoff/12000000.0);
        const double x=3.14159265358979323846*frequency/sampleRate;
        // Kontakt's measured curve follows the fifth-order tangent expansion,
        // including a ceiling, rather than tan(pi*f/fs).
        g=std::min(5.6189228088,x*(1+x*x*(1.0/3.0+2.0/15.0*x*x)));
        r=resonance/100.0;
        gain=std::pow(10.0,-2.4*r/20.0);
        baseDamping=std::min(2.0,floor+(2.0-floor)*std::pow(10.0,0.3-r));
        feedback=(2.0-baseDamping)/(baseDamping-floor);
    }
    std::array<double,2> process(std::array<double,2> input,bool lowPass) noexcept
    {
        // Detector units are before the engine's reference output trim.
        constexpr double threshold=0.452844742/(0.49165056986916567/0.5);
        const double level=std::max(1.0,envelope/threshold);
        const double target=floor+(2.0-floor)*level/(feedback+level);
        damping+=(target-damping)*smoothing;
        double detector=0;
        std::array<double,2> output{};
        for(size_t channel=0;channel<2;++channel)
        {
            const auto first=stages[channel][0].process(input[channel],g,damping);
            const auto second=stages[channel][1].process(first.band,g,damping);
            output[channel]=gain*((lowPass ? first.low : first.high)+2*r*second.band);
            detector+=std::abs(second.band);
        }
        envelope=std::max(detector,envelope*decay);
        if(envelope<1e-30) envelope=0;
        return output;
    }
private:
    struct Outputs { double low,band,high; };
    struct Stage
    {
        double a=0,b=0;
        Outputs process(double input,double g,double k) noexcept
        {
            const double band=(a+g*(input-b))/(1+g*(g+k));
            const double low=b+g*band;
            a=2*band-a;b=2*low-b;
            if(std::abs(a)<1e-30) a=0;
            if(std::abs(b)<1e-30) b=0;
            return {low,band,input-k*band-low};
        }
    };
    std::array<std::array<Stage,2>,2> stages{};
    static constexpr double floor=0.28;
    double sampleRate=48000,g=0,r=0,gain=1,baseDamping=2,damping=2,feedback=0;
    double envelope=0,smoothing=1,decay=0;
    int cutoff=476191,resonance=49;
};
}
