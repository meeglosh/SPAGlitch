#pragma once
#include <juce_graphics/juce_graphics.h>
#include <array>

// UI-only geometry: repainting never changes the random sequence or the audio.
class BlastAnimation
{
public:
    void advance(float energy,bool enabled)
    {
        if(!enabled || energy<.002f) { bolts={};sparks={};jolt={};return; }
        jolt*=.55f;
        if(random.nextFloat()<.32f)
            jolt={(random.nextFloat()-.5f)*4.2f*energy,(random.nextFloat()-.5f)*1.8f*energy};
        for(auto& bolt:bolts)
        {
            if(--bolt.life<=0)
            {
                bolt.path.clear();
                const bool left=random.nextBool();
                const float y=.12f+random.nextFloat()*.72f;
                juce::Point<float> start(left?.32f:.68f,y);
                juce::Point<float> end(left?-.08f:1.08f,juce::jlimit(-.15f,1.15f,y+(random.nextFloat()-.5f)*.9f));
                bolt.path.startNewSubPath(start);
                juce::Point<float> previous=start;
                for(int i=1;i<=24;++i)
                {
                    const float t=i/24.f;
                    auto point=start+(end-start)*t;
                    point.y+=(random.nextFloat()-.5f)*.045f;
                    bolt.path.lineTo(point);
                    if(i==10 || i==18)
                    {
                        bolt.path.startNewSubPath(previous);
                        bolt.path.lineTo(point.x,point.y-.06f);
                        bolt.path.lineTo(point.x+(left?-.08f:.08f),point.y-.12f);
                        bolt.path.startNewSubPath(point);
                    }
                    previous=point;
                }
                bolt.life=2+random.nextInt(5);
                bolt.strength=.45f+random.nextFloat()*.55f;
            }
            else bolt.strength*=.72f;
        }
        for(auto& spark:sparks)
        {
            if(--spark.life<=0)
            {
                const float side=random.nextBool()?-1.f:1.f;
                spark.position={.5f+side*(.18f+random.nextFloat()*.12f),.1f+random.nextFloat()*.8f};
                spark.velocity={side*(.009f+random.nextFloat()*.023f),(random.nextFloat()-.5f)*.023f};
                spark.life=6+random.nextInt(12);
            }
            spark.position+=spark.velocity;
        }
    }
    juce::Point<float> displacement() const noexcept { return jolt; }
    void draw(juce::Graphics& g,juce::Rectangle<float> bounds,float energy) const
    {
        const auto transform=juce::AffineTransform::scale(bounds.getWidth(),bounds.getHeight()).translated(bounds.getX(),bounds.getY());
        for(const auto& bolt:bolts)
        {
            if(bolt.life<=0) continue;
            auto path=bolt.path;path.applyTransform(transform);
            const float alpha=energy*bolt.strength;
            g.setColour(juce::Colour(0xff00bfff).withAlpha(alpha*.12f));
            g.strokePath(path,juce::PathStrokeType(10.f));
            g.setColour(juce::Colour(0xff39ddff).withAlpha(alpha*.4f));
            g.strokePath(path,juce::PathStrokeType(3.5f));
            g.setColour(juce::Colour(0xffe7ffff).withAlpha(alpha*.9f));
            g.strokePath(path,juce::PathStrokeType(.9f));
        }
        for(const auto& spark:sparks)
        {
            if(spark.life<=0) continue;
            auto end=spark.position.transformedBy(transform);
            auto start=(spark.position-spark.velocity*.8f).transformedBy(transform);
            g.setColour(juce::Colour(0xffffe8a0).withAlpha(energy*std::min(1.f,spark.life/5.f)*.8f));
            g.drawLine({start,end},1.2f);
        }
    }
private:
    struct Bolt { juce::Path path;int life=0;float strength=0; };
    struct Spark { juce::Point<float> position,velocity;int life=0; };
    std::array<Bolt,5> bolts;
    std::array<Spark,32> sparks;
    juce::Random random;
    juce::Point<float> jolt;
};
