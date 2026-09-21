#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include <vector>

namespace glitch
{

// Preset save/load/scan. A preset is human-readable XML wrapping the same
// parameter tree the host chunk uses, minus everything machine-specific: the
// sample-content path, the MIDI-learn map, the drawer states and the
// randomizer's wildness/lock settings all stay with the instance, not the
// patch.
//
// Layout under the presets root:
//   Factory/<Name>.spaglitch   installed on first run, read-only in practice
//   User/<Name>.spaglitch      whatever the user saves
class PresetManager : public juce::ChangeBroadcaster
{
public:
    struct Info
    {
        juce::String name, category;   // category: "Factory" or "User"
        juce::File file;
        bool isUser = false;
    };

    PresetManager (std::function<juce::ValueTree()> capture,
                   std::function<void (const juce::ValueTree&)> apply);

    static juce::File presetsRoot();
    static constexpr const char* extension = ".spaglitch";

    // Writes the bundled factory patches the first time SPAGlitch runs, then
    // scans. Missing factory files are re-written, so deleting one restores it.
    void installFactoryAndRescan();
    void rescan();

    const std::vector<Info>& all() const { return presets; }
    juce::StringArray categories() const;

    bool load (const Info&);
    bool save (const juce::String& name);          // into User/, overwrites
    bool remove (const Info&);

    // Favourites are a per-user setting, not part of any preset.
    static juce::String favouriteKey (const Info&);
    bool isFavourite (const Info&) const;
    void toggleFavourite (const Info&);
    const juce::StringArray& favourites() const { return favouriteKeys; }

    juce::String currentName() const { return current; }

    // Step to the next/previous preset in the browser's current ordering.
    bool step (int delta, const std::vector<int>& visibleIndices);

private:
    void loadFavourites();
    void saveFavourites();

    std::function<juce::ValueTree()> captureState;
    std::function<void (const juce::ValueTree&)> applyState;
    std::vector<Info> presets;
    juce::StringArray favouriteKeys;
    juce::String current;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};

} // namespace glitch
