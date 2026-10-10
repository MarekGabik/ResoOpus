#pragma once

#include <juce_data_structures/juce_data_structures.h>

// Settings shared by all instances (Help menu options, default window size and scaling).
// Access through juce::SharedResourcePointer<GlobalSettings>.
class GlobalSettings : public juce::ChangeBroadcaster
{
public:
    GlobalSettings();

    static inline const juce::String tooltips             { "tooltips" };
    static inline const juce::String scaling              { "scaling" };      // percent
    static inline const juce::String silentPresets        { "silentPresets" };

    bool getBool (const juce::String& key, bool fallback = true) const;
    void setBool (const juce::String& key, bool value);
    int getInt (const juce::String& key, int fallback) const;
    void setInt (const juce::String& key, int value);

private:
    std::unique_ptr<juce::PropertiesFile> file;
};
