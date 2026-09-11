#include "Plugin.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout GlitchProcessor::layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    juce::StringArray groups; for(auto name:glitch::categories) groups.add(name);
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"category",1},"Category",groups,0));
    p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"pitch",1},"Pitch",-12,12,0));
    p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"lofi",1},"Lo-Fi",0,8,4));
    p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"drive",1},"Distortion",0,1000000,488095));
    p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"cutoff",1},"Cutoff",0,1000000,476191));
    p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"resonance",1},"Resonance",0,100,49));
    p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"randomness",1},"Randomness",0,100,0));
    // 0 enables effects and 1 bypasses, matching the original UI callback.
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"destroy",1},"Destroy",juce::StringArray{"On","Off"},1));
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"filter",1},"Filter",juce::StringArray{"High-pass","Off","Low-pass"},1));
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"gain",1},"Output",-60.0f,6.0f,0.0f));
    return p;
}
GlitchProcessor::GlitchProcessor()
 : AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)),
   parameters(*this,nullptr,"SPAGlitch",layout())
{
    const char* ids[]{"category","pitch","lofi","drive","cutoff","resonance","randomness","destroy","filter","gain"};
    for(size_t i=0;i<values.size();++i) values[i]=parameters.getRawParameterValue(ids[i]);
    publishedRuntime.write(engine.runtimeState());
}
GlitchProcessor::~GlitchProcessor() { engine.setBank(nullptr); }
glitch::Controls GlitchProcessor::controls() const noexcept
{
    glitch::Controls c;
    c.category=(int)values[0]->load(); c.pitch=(int)values[1]->load(); c.lofi=(int)values[2]->load();
    c.drive=(int)values[3]->load(); c.cutoff=(int)values[4]->load(); c.resonance=(int)values[5]->load();
    c.randomness=(int)values[6]->load(); c.destroy=(int)values[7]->load(); c.filter=(int)values[8]->load(); c.gainDb=values[9]->load();
    return c;
}
bool GlitchProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    return l.getMainInputChannelSet().isDisabled() && (l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo() || l.getMainOutputChannelSet()==juce::AudioChannelSet::mono());
}
void GlitchProcessor::prepareToPlay(double rate,int)
{
    engine.setControls(controls()); engine.prepare(rate); keyboard.reset();
}
void GlitchProcessor::processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals guard;
    b.clear();
    // Offline rendering can wait for restore; a real-time callback never waits.
    if(isNonRealtime() && content.busy()) content.waitUntilReady(60000);
    auto* bank=content.adoptForAudio();
    if(bank!=currentBank) { engine.setBank(bank); currentBank=bank; }
    if(auto seed=restoredSeed.exchange(0)) engine.setSeed(seed);
    engine.setControls(controls());
    glitch::Engine::RuntimeState runtime; uint64_t revision=0;
    if(pendingRuntime.read(runtime,revision) && revision!=appliedRuntime.load())
    { engine.restoreRuntimeState(runtime); engine.setControls(controls()); appliedRuntime=revision; }
    if(panicRequested.exchange(false)) { engine.reset(); keyboard.reset(); }
    keyboard.processNextMidiBuffer(midi,0,b.getNumSamples(),true);
    bool containsNoteOn=false;
    for(const auto metadata:midi) if(metadata.numBytes<=3 && metadata.getMessage().isNoteOn()) containsNoteOn=true;
    engine.beginBlock(containsNoteOn);
    int position=0;
    for(const auto metadata:midi)
    {
        const int eventPosition=juce::jlimit(position,b.getNumSamples(),metadata.samplePosition);
        engine.render(b,position,eventPosition-position);
        if(metadata.numBytes<=3) engine.handle(metadata.getMessage());
        position=eventPosition;
    }
    engine.render(b,position,b.getNumSamples()-position);
    currentSeed=engine.seed(); playingCategory=engine.effective().category; playingPitch=engine.effective().pitchUnits;
    voiceCount=engine.activeVoices();
    publishedRuntime.write(engine.runtimeState());
    if(b.getNumSamples()>0)
    {
        leftPeak=b.getMagnitude(0,0,b.getNumSamples());
        rightPeak=b.getMagnitude(b.getNumChannels()>1?1:0,0,b.getNumSamples());
    }
    midi.clear();
}
void GlitchProcessor::getStateInformation(juce::MemoryBlock& out)
{
    auto state=parameters.copyState();
    state.setProperty("stateVersion",3,nullptr);
    glitch::Engine::RuntimeState runtime; uint64_t revision=0;
    while(!pendingRuntime.read(runtime,revision)) juce::Thread::yield();
    if(revision==appliedRuntime.load())
        while(!publishedRuntime.read(runtime,revision)) juce::Thread::yield();
    for(size_t i=0;i<runtime.size();++i) state.setProperty("runtime"+juce::String((int)i),runtime[i],nullptr);
    const auto location=content.location();
    state.setProperty("contentPath",location.path,nullptr);
    state.setProperty("singleSample",location.single,nullptr);
    const auto pendingSeed=restoredSeed.load();
    state.setProperty("randomSeed",(juce::int64)(pendingSeed ? pendingSeed : currentSeed.load()),nullptr);
    if(auto xml=state.createXml()) copyXmlToBinary(*xml,out);
}
void GlitchProcessor::setStateInformation(const void* data,int size)
{
    if(auto xml=getXmlFromBinary(data,size))
        if(xml->hasTagName("SPAGlitch"))
        {
            auto state=juce::ValueTree::fromXml(*xml);
            parameters.replaceState(state);
            const auto seed=(uint32_t)(juce::int64)state.getProperty("randomSeed",(juce::int64)0x47544348u);
            restoredSeed=seed ? seed : 1;
            // Legacy states reconstruct their effective settings from controls;
            // version 3 preserves latent insert values and script transition state.
            glitch::Engine initial; initial.setControls(controls());
            auto runtime=initial.runtimeState();
            if((int)state.getProperty("stateVersion",1)>=3)
                for(size_t i=0;i<runtime.size();++i)
                    runtime[i]=(int)state.getProperty("runtime"+juce::String((int)i),runtime[i]);
            pendingRuntime.write(runtime);
            const bool legacy=state.hasProperty("samplePath") && !state.hasProperty("contentPath");
            content.request(state.getProperty(legacy?"samplePath":"contentPath").toString(),legacy || (bool)state.getProperty("singleSample",false));
        }
}
juce::AudioProcessorEditor* GlitchProcessor::createEditor() { return new GlitchEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GlitchProcessor(); }
