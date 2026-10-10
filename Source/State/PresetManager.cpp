#include "PresetManager.h"
#include "FactoryPresets.h"
#include "PluginProcessor.h"

using namespace juce;

static const char* presetExtension = ".ropreset";

PresetManager::PresetManager (ResoOpusProcessor& p) : proc (p)
{
    addFactoryPresets();
    rescan();
    currentSnapshot = factory.getReference (0).state.createCopy();
}

StringArray PresetManager::categories()
{
    return { "Basics", "Drones", "Cinematic", "Textures", "Orbits", "Lead", "Pulse" };
}

float PresetManager::valueIn (const ValueTree& state, const String& id, float fallback)
{
    auto child = state.getChildWithProperty ("id", id);
    return child.isValid() ? (float) child.getProperty ("value") : fallback;
}

void PresetManager::addFactoryPresets()
{
    for (auto& fp : factoryPresets())
    {
        // parameter defaults, then the preset's values, then its modulations in the first slots
        std::map<String, float> values;
        for (auto* prm : proc.getParameters())
            if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm))
                values[rp->paramID] = rp->convertFrom0to1 (rp->getDefaultValue());
        for (auto& v : fp.values) values[v.first] = v.second;
        int slot = 0;
        for (auto& m : fp.mods)
        {
            if (slot >= Params::numModSlots) break;
            values[Params::modID (slot, "on")] = 1.0f;
            values[Params::modID (slot, "src")] = (float) m.source;
            values[Params::modID (slot, "dst")] = (float) Mod::destCode (m.dest);
            values[Params::modID (slot, "amt")] = m.amount;
            values[Params::modID (slot, "bi")] = m.bipolar ? 1.0f : 0.0f;
            values[Params::modID (slot, "fn")] = (float) m.function;
            values[Params::modID (slot, "fnamt")] = m.functionAmount;
            values[Params::modID (slot, "ctl")] = (float) m.controller;
            ++slot;
        }
        ValueTree tree ("PARAMS");
        for (auto& kv : values) tree.appendChild (ValueTree ("PARAM", { { "id", kv.first }, { "value", kv.second } }), nullptr);

        Preset p;
        p.name = fp.name;
        p.category = fp.category;
        p.description = fp.description;
        p.isFactory = true;
        p.state = tree;
        factory.add (p);
    }
}

void PresetManager::rescan()
{
    presets = factory;
    for (auto& f : GlobalSettings::getUserPresetFolder().findChildFiles (File::findFiles, true, String ("*") + presetExtension))
    {
        auto xml = parseXML (f);
        if (xml == nullptr) continue;
        Preset p;
        p.isFactory = false;
        p.file = f;
        p.name = xml->getStringAttribute ("name", f.getFileNameWithoutExtension());
        p.category = xml->getStringAttribute ("category", "Basics");
        p.description = xml->getStringAttribute ("description");
        if (auto* params = xml->getChildByName ("PARAMS")) p.state = ValueTree::fromXml (*params);
        if (p.state.isValid()) presets.add (p);
    }
    sendChangeMessage();
}

int PresetManager::indexOf (const String& key) const
{
    for (int i = 0; i < presets.size(); ++i)
        if (presets.getReference (i).key() == key) return i;
    return -1;
}

const Preset* PresetManager::getCurrent() const
{
    const int i = indexOf (currentKey);
    return i >= 0 ? &presets.getReference (i) : nullptr;
}

ValueTree PresetManager::captureParameters() const
{
    ValueTree tree ("PARAMS");
    for (auto* p : proc.getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (p))
            tree.appendChild (ValueTree ("PARAM", { { "id", rp->paramID }, { "value", rp->convertFrom0to1 (rp->getValue()) } }), nullptr);
    return tree;
}

void PresetManager::applyParameters (const ValueTree& tree)
{
    for (auto* p : proc.getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (p))
        {
            const float value = valueIn (tree, rp->paramID, rp->convertFrom0to1 (rp->getDefaultValue()));
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->convertTo0to1 (value));
            rp->endChangeGesture();
        }
    currentSnapshot = captureParameters();
}

void PresetManager::load (int index, bool silently)
{
    if (! isPositiveAndBelow (index, presets.size())) return;
    const auto p = presets.getReference (index);
    auto apply = [this, p, index]
    {
        applyParameters (p.state);
        currentKey = p.key();
        currentName = p.name;
        proc.presetApplied (p.name, p.key(), p.isFactory ? index : -1);
        sendChangeMessage();
    };
    if (silently && settings->getBool (GlobalSettings::silentPresets, true)) proc.changeSilently (apply);
    else apply();
}

void PresetManager::loadFromFile (const File& file)
{
    auto xml = parseXML (file);
    if (xml == nullptr) return;
    if (auto* params = xml->getChildByName ("PARAMS"))
    {
        const auto tree = ValueTree::fromXml (*params);
        const auto name = xml->getStringAttribute ("name", file.getFileNameWithoutExtension());
        auto apply = [this, tree, name, file]
        {
            applyParameters (tree);
            currentKey = file.getFullPathName();
            currentName = name;
            proc.presetApplied (name, currentKey, -1);
            sendChangeMessage();
        };
        if (settings->getBool (GlobalSettings::silentPresets, true)) proc.changeSilently (apply); else apply();
    }
}

void PresetManager::next (int direction, bool silently)
{
    if (presets.isEmpty()) return;
    const int i = indexOf (currentKey);
    load (((i < 0 ? 0 : i) + direction + presets.size()) % presets.size(), silently);
}

bool PresetManager::writePresetFile (const Preset& p) const
{
    XmlElement xml ("ResoOpusPreset");
    xml.setAttribute ("name", p.name);
    xml.setAttribute ("category", p.category);
    xml.setAttribute ("description", p.description);
    if (auto stateXml = p.state.createXml()) xml.addChildElement (stateXml.release());
    return xml.writeTo (p.file);
}

bool PresetManager::saveCurrent()
{
    const int i = indexOf (currentKey);
    if (i < 0 || presets.getReference (i).isFactory) return false;
    auto p = presets.getReference (i);
    p.state = captureParameters();
    if (! writePresetFile (p)) return false;
    currentSnapshot = p.state.createCopy();
    rescan();
    return true;
}

bool PresetManager::saveAs (const String& name, const String& category, const String& description)
{
    const auto safeName = File::createLegalFileName (name.trim());
    if (safeName.isEmpty()) return false;
    Preset p;
    p.isFactory = false;
    p.name = name.trim();
    p.category = category;
    p.description = description;
    p.file = GlobalSettings::getUserPresetFolder().getChildFile (safeName + presetExtension);
    p.state = captureParameters();
    if (! writePresetFile (p)) return false;
    currentKey = p.key();
    currentName = p.name;
    currentSnapshot = p.state.createCopy();
    proc.presetApplied (p.name, p.key(), -1);
    rescan();
    return true;
}

bool PresetManager::updateInfo (int index, const String& category, const String& description)
{
    if (! isPositiveAndBelow (index, presets.size()) || presets.getReference (index).isFactory) return false;
    auto p = presets.getReference (index);
    p.category = category;
    p.description = description;
    const bool ok = writePresetFile (p);
    rescan();
    return ok;
}

bool PresetManager::remove (int index)
{
    if (! isPositiveAndBelow (index, presets.size()) || presets.getReference (index).isFactory) return false;
    const auto key = presets.getReference (index).key();
    const bool ok = presets.getReference (index).file.deleteFile();
    if (settings->getDefaultPreset() == key) settings->setDefaultPreset ({});
    settings->setFavourite (key, false);
    if (currentKey == key) currentKey = {};
    rescan();
    return ok;
}

void PresetManager::setCurrent (const String& key, const String& name)
{
    currentKey = key;
    currentName = name.isNotEmpty() ? name : String ("Init");
    if (auto* p = getCurrent()) currentSnapshot = p->state.createCopy();
    else currentSnapshot = captureParameters();
    sendChangeMessage();
}

bool PresetManager::isModified() const
{
    if (! currentSnapshot.isValid()) return false;
    for (auto* p : proc.getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (p))
        {
            auto child = currentSnapshot.getChildWithProperty ("id", rp->paramID);
            if (! child.isValid()) continue;
            if (std::abs (rp->convertTo0to1 ((float) child.getProperty ("value")) - rp->getValue()) > 1.0e-4f) return true;
        }
    return false;
}
