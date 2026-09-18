#include "Settings.hpp"

#include <cstdlib>
#include <cstring>
#include <string>
#include <SDL3/SDL.h>

namespace
{
    ClientSettings gSettings;
    std::string gSettingsPath;

    std::string SettingsPath()
    {
        char* pref = SDL_GetPrefPath("", "SonyHeadphonesClient");
        if (!pref)
            return {};
        std::string path = pref;
        SDL_free(pref);
        return path + "settings.ini";
    }

    void Apply(const std::string& key, const std::string& value)
    {
        auto asBool = [&] { return value == "1" || value == "true"; };
        if (key == "last_device_address") gSettings.lastDeviceAddress = value;
        else if (key == "last_device_name") gSettings.lastDeviceName = value;
        else if (key == "last_device_protocol") gSettings.lastDeviceProtocol = std::atoi(value.c_str());
        else if (key == "last_device_ble") gSettings.lastDeviceBLE = asBool();
        else if (key == "close_to_tray") gSettings.closeToTray = asBool();
        else if (key == "animations") gSettings.animations = asBool();
        else if (key == "notifications") gSettings.notifications = asBool();
        else if (key == "auto_start") gSettings.autoStart = asBool();
        else if (key == "tray_hint_shown") gSettings.trayHintShown = asBool();
        else if (key == "language") gSettings.language = std::atoi(value.c_str());
    }
}

ClientSettings& clientSettings()
{
    return gSettings;
}

void clientSettingsLoad()
{
    gSettings = {};
    gSettingsPath = SettingsPath();
    if (gSettingsPath.empty())
        return;
    size_t size = 0;
    void* data = SDL_LoadFile(gSettingsPath.c_str(), &size);
    if (!data)
        return;
    const std::string text(static_cast<const char*>(data), size);
    SDL_free(data);
    size_t pos = 0;
    while (pos < text.size())
    {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos)
            end = text.size();
        std::string line = text.substr(pos, end - pos);
        pos = end + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
            line.pop_back();
        if (line.empty() || line[0] == '#')
            continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        Apply(line.substr(0, eq), line.substr(eq + 1));
    }
}

void clientSettingsSave()
{
    if (gSettingsPath.empty())
        gSettingsPath = SettingsPath();
    if (gSettingsPath.empty())
        return;
    std::string out;
    out += "# SonyHeadphonesClient settings\n";
    out += "last_device_address=" + gSettings.lastDeviceAddress + "\n";
    out += "last_device_name=" + gSettings.lastDeviceName + "\n";
    out += "last_device_protocol=" + std::to_string(gSettings.lastDeviceProtocol) + "\n";
    out += std::string("last_device_ble=") + (gSettings.lastDeviceBLE ? "1" : "0") + "\n";
    out += std::string("close_to_tray=") + (gSettings.closeToTray ? "1" : "0") + "\n";
    out += std::string("auto_start=") + (gSettings.autoStart ? "1" : "0") + "\n";
    out += std::string("tray_hint_shown=") + (gSettings.trayHintShown ? "1" : "0") + "\n";
    out += std::string("animations=") + (gSettings.animations ? "1" : "0") + "\n";
    out += std::string("notifications=") + (gSettings.notifications ? "1" : "0") + "\n";
    out += "language=" + std::to_string(gSettings.language) + "\n";
    if (!SDL_SaveFile(gSettingsPath.c_str(), out.data(), out.size()))
        SDL_Log("Unable to save settings to %s: %s", gSettingsPath.c_str(), SDL_GetError());
}
