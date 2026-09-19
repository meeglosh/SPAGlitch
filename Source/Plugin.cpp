#include "Plugin.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout GlitchProcessor::layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    juce::StringArray groups; for(auto name:glitch::categories) groups.add(name);
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"category",1},"Category",groups,0));
    p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"pitch",1},"Pitch",-12,12,0));
    p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"cutoff",1},"Cutoff",0,1000000,476191));
    p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"resonance",1},"Resonance",0,100,49));
    p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"randomness",1},"Randomness",0,100,0));
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"filter",1},"Filter",juce::StringArray{"High-pass","Off","Low-pass"},1));
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"gain",1},"Output",-60.0f,6.0f,0.0f));
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"keyRange",1},"Key range",juce::StringArray{"Kontakt keys","Middle keys"},1));
    glitch::fx::params::addToLayout(p);
    return p;
}
juce::File GlitchProcessor::installedLibrary()
{
#if JUCE_MAC
    return juce::File("/Library/Application Support/Silverplatter Audio/SPAGlitch/Samples");
#else
    return juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory).getChildFile("Silverplatter Audio/SPAGlitch/Samples");
#endif
}
GlitchProcessor::GlitchProcessor(juce::File factory)
 : AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)),
   parameters(*this,nullptr,"SPAGlitch",layout()),factoryLibrary(std::move(factory))
{
    const char* ids[]{"category","pitch","cutoff","resonance","randomness","filter","gain","keyRange"};
    for(size_t i=0;i<values.size();++i) values[i]=parameters.getRawParameterValue(ids[i]);
    publishedRuntime.write(engine.runtimeState());
    fxSnapshot.bind(parameters);
    startTimer(200);
    if(factoryLibrary.isDirectory()) loadInstalledLibrary();
}
GlitchProcessor::~GlitchProcessor() { engine.setBank(nullptr); }
glitch::Controls GlitchProcessor::controls() const noexcept
{
    glitch::Controls c;
    c.category=(int)values[0]->load(); c.pitch=(int)values[1]->load();
    c.cutoff=(int)values[2]->load(); c.resonance=(int)values[3]->load();
    c.randomness=(int)values[4]->load(); c.filter=(int)values[5]->load();
    c.gainDb=values[6]->load(); c.middleKeys=values[7]->load()>.5f;
    // lofi / drive / destroy keep their Controls defaults. The destroy stage
    // (the measured Kontakt lo-fi + tube path) has no front-panel controls any
    // more, and Controls::destroy defaults to 1 = bypassed, which is exactly
    // what it defaulted to when the DESTROY menu still existed.
    return c;
}
bool GlitchProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    return l.getMainInputChannelSet().isDisabled() && (l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo() || l.getMainOutputChannelSet()==juce::AudioChannelSet::mono());
}
void GlitchProcessor::prepareToPlay(double rate,int blockSize)
{
    visualPeak.store(0,std::memory_order_relaxed);
    engine.setControls(controls()); engine.prepare(rate); keyboard.reset();
    fxChain.prepare(rate,juce::jmax(1,blockSize));
    for(auto& sample:scope) sample.store(0,std::memory_order_relaxed);
    scopeWrite.store(0,std::memory_order_relaxed); scopeFilled.store(false,std::memory_order_relaxed);
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

    // The ported SPASynth chain runs on the engine's stereo output. Every
    // module defaults to off, so the measured Kontakt path is untouched until
    // an effect is switched on.
    double bpm=120;
    if(auto* head=getPlayHead())
        if(auto info=head->getPosition())
            if(auto hostBpm=info->getBpm())
                bpm=*hostBpm;
    glitch::fx::FXChain::Params fx;
    fxSnapshot.read(fx,bpm,fxOrderPacked.load(std::memory_order_relaxed));
    fxChain.process(b,fx);
    publishTailAndLatency(fx);
    pushScope(b);

    if(b.getNumSamples()>0)
    {
        leftPeak=b.getMagnitude(0,0,b.getNumSamples());
        rightPeak=b.getMagnitude(b.getNumChannels()>1?1:0,0,b.getNumSamples());
        const float peak=std::max(leftPeak.load(std::memory_order_relaxed),rightPeak.load(std::memory_order_relaxed));
        auto held=visualPeak.load(std::memory_order_relaxed);
        while(peak>held && !visualPeak.compare_exchange_weak(held,peak,std::memory_order_relaxed)) {}
    }
    midi.clear();
}
void GlitchProcessor::getStateInformation(juce::MemoryBlock& out)
{
    auto state=parameters.copyState();
    state.setProperty("stateVersion",6,nullptr);
    state.setProperty("fxOrder",(juce::int64)fxOrderPacked.load(std::memory_order_relaxed),nullptr);
    state.setProperty("fxCollapsed",fxCollapsed.load(),nullptr);
    state.setProperty("keyboardCollapsed",keyboardCollapsed.load(),nullptr);
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
            // Older projects keep their original MIDI/sample mapping.
            if((int)state.getProperty("stateVersion",1)<4)
            {
                auto mapping=state.getChildWithProperty("id","keyRange");
                if(!mapping.isValid()) { mapping=juce::ValueTree("PARAM");mapping.setProperty("id","keyRange",nullptr);state.addChild(mapping,-1,nullptr); }
                mapping.setProperty("value",0,nullptr);
            }
            parameters.replaceState(state);
            // Absent in pre-v5 states; unpackOrder falls back to the natural
            // order if the stored permutation is ever invalid.
            fxOrderPacked.store((juce::uint64)(juce::int64)state.getProperty(
                "fxOrder",(juce::int64)glitch::fx::FXChain::defaultOrderPacked()),
                std::memory_order_relaxed);
            fxCollapsed.store((bool)state.getProperty("fxCollapsed",false));
            keyboardCollapsed.store((bool)state.getProperty("keyboardCollapsed",false));
            const auto seed=(uint32_t)(juce::int64)state.getProperty("randomSeed",(juce::int64)0x47544348u);
            restoredSeed=seed ? seed : 1;
            // Legacy states reconstruct their effective settings from controls;
            // version 3 preserves latent insert values and script transition state.
            glitch::Engine initial; initial.setControls(controls());
            auto runtime=initial.runtimeState();
            if((int)state.getProperty("stateVersion",1)>=3)
                for(size_t i=0;i<runtime.size();++i)
                    runtime[i]=(int)state.getProperty("runtime"+juce::String((int)i),runtime[i]);
            // The destroy stage is gone from the front panel, so a project saved
            // while it was switched on (by the DESTROY menu, or by Boom
            // randomising it) must not resurrect an effect the user can no
            // longer see or turn off. Indices 4 and 24 are settings.destroy and
            // controls.destroy; 1 is bypassed.
            runtime[4]=1; runtime[24]=1;
            pendingRuntime.write(runtime);
            const bool legacy=state.hasProperty("samplePath") && !state.hasProperty("contentPath");
            const bool single=legacy || (bool)state.getProperty("singleSample",false);
            if(!single && factoryLibrary.isDirectory()) loadInstalledLibrary();
            else content.request(state.getProperty(legacy?"samplePath":"contentPath").toString(),single);
        }
}
void GlitchProcessor::setFxOrder(const juce::Array<int>& moduleIds)
{
    if(moduleIds.size()!=glitch::fx::FXChain::numModules) return;
    // Must be a full permutation. A duplicate would pack into a value that
    // unpackOrder rejects, and the audio thread would then silently fall back
    // to the natural order -- quietly rearranging the chain rather than
    // leaving the existing one alone.
    bool seen[glitch::fx::FXChain::numModules]={};
    glitch::fx::FXChain::Module order[glitch::fx::FXChain::numModules];
    for(int i=0;i<glitch::fx::FXChain::numModules;++i)
    {
        const int id=moduleIds[i];
        if(!juce::isPositiveAndBelow(id,glitch::fx::FXChain::numModules) || seen[id]) return;
        seen[id]=true;
        order[i]=(glitch::fx::FXChain::Module)id;
    }
    fxOrderPacked.store(glitch::fx::FXChain::packOrder(order),std::memory_order_relaxed);
}
juce::Array<int> GlitchProcessor::getFxOrder() const
{
    glitch::fx::FXChain::Module order[glitch::fx::FXChain::numModules];
    glitch::fx::FXChain::unpackOrder(fxOrderPacked.load(std::memory_order_relaxed),order);
    juce::Array<int> ids;
    for(auto module:order) ids.add((int)module);
    return ids;
}
void GlitchProcessor::publishTailAndLatency(const glitch::fx::FXChain::Params& fx)
{
    // Lookahead limiting is the only latency the chain adds. Only publish it
    // here; the timer applies it, since setLatencySamples notifies the host.
    desiredLatency.store(fxChain.limiterLatencySamples(fx),std::memory_order_relaxed);
    fxTailSeconds.store(fxChain.tailSeconds(fx),std::memory_order_relaxed);
}
void GlitchProcessor::timerCallback()
{
    const int latency=desiredLatency.load(std::memory_order_relaxed);
    if(latency!=reportedLatency) { reportedLatency=latency; setLatencySamples(latency); }
}
void GlitchProcessor::pushScope(const juce::AudioBuffer<float>& b)
{
    const int n=b.getNumSamples();
    if(n<=0 || b.getNumChannels()<=0) return;
    const auto* left=b.getReadPointer(0);
    const auto* right=b.getNumChannels()>1 ? b.getReadPointer(1) : left;
    int w=scopeWrite.load(std::memory_order_relaxed);
    for(int i=0;i<n;++i)
    {
        scope[(size_t)w].store(0.5f*(left[i]+right[i]),std::memory_order_relaxed);
        w=(w+1)&(scopeSize-1);
    }
    scopeWrite.store(w,std::memory_order_release);
    scopeFilled.store(true,std::memory_order_relaxed);
}
bool GlitchProcessor::readScope(float* dest,int numSamples) const
{
    if(dest==nullptr || numSamples!=scopeSize || !scopeFilled.load(std::memory_order_relaxed)) return false;
    const int w=scopeWrite.load(std::memory_order_acquire);
    for(int i=0;i<numSamples;++i)
        dest[i]=scope[(size_t)((w+i)&(scopeSize-1))].load(std::memory_order_relaxed);   // oldest -> newest
    return true;
}
juce::AudioProcessorEditor* GlitchProcessor::createEditor() { return new GlitchEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GlitchProcessor(); }
