#include <Windows.h>
#include <new>
#include <mdr/Protocol.hpp>
#include <mdr-bt/ConnectionWindows.h>
#include "../Platform.hpp"

extern "C" {
// Loads a system font with CJK coverage so ImGui can render non-Latin text (track titles,
// device names). The buffer lives for the process lifetime; ImGui merges it with
// FontDataOwnedByAtlas = false and pulls glyphs on demand (dynamic font loading, ImGui >= 1.92).
int clientPlatformLocateFontBinary(const char** outData)
{
    static char* fontData = nullptr;
    static int fontSize = 0;
    static bool attempted = false;
    if (!attempted)
    {
        attempted = true;
        wchar_t fontsDir[MAX_PATH];
        const UINT length = GetWindowsDirectoryW(fontsDir, MAX_PATH);
        if (length > 0 && length < MAX_PATH)
        {
            // Preference order: Traditional Chinese, Simplified Chinese, Japanese, Korean.
            // Each of these also covers the CJK Unified Ideographs, so the first present wins.
            static const wchar_t* const kCandidates[] = {
                L"\\Fonts\\msjh.ttc",   // Microsoft JhengHei
                L"\\Fonts\\msyh.ttc",   // Microsoft YaHei
                L"\\Fonts\\meiryo.ttc", // Meiryo
                L"\\Fonts\\malgun.ttf", // Malgun Gothic
            };
            for (const wchar_t* candidate : kCandidates)
            {
                wchar_t path[MAX_PATH];
                wcscpy_s(path, fontsDir);
                wcscat_s(path, candidate);
                HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                          FILE_ATTRIBUTE_NORMAL, nullptr);
                if (file == INVALID_HANDLE_VALUE)
                    continue;
                LARGE_INTEGER size{};
                if (GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart < (1ll << 30))
                {
                    char* buffer = new (std::nothrow) char[static_cast<size_t>(size.QuadPart)];
                    DWORD read = 0;
                    if (buffer && ReadFile(file, buffer, static_cast<DWORD>(size.QuadPart), &read, nullptr) &&
                        read == static_cast<DWORD>(size.QuadPart))
                    {
                        fontData = buffer;
                        fontSize = static_cast<int>(size.QuadPart);
                    }
                    else
                        delete[] buffer;
                }
                CloseHandle(file);
                if (fontData)
                    break;
            }
        }
    }
    *outData = fontData;
    return fontSize;
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

void clientPlatformDestroy()
{
    clientPlatformTrayDestroy();
    clientPlatformConnectionDestroy();
}
}
