#include "ContentLoader.h"
#include <cmath>
#if JUCE_MAC
#include <sys/stat.h>
#endif

ContentLoader::ContentLoader():Thread("SPAGlitch sample loader")
{
    formats.registerBasicFormats(); startThread();
}
ContentLoader::~ContentLoader()
{
    signalThreadShouldExit(); notify(); stopThread(-1);
    delete pending.exchange(nullptr); delete retired.exchange(nullptr); delete active;
}
void ContentLoader::request(const juce::String& p,bool single)
{
    { const juce::ScopedLock guard(lock);
      requestedPath=p; audition=single; fraction=0; loading=true;
      message=p.isEmpty() ? "Clearing library..." : "Loading samples...";
      generation.fetch_add(1); }
    notify();
}
ContentLoader::Location ContentLoader::location() const { const juce::ScopedLock guard(lock); return {requestedPath,audition}; }
juce::String ContentLoader::path() const { const juce::ScopedLock guard(lock); return requestedPath; }
juce::String ContentLoader::status() const { const juce::ScopedLock guard(lock); return message; }
bool ContentLoader::singleSample() const { const juce::ScopedLock guard(lock); return audition; }
const glitch::Bank* ContentLoader::adoptForAudio() noexcept
{
    if(retired.load(std::memory_order_acquire)==nullptr)
        if(auto* next=pending.exchange(nullptr,std::memory_order_acq_rel))
        {
            auto* old=active; active=next;
            retired.store(old,std::memory_order_release);
        }
    return active && active->generation==generation.load(std::memory_order_acquire) ? active : nullptr;
}
bool ContentLoader::waitUntilReady(int timeoutMs) const
{
    const auto end=juce::Time::getMillisecondCounterHiRes()+timeoutMs;
    while(busy() && juce::Time::getMillisecondCounterHiRes()<end) juce::Thread::sleep(2);
    return !busy() && ready();
}
std::unique_ptr<glitch::Sample> ContentLoader::read(const juce::File& file,juce::String& error,int64_t& memory)
{
#if JUCE_MAC
    struct stat info{};
    if(::stat(file.getFullPathName().toRawUTF8(),&info)==0 && (info.st_flags & SF_DATALESS)!=0)
    {
        error="Cloud-only sample: "+file.getFileName()+". Download/keep the entire folder offline in Finder, then Locate library again.";
        return {};
    }
#endif
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if(!reader) { error="Cannot read "+file.getFileName()+". Download the library locally, then retry."; return {}; }
    if(reader->numChannels<1 || reader->numChannels>2 || reader->sampleRate<8000 || reader->sampleRate>384000
       || reader->lengthInSamples<1 || reader->lengthInSamples>reader->sampleRate*60.0)
    { error="Unsupported sample format or duration: "+file.getFileName(); return {}; }
    memory+=reader->lengthInSamples*reader->numChannels*sizeof(float);
    if(memory>1024LL*1024*1024) { error="Decoded library exceeds the 1 GB loading limit."; return {}; }
    auto sample=std::make_unique<glitch::Sample>();
    sample->sampleRate=reader->sampleRate;
    sample->audio.setSize((int)reader->numChannels,(int)reader->lengthInSamples);
    if(!reader->read(&sample->audio,0,sample->audio.getNumSamples(),0,true,true))
    { error="Read failed: "+file.getFileName(); return {}; }
    for(int ch=0;ch<sample->audio.getNumChannels();++ch)
        for(int i=0;i<sample->audio.getNumSamples();++i)
            if(!std::isfinite(sample->audio.getSample(ch,i)))
            { error="Non-finite audio data: "+file.getFileName(); return {}; }
    return sample;
}
std::unique_ptr<glitch::Bank> ContentLoader::load(const juce::String& p,bool single,uint64_t id,juce::String& error)
{
    auto bank=std::make_unique<glitch::Bank>(); bank->generation=id;
    if(p.isEmpty()) return bank;
    if(!juce::File::isAbsolutePath(p)) { error="Choose an absolute library location."; return {}; }
    juce::File root(p); int64_t memory=0;
    if(single)
    {
        bank->audition=read(root,error,memory);
        if(!bank->audition) return {};
        bank->size=1; return bank;
    }
    // Accept the bundle, Kontakt Files, or the actual sample folder.
    const juce::String folder="Silverplatter Audio - Glitch Bundle Samples";
    if(root.getChildFile("Kontakt Files").isDirectory()) root=root.getChildFile("Kontakt Files");
    if(root.getChildFile(folder).isDirectory()) root=root.getChildFile(folder);
    for(int group=0;group<9;++group)
        for(int i=0;i<glitch::counts[(size_t)group];++i)
        {
            if(threadShouldExit() || generation.load()!=id) return {};
            auto name=juce::String(glitch::categories[(size_t)group])+" "+juce::String(i+1).paddedLeft('0',2)+".wav";
            auto sample=read(root.getChildFile(name),error,memory);
            if(!sample) return {};
            bank->samples[(size_t)group][(size_t)i]=std::move(sample);
            fraction.store((float)++bank->size/479.0f);
        }
    return bank;
}
void ContentLoader::run()
{
    uint64_t handled=0;
    while(!threadShouldExit())
    {
        delete retired.exchange(nullptr,std::memory_order_acq_rel);
        juce::String p; bool single=false; uint64_t id=0;
        { const juce::ScopedLock guard(lock);
          id=generation.load(); if(id!=handled) { p=requestedPath; single=audition; } }
        if(id==handled) { wait(20); continue; }
        juce::String error;
        std::unique_ptr<glitch::Bank> bank;
        try { bank=load(p,single,id,error); }
        catch(const std::exception& e) { error="Loading failed: "+juce::String(e.what()); }
        { const juce::ScopedLock guard(lock);
          if(generation.load()==id)
          {
              if(bank)
              {
                  message=bank->size==0 ? "Choose the Glitch Bundle sample folder to begin." : juce::String(bank->size)+" samples ready";
                  delete pending.exchange(bank.release(),std::memory_order_acq_rel);
                  fraction=1; readyGeneration=id;
              }
              else message=error.isEmpty() ? "Loading cancelled." : error;
              loading=false;
          } }
        handled=id;
    }
}
