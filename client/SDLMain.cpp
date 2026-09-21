// SDL_Renderer backend from https://github.com/ocornut/imgui/blob/master/examples/example_sdl3_sdlrenderer3
#include <cstdio>
#include <cstring>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>
#endif

#include <mdr/Protocol.hpp>
#include <imgui.h>
#ifdef IMGUI_ENABLE_FREETYPE
#include <misc/freetype/imgui_freetype.h>
#endif
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_main.h>

#include "Platform/Platform.hpp"
#include "PayloadRecorder.hpp"
#include "Settings.hpp"
#include "Localization.hpp"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "Fonts/PlexSansIcon.h"
#include "MaterialYouTheme.hpp"
#ifdef MDR_CLIENT_DEBUGGER
#include "Debugger.hpp"
#endif
// Implemented by Client.cpp
extern bool clientShouldExit();
extern void clientShutdown();
#ifdef MDR_CLIENT_DEBUGGER
extern void clientEnterDebuggerReplayMode();
#endif

bool gShouldClose = false;
bool gTrayAvailable = false;
// Set when the close button needs an answer; Client.cpp draws the prompt in the app's own style.
bool gCloseAskPending = false;

SDL_Window* gWindow = nullptr;
SDL_Renderer* gRenderer = nullptr;

void clientHideToTray()
{
    SDL_HideWindow(gWindow);
    ClientSettings& settings = clientSettings();
    if (!settings.trayHintShown)
    {
        clientPlatformTrayNotify("SonyHeadphonesClient",
                                 tr("Still running in the system tray. Left-click the icon to reopen, right-click for options."));
        settings.trayHintShown = true;
        clientSettingsSave();
    }
}

// Close button: minimize to the tray, quit, or ask - whichever the settings say.
static void HandleCloseRequested()
{
    ClientSettings& settings = clientSettings();
    if (!gTrayAvailable) // Nowhere to minimize to
    {
        gShouldClose = true;
        return;
    }
    if (settings.closeAction == CLIENT_CLOSE_MINIMIZE)
    {
        clientHideToTray();
        return;
    }
    if (settings.closeAction == CLIENT_CLOSE_EXIT)
    {
        gShouldClose = true;
        return;
    }
    gCloseAskPending = true; // Answered by the in-app prompt
}

#ifdef _WIN32
static HWND NativeWindowHandle()
{
    return static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(gWindow),
                                                    SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
}

// Extending DWM glass resurrects the native caption buttons on SDL's borderless window, and our
// own chrome already provides those actions. SDL puts WS_SYSMENU back whenever it re-applies the
// window style - showing the window again after the tray hides it, restoring, maximizing - so this
// is checked every frame rather than only at startup, and costs one style read when nothing changed.
static void SuppressNativeCaption()
{
    HWND hwnd = NativeWindowHandle();
    if (!hwnd)
        return;
    const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    if (!(style & WS_SYSMENU))
        return;
    SetWindowLongPtrW(hwnd, GWL_STYLE, style & ~WS_SYSMENU);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

static bool ConfigureGlassWindow()
{
    HWND hwnd = NativeWindowHandle();
    if (!hwnd) return false;
    SuppressNativeCaption();
    // Numeric attribute values keep compilation compatible with older Windows SDKs.
    const DWORD round = 2; // DWMWCP_ROUND
    DwmSetWindowAttribute(hwnd, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &round, sizeof(round));
    const DWORD acrylic = 3; // DWMSBT_TRANSIENTWINDOW (Windows 11 22H2+)
    const HRESULT result = DwmSetWindowAttribute(hwnd, 38 /* DWMWA_SYSTEMBACKDROP_TYPE */,
                                                &acrylic, sizeof(acrylic));
    if (FAILED(result))
    {
        SDL_Log("Acrylic backdrop unavailable; using opaque surfaces (0x%lx)", static_cast<unsigned long>(result));
        return false;
    }
    const MARGINS margins{-1, -1, -1, -1};
    return SUCCEEDED(DwmExtendFrameIntoClientArea(hwnd, &margins));
}
#endif

// Custom Windows chrome uses SDL hit testing so dragging/resizing remains native.
float clientWindowChromeHeight()
{
#ifdef _WIN32
    return (SDL_GetWindowFlags(gWindow) & SDL_WINDOW_BORDERLESS)
        ? 36.0f * SDL_GetWindowDisplayScale(gWindow) : 0.0f;
#else
    return 0.0f;
#endif
}

#ifdef _WIN32
static int ChromeButton(float x, float y)
{
    int width = 0;
    SDL_GetWindowSize(gWindow, &width, nullptr);
    const float h = clientWindowChromeHeight();
    if (h == 0.0f || y < 5.0f || y >= h || x < width - h * 3 || x >= width - 5.0f)
        return -1;
    return static_cast<int>((x - (width - h * 3)) / h);
}

static SDL_HitTestResult SDLCALL WindowHitTest(SDL_Window* window, const SDL_Point* point, void*)
{
    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    const float edge = 5.0f * SDL_GetWindowDisplayScale(window);
    if (!(SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED))
    {
        const bool left = point->x < edge, right = point->x >= width - edge;
        const bool top = point->y < edge, bottom = point->y >= height - edge;
        if (top) return left ? SDL_HITTEST_RESIZE_TOPLEFT : right ? SDL_HITTEST_RESIZE_TOPRIGHT : SDL_HITTEST_RESIZE_TOP;
        if (bottom) return left ? SDL_HITTEST_RESIZE_BOTTOMLEFT : right ? SDL_HITTEST_RESIZE_BOTTOMRIGHT : SDL_HITTEST_RESIZE_BOTTOM;
        if (left) return SDL_HITTEST_RESIZE_LEFT;
        if (right) return SDL_HITTEST_RESIZE_RIGHT;
    }
    if (point->y < clientWindowChromeHeight() && point->x < width - clientWindowChromeHeight() * 3)
        return SDL_HITTEST_DRAGGABLE;
    return SDL_HITTEST_NORMAL;
}

static void DrawWindowChrome()
{
    const float h = clientWindowChromeHeight();
    if (h == 0.0f) return;
    const float width = ImGui::GetIO().DisplaySize.x;
    auto* draw = ImGui::GetForegroundDrawList();
    ImVec4 background = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
    if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
    {
        const ImVec4 dim = ImGui::GetStyleColorVec4(ImGuiCol_ModalWindowDimBg);
        background.x = background.x * (1.0f - dim.w) + dim.x * dim.w;
        background.y = background.y * (1.0f - dim.w) + dim.y * dim.w;
        background.z = background.z * (1.0f - dim.w) + dim.z * dim.w;
    }
    draw->AddRectFilled({0, 0}, {width, h}, ImGui::ColorConvertFloat4ToU32(background));
    draw->AddText({h * 0.5f, (h - ImGui::GetFontSize()) * 0.5f},
                  ImGui::GetColorU32(ImGuiCol_TextDisabled), "Sony Headphones");
    float mx, my;
    SDL_GetMouseState(&mx, &my);
    const int hovered = (SDL_GetWindowFlags(gWindow) & SDL_WINDOW_MOUSE_FOCUS) ? ChromeButton(mx, my) : -1;
    for (int i = 0; i < 3; ++i)
    {
        const float x = width - h * (3 - i);
        const ImU32 color = hovered == i && i == 2 ? IM_COL32_WHITE : ImGui::GetColorU32(ImGuiCol_Text);
        if (hovered == i)
            draw->AddRectFilled({x + 2, 4}, {x + h - 2, h - 4},
                                i == 2 ? IM_COL32(220, 55, 65, 255) : ImGui::GetColorU32(ImGuiCol_FrameBg), 8);
        const float cx = x + h * 0.5f, cy = h * 0.5f, r = h * 0.13f;
        if (i == 0)
            draw->AddLine({cx - r, cy}, {cx + r, cy}, color, 1.5f);
        else if (i == 1)
        {
            if (SDL_GetWindowFlags(gWindow) & SDL_WINDOW_MAXIMIZED)
                draw->AddRect({cx - r + 3, cy - r - 3}, {cx + r + 3, cy + r - 3}, color);
            draw->AddRect({cx - r, cy - r}, {cx + r, cy + r}, color);
        }
        else
        {
            draw->AddLine({cx - r, cy - r}, {cx + r, cy + r}, color, 1.5f);
            draw->AddLine({cx + r, cy - r}, {cx - r, cy + r}, color, 1.5f);
        }
    }
}
#endif

void mainLoop()
{
    ImGuiIO& io = ImGui::GetIO();
#ifdef _WIN32
    SuppressNativeCaption();
#endif
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
#ifdef _WIN32
        static int pressedChrome = -1;
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.windowID == SDL_GetWindowID(gWindow) && event.button.button == SDL_BUTTON_LEFT)
            pressedChrome = ChromeButton(event.button.x, event.button.y);
        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.windowID == SDL_GetWindowID(gWindow) && event.button.button == SDL_BUTTON_LEFT)
        {
            const int released = ChromeButton(event.button.x, event.button.y);
            if (released >= 0 && released == pressedChrome)
            {
                if (released == 0) SDL_MinimizeWindow(gWindow);
                else if (released == 1)
                {
                    if (SDL_GetWindowFlags(gWindow) & SDL_WINDOW_MAXIMIZED) SDL_RestoreWindow(gWindow);
                    else SDL_MaximizeWindow(gWindow);
                }
                else HandleCloseRequested();
            }
            pressedChrome = -1;
        }
        if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) pressedChrome = -1;
#endif
        ImGui_ImplSDL3_ProcessEvent(&event);
        if (event.type == SDL_EVENT_QUIT)
            gShouldClose = true;
        if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(gWindow))
            HandleCloseRequested();
#ifdef MDR_CLIENT_DEBUGGER
        if (event.type == SDL_EVENT_DROP_FILE && event.drop.windowID == SDL_GetWindowID(gWindow))
        {
            size_t replayed{};
            if (clientDebuggerReplayPath(event.drop.data, &replayed))
            {
                clientEnterDebuggerReplayMode();
                SDL_Log("Replayed %zu packet(s) from %s", replayed, event.drop.data);
            }
            else
            {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to replay %s: %s",
                             event.drop.data, SDL_GetError());
            }
        }
#endif
    }
    // While minimized the frame is still built (so the device keeps being polled and tray
    // actions keep being applied), but nothing is rendered and the loop is throttled.
    const bool minimized = (SDL_GetWindowFlags(gWindow) & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN)) != 0;
    // Start the Dear ImGui frame
    {
        // Platform font loading - if available
        // This is only done once per session. See @ref clientPlatformLocateFontBinary for more info.
        static bool platformFontsMerged = false;
        if (!platformFontsMerged)
        {
            platformFontsMerged = true;
            ImFontConfig merge_config{};
            merge_config.MergeMode = true;
            merge_config.FontDataOwnedByAtlas = false; // Platform keeps the buffers alive
#ifdef IMGUI_ENABLE_FREETYPE
            // Vertical hinting improves small text without squeezing horizontal CJK strokes.
            merge_config.FontLoaderFlags = ImGuiFreeTypeLoaderFlags_LightHinting;
#endif
            // XXX: PlexSansIcon covered latin-1 pages. New ones won't overwrite them.
            // External fonts are meant to cover missing glyphs e.g. CJK ones anyway - so this is fine.
            // Order matters: glyph lookup falls through the merged fonts in the order they were added.
            const char* fontData = nullptr;
            // Latin Extended / Greek / Cyrillic first: the CJK fonts carry only a partial Latin set.
            if (const int size = clientPlatformLocateLatinFontBinary(&fontData))
            {
                SDL_Log("Loading platform Latin font of size %d bytes", size);
                io.Fonts->AddFontFromMemoryTTF((void*)fontData, size, 16.0f, &merge_config);
            }
            if (const int size = clientPlatformLocateFontBinary(&fontData))
            {
                SDL_Log("Loading platform font of size %d bytes", size);
                io.Fonts->AddFontFromMemoryTTF((void*)fontData, size, 16.0f, &merge_config);
            }
            if (const int size = clientPlatformLocateEmojiFontBinary(&fontData))
            {
                SDL_Log("Loading platform emoji font of size %d bytes", size);
                ImFontConfig emoji_config = merge_config;
#ifdef IMGUI_ENABLE_FREETYPE
                // Composite COLR layers into colour bitmaps instead of the monochrome outline.
                emoji_config.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_LoadColor;
#endif
                io.Fonts->AddFontFromMemoryTTF((void*)fontData, size, 16.0f, &emoji_config);
            }
        }
        // New frame
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
    }    
    // Call first, then read the flag: the prompt inside can set gShouldClose during this call.
    const bool exitRequested = clientShouldExit();
    if (exitRequested)
        gShouldClose = true;

#ifdef _WIN32
    DrawWindowChrome();
#endif
    ImGui::Render();
    if (minimized)
    {
        SDL_Delay(50);
        return;
    }
    // Rendering
    {
        SDL_SetRenderScale(gRenderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 0);
        SDL_RenderClear(gRenderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), gRenderer);
        SDL_RenderPresent(gRenderer);
    }
#ifdef __EMSCRIPTEN__
    if (gShouldClose)
        emscripten_cancel_main_loop();
#endif
}

#define CLIENT_WINDOW_WIDTH 800
#define CLIENT_WINDOW_HEIGHT 600

namespace
{
#ifdef _WIN32
    void OpenConsole()
    {
        if (!AllocConsole() && GetLastError() != ERROR_ACCESS_DENIED)
            return;

        std::freopen("CONOUT$", "w", stdout);
        std::freopen("CONOUT$", "w", stderr);
        std::freopen("CONIN$", "r", stdin);
        SetConsoleOutputCP(CP_UTF8);
    }
#endif

    struct ClientOptions
    {
        const char* recordDirectory{};
        const char* replayPath{};
        bool showHelp{};
        bool startMinimized{};
    };

    void PrintUsage()
    {
        MDR_LOG(
            "Usage: SonyHeadphonesClient [-con] [--minimized] [--record <capture-folder>]\n"
            "       SonyHeadphonesClient [-con] [--replay <packet-file-or-folder>]\n"
            "\n"
            "-con opens a diagnostic console on Windows.\n"
            "--minimized starts hidden in the system tray (used by launch-at-login).\n"
            "Packet replay requires a client build with the debugger enabled."
        );
    }

    bool ParseOptions(int argc, char** argv, ClientOptions& options)
    {
        for (int index = 1; index < argc; ++index)
        {
            const char* argument = argv[index];
            if (std::strcmp(argument, "--help") == 0 || std::strcmp(argument, "-h") == 0)
            {
                options.showHelp = true;
                continue;
            }
            if (std::strcmp(argument, "--minimized") == 0)
            {
                options.startMinimized = true;
                continue;
            }
            if (std::strcmp(argument, "-con") == 0)
            {
#ifdef _WIN32
                OpenConsole();
#endif
                continue;
            }

            const bool record = std::strcmp(argument, "--record") == 0;
            const bool replay = std::strcmp(argument, "--replay") == 0;
            if (record || replay)
            {
                if (index + 1 >= argc)
                {
                    MDR_LOG("Missing path after {}.", argument);
                    return false;
                }
                const char* path = argv[++index];
                const char*& destination = record ? options.recordDirectory : options.replayPath;
                if (destination)
                {
                    MDR_LOG("{} may only be specified once.", argument);
                    return false;
                }
                destination = path;
                continue;
            }

            MDR_LOG("Unknown argument: {}", argument);
            return false;
        }

        if (options.recordDirectory && options.replayPath)
        {
            MDR_LOG("--record and --replay cannot be used together.");
            return false;
        }
        return true;
    }
}

int main(int argc, char** argv)
{
    ClientOptions options;
    if (!ParseOptions(argc, argv, options))
    {
        PrintUsage();
        return 2;
    }
    if (options.showHelp)
    {
        PrintUsage();
        return 0;
    }
#ifndef MDR_CLIENT_DEBUGGER
    if (options.replayPath)
    {
        MDR_LOG("Packet replay is unavailable because this client was built without the debugger.");
        return 2;
    }
#endif

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        MDR_LOG("SDL_Init Error: {}", SDL_GetError());
        return 1;
    }
    clientSettingsLoad();
    clientLocalizationSetLanguage(static_cast<ClientLanguage>(clientSettings().language));
    // Keep the launch-at-login registration pointing at this executable (it may have moved).
    if (clientSettings().autoStart && clientPlatformAutoStartSupported())
        clientPlatformAutoStartSet(1);
    if (options.recordDirectory)
    {
        if (!clientPayloadRecorderConfigure(options.recordDirectory))
        {
            MDR_LOG("Unable to prepare capture folder {}: {}", options.recordDirectory, SDL_GetError());
            SDL_Quit();
            return 1;
        }
        MDR_LOG("Recording MDR packets to {}. Existing mdr-packet-*.bin files were cleared. Captures may contain device addresses, names, and playback metadata.", options.recordDirectory);
    }
#ifdef MDR_CLIENT_DEBUGGER
    if (options.replayPath)
    {
        size_t replayed{};
        if (!clientDebuggerReplayPath(options.replayPath, &replayed))
        {
            MDR_LOG("Unable to replay packet path {}: {}", options.replayPath, SDL_GetError());
            SDL_Quit();
            return 1;
        }
        clientEnterDebuggerReplayMode();
        MDR_LOG("Replayed {} packet(s) from {} in debugger-only mode.", replayed, options.replayPath);
    }
#endif
    // SDL turns a close request on the last window into SDL_EVENT_QUIT by default; we decide
    // ourselves whether a close hides to the tray or quits (see HandleCloseRequested).
    SDL_SetHint(SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE, "0");
    // https://github.com/libsdl-org/SDL/blob/main/docs/README-highdpi.md#numeric-example
    // This should only be effective (!=1.0f) on Windows and X11 platforms
    float displayScale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    gWindow = SDL_CreateWindow(
        "SonyHeadphonesClient",
        CLIENT_WINDOW_WIDTH * displayScale, CLIENT_WINDOW_HEIGHT * displayScale,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN
#ifdef _WIN32
        | SDL_WINDOW_BORDERLESS | SDL_WINDOW_TRANSPARENT
#endif
    );
    if (!gWindow)
    {
        SDL_Log("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return 1;
    }
#ifdef _WIN32
    SDL_SetWindowMinimumSize(gWindow, static_cast<int>(400 * displayScale), static_cast<int>(360 * displayScale));
    if (!SDL_SetWindowHitTest(gWindow, WindowHitTest, nullptr))
    {
        SDL_Log("Custom window hit testing unavailable: %s", SDL_GetError());
        SDL_SetWindowBordered(gWindow, true);
    }
#endif
#ifdef MDR_CLIENT_DEBUGGER
    clientDebuggerSetWindow(gWindow);
#endif
    gTrayAvailable = clientPlatformTrayInit() != 0;
    if (!gTrayAvailable)
        SDL_Log("System tray is not available on this platform");
    // Created hidden so a --minimized launch never flashes a window.
    if (!(options.startMinimized && gTrayAvailable))
        SDL_ShowWindow(gWindow);
    gRenderer = SDL_CreateRenderer(gWindow, nullptr);
    SDL_SetRenderVSync(gRenderer, 1);
    if (!gRenderer)
    {
        SDL_Log("Error: SDL_CreateRenderer()\n");
        return 1;
    }
    // Setup Dear ImGui context
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
    }
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    // Setup Material You theme (Sony Sound Connect style)
    ImGui::StyleColorsDark(); // Base fallback
#ifdef _WIN32
    MaterialYouTheme::glassEnabled = ConfigureGlassWindow();
#endif
    MaterialYouTheme::ApplyDefault();
    auto& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(24.0f, 20.0f);
    style.FramePadding = ImVec2(12.0f, 9.0f);
    style.ItemSpacing = ImVec2(10.0f, 12.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
    style.CellPadding = ImVec2(12.0f, 8.0f);
    style.WindowRounding = 24.0f;
    style.ChildRounding = 18.0f;
    style.PopupRounding = 12.0f;
    style.FrameRounding = 10.0f;
    style.GrabRounding = 8.0f;
    style.TabRounding = 8.0f;
    style.ScrollbarSize = 10.0f;
    style.ScrollbarRounding = 8.0f;
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 1.0f;
    style.ScaleAllSizes(displayScale);
    style.FontScaleDpi = displayScale;
    style.CircleTessellationMaxError = 0.01f;
    // Setup Platform/Renderer backends
    {
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls
        io.ConfigErrorRecoveryEnableAssert = true; // Don't assert on errors
        ImGui_ImplSDL3_InitForSDLRenderer(gWindow, gRenderer);
        ImGui_ImplSDLRenderer3_Init(gRenderer);
    }
    // Load our default font
    {
        io.Fonts->Clear();
#ifdef MDR_CLIENT_DEBUGGER
        ImFont* monospaceFont = io.Fonts->AddFontDefault();
#endif
        io.FontDefault = io.Fonts->AddFontFromMemoryCompressedBase85TTF(kEmbedFontPlexSansIcon, 16.0f);
#ifdef MDR_CLIENT_DEBUGGER
        clientDebuggerSetMonospaceFont(monospaceFont);
#endif
    }
    // Main loop

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(mainLoop, 0, 1);
#else
    while (!gShouldClose)
        mainLoop();
#endif

    // Cleanup
    {
        clientShutdown(); // Close the headphone session before anything else goes away
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();

        SDL_DestroyRenderer(gRenderer);
        SDL_DestroyWindow(gWindow);
        SDL_Quit();

        clientPlatformDestroy();
    }
    return 0;
}
