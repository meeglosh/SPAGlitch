#pragma once
#include <array>
#include <cmath>

namespace glitch
{
// Measured at 44.1/48/96 kHz: the saved reducer setting holds 11 frames.
// The host calls beginBlock once, before splitting a block at MIDI events.
class LoFiModel
{
public:
    void reset() noexcept { countdown.fill(10);held.fill(0);previousActive=false;running=true; }
    void setEnabled(bool value) noexcept
    {
        if(enabled!=value) { countdown.fill(10);held.fill(0); }
        enabled=value;
    }
    void setLevels(double value) noexcept { levels=value; }
    void beginBlock(bool active) noexcept
    {
        running=active || previousActive;
        previousActive=active;
    }
    double process(double input,int channel) noexcept
    {
        if(!enabled) return input;
        if(!running) return 0;
        auto& remaining=countdown[(size_t)channel];
        if(remaining==0)
        {
            held[(size_t)channel]=std::trunc(input/0.5*levels)/levels*0.5;
            remaining=10;
        }
        else --remaining;
        return held[(size_t)channel];
    }
private:
    std::array<int,2> countdown{10,10};
    std::array<double,2> held{};
    double levels=128;
    bool enabled=false,previousActive=false,running=true;
};
}
