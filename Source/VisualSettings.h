#pragma once
#include <juce_data_structures/juce_data_structures.h>

namespace glitch
{

// Calm mode and the flash notice are stored per machine rather than per patch
// or per project. Someone who needs the flashing off needs it off in every
// instance, in every session, from the moment the plugin opens -- a setting
// that travelled with the patch would mean the next project someone opened
// could flash at them again.
struct VisualSettings
{
    // The test suite switches this on so a machine's real preference can never
    // change what the suite renders or asserts, and so running the tests can
    // never write to the settings a person actually uses.
    static bool& inMemoryOnly() { static bool value = false; return value; }
    static void useInMemoryStore() { inMemoryOnly() = true; }

    static juce::PropertiesFile::Options options()
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "SPAGlitch";
        o.filenameSuffix = "settings";
        o.folderName = "Silverplatter Audio/SPAGlitch";
        o.osxLibrarySubFolder = "Application Support";
        return o;
    }

    static bool calmMode()
    {
        if (inMemoryOnly()) return memoryCalm();
        juce::PropertiesFile file (options());
        return file.getBoolValue ("calmMode", false);
    }

    static void setCalmMode (bool on)
    {
        if (inMemoryOnly()) { memoryCalm() = on; return; }
        juce::PropertiesFile file (options());
        file.setValue ("calmMode", on);
        file.saveIfNeeded();
    }

    // Shown once per machine, before anyone has played a note.
    static bool hasSeenFlashNotice()
    {
        if (inMemoryOnly()) return memorySeen();
        juce::PropertiesFile file (options());
        return file.getBoolValue ("seenFlashNotice", false);
    }

    static void setSeenFlashNotice()
    {
        if (inMemoryOnly()) { memorySeen() = true; return; }
        juce::PropertiesFile file (options());
        file.setValue ("seenFlashNotice", true);
        file.saveIfNeeded();
    }

private:
    static bool& memoryCalm() { static bool value = false; return value; }
    static bool& memorySeen() { static bool value = false; return value; }
};

} // namespace glitch
