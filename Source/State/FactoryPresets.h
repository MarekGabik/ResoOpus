#pragma once

#include <vector>
#include <utility>

// Factory presets defined in code. Values are real (unnormalised) parameter values set on top of the defaults.
struct PresetMod
{
    int source;
    const char* dest;
    float amount;
    int function = 0;
    float functionAmount = 50.0f;
    int controller = 0;
    bool bipolar = true;
};

struct FactoryPreset
{
    const char* name;
    const char* category;
    const char* description;   // what it is for and what to tweak
    std::vector<std::pair<const char*, float>> values;
    std::vector<PresetMod> mods;
};

const std::vector<FactoryPreset>& factoryPresets();
