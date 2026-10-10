#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "GlobalSettings.h"

class ResoOpusProcessor;

struct Preset
{
    juce::String name, category, description;
    bool isFactory = true;
    juce::File file;                 // user presets only
    juce::ValueTree state;           // PARAMS tree: one PARAM child (id, value) per parameter

    juce::String key() const { return isFactory ? "factory:" + name : file.getFullPathName(); }
};

// Factory presets (from code) and user presets (files in Documents/Gavr/ResoOpus Presets), favourites,
// a user default, "modified" detection. Loading goes through the processor's silent change when that is on.
class PresetManager : public juce::ChangeBroadcaster
{
public:
    explicit PresetManager (ResoOpusProcessor&);

    static juce::StringArray categories();

    void rescan();
    const juce::Array<Preset>& getPresets() const { return presets; }
    int indexOf (const juce::String& key) const;
    int numFactory() const { return factory.size(); }

    void load (int index, bool silently = true);
    void loadFromFile (const juce::File&);
    void next (int direction, bool silently = true);

    bool saveCurrent();   // overwrites the current user preset
    bool saveAs (const juce::String& name, const juce::String& category, const juce::String& description);
    bool updateInfo (int index, const juce::String& category, const juce::String& description);
    bool remove (int index);

    juce::String getDefaultKey() const { return settings->getDefaultPreset(); }
    juce::String getCurrentName() const { return currentName; }
    juce::String getCurrentKey() const { return currentKey; }
    const Preset* getCurrent() const;
    bool isModified() const;
    void setCurrent (const juce::String& key, const juce::String& name);   // restored from the plugin state

    static float valueIn (const juce::ValueTree& state, const juce::String& id, float fallback);

private:
    void addFactoryPresets();
    juce::ValueTree captureParameters() const;
    void applyParameters (const juce::ValueTree& tree);
    bool writePresetFile (const Preset&) const;

    ResoOpusProcessor& proc;
    juce::SharedResourcePointer<GlobalSettings> settings;
    juce::Array<Preset> factory, presets;
    juce::String currentKey { "factory:Init" }, currentName { "Init" };
    juce::ValueTree currentSnapshot;
};
