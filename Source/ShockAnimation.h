#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <cmath>

// Message-thread animation state. Its RNG never touches instrument modulation.
class ShockAnimation
{
public:
    void advance(float peak,bool motion)
    {
        const bool sounding=peak>1e-7f;
        if(sounding && (!wasSounding || (motion && --remaining<=0))) selectNext();
        wasSounding=sounding;
        intensity=sounding ? 1.f : 0.f;
    }
    int variation() const noexcept { return selected; }
    float energy() const noexcept { return intensity; }
private:
    void selectNext()
    {
        if(cursor==5)
        {
            for(int i=4;i>0;--i) std::swap(order[(size_t)i],order[(size_t)random.nextInt(i+1)]);
            if(order[0]==selected) std::swap(order[0],order[1]);
            cursor=0;
        }
        selected=order[(size_t)cursor++];
        remaining=9+random.nextInt(7);
    }
    juce::Random random;
    std::array<int,5> order{0,1,2,3,4};
    int cursor=5,selected=-1,remaining=0;
    float intensity=0;
    bool wasSounding=false;
};
