#include <Windows.h>
#include <new>
#include <mdr/Protocol.hpp>
#include <mdr-bt/ConnectionWindows.h>
#include "../Platform.hpp"

extern "C" {
// Reads the first existing file among `candidates` (relative to the Windows directory) into a
// heap buffer that lives for the process lifetime. ImGui merges these fonts with
// FontDataOwnedByAtlas = false and pulls glyphs on demand (dynamic font loading, ImGui >= 1.92).
static int LoadFirstSystemFont(const wchar_t* const* candidates, size_t count, const char** outData)
{
    *outData = nullptr;
    wchar_t fontsDir[MAX_PATH];
    const UINT length = GetWindowsDirectoryW(fontsDir, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
        return 0;
    for (size_t i = 0; i < count; ++i)
    {
        wchar_t path[MAX_PATH];
        wcscpy_s(path, fontsDir);
        wcscat_s(path, candidates[i]);
        HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            continue;
        int result = 0;
        LARGE_INTEGER size{};
        if (GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart < (1ll << 30))
        {
            char* buffer = new (std::nothrow) char[static_cast<size_t>(size.QuadPart)];
            DWORD read = 0;
            if (buffer && ReadFile(file, buffer, static_cast<DWORD>(size.QuadPart), &read, nullptr) &&
                read == static_cast<DWORD>(size.QuadPart))
            {
                *outData = buffer;
                result = static_cast<int>(size.QuadPart);
            }
            else
                delete[] buffer;
        }
        CloseHandle(file);
        if (result)
            return result;
    }
    return 0;
}

// CJK coverage for track titles and device names.
int clientPlatformLocateFontBinary(const char** outData)
{
    // Preference order: Traditional Chinese, Simplified Chinese, Japanese, Korean.
    // Each of these also covers the CJK Unified Ideographs, so the first present wins.
    // Bold faces first: the bundled Latin font is a medium weight, and regular CJK strokes
    // look thin and washed out next to it.
    static const wchar_t* const kCandidates[] = {
        L"\\Fonts\\msjhbd.ttc",   // Microsoft JhengHei Bold
        L"\\Fonts\\msjh.ttc",     // Microsoft JhengHei
        L"\\Fonts\\msyhbd.ttc",   // Microsoft YaHei Bold
        L"\\Fonts\\msyh.ttc",     // Microsoft YaHei
        L"\\Fonts\\meiryob.ttc",  // Meiryo Bold
        L"\\Fonts\\meiryo.ttc",   // Meiryo
        L"\\Fonts\\malgunbd.ttf", // Malgun Gothic Bold
        L"\\Fonts\\malgun.ttf",   // Malgun Gothic
    };
    static const char* data = nullptr;
    static int size = -1;
    if (size < 0)
        size = LoadFirstSystemFont(kCandidates, sizeof(kCandidates) / sizeof(kCandidates[0]), &data);
    *outData = data;
    return size;
}

// Latin Extended / Greek / Cyrillic: Segoe UI. Semibold matches the bundled font's weight.
int clientPlatformLocateLatinFontBinary(const char** outData)
{
    static const wchar_t* const kCandidates[] = {
        L"\\Fonts\\seguisb.ttf",  // Segoe UI Semibold
        L"\\Fonts\\segoeuib.ttf", // Segoe UI Bold
        L"\\Fonts\\segoeui.ttf",  // Segoe UI
    };
    static const char* data = nullptr;
    static int size = -1;
    if (size < 0)
        size = LoadFirstSystemFont(kCandidates, sizeof(kCandidates) / sizeof(kCandidates[0]), &data);
    *outData = data;
    return size;
}

// Emoji: Segoe UI Emoji ships monochrome outlines alongside its colour layers, which is what
// ImGui's rasterizer can use. Segoe UI Symbol is the pre-Windows 8.1 fallback.
int clientPlatformLocateEmojiFontBinary(const char** outData)
{
    static const wchar_t* const kCandidates[] = {
        L"\\Fonts\\seguiemj.ttf",
        L"\\Fonts\\seguisym.ttf",
    };
    static const char* data = nullptr;
    static int size = -1;
    if (size < 0)
        size = LoadFirstSystemFont(kCandidates, sizeof(kCandidates) / sizeof(kCandidates[0]), &data);
    *outData = data;
    return size;
}

static MDRConnectionWindows* gConnClassic = nullptr;
#ifdef MDR_BLE
static MDRConnectionWindowsBLE* gConnBLE = nullptr;
#endif

int clientPlatformConnectionInit(int flags)
{
    if (gConnClassic != nullptr
#ifdef MDR_BLE
        || gConnBLE != nullptr
#endif
    )
        return MDR_RESULT_ERROR_GENERAL;

    if (flags & MDR_INIT_BT_BLE) {
#ifdef MDR_BLE
        gConnBLE = mdrConnectionWindowsBLECreate();
        gConnClassic = nullptr;
#else
        return MDR_RESULT_ERROR_NOT_SUPPORTED;
#endif
    } else {
        gConnClassic = mdrConnectionWindowsCreate();
#ifdef MDR_BLE
        gConnBLE = nullptr;
#endif
    }
    return MDR_RESULT_OK;
}

void clientPlatformConnectionDestroy()
{
#ifdef MDR_BLE
    if (gConnBLE) { mdrConnectionWindowsBLEDestroy(gConnBLE); gConnBLE = nullptr; }
#endif
    if (gConnClassic) { mdrConnectionWindowsDestroy(gConnClassic); gConnClassic = nullptr; }
}

MDRConnection* clientPlatformConnectionGet()
{
    if (gConnClassic != nullptr)
        return mdrConnectionWindowsGet(gConnClassic);
#ifdef MDR_BLE
    if (gConnBLE != nullptr)
        return mdrConnectionWindowsBLEGet(gConnBLE);
#endif
    [[unlikely]] return nullptr;
}

/* ---- Launch at login: HKCU\Software\Microsoft\Windows\CurrentVersion\Run ---- */
static const wchar_t* const kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t* const kRunValue = L"SonyHeadphonesClient";

int clientPlatformAutoStartSupported(void)
{
    return 1;
}

int clientPlatformAutoStartGet(void)
{
    DWORD size = 0;
    return RegGetValueW(HKEY_CURRENT_USER, kRunKey, kRunValue, RRF_RT_REG_SZ, nullptr, nullptr, &size) ==
        ERROR_SUCCESS;
}

int clientPlatformAutoStartSet(int enabled)
{
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) !=
        ERROR_SUCCESS)
        return 0;
    LSTATUS status;
    if (enabled)
    {
        wchar_t exe[MAX_PATH];
        const DWORD length = GetModuleFileNameW(nullptr, exe, MAX_PATH);
        if (length == 0 || length >= MAX_PATH)
        {
            RegCloseKey(key);
            return 0;
        }
        wchar_t command[MAX_PATH + 32];
        wcscpy_s(command, L"\"");
        wcscat_s(command, exe);
        wcscat_s(command, L"\" --minimized");
        status = RegSetValueExW(key, kRunValue, 0, REG_SZ, reinterpret_cast<const BYTE*>(command),
                                static_cast<DWORD>((wcslen(command) + 1) * sizeof(wchar_t)));
    }
    else
    {
        status = RegDeleteValueW(key, kRunValue);
        if (status == ERROR_FILE_NOT_FOUND)
            status = ERROR_SUCCESS;
    }
    RegCloseKey(key);
    return status == ERROR_SUCCESS;
}

void clientPlatformDestroy()
{
    clientPlatformTrayDestroy();
    clientPlatformConnectionDestroy();
}
}
