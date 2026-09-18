#pragma once
#include <string>

/**
 * Persistent client preferences. Stored as a small key=value file in SDL's preference
 * directory (e.g. %APPDATA%\SonyHeadphonesClient\settings.ini on Windows).
 */
struct ClientSettings
{
    // Last device that connected successfully; used for auto-connect.
    std::string lastDeviceAddress;
    std::string lastDeviceName;
    int lastDeviceProtocol = 0; // 0 unknown, 1 = V1, 2 = V2
    bool lastDeviceBLE = false;
    // Behaviour
    bool closeToTray = true;
    bool autoStart = false;
    bool trayHintShown = false;
    int language = 0; // ClientLanguage: 0 auto, 1 English, 2 Traditional Chinese
};

ClientSettings& clientSettings();
void clientSettingsLoad();
void clientSettingsSave();
