#include <juce_audio_utils/juce_audio_utils.h>

// Local measurement utility, not part of the shipped instrument. The user loads
// a licensed Kontakt instrument in its own editor; no NKI parsing or patching.
class Panel final : public juce::Component
{
public:
    Panel()
    {
        for(auto* c:std::initializer_list<juce::Component*>{&load,&save,&render,&name,&status,&reveal,&instructions,&sampleRate,&matrix}) addAndMakeVisible(c);
        sampleRate.addItem("48 kHz",48000);sampleRate.addItem("44.1 kHz",44100);sampleRate.addItem("96 kHz",96000);sampleRate.setSelectedId(48000);
        sampleRate.setTooltip("Choose before loading Kontakt. Restart this host to change rate.");
        matrix.addItem("All velocities",1);matrix.addItem("Three velocities",2);matrix.setSelectedId(1);
        name.setText("dry"); status.setText("Load Kontakt, then load the reference NKI in its editor.",juce::dontSendNotification);
        save.setEnabled(false);render.setEnabled(false);
        instructions.setText("Choose the rate before Load Kontakt, then drag Glitch.nki into Kontakt below.\nCapture uses offline rendering at a fixed rate. Restart the host to change rate.",juce::dontSendNotification);
        instructions.setJustificationType(juce::Justification::centredLeft);
        reveal.onClick=[this]
        {
            const juce::File reference("/private/tmp/spaglitch-reference/Glitch.nki");
            if(reference.existsAsFile()) reference.revealToUser();
            else status.setText("Reference copy missing. Drag the original Glitch.nki from your library into Kontakt.",juce::dontSendNotification);
        };
        load.onClick=[this]{loadPlugin();}; save.onClick=[this]{saveState();}; render.onClick=[this]{capture();};
        setSize(1100,850);
    }
    ~Panel() override { editor.reset(); if(plugin) plugin->releaseResources(); }
    void resized() override
    {
        load.setBounds(10,10,140,28); save.setBounds(160,10,120,28); name.setBounds(290,10,160,28); render.setBounds(460,10,160,28);
        status.setBounds(10,44,getWidth()-20,28);
        reveal.setBounds(630,10,160,28);
        instructions.setBounds(10,74,getWidth()-20,44);
        sampleRate.setBounds(10,120,140,26);matrix.setBounds(160,120,190,26);
        if(editor) editor->setBounds(0,156,editor->getWidth(),editor->getHeight());
    }
private:
    juce::VST3PluginFormat format;
    std::unique_ptr<juce::AudioPluginInstance> plugin;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    juce::TextButton load{"Load Kontakt"},save{"Save state"},render{"Capture matrix"};
    juce::TextButton reveal{"Reveal Glitch.nki"};
    juce::TextEditor name;
    juce::Label status,instructions;
    juce::ComboBox sampleRate,matrix;
    int rate=48000;
    juce::File root=juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("ChatGPT/SPAGlitch/local/parity");
    void loadPlugin()
    {
        if(plugin) { status.setText("Kontakt is already loaded. Drag Glitch.nki into its editor below.",juce::dontSendNotification); return; }
        juce::OwnedArray<juce::PluginDescription> types;
        format.findAllTypesForFile(types,"/Library/Audio/Plug-Ins/VST3/Kontakt 8.vst3");
        if(types.isEmpty()) { status.setText("Kontakt VST3 not found",juce::dontSendNotification); return; }
        juce::String error;
        rate=sampleRate.getSelectedId();
        plugin=format.createInstanceFromDescription(*types[0],rate,256,error);
        if(!plugin) { status.setText(error,juce::dontSendNotification); return; }
        plugin->enableAllBuses();plugin->setNonRealtime(true);
        plugin->setRateAndBufferSizeDetails(rate,256);plugin->prepareToPlay(rate,256);
        editor.reset(plugin->createEditorAndMakeActive());
        if(!editor)
        {
            plugin->releaseResources();plugin.reset();
            status.setText("Kontakt loaded but its editor could not open. Click Load Kontakt to retry.",juce::dontSendNotification);
            return;
        }
        addAndMakeVisible(*editor); setSize(juce::jmax(800,editor->getWidth()),editor->getHeight()+156); resized();
        sampleRate.setEnabled(false);
        load.setButtonText("Kontakt loaded");load.setEnabled(false);
        save.setEnabled(true);render.setEnabled(true);
        status.setText("Kontakt is ready. Now load Glitch.nki inside Kontakt, then wait for its samples.",juce::dontSendNotification);
    }
    juce::File directory()
    {
        auto label=juce::File::createLegalFileName(name.getText().trim());
        if(label.isEmpty()) label="capture";
        auto dir=root.getNonexistentChildFile(label,""); dir.createDirectory(); return dir;
    }
    void saveState()
    {
        if(!plugin) return;
        const auto dir=directory(); juce::MemoryBlock state; plugin->getStateInformation(state);
        dir.getChildFile("kontakt.state").replaceWithData(state.getData(),state.getSize());
        status.setText("Saved "+dir.getFullPathName(),juce::dontSendNotification);
    }
    void capture()
    {
        if(!plugin) return;
        const auto dir=directory();
        juce::MemoryBlock state; plugin->getStateInformation(state);
        dir.getChildFile("kontakt.state").replaceWithData(state.getData(),state.getSize());
        juce::String report="note,velocity,gate_frames,rate,peak\n";
        bool silent=false;
        int silentTakes=0,totalTakes=0;
        // Identical note, varied velocity and gate: separates amp response from
        // sample content. Every capture includes a long held-note reference.
        for(int velocity=127;velocity>=1;--velocity) for(const int gateMs:{50,500,3000})
        {
            if(matrix.getSelectedId()==2 && velocity!=127 && velocity!=64 && velocity!=32) continue;
            if(velocity!=127 && velocity!=64 && velocity!=32 && gateMs!=3000) continue;
            const int frames=rate*4,gate=rate*gateMs/1000;
            constexpr int block=256;
            const int channels=juce::jmax(2,plugin->getTotalNumOutputChannels());
            juce::AudioBuffer<float> buffer(channels,block), output(2,frames); output.clear();
            juce::MidiBuffer midi;
            // Drain previous voices without restoring state, which may reload
            // samples asynchronously and invalidate an immediate render.
            for(int p=0;p<rate;p+=block) { buffer.clear(); midi.clear(); if(p==0) midi.addEvent(juce::MidiMessage::allSoundOff(1),0); plugin->processBlock(buffer,midi); }
            for(int p=0;p<frames;p+=block)
            {
                const int n=juce::jmin(block,frames-p); buffer.clear(); midi.clear();
                if(p==0) midi.addEvent(juce::MidiMessage::noteOn(1,12,(juce::uint8)velocity),0);
                if(gate>=p && gate<p+n) midi.addEvent(juce::MidiMessage::noteOff(1,12),gate-p);
                plugin->processBlock(buffer,midi);
                for(int ch=0;ch<2;++ch) output.copyFrom(ch,p,buffer,ch,0,n);
            }
            const auto prefix=rate==48000 ? juce::String{} : "r"+juce::String(rate)+"-";
            const auto file=dir.getChildFile(prefix+"n12-v"+juce::String(velocity)+"-g"+juce::String(gate)+".wav");
            juce::WavAudioFormat wav; auto stream=file.createOutputStream();
            if(!stream) { status.setText("Cannot open capture WAV",juce::dontSendNotification); return; }
            std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),rate,2,24,{},0));
            if(!writer || !writer->writeFromAudioSampleBuffer(output,0,frames)) { status.setText("WAV write failed",juce::dontSendNotification); return; }
            report+="12,"+juce::String(velocity)+","+juce::String(gate)+","+juce::String(rate)+","+juce::String(output.getMagnitude(0,frames),9)+"\n";
            // Low velocities can legitimately be silent; a full-velocity take cannot.
            silent|=velocity==127 && output.getMagnitude(0,frames)==0;
            ++totalTakes;if(output.getMagnitude(0,frames)==0)++silentTakes;
        }
        dir.getChildFile("capture.csv").replaceWithText(report);
        dir.getChildFile("render-mode.txt").replaceWithText("VST3 offline processing; fixed sample rate "+juce::String(rate)+" Hz; block size 256\n");
        dir.getChildFile("capture-summary.txt").replaceWithText(juce::String(totalTakes)+" takes; "+juce::String(silentTakes)+" silent.\nSilence may be legitimate at low velocity, or caused by missing content/licensing. Verify before calibration.\n");
        status.setText(silent ? "Capture contains silence. Check the NKI, samples, MIDI channel 1 and Kontakt demo status."
                              : silentTakes>0 ? "Captured "+juce::String(totalTakes)+" takes; "+juce::String(silentTakes)+" silent. Verify content/license before using them."
                                              : "Captured "+dir.getFullPathName(),juce::dontSendNotification);
    }
};
class App final : public juce::JUCEApplication
{
    class Window final : public juce::DocumentWindow
    {
    public:
        Window():DocumentWindow("Glitch Reference Host",juce::Colours::darkgrey,allButtons)
        { setUsingNativeTitleBar(true); setContentOwned(new Panel(),true); centreWithSize(getWidth(),getHeight()); setVisible(true); }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };
    std::unique_ptr<Window> window;
public:
    const juce::String getApplicationName() override { return "Glitch Reference Host"; }
    const juce::String getApplicationVersion() override { return "0.1"; }
    void initialise(const juce::String&) override { window=std::make_unique<Window>(); }
    void shutdown() override { window.reset(); }
};
START_JUCE_APPLICATION(App)
