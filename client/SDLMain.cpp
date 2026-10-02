// SDL_Renderer backend from https://github.com/ocornut/imgui/blob/master/examples/example_sdl3_sdlrenderer3
#include <algorithm>
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
#include "DemoConnection.hpp"
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
extern void clientReapplyTheme();
extern void clientDemoSelectTab(int tab);
extern void clientDemoScroll(float pixels);
extern void clientNoteAppearanceSwitch(); // Cross-fade drawn by Client.cpp
extern void clientDrawAppearanceFade();
#ifdef MDR_CLIENT_DEBUGGER
extern void clientEnterDebuggerReplayMode();
#endif

bool gShouldClose = false;
bool gTrayAvailable = false;
// Set by the UI each frame: the window chrome only grows a separator once content slides under it.
bool gContentScrolled = false;
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
    // DWMSBT_NONE = 1, DWMSBT_TRANSIENTWINDOW (acrylic) = 3. Acrylic is the blur behind the app.
    const DWORD backdrop = clientSettings().glassLevel == CLIENT_GLASS_OFF ? 1u : 3u;
    const HRESULT result = DwmSetWindowAttribute(hwnd, 38 /* DWMWA_SYSTEMBACKDROP_TYPE */,
                                                &backdrop, sizeof(backdrop));
    if (FAILED(result))
    {
        SDL_Log("Acrylic backdrop unavailable; using opaque surfaces (0x%lx)", static_cast<unsigned long>(result));
        return false;
    }
    const MARGINS margins{-1, -1, -1, -1};
    return SUCCEEDED(DwmExtendFrameIntoClientArea(hwnd, &margins));
}
#endif

// Light or dark: the setting, or the OS when set to follow it.
static bool ResolveDarkAppearance()
{
    const int appearance = clientSettings().appearance;
    return appearance == CLIENT_APPEARANCE_DARK ||
           (appearance == CLIENT_APPEARANCE_AUTO && !clientPlatformSystemUsesLightTheme());
}

// Applies the resolved appearance to the palette, the compositor (so the acrylic tint and the
// frame follow) and the ImGui style. Called at startup, when the setting changes, and when the
// OS flips while following it.
void clientApplyAppearance()
{
    static bool initialized = false;
    const bool dark = ResolveDarkAppearance();
    if (initialized && dark != MaterialYouTheme::darkMode)
        clientNoteAppearanceSwitch();
    initialized = true;
    MaterialYouTheme::SetAppearance(dark);
#ifdef _WIN32
    if (HWND hwnd = NativeWindowHandle())
    {
        const BOOL immersiveDark = dark ? TRUE : FALSE;
        DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &immersiveDark, sizeof(immersiveDark));
    }
#endif
    clientReapplyTheme();
}

// Called when the transparency setting changes so the backdrop and the surface alphas agree.
void clientRefreshWindowBackdrop()
{
#ifdef _WIN32
    MaterialYouTheme::glassEnabled = ConfigureGlassWindow();
#endif
    MaterialYouTheme::glassLevel = clientSettings().glassLevel;
    clientReapplyTheme();
}

// Platform fonts merged into the UI font. Order matters: glyph lookup falls through the sources in
// the order they were added, and Japanese uses different forms of many ideographs than Chinese for
// the same code points, so the language's own script font goes first.
static bool gFontsDirty = false;
void clientRequestFontRebuild() { gFontsDirty = true; }

static ImFont* gHeadingFont = nullptr;
ImFont* clientHeadingFont() { return gHeadingFont; }

// Adds the platform faces to the font that was added last: the regular faces for the body, or
// for headings the designed bold faces. A script without a bold face gets its regular one
// emboldened by FreeType instead, which clogs dense ideographs, so it is only the fallback.
static void AddPlatformSources(ImGuiIO& io, unsigned flags, bool heading)
{
    ImFontConfig merge_config{};
    merge_config.MergeMode = true;
    merge_config.FontDataOwnedByAtlas = false; // Platform keeps the buffers alive
    auto add = [&](int script, int (*regular)(const char**))
    {
        ImFontConfig config = merge_config;
        const char* data = nullptr;
        int size = heading ? clientPlatformLocateBoldFontBinary(script, &data) : 0;
#ifdef IMGUI_ENABLE_FREETYPE
        config.FontLoaderFlags = flags;
        if (size && heading)
            config.FontLoaderFlags &= ~ImGuiFreeTypeLoaderFlags_Bold;
#endif
        if (!size)
            size = regular(&data);
        // Regular ideographs are drawn with hairline strokes that fade to grey at body sizes, beside
        // the heavier bundled Latin font. Darkening their anti-aliasing fills the strokes out
        // without the clogging of a synthetic bold.
        if (!heading && script != CLIENT_FONT_LATIN)
            config.RasterizerMultiply = 1.3f;
        if (size)
            io.Fonts->AddFontFromMemoryTTF((void*)data, size, 16.0f, &config);
    };
#ifdef IMGUI_ENABLE_FREETYPE
    merge_config.FontLoaderFlags = flags;
#else
    (void)flags;
#endif
    // Latin Extended / Greek / Cyrillic first: the CJK fonts carry only a partial Latin set.
    add(CLIENT_FONT_LATIN, clientPlatformLocateLatinFontBinary);
    const bool japanese = clientLocalizationEffectiveLanguage() == ClientLanguage::Japanese;
    if (japanese)
        add(CLIENT_FONT_JAPANESE, clientPlatformLocateJapaneseFontBinary);
    add(CLIENT_FONT_CJK, clientPlatformLocateFontBinary);
    if (!japanese) // Kana in track titles, whatever the UI language
        add(CLIENT_FONT_JAPANESE, clientPlatformLocateJapaneseFontBinary);
    const char* fontData = nullptr;
    if (!heading)
        if (const int size = clientPlatformLocateEmojiFontBinary(&fontData))
        {
            ImFontConfig emoji_config = merge_config;
#ifdef IMGUI_ENABLE_FREETYPE
            // Composite COLR layers into colour bitmaps instead of the monochrome outline.
            emoji_config.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_LoadColor;
#endif
            io.Fonts->AddFontFromMemoryTTF((void*)fontData, size, 16.0f, &emoji_config);
        }
}

static void MergePlatformFonts(ImGuiIO& io)
{
    // Body: the bundled font (already added) plus the platform faces.
    unsigned body = 0, heading = 0;
#ifdef IMGUI_ENABLE_FREETYPE
    // Vertical hinting improves small text without squeezing horizontal CJK strokes.
    body = ImGuiFreeTypeLoaderFlags_LightHinting;
    heading = ImGuiFreeTypeLoaderFlags_LightHinting | ImGuiFreeTypeLoaderFlags_Bold;
#endif
    AddPlatformSources(io, body, false);
    // Headings: the bundled font emboldened, then the platform's bold faces, as a second font.
    ImFontConfig bold{};
#ifdef IMGUI_ENABLE_FREETYPE
    bold.FontLoaderFlags = heading;
#endif
    gHeadingFont = io.Fonts->AddFontFromMemoryCompressedBase85TTF(kEmbedFontPlexSansIcon, 16.0f, &bold);
    AddPlatformSources(io, heading, true);
    SDL_Log("Platform fonts merged (%s first)", clientLocalizationEffectiveLanguage() == ClientLanguage::Japanese ? "Japanese" : "CJK");
}

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
    // 0 appearance toggle, 1 minimize, 2 maximize, 3 close.
    if (h == 0.0f || y < 5.0f || y >= h || x < width - h * 4 || x >= width - 5.0f)
        return -1;
    return static_cast<int>((x - (width - h * 4)) / h);
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
    if (point->y < clientWindowChromeHeight() && point->x < width - clientWindowChromeHeight() * 4)
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
    // The lit top edge of the pane, inset so it stops at the compositor's rounded corners.
    draw->AddLine({8.0f, 0.5f}, {width - 8.0f, 0.5f}, MaterialYouTheme::glassEdgeHighlight());
    // Liquid Glass toolbars stay borderless until content scrolls underneath them.
    if (gContentScrolled)
        draw->AddLine({0, h - 0.5f}, {width, h - 0.5f}, MaterialYouTheme::glassEdgeShadow());
    draw->AddText({h * 0.5f, (h - ImGui::GetFontSize()) * 0.5f},
                  ImGui::GetColorU32(ImGuiCol_TextDisabled), "Sony Headphones");
    float mx, my;
    SDL_GetMouseState(&mx, &my);
    const int hovered = (SDL_GetWindowFlags(gWindow) & SDL_WINDOW_MOUSE_FOCUS) ? ChromeButton(mx, my) : -1;
    for (int i = 0; i < 4; ++i)
    {
        const float x = width - h * (4 - i);
        const ImU32 color = hovered == i && i == 3 ? IM_COL32_WHITE : ImGui::GetColorU32(ImGuiCol_Text);
        if (hovered == i)
            draw->AddRectFilled({x + 2, 4}, {x + h - 2, h - 4},
                                i == 3 ? IM_COL32(220, 55, 65, 255) : ImGui::GetColorU32(ImGuiCol_FrameBg), 8);
        const float cx = x + h * 0.5f, cy = h * 0.5f, r = h * 0.13f;
        if (i == 0)
        {
            // Appearance toggle: a half-filled disc, the usual contrast glyph.
            const float size = h * 0.42f;
            const ImVec2 glyph = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, PSI_ADJUST);
            draw->AddText(ImGui::GetFont(), size, {cx - glyph.x * 0.5f, cy - glyph.y * 0.5f},
                          ImGui::GetColorU32(ImGuiCol_TextDisabled), PSI_ADJUST);
        }
        else if (i == 1)
            draw->AddLine({cx - r, cy}, {cx + r, cy}, color, 1.5f);
        else if (i == 2)
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

// Textures the client fills itself (the 3D illustration renders into one): RGBA, straight alpha.
ImTextureID clientTextureCreate(int width, int height)
{
    SDL_Texture* texture = SDL_CreateTexture(gRenderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height);
    if (!texture)
        return ImTextureID_Invalid;
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
    return static_cast<ImTextureID>(reinterpret_cast<intptr_t>(texture));
}

void clientTextureUpdate(ImTextureID texture, const void* pixels, int width, int height, int pitch)
{
    const SDL_Rect area{0, 0, width, height};
    SDL_UpdateTexture(reinterpret_cast<SDL_Texture*>(static_cast<intptr_t>(texture)), &area, pixels, pitch);
}

void clientTextureDestroy(ImTextureID texture)
{
    SDL_DestroyTexture(reinterpret_cast<SDL_Texture*>(static_cast<intptr_t>(texture)));
}

// ImGui_ImplSDLRenderer3_RenderDrawData, handing SDL only the vertices each draw command uses.
// The stock backend passes every vertex from the command's offset to the end of its draw list, and
// both it (converting the colours) and SDL (checking the texture coordinates) walk all of them once
// per command. That is quadratic in a busy draw list and was most of the frame's CPU time. Here a
// list's colours are converted once, and each command gets its own vertex range with its indices
// rebased onto it.
static void RenderDrawData(ImDrawData* drawData, SDL_Renderer* renderer)
{
    // A scale set on the renderer (SDL_SetRenderScale) already applies to the coordinates.
    float rsx = 1.0f, rsy = 1.0f;
    SDL_GetRenderScale(renderer, &rsx, &rsy);
    const ImVec2 renderScale(rsx == 1.0f ? drawData->FramebufferScale.x : 1.0f,
                             rsy == 1.0f ? drawData->FramebufferScale.y : 1.0f);
    const float fbWidth = static_cast<float>(static_cast<int>(drawData->DisplaySize.x * renderScale.x));
    const float fbHeight = static_cast<float>(static_cast<int>(drawData->DisplaySize.y * renderScale.y));
    if (fbWidth <= 0.0f || fbHeight <= 0.0f)
        return;
    if (drawData->Textures)
        for (ImTextureData* texture : *drawData->Textures)
            if (texture->Status != ImTextureStatus_OK)
                ImGui_ImplSDLRenderer3_UpdateTexture(texture);

    SDL_Rect oldViewport{}, oldClip{};
    const bool viewportSet = SDL_RenderViewportSet(renderer), clipEnabled = SDL_RenderClipEnabled(renderer);
    SDL_GetRenderViewport(renderer, &oldViewport);
    SDL_GetRenderClipRect(renderer, &oldClip);
    SDL_SetRenderViewport(renderer, nullptr);
    SDL_SetRenderClipRect(renderer, nullptr);
    ImGui_ImplSDLRenderer3_RenderState renderState{renderer};
    ImGui::GetPlatformIO().Renderer_RenderState = &renderState;

    static ImVector<SDL_FColor> colours;
    static ImVector<ImDrawIdx> indices;
    const ImVec2 clipOffset = drawData->DisplayPos;
    for (const ImDrawList* list : drawData->CmdLists)
    {
        const ImDrawVert* vertices = list->VtxBuffer.Data;
        colours.resize(list->VtxBuffer.Size);
        for (int i = 0; i < list->VtxBuffer.Size; ++i)
        {
            const ImU32 c = vertices[i].col;
            colours[i] = {static_cast<float>((c >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f,
                          static_cast<float>((c >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f,
                          static_cast<float>((c >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f,
                          static_cast<float>((c >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f};
        }
        for (const ImDrawCmd& cmd : list->CmdBuffer)
        {
            if (cmd.UserCallback)
            {
                if (cmd.UserCallback == ImDrawCallback_ResetRenderState)
                {
                    SDL_SetRenderViewport(renderer, nullptr);
                    SDL_SetRenderClipRect(renderer, nullptr);
                }
                else
                    cmd.UserCallback(list, &cmd);
                continue;
            }
            // Project the clipping rectangle into framebuffer space.
            const float x0 = std::max(0.0f, (cmd.ClipRect.x - clipOffset.x) * renderScale.x);
            const float y0 = std::max(0.0f, (cmd.ClipRect.y - clipOffset.y) * renderScale.y);
            const float x1 = std::min(fbWidth, (cmd.ClipRect.z - clipOffset.x) * renderScale.x);
            const float y1 = std::min(fbHeight, (cmd.ClipRect.w - clipOffset.y) * renderScale.y);
            if (x1 <= x0 || y1 <= y0 || cmd.ElemCount == 0)
                continue;
            const SDL_Rect clip{static_cast<int>(x0), static_cast<int>(y0), static_cast<int>(x1 - x0), static_cast<int>(y1 - y0)};
            SDL_SetRenderClipRect(renderer, &clip);

            const ImDrawIdx* source = list->IdxBuffer.Data + cmd.IdxOffset;
            unsigned lowest = ~0u, highest = 0;
            for (unsigned i = 0; i < cmd.ElemCount; ++i)
            {
                lowest = std::min<unsigned>(lowest, source[i]);
                highest = std::max<unsigned>(highest, source[i]);
            }
            indices.resize(static_cast<int>(cmd.ElemCount));
            for (unsigned i = 0; i < cmd.ElemCount; ++i)
                indices[static_cast<int>(i)] = static_cast<ImDrawIdx>(source[i] - lowest);
            const unsigned first = cmd.VtxOffset + lowest;
            SDL_RenderGeometryRaw(renderer, reinterpret_cast<SDL_Texture*>(static_cast<intptr_t>(cmd.GetTexID())),
                                  &vertices[first].pos.x, sizeof(ImDrawVert),
                                  colours.Data + first, sizeof(SDL_FColor),
                                  &vertices[first].uv.x, sizeof(ImDrawVert),
                                  static_cast<int>(highest - lowest + 1),
                                  indices.Data, static_cast<int>(cmd.ElemCount), sizeof(ImDrawIdx));
        }
    }
    ImGui::GetPlatformIO().Renderer_RenderState = nullptr;
    SDL_SetRenderViewport(renderer, viewportSet ? &oldViewport : nullptr);
    SDL_SetRenderClipRect(renderer, clipEnabled ? &oldClip : nullptr);
}

void mainLoop()
{
    ImGuiIO& io = ImGui::GetIO();
#ifdef _WIN32
    SuppressNativeCaption();
#endif
    // Following the OS appearance: a registry read every two seconds is cheaper than a hook.
    {
        static uint64_t lastAppearancePollMs = 0;
        if (clientSettings().appearance == CLIENT_APPEARANCE_AUTO && SDL_GetTicks() - lastAppearancePollMs >= 2000)
        {
            lastAppearancePollMs = SDL_GetTicks();
            if (ResolveDarkAppearance() != MaterialYouTheme::darkMode)
                clientApplyAppearance();
        }
    }
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
                if (released == 0)
                {
                    // Quick toggle between the two explicit appearances; "follow the OS" is in settings.
                    clientSettings().appearance = MaterialYouTheme::darkMode ? CLIENT_APPEARANCE_LIGHT : CLIENT_APPEARANCE_DARK;
                    clientSettingsSave();
                    clientApplyAppearance();
                }
                else if (released == 1) SDL_MinimizeWindow(gWindow);
                else if (released == 2)
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
        // Platform fonts, merged once - and again when the language changes, because the order of
        // the sources decides which font a shared ideograph comes from. See MergePlatformFonts.
        static bool platformFontsMerged = false;
        if (!platformFontsMerged || gFontsDirty)
        {
            if (gFontsDirty)
            {
                // The atlas is unlocked between frames when the backend manages textures, so it can
                // be rebuilt from scratch here; new glyphs are rasterized on demand afterwards.
                io.Fonts->Clear();
#ifdef MDR_CLIENT_DEBUGGER
                clientDebuggerSetMonospaceFont(io.Fonts->AddFontDefault());
#endif
                io.FontDefault = io.Fonts->AddFontFromMemoryCompressedBase85TTF(kEmbedFontPlexSansIcon, 16.0f);
            }
            platformFontsMerged = true;
            gFontsDirty = false;
            MergePlatformFonts(io);
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
    clientDrawAppearanceFade();
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
        RenderDrawData(ImGui::GetDrawData(), gRenderer);
        SDL_RenderPresent(gRenderer);
        // Frame pacing: 60 fps is plenty for this UI. On a high-refresh display vsync alone runs
        // at 120+ fps and burns a core on nothing; unfocused, 30 fps keeps animations alive but
        // cheap. One plain sleep, ending a little before the period so the following vblank still
        // lines up (a sleep overshoots by a fraction of a millisecond). SDL_DelayPrecise wakes
        // every millisecond and spins through the end, which cost about as much as a frame.
        static uint64_t lastFrameNs = 0;
        const bool focused = (SDL_GetWindowFlags(gWindow) & SDL_WINDOW_INPUT_FOCUS) != 0;
        const uint64_t periodNs = focused ? 15'700'000ull : 32'300'000ull;
        const uint64_t nowNs = SDL_GetTicksNS();
        if (lastFrameNs != 0 && nowNs - lastFrameNs < periodNs)
            SDL_DelayNS(periodNs - (nowNs - lastFrameNs));
        lastFrameNs = SDL_GetTicksNS();
    }
#ifdef __EMSCRIPTEN__
    if (gShouldClose)
        emscripten_cancel_main_loop();
#endif
}

#define CLIENT_WINDOW_WIDTH 820
#define CLIENT_WINDOW_HEIGHT 760

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
        const char* demoDirectory{}; // Replay a recorded session as if the headphones were here
        int demoTab = -1;            // With --demo: the screen to open on; see clientDemoSelectTab
        float demoScroll = -1.0f;    // With --demo: scroll the connected page this far
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
            "--demo <capture-folder> replays a recorded session so the connected UI runs without headphones.\n"
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
            if (std::strcmp(argument, "--demo") == 0)
            {
                if (index + 1 >= argc)
                    return false;
                options.demoDirectory = argv[++index];
                continue;
            }
            if (std::strcmp(argument, "--demo-tab") == 0)
            {
                if (index + 1 >= argc)
                    return false;
                options.demoTab = std::atoi(argv[++index]);
                continue;
            }
            if (std::strcmp(argument, "--demo-scroll") == 0)
            {
                if (index + 1 >= argc)
                    return false;
                options.demoScroll = static_cast<float>(std::atof(argv[++index]));
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
    // Two clients racing for the headphones' single MDR session leave half-open RFCOMM channels
    // behind and clobber each other's settings file, so only one may run. Replaying a capture
    // touches neither, and is allowed alongside a running client.
    if (!options.replayPath &&
        !clientPlatformSingleInstanceAcquire(options.startMinimized ? 0 : 1))
    {
        MDR_LOG("SonyHeadphonesClient is already running; asked it to show its window.");
        return 0;
    }

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        MDR_LOG("SDL_Init Error: {}", SDL_GetError());
        return 1;
    }
    clientSettingsLoad();
    clientLocalizationSetLanguage(static_cast<ClientLanguage>(clientSettings().language));
    if (options.demoDirectory)
    {
        // The replayed device borrows the remembered address (and name) so auto-connect picks it up at once.
        const ClientSettings& settings = clientSettings();
        MDRConnection* demo = clientDemoConnectionCreate(options.demoDirectory, settings.lastDeviceAddress.c_str(),
                                                         settings.lastDeviceName.c_str());
        if (!demo)
        {
            MDR_LOG("No recorded packets found in {}", options.demoDirectory);
            SDL_Quit();
            return 1;
        }
        clientPlatformConnectionOverride(demo);
        clientDemoScroll(options.demoScroll);
    }
    clientDemoSelectTab(options.demoTab); // 8 (the connection sheet) also works without --demo
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
    MaterialYouTheme::glassLevel = clientSettings().glassLevel;
    clientApplyAppearance(); // Also runs ApplyDefault through clientReapplyTheme
    auto& style = ImGui::GetStyle();
    // Spacing on Apple's 8pt grid; radii follow its container / control / detail hierarchy.
    style.WindowPadding = ImVec2(24.0f, 20.0f);
    style.FramePadding = ImVec2(16.0f, 8.0f); // 32pt controls: this is a mouse-driven desktop app
    style.ItemSpacing = ImVec2(8.0f, 8.0f);   // 1.5 leading on body text
    style.ItemInnerSpacing = ImVec2(8.0f, 8.0f);
    style.CellPadding = ImVec2(12.0f, 8.0f);
    style.WindowRounding = 16.0f;
    style.ChildRounding = 16.0f;
    style.PopupRounding = 16.0f;
    style.FrameRounding = 12.0f;
    style.GrabRounding = 8.0f;
    style.TabRounding = 12.0f;
    style.ScrollbarSize = 10.0f;
    style.ScrollbarRounding = 8.0f;
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 1.0f;
    style.SeparatorTextBorderSize = 0.0f; // Section headings are plain labels, not ruled lines
    style.SeparatorTextPadding = ImVec2(0.0f, 4.0f); // ImSectionHeading tightens the gap below
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
