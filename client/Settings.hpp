#pragma once
#include <string>

/** What the window's close button does. */
enum ClientCloseAction
{
    CLIENT_CLOSE_ASK = 0,      // Prompt, with an option to remember the answer
    CLIENT_CLOSE_MINIMIZE = 1, // Hide to the system tray, keep running
    CLIENT_CLOSE_EXIT = 2,     // Quit the app
};

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
    int closeAction = CLIENT_CLOSE_ASK;
    bool autoStart = false;
    bool animations = true;
    bool notifications = true;
    bool trayHintShown = false;
    int language = 0; // ClientLanguage: 0 auto, 1 English, 2 Traditional Chinese
};

ClientSettings& clientSettings();
void clientSettingsLoad();
void clientSettingsSave();
