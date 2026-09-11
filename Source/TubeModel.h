#pragma once
#include <array>

namespace glitch
{
// Empirical model of the original Tube insert. No allocation or locking on the
// audio thread. 48 kHz is measured; rate conversion remains to be validated.
class TubeModel
{
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void setParameters(int driveUnits,int outputUnits) noexcept;
    double process(double input,int channel) noexcept;
    static double outputGain(int units) noexcept;
private:
    struct State { std::array<double,3> input{},output{}; };
    std::array<State,2> states{};
    std::array<double,4> denominator{1,0,0,0};
    std::array<double,13> negative{},positive{};
    double negativeDry=1,positiveDry=1,gain=1,rateGain=1;
};
}
