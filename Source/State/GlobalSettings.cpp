#include "GlobalSettings.h"

using namespace juce;

GlobalSettings::GlobalSettings()
{
    PropertiesFile::Options o;
    o.applicationName = "ResoOpus";
    o.filenameSuffix = ".settings";
    o.folderName = "Gavr" + String (File::getSeparatorString()) + "ResoOpus";
    o.osxLibrarySubFolder = "Application Support";
    o.storageFormat = PropertiesFile::storeAsXML;
    file = std::make_unique<PropertiesFile> (o);
}

bool GlobalSettings::getBool (const String& key, bool fallback) const
{
    return file->getBoolValue (key, fallback);
}

void GlobalSettings::setBool (const String& key, bool value)
{
    file->setValue (key, value);
    file->saveIfNeeded();
    sendChangeMessage();
}

int GlobalSettings::getInt (const String& key, int fallback) const
{
    return file->getIntValue (key, fallback);
}

bool GlobalSettings::isFavourite (const String& key) const
{
    return StringArray::fromLines (file->getValue ("favourites")).contains (key);
}

void GlobalSettings::setFavourite (const String& key, bool fav)
{
    auto list = StringArray::fromLines (file->getValue ("favourites"));
    list.removeEmptyStrings();
    if (fav) list.addIfNotAlreadyThere (key); else list.removeString (key);
    file->setValue ("favourites", list.joinIntoString ("\n"));
    file->saveIfNeeded();
    sendChangeMessage();
}

String GlobalSettings::getDefaultPreset() const { return file->getValue ("defaultPreset"); }

void GlobalSettings::setDefaultPreset (const String& key)
{
    file->setValue ("defaultPreset", key);
    file->saveIfNeeded();
    sendChangeMessage();
}

File GlobalSettings::getUserPresetFolder()
{
    auto f = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("Gavr").getChildFile ("ResoOpus Presets");
    f.createDirectory();
    return f;
}

void GlobalSettings::setInt (const String& key, int value)
{
    file->setValue (key, value);
    file->saveIfNeeded();
    sendChangeMessage();
}
