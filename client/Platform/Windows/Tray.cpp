// Windows system tray (notification area) integration.
//
// Shows a headphone glyph whose fill reflects the battery tier of the connected headphones and a right-click
// menu to switch the ambient sound mode. Everything here runs on the UI thread:
// the hidden window's messages are dispatched by SDL's own event pump, and user
// actions are queued for the client to pick up with clientPlatformTrayPollEvent.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <objidl.h>
#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstring>
#include <cwchar>
using std::max;
using std::min;
#include <gdiplus.h>

#include "../Platform.hpp"

namespace
{
    constexpr UINT kTrayCallback = WM_APP + 1;
    constexpr UINT kTrayId = 1;
    constexpr UINT kMenuNoiseCancelling = 1001;
    constexpr UINT kMenuAmbientSound = 1002;
    constexpr UINT kMenuOff = 1003;
    constexpr UINT kMenuShowWindow = 1010;
    constexpr UINT kMenuExit = 1011;
    constexpr wchar_t kWindowClass[] = L"SonyHeadphonesClientTray";

    HWND gTrayWnd = nullptr;
    HICON gTrayIcon = nullptr;
    UINT gTaskbarCreatedMsg = 0;
    bool gIconAdded = false;
    bool gLightTheme = false;

    // Cached status, compared on every update so the shell is only touched on changes.
    struct TrayStatus
    {
        bool connected = false;
        int batteryPercent = -1;
        bool charging = false;
        int noiseMode = CLIENT_TRAY_NOISE_UNAVAILABLE;
        bool noiseCancellingAvailable = false;
        bool ambientSoundAvailable = false;
        wchar_t deviceName[128] = L"";
        wchar_t textNotConnected[64] = L"Not connected";
        wchar_t textNoiseCancelling[64] = L"&Noise Cancelling";
        wchar_t textAmbientSound[64] = L"&Ambient Sound";
        wchar_t textOff[64] = L"&Off";
        wchar_t textShowWindow[64] = L"&Show Window";
        wchar_t textExit[64] = L"E&xit";
        wchar_t textCharging[64] = L"charging";
    } gStatus;

    // UTF-8 -> UTF-16 into a fixed buffer; keeps the English default when `utf8` is NULL.
    void CopyLabel(wchar_t* out, size_t capacity, const char* utf8, const wchar_t* fallback)
    {
        if (!utf8 || !*utf8)
        {
            wcsncpy_s(out, capacity, fallback, _TRUNCATE);
            return;
        }
        if (MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out, static_cast<int>(capacity)) <= 0)
            wcsncpy_s(out, capacity, fallback, _TRUNCATE);
    }
    // The status the current icon / tooltip were built from.
    int gIconPercent = INT_MIN;
    bool gIconConnected = false;
    bool gIconCharging = false;
    bool gIconLightTheme = false;
    wchar_t gTip[128] = L"";

    constexpr int kEventCapacity = 8;
    ClientTrayEvent gEvents[kEventCapacity];
    int gEventHead = 0;
    int gEventCount = 0;

    void PushEvent(int action, int noiseMode = CLIENT_TRAY_NOISE_UNAVAILABLE)
    {
        if (gEventCount == kEventCapacity)
            return; // Drop: the client polls every frame, this never realistically fills up.
        ClientTrayEvent& event = gEvents[(gEventHead + gEventCount) % kEventCapacity];
        event.action = action;
        event.noiseMode = noiseMode;
        gEventCount++;
    }

    bool SystemUsesLightTheme()
    {
        DWORD value = 0;
        DWORD size = sizeof(value);
        if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                         L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &size) == ERROR_SUCCESS)
            return value != 0;
        return false;
    }

    int TrayIconSize()
    {
        const UINT dpi = GetDpiForSystem();
        int size = GetSystemMetricsForDpi(SM_CXSMICON, dpi);
        return size > 0 ? size : 16;
    }

    ULONG_PTR gGdiplusToken = 0;

    // Battery tiers shown by the headphone glyph. A percentage maps to the first entry whose
    // upper bound it does not exceed; the last entry is the full glyph. Edit here to retune.
    struct BatteryTier
    {
        int maxPercent;
        float fill; // Fraction of the glyph height that is drawn solid, from the bottom.
    };
    constexpr BatteryTier kBatteryTiers[] = {
        {20, 0.25f},
        {40, 0.50f},
        {60, 0.75f},
        {100, 1.00f},
    };

    float TierFill(int percent)
    {
        for (const BatteryTier& tier : kBatteryTiers)
            if (percent <= tier.maxPercent)
                return tier.fill;
        return 1.0f;
    }

    // Headphones: a thick headband arc plus two rounded ear cups, in a unit square.
    void AddHeadphonesPath(Gdiplus::GraphicsPath& path, float scale)
    {
        const float cupWidth = 0.28f * scale, cupHeight = 0.42f * scale, cupRadius = 0.09f * scale;
        const float cupTop = 0.50f * scale;
        auto addCup = [&](float left)
        {
            Gdiplus::GraphicsPath cup;
            const float d = cupRadius * 2.0f;
            cup.AddArc(left, cupTop, d, d, 180.0f, 90.0f);
            cup.AddArc(left + cupWidth - d, cupTop, d, d, 270.0f, 90.0f);
            cup.AddArc(left + cupWidth - d, cupTop + cupHeight - d, d, d, 0.0f, 90.0f);
            cup.AddArc(left, cupTop + cupHeight - d, d, d, 90.0f, 90.0f);
            cup.CloseFigure();
            path.AddPath(&cup, FALSE);
        };
        addCup(0.06f * scale);
        addCup((1.0f - 0.06f - 0.28f) * scale);

        // Headband: outer arc minus inner arc so it becomes a thick stroke.
        Gdiplus::GraphicsPath band;
        const float outerL = 0.10f * scale, outerT = 0.06f * scale, outerW = 0.80f * scale, outerH = 1.05f * scale;
        const float thick = 0.13f * scale;
        band.AddArc(outerL, outerT, outerW, outerH, 180.0f, 180.0f);
        band.AddArc(outerL + thick, outerT + thick, outerW - 2 * thick, outerH - 2 * thick, 0.0f, -180.0f);
        band.CloseFigure();
        path.AddPath(&band, FALSE);
    }

    // Renders the headphone glyph in `color`. The bottom `fill` fraction of the icon is solid and
    // the remainder is a translucent silhouette, so the shape reads as headphones at every tier.
    void RenderHeadphones(Gdiplus::Bitmap& bitmap, float fill, COLORREF color)
    {
        const int size = static_cast<int>(bitmap.GetWidth());
        Gdiplus::Graphics graphics(&bitmap);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        graphics.Clear(Gdiplus::Color(0, 0, 0, 0));

        Gdiplus::GraphicsPath glyph(Gdiplus::FillModeWinding);
        AddHeadphonesPath(glyph, static_cast<float>(size));
        const BYTE r = GetRValue(color), g = GetGValue(color), b = GetBValue(color);

        Gdiplus::SolidBrush ghost(Gdiplus::Color(110, r, g, b));
        graphics.FillPath(&ghost, &glyph);
        const float top = size * (1.0f - std::clamp(fill, 0.0f, 1.0f));
        graphics.SetClip(Gdiplus::RectF(0.0f, top, static_cast<float>(size), size - top));
        Gdiplus::SolidBrush solid(Gdiplus::Color(255, r, g, b));
        graphics.FillPath(&solid, &glyph);
        graphics.ResetClip();
    }

    HICON CreateHeadphonesIcon(float fill, COLORREF color, int size)
    {
        if (!gGdiplusToken)
            return nullptr;
        Gdiplus::Bitmap bitmap(size, size, PixelFormat32bppPARGB);
        RenderHeadphones(bitmap, fill, color);
        HICON icon = nullptr;
        if (bitmap.GetHICON(&icon) != Gdiplus::Ok)
            return nullptr;
        return icon;
    }

    HICON BuildStatusIcon()
    {
        const int size = TrayIconSize();
        if (!gStatus.connected)
            return CreateHeadphonesIcon(1.0f, RGB(150, 150, 150), size); // solid grey: no device
        COLORREF color = gLightTheme ? RGB(32, 32, 32) : RGB(255, 255, 255);
        if (gStatus.batteryPercent >= 0 && gStatus.batteryPercent <= kBatteryTiers[0].maxPercent)
            color = RGB(232, 72, 72);
        else if (gStatus.charging)
            color = RGB(80, 200, 120);
        const float fill = gStatus.batteryPercent < 0 ? 0.0f : TierFill(gStatus.batteryPercent);
        return CreateHeadphonesIcon(fill, color, size);
    }

    void BuildTip(wchar_t* tip, size_t capacity)
    {
        if (!gStatus.connected)
        {
            std::swprintf(tip, capacity, L"SonyHeadphonesClient \u2014 %s", gStatus.textNotConnected);
            return;
        }
        const wchar_t* name = gStatus.deviceName[0] ? gStatus.deviceName : L"Headphones";
        if (gStatus.batteryPercent < 0)
            std::swprintf(tip, capacity, L"%s", name);
        else
            std::swprintf(tip, capacity, L"%s \u2014 %d%%%s%s%s", name, gStatus.batteryPercent,
                          gStatus.charging ? L" (" : L"", gStatus.charging ? gStatus.textCharging : L"",
                          gStatus.charging ? L")" : L"");
    }

    NOTIFYICONDATAW MakeNotifyData()
    {
        NOTIFYICONDATAW data{};
        data.cbSize = sizeof(data);
        data.hWnd = gTrayWnd;
        data.uID = kTrayId;
        return data;
    }

    void AddIcon()
    {
        if (!gTrayIcon)
            gTrayIcon = BuildStatusIcon();
        BuildTip(gTip, sizeof(gTip) / sizeof(gTip[0]));
        NOTIFYICONDATAW data = MakeNotifyData();
        data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
        data.uCallbackMessage = kTrayCallback;
        data.hIcon = gTrayIcon;
        std::wcsncpy(data.szTip, gTip, sizeof(data.szTip) / sizeof(data.szTip[0]) - 1);
        if (!Shell_NotifyIconW(NIM_ADD, &data))
            return;
        data.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &data);
        gIconAdded = true;
        gIconPercent = gStatus.batteryPercent;
        gIconConnected = gStatus.connected;
        gIconCharging = gStatus.charging;
        gIconLightTheme = gLightTheme;
    }

    void RefreshIcon()
    {
        if (!gIconAdded)
        {
            // NIM_ADD can fail while the shell is starting; retry occasionally instead of every frame.
            static ULONGLONG lastAttempt = 0;
            const ULONGLONG now = GetTickCount64();
            if (now - lastAttempt < 5000)
                return;
            lastAttempt = now;
            AddIcon();
            return;
        }
        const bool iconDirty = gIconPercent != gStatus.batteryPercent || gIconConnected != gStatus.connected ||
            gIconCharging != gStatus.charging || gIconLightTheme != gLightTheme || !gTrayIcon;
        wchar_t tip[128];
        BuildTip(tip, sizeof(tip) / sizeof(tip[0]));
        const bool tipDirty = std::wcscmp(tip, gTip) != 0;
        if (!iconDirty && !tipDirty)
            return;

        NOTIFYICONDATAW data = MakeNotifyData();
        HICON previous = gTrayIcon;
        if (iconDirty)
        {
            gTrayIcon = BuildStatusIcon();
            data.uFlags |= NIF_ICON;
            data.hIcon = gTrayIcon;
        }
        if (tipDirty)
        {
            std::wcscpy(gTip, tip);
            data.uFlags |= NIF_TIP | NIF_SHOWTIP;
            std::wcsncpy(data.szTip, gTip, sizeof(data.szTip) / sizeof(data.szTip[0]) - 1);
        }
        Shell_NotifyIconW(NIM_MODIFY, &data);
        if (iconDirty)
        {
            if (previous)
                DestroyIcon(previous);
            gIconPercent = gStatus.batteryPercent;
            gIconConnected = gStatus.connected;
            gIconCharging = gStatus.charging;
            gIconLightTheme = gLightTheme;
        }
    }

    void ShowMenu(int x, int y)
    {
        HMENU menu = CreatePopupMenu();
        if (!menu)
            return;

        wchar_t header[192];
        if (!gStatus.connected)
            std::wcscpy(header, gStatus.textNotConnected);
        else if (gStatus.batteryPercent < 0)
            std::swprintf(header, 192, L"%s", gStatus.deviceName[0] ? gStatus.deviceName : L"Headphones");
        else
            std::swprintf(header, 192, L"%s  \u2014  %d%%", gStatus.deviceName[0] ? gStatus.deviceName : L"Headphones",
                          gStatus.batteryPercent);
        AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, header);
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

        const bool noiseUsable = gStatus.connected && gStatus.noiseMode != CLIENT_TRAY_NOISE_UNAVAILABLE;
        const UINT grayed = MF_STRING | MF_GRAYED;
        AppendMenuW(menu, noiseUsable && gStatus.noiseCancellingAvailable ? MF_STRING : grayed, kMenuNoiseCancelling,
                    gStatus.textNoiseCancelling);
        AppendMenuW(menu, noiseUsable && gStatus.ambientSoundAvailable ? MF_STRING : grayed, kMenuAmbientSound,
                    gStatus.textAmbientSound);
        AppendMenuW(menu, noiseUsable ? MF_STRING : grayed, kMenuOff, gStatus.textOff);
        if (noiseUsable)
        {
            UINT checked = kMenuOff;
            if (gStatus.noiseMode == CLIENT_TRAY_NOISE_CANCELLING)
                checked = kMenuNoiseCancelling;
            else if (gStatus.noiseMode == CLIENT_TRAY_NOISE_AMBIENT)
                checked = kMenuAmbientSound;
            CheckMenuRadioItem(menu, kMenuNoiseCancelling, kMenuOff, checked, MF_BYCOMMAND);
        }
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, kMenuShowWindow, gStatus.textShowWindow);
        AppendMenuW(menu, MF_STRING, kMenuExit, gStatus.textExit);

        // Required so the menu closes when the user clicks elsewhere (KB Q135788).
        SetForegroundWindow(gTrayWnd);
        const UINT command = static_cast<UINT>(TrackPopupMenuEx(
            menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_LEFTALIGN | TPM_BOTTOMALIGN, x, y, gTrayWnd, nullptr));
        PostMessageW(gTrayWnd, WM_NULL, 0, 0);
        DestroyMenu(menu);

        switch (command)
        {
        case kMenuNoiseCancelling:
            PushEvent(CLIENT_TRAY_ACTION_SET_NOISE_MODE, CLIENT_TRAY_NOISE_CANCELLING);
            break;
        case kMenuAmbientSound:
            PushEvent(CLIENT_TRAY_ACTION_SET_NOISE_MODE, CLIENT_TRAY_NOISE_AMBIENT);
            break;
        case kMenuOff:
            PushEvent(CLIENT_TRAY_ACTION_SET_NOISE_MODE, CLIENT_TRAY_NOISE_OFF);
            break;
        case kMenuShowWindow:
            PushEvent(CLIENT_TRAY_ACTION_SHOW_WINDOW);
            break;
        case kMenuExit:
            PushEvent(CLIENT_TRAY_ACTION_EXIT);
            break;
        default:
            break;
        }
    }

    LRESULT CALLBACK TrayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (msg == kTrayCallback)
        {
            switch (LOWORD(lParam))
            {
            case WM_CONTEXTMENU:
                ShowMenu(GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam));
                break;
            case NIN_SELECT:
            case NIN_KEYSELECT:
            case WM_LBUTTONDBLCLK:
                PushEvent(CLIENT_TRAY_ACTION_SHOW_WINDOW);
                break;
            default:
                break;
            }
            return 0;
        }
        if (gTaskbarCreatedMsg && msg == gTaskbarCreatedMsg)
        {
            // Explorer restarted: the icon is gone, add it again.
            gIconAdded = false;
            AddIcon();
            return 0;
        }
        if (msg == WM_SETTINGCHANGE)
        {
            gLightTheme = SystemUsesLightTheme();
            RefreshIcon();
            return 0;
        }
        if (msg == WM_CLOSE)
        {
            // Lets tooling (or a future updater) ask the app to quit cleanly instead of killing it,
            // which would leave the headphones holding a half-open MDR session for a while.
            PushEvent(CLIENT_TRAY_ACTION_EXIT);
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

extern "C" {
int clientPlatformTrayInit(void)
{
    if (gTrayWnd)
        return 1;
    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = TrayWndProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = kWindowClass;
    if (!RegisterClassW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return 0;
    Gdiplus::GdiplusStartupInput startupInput;
    if (Gdiplus::GdiplusStartup(&gGdiplusToken, &startupInput, nullptr) != Gdiplus::Ok)
        gGdiplusToken = 0;
    gTaskbarCreatedMsg = RegisterWindowMessageW(L"TaskbarCreated");
    // A hidden top-level window (not HWND_MESSAGE) so it still receives broadcasts
    // such as TaskbarCreated and WM_SETTINGCHANGE.
    gTrayWnd = CreateWindowExW(WS_EX_TOOLWINDOW, kWindowClass, L"SonyHeadphonesClient", WS_OVERLAPPED, 0, 0, 0, 0,
                               nullptr, nullptr, instance, nullptr);
    if (!gTrayWnd)
        return 0;
    gLightTheme = SystemUsesLightTheme();
    gStatus = {};
    AddIcon();
    return gIconAdded ? 1 : 0;
}

void clientPlatformTrayUpdate(const ClientTrayStatus* status)
{
    if (!gTrayWnd || !status)
        return;
    gStatus.connected = status->connected != 0;
    gStatus.batteryPercent = status->connected ? status->batteryPercent : -1;
    gStatus.charging = status->connected && status->charging != 0;
    gStatus.noiseMode = status->connected ? status->noiseMode : CLIENT_TRAY_NOISE_UNAVAILABLE;
    gStatus.noiseCancellingAvailable = status->noiseCancellingAvailable != 0;
    gStatus.ambientSoundAvailable = status->ambientSoundAvailable != 0;
    // Menu labels: keep an accelerator so keyboard users can still pick items.
    auto label = [](wchar_t* out, size_t cap, const char* utf8, const wchar_t* fallback, const wchar_t* accel)
    {
        CopyLabel(out, cap, utf8, fallback);
        if (utf8 && *utf8 && !wcschr(out, L'&') && accel)
            wcsncat_s(out, cap, accel, _TRUNCATE); // e.g. "降噪 (&N)"
    };
    CopyLabel(gStatus.textNotConnected, 64, status->textNotConnected, L"Not connected");
    label(gStatus.textNoiseCancelling, 64, status->textNoiseCancelling, L"&Noise Cancelling", L" (&N)");
    label(gStatus.textAmbientSound, 64, status->textAmbientSound, L"&Ambient Sound", L" (&A)");
    label(gStatus.textOff, 64, status->textOff, L"&Off", L" (&O)");
    label(gStatus.textShowWindow, 64, status->textShowWindow, L"&Show Window", L" (&S)");
    label(gStatus.textExit, 64, status->textExit, L"E&xit", L" (&X)");
    CopyLabel(gStatus.textCharging, 64, status->textCharging, L"charging");
    if (status->deviceName && status->deviceName[0])
    {
        const int written = MultiByteToWideChar(CP_UTF8, 0, status->deviceName, -1, gStatus.deviceName,
                                                static_cast<int>(sizeof(gStatus.deviceName) / sizeof(wchar_t)) - 1);
        if (written <= 0)
            gStatus.deviceName[0] = L'\0';
        gStatus.deviceName[sizeof(gStatus.deviceName) / sizeof(wchar_t) - 1] = L'\0';
    }
    else
    {
        gStatus.deviceName[0] = L'\0';
    }
    RefreshIcon();
}

void clientPlatformTrayNotify(const char* title, const char* message)
{
    if (!gTrayWnd || !gIconAdded)
        return;
    NOTIFYICONDATAW data = MakeNotifyData();
    data.uFlags = NIF_INFO;
    data.dwInfoFlags = NIIF_INFO | NIIF_RESPECT_QUIET_TIME;
    if (title)
        MultiByteToWideChar(CP_UTF8, 0, title, -1, data.szInfoTitle, sizeof(data.szInfoTitle) / sizeof(wchar_t));
    if (message)
        MultiByteToWideChar(CP_UTF8, 0, message, -1, data.szInfo, sizeof(data.szInfo) / sizeof(wchar_t));
    Shell_NotifyIconW(NIM_MODIFY, &data);
}

int clientPlatformTrayPollEvent(ClientTrayEvent* outEvent)
{
    if (!outEvent || gEventCount == 0)
        return 0;
    *outEvent = gEvents[gEventHead];
    gEventHead = (gEventHead + 1) % kEventCapacity;
    gEventCount--;
    return 1;
}

void clientPlatformTrayDestroy(void)
{
    if (gIconAdded)
    {
        NOTIFYICONDATAW data = MakeNotifyData();
        Shell_NotifyIconW(NIM_DELETE, &data);
        gIconAdded = false;
    }
    if (gTrayIcon)
    {
        DestroyIcon(gTrayIcon);
        gTrayIcon = nullptr;
    }
    if (gTrayWnd)
    {
        DestroyWindow(gTrayWnd);
        gTrayWnd = nullptr;
    }
    UnregisterClassW(kWindowClass, GetModuleHandleW(nullptr));
    gEventHead = gEventCount = 0;
    if (gGdiplusToken)
    {
        Gdiplus::GdiplusShutdown(gGdiplusToken);
        gGdiplusToken = 0;
    }
}
}
