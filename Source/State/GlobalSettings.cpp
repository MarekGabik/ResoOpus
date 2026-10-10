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

void GlobalSettings::setInt (const String& key, int value)
{
    file->setValue (key, value);
    file->saveIfNeeded();
    sendChangeMessage();
}
