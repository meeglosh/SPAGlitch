#include "PresetManager.h"
#include <BinaryData.h>

namespace glitch
{
namespace
{
const juce::Identifier presetTag { "SPAGlitchPreset" };

juce::PropertiesFile::Options favouriteOptions()
{
    juce::PropertiesFile::Options options;
    options.applicationName = "SPAGlitch";
    options.filenameSuffix = "settings";
    options.folderName = "Silverplatter Audio/SPAGlitch";
    options.osxLibrarySubFolder = "Application Support";
    return options;
}

// The factory patches are compiled in, so a fresh install has content before
// anything else is set up.
struct Bundled { const char* name; const char* data; int size; };
std::vector<Bundled> bundledFactory()
{
    std::vector<Bundled> out;
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const juce::String resource (BinaryData::namedResourceList[i]);
        if (! resource.endsWith ("_spaglitch")) continue;

        int size = 0;
        if (const auto* data = BinaryData::getNamedResource (resource.toRawUTF8(), size))
            out.push_back ({ BinaryData::getNamedResourceOriginalFilename (resource.toRawUTF8()),
                             data, size });
    }
    return out;
}
} // namespace

PresetManager::PresetManager (std::function<juce::ValueTree()> capture,
                              std::function<void (const juce::ValueTree&)> apply)
    : captureState (std::move (capture)), applyState (std::move (apply))
{
    loadFavourites();
}

juce::File PresetManager::presetsRoot()
{
#if JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
             .getChildFile ("Library/Silverplatter Audio/SPAGlitch/Presets");
#else
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile ("Silverplatter Audio/SPAGlitch/Presets");
#endif
}

void PresetManager::installFactoryAndRescan()
{
    auto factory = presetsRoot().getChildFile ("Factory");
    factory.createDirectory();

    // Re-written when absent rather than only on a first run, so deleting a
    // factory patch by accident restores it rather than losing it for good.
    for (const auto& preset : bundledFactory())
    {
        auto file = factory.getChildFile (juce::String (preset.name));
        if (! file.existsAsFile())
            file.replaceWithData (preset.data, (size_t) preset.size);
    }

    presetsRoot().getChildFile ("User").createDirectory();
    rescan();
}

void PresetManager::rescan()
{
    presets.clear();

    for (const auto* folder : { "Factory", "User" })
    {
        auto dir = presetsRoot().getChildFile (folder);
        if (! dir.isDirectory()) continue;

        for (const auto& entry : juce::RangedDirectoryIterator (dir, true, juce::String ("*") + extension))
        {
            Info info;
            info.file = entry.getFile();
            info.name = info.file.getFileNameWithoutExtension();
            info.category = folder;
            info.isUser = juce::String (folder) == "User";
            presets.push_back (info);
        }
    }

    std::sort (presets.begin(), presets.end(), [] (const Info& a, const Info& b)
    {
        if (a.category != b.category) return a.category < b.category;
        return a.name.compareIgnoreCase (b.name) < 0;
    });

    sendChangeMessage();
}

juce::StringArray PresetManager::categories() const
{
    juce::StringArray out;
    for (const auto& p : presets)
        out.addIfNotAlreadyThere (p.category);
    out.sort (true);
    return out;
}

bool PresetManager::load (const Info& info)
{
    if (! info.file.existsAsFile() || ! applyState) return false;

    auto xml = juce::XmlDocument::parse (info.file);
    if (xml == nullptr || ! xml->hasTagName (presetTag.toString())) return false;

    applyState (juce::ValueTree::fromXml (*xml));
    current = info.name;
    sendChangeMessage();
    return true;
}

bool PresetManager::save (const juce::String& name)
{
    const auto trimmed = name.trim();
    if (trimmed.isEmpty() || ! captureState) return false;

    auto dir = presetsRoot().getChildFile ("User");
    dir.createDirectory();
    auto file = dir.getChildFile (juce::File::createLegalFileName (trimmed) + extension);

    auto tree = captureState();
    tree = tree.createCopy();
    // The tag identifies a preset file; the tree itself is the parameter tree.
    juce::ValueTree wrapper (presetTag);
    wrapper.copyPropertiesFrom (tree, nullptr);
    for (const auto& child : tree)
        wrapper.appendChild (child.createCopy(), nullptr);

    if (auto xml = wrapper.createXml())
        if (file.replaceWithText (xml->toString()))
        {
            current = trimmed;
            rescan();
            return true;
        }

    return false;
}

bool PresetManager::remove (const Info& info)
{
    if (! info.isUser || ! info.file.existsAsFile()) return false;
    if (! info.file.deleteFile()) return false;
    rescan();
    return true;
}

bool PresetManager::step (int delta, const std::vector<int>& visibleIndices)
{
    if (visibleIndices.empty()) return false;

    int position = -1;
    for (size_t i = 0; i < visibleIndices.size(); ++i)
        if (presets[(size_t) visibleIndices[i]].name == current)
            position = (int) i;

    position = (position < 0 ? 0 : position + delta);
    position = (position + (int) visibleIndices.size()) % (int) visibleIndices.size();
    return load (presets[(size_t) visibleIndices[(size_t) position]]);
}

// --- favourites -------------------------------------------------------------

juce::String PresetManager::favouriteKey (const Info& info)
{
    return info.category + "/" + info.name;
}

bool PresetManager::isFavourite (const Info& info) const
{
    return favouriteKeys.contains (favouriteKey (info));
}

void PresetManager::toggleFavourite (const Info& info)
{
    const auto key = favouriteKey (info);
    if (favouriteKeys.contains (key)) favouriteKeys.removeString (key);
    else                              favouriteKeys.add (key);
    saveFavourites();
    sendChangeMessage();
}

void PresetManager::loadFavourites()
{
    juce::PropertiesFile file (favouriteOptions());
    favouriteKeys.clear();
    favouriteKeys.addTokens (file.getValue ("favouritePresets"), "\n", "");
    favouriteKeys.removeEmptyStrings();
}

void PresetManager::saveFavourites()
{
    juce::PropertiesFile file (favouriteOptions());
    file.setValue ("favouritePresets", favouriteKeys.joinIntoString ("\n"));
    file.saveIfNeeded();
}

} // namespace glitch
