#pragma once
#include <imgui.h>
#include <cstdint>
#include <iterator>

namespace MaterialYouTheme {

using Argb = uint32_t;

// Kept in step with ClientGlassLevel without dragging Settings.hpp into the theme header.
constexpr int CLIENT_GLASS_CLEAR_LEVEL = 2;

// Set only after the native compositor accepts the acrylic backdrop.
inline bool glassEnabled = false;
// Dark appearance. Set through SetAppearance() so the palette swaps with it.
inline bool darkMode = false;
// ClientGlassLevel, mirrored from the settings before Apply() runs: 0 off, 1 regular, 2 clear.
inline int glassLevel = 1;

inline bool glassActive() { return glassEnabled && glassLevel > 0; }

// Apple's two glass variants: regular glass stays legible on its own, clear glass lets more of
// the desktop through. iOS 27 dials the default back towards regular for readability.
inline float glassAlpha(float regular = 0.80f, float clear = 0.62f) {
    if (!glassActive()) return 1.0f;
    float alpha = glassLevel >= CLIENT_GLASS_CLEAR_LEVEL ? clear : regular;
    // Dark glass wants more tint: clear glass over a dark desktop turns to mud.
    if (darkMode) alpha = alpha + (1.0f - alpha) * 0.35f;
    return alpha;
}

// iOS 27 adds a darkened outer edge for separation and a brighter specular highlight on the lit
// (top) edge. Together they read as a pane of glass rather than a flat translucent rectangle.
inline ImU32 glassEdgeShadow() {
    if (darkMode) return IM_COL32(0, 0, 0, glassActive() ? 120 : 90);
    return IM_COL32(0, 0, 0, glassActive() ? 40 : 30);
}
inline ImU32 glassEdgeHighlight() {
    // A dark pane catches only a thin rim of light; a bright one gets the full specular edge.
    if (darkMode) return IM_COL32(255, 255, 255, glassActive() ? 46 : 30);
    return IM_COL32(255, 255, 255, glassActive() ? 190 : 120);
}

inline ImVec4 ArgbToImVec4(Argb argb) {
    return ImVec4(
        static_cast<float>((argb >> 16) & 0xFF) / 255.0f,
        static_cast<float>((argb >> 8) & 0xFF) / 255.0f,
        static_cast<float>(argb & 0xFF) / 255.0f,
        static_cast<float>((argb >> 24) & 0xFF) / 255.0f
    );
}

inline ImVec4 ArgbToImVec4(Argb argb, float alpha) {
    return ImVec4(
        static_cast<float>((argb >> 16) & 0xFF) / 255.0f,
        static_cast<float>((argb >> 8) & 0xFF) / 255.0f,
        static_cast<float>(argb & 0xFF) / 255.0f,
        alpha
    );
}

inline ImU32 ArgbToImU32(Argb argb) {
    return ImGui::ColorConvertFloat4ToU32(ArgbToImVec4(argb));
}

inline ImU32 ArgbToImU32(Argb argb, float alpha) {
    return ImGui::ColorConvertFloat4ToU32(ArgbToImVec4(argb, alpha));
}

// Surface palette, swapped at runtime between light and dark. Members stay addressable as
// MaterialYouTheme::FixedSurfaceColors::x so the rest of the client does not care which is active.
struct FixedSurfaceColors {
    inline static Argb surface                 = 0xFFF5F5F7;
    inline static Argb surfaceContainerLow     = 0xFFFFFFFF;
    inline static Argb surfaceContainerHigh    = 0xFFF0F0F3;
    inline static Argb surfaceContainerHighest = 0xFFE5E5EA;
    inline static Argb onSurface               = 0xFF1D1D1F;
    inline static Argb onSurfaceVariant        = 0xFF636366;
    inline static Argb outline                 = 0xFF8E8E93;
    inline static Argb outlineVariant          = 0xFFD1D1D6;
    inline static Argb inverseSurface          = 0xFF1D1D1F;
    inline static Argb inverseOnSurface        = 0xFFF5F5F7;
    inline static Argb error                   = 0xFFB42318;
    inline static Argb errorContainer          = 0xFF8C1D18;
    inline static Argb segmentPill             = 0xFFFFFFFF; // The sliding pill of a segmented control
    inline static Argb heroDisc                = 0xFFFFFFFF; // The disc behind the headphone illustration
    inline static Argb toggleOff               = 0xFFD1D1D6; // Switch track when off
    inline static Argb segmentTrack            = 0xFFE5E5EA; // The groove a segmented control sits in
};

struct SurfacePalette {
    Argb surface, surfaceContainerLow, surfaceContainerHigh, surfaceContainerHighest;
    Argb onSurface, onSurfaceVariant, outline, outlineVariant, inverseSurface, inverseOnSurface;
    Argb error, errorContainer, segmentPill, heroDisc, toggleOff, segmentTrack;
};

inline constexpr SurfacePalette kLightSurface{
    0xFFF5F5F7, 0xFFFFFFFF, 0xFFF0F0F3, 0xFFE5E5EA,
    0xFF1D1D1F, 0xFF505055, 0xFF8E8E93, 0xFFD1D1D6, 0xFF1D1D1F, 0xFFF5F5F7,
    0xFFB42318, 0xFF8C1D18, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFD1D1D6, 0xFFE5E5EA,
};

// Dark follows Apple's Dark Mode guidance: no pure black, higher surfaces get lighter rather
// than darker, secondary text stays well above the 4.5:1 contrast floor, and the accent is a
// touch brighter than in light mode so it does not sink into the background.
inline constexpr SurfacePalette kDarkSurface{
    0xFF1C1C1E, 0xFF2C2C2E, 0xFF3A3A3C, 0xFF3F3F44,
    0xFFF5F5F7, 0xFFB4B4B9, 0xFF6E6E73, 0xFF3A3A3C, 0xFFF5F5F7, 0xFF1D1D1F,
    0xFFFF6B6B, 0xFF5C1A1A, 0xFF636366, 0xFF3A3A3C, 0xFF48484A, 0xFF232326,
};

inline void SetAppearance(bool dark) {
    darkMode = dark;
    const SurfacePalette& p = dark ? kDarkSurface : kLightSurface;
    FixedSurfaceColors::surface = p.surface;
    FixedSurfaceColors::surfaceContainerLow = p.surfaceContainerLow;
    FixedSurfaceColors::surfaceContainerHigh = p.surfaceContainerHigh;
    FixedSurfaceColors::surfaceContainerHighest = p.surfaceContainerHighest;
    FixedSurfaceColors::onSurface = p.onSurface;
    FixedSurfaceColors::onSurfaceVariant = p.onSurfaceVariant;
    FixedSurfaceColors::outline = p.outline;
    FixedSurfaceColors::outlineVariant = p.outlineVariant;
    FixedSurfaceColors::inverseSurface = p.inverseSurface;
    FixedSurfaceColors::inverseOnSurface = p.inverseOnSurface;
    FixedSurfaceColors::error = p.error;
    FixedSurfaceColors::errorContainer = p.errorContainer;
    FixedSurfaceColors::segmentPill = p.segmentPill;
    FixedSurfaceColors::heroDisc = p.heroDisc;
    FixedSurfaceColors::toggleOff = p.toggleOff;
    FixedSurfaceColors::segmentTrack = p.segmentTrack;
}

struct Theme {
    Argb primary;
    Argb onPrimary;
    Argb primaryContainer;
    Argb onPrimaryContainer;
};

// Precomputed Material You dark theme palettes per ModelColor.
// Regenerate: cd tooling/theme-generator && npm install && npm run generate
#include "MaterialYouThemeTable.inc"

inline const Theme& ThemeForModelColor(uint8_t modelColor) {
    if (modelColor < std::size(kThemeTable)) return kThemeTable[modelColor];
    return kThemeTable[0]; // DEFAULT
}

// Controls stay blue in both appearances; model palettes are unsuitable on neutral surfaces.
inline Theme AccentTheme() {
    return darkMode ? Theme{0xFF0A84FF, 0xFFFFFFFF, 0xFF1F3550, 0xFFCFE2FF}
                    : Theme{0xFF0071E3, 0xFFFFFFFF, 0xFFE5F0FF, 0xFF003366};
}

inline void Apply(const Theme&) {
    const Theme theme = AccentTheme();
    // A 10% accent wash reads on white; on a dark surface it needs to be stronger to show at all.
    const float tint = darkMode ? 0.22f : 0.10f;
    auto& style = ImGui::GetStyle();
    ImVec4* c = style.Colors;

    // Fixed surface colors (Sony standard)
    c[ImGuiCol_WindowBg]        = ArgbToImVec4(FixedSurfaceColors::surface, glassAlpha());
    c[ImGuiCol_ChildBg]         = ArgbToImVec4(FixedSurfaceColors::surface, 0.0f);
    // Opaque: menus and dropdowns sit over app content, which no compositor blurs for us.
    c[ImGuiCol_PopupBg]         = ArgbToImVec4(FixedSurfaceColors::surfaceContainerLow);
    c[ImGuiCol_MenuBarBg]       = ArgbToImVec4(FixedSurfaceColors::surfaceContainerHigh);
    c[ImGuiCol_ScrollbarBg]     = ArgbToImVec4(FixedSurfaceColors::surface, 0.5f);
    c[ImGuiCol_TableRowBg]      = ArgbToImVec4(FixedSurfaceColors::surface, 0.0f);
    c[ImGuiCol_TableRowBgAlt]   = ArgbToImVec4(FixedSurfaceColors::surfaceContainerLow, 0.5f);

    // Title bar
    c[ImGuiCol_TitleBg]          = ArgbToImVec4(FixedSurfaceColors::surfaceContainerLow);
    c[ImGuiCol_TitleBgActive]    = ArgbToImVec4(FixedSurfaceColors::surfaceContainerHigh);
    c[ImGuiCol_TitleBgCollapsed] = ArgbToImVec4(FixedSurfaceColors::surface);

    // Text (fixed)
    c[ImGuiCol_Text]             = ArgbToImVec4(FixedSurfaceColors::onSurface);
    c[ImGuiCol_TextDisabled]     = ArgbToImVec4(FixedSurfaceColors::onSurfaceVariant);

    // Borders (fixed)
    c[ImGuiCol_Border]           = ArgbToImVec4(FixedSurfaceColors::outlineVariant, 0.6f);
    c[ImGuiCol_BorderShadow]     = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_Separator]        = ArgbToImVec4(FixedSurfaceColors::outlineVariant);
    c[ImGuiCol_SeparatorHovered] = ArgbToImVec4(theme.primary, 0.78f);
    c[ImGuiCol_SeparatorActive]  = ArgbToImVec4(theme.primary);
    c[ImGuiCol_TableHeaderBg]    = ArgbToImVec4(FixedSurfaceColors::surfaceContainerHighest);
    c[ImGuiCol_TableBorderStrong]= ArgbToImVec4(FixedSurfaceColors::outline);
    c[ImGuiCol_TableBorderLight] = ArgbToImVec4(FixedSurfaceColors::outlineVariant);

    // Frame backgrounds: primary tint with increasing opacity
    // Opaque. A translucent control frame disappears against the white sheets it also sits on
    // (the close prompt, menus), taking the checkbox box and the combo outline with it.
    c[ImGuiCol_FrameBg]          = ArgbToImVec4(FixedSurfaceColors::surfaceContainerHighest);
    c[ImGuiCol_FrameBgHovered]   = ArgbToImVec4(theme.primary, 0.24f);
    c[ImGuiCol_FrameBgActive]    = ArgbToImVec4(theme.primary, 0.35f);

    // Scrollbar
    c[ImGuiCol_ScrollbarGrab]        = ArgbToImVec4(FixedSurfaceColors::outline);
    c[ImGuiCol_ScrollbarGrabHovered] = ArgbToImVec4(FixedSurfaceColors::onSurfaceVariant);
    c[ImGuiCol_ScrollbarGrabActive]  = ArgbToImVec4(theme.primary);

    // Dynamic accent colors
    c[ImGuiCol_CheckMark]        = ArgbToImVec4(theme.primary);
    c[ImGuiCol_SliderGrab]       = ArgbToImVec4(theme.primary, 0.80f);
    c[ImGuiCol_SliderGrabActive] = ArgbToImVec4(theme.primary);

    // Buttons: primary tint, brighter on hover/active
    c[ImGuiCol_Button]           = ArgbToImVec4(theme.primary, tint);
    c[ImGuiCol_ButtonHovered]    = ArgbToImVec4(theme.primary, tint + 0.08f);
    c[ImGuiCol_ButtonActive]     = ArgbToImVec4(theme.primary, tint + 0.16f);

    // Headers: primary tint with increasing opacity
    c[ImGuiCol_Header]           = ArgbToImVec4(theme.primary, 0.22f);
    c[ImGuiCol_HeaderHovered]    = ArgbToImVec4(theme.primary, 0.30f);
    c[ImGuiCol_HeaderActive]     = ArgbToImVec4(theme.primary, 0.40f);

    // Tabs
    c[ImGuiCol_Tab]                    = ArgbToImVec4(FixedSurfaceColors::surfaceContainerLow);
    c[ImGuiCol_TabSelected]            = ArgbToImVec4(theme.primaryContainer);
    c[ImGuiCol_TabSelectedOverline]    = ArgbToImVec4(theme.primary);
    c[ImGuiCol_TabHovered]             = ArgbToImVec4(theme.primaryContainer);
    c[ImGuiCol_TabDimmed]              = ArgbToImVec4(FixedSurfaceColors::surface);
    c[ImGuiCol_TabDimmedSelected]      = ArgbToImVec4(FixedSurfaceColors::surfaceContainerHigh);
    c[ImGuiCol_TabDimmedSelectedOverline] = ArgbToImVec4(FixedSurfaceColors::outline);

    // Misc
    c[ImGuiCol_TextLink]         = ArgbToImVec4(theme.primary);
    c[ImGuiCol_TextSelectedBg]   = ArgbToImVec4(theme.primaryContainer, 0.4f);
    c[ImGuiCol_NavCursor]        = ArgbToImVec4(theme.primary);
    c[ImGuiCol_DragDropTarget]   = ArgbToImVec4(theme.primary);

    // Resize grip: primary tint
    c[ImGuiCol_ResizeGrip]        = ArgbToImVec4(theme.primary, 0.20f);
    c[ImGuiCol_ResizeGripHovered] = ArgbToImVec4(theme.primary, 0.67f);
    c[ImGuiCol_ResizeGripActive]  = ArgbToImVec4(theme.primary, 0.95f);

    // Plot
    c[ImGuiCol_PlotLines]         = ArgbToImVec4(theme.primary);
    c[ImGuiCol_PlotLinesHovered]  = ArgbToImVec4(theme.primary);
    c[ImGuiCol_PlotHistogram]     = ArgbToImVec4(theme.primary);
    c[ImGuiCol_PlotHistogramHovered] = ArgbToImVec4(theme.primary);

    // Modal dim
    c[ImGuiCol_ModalWindowDimBg]      = darkMode ? ImVec4(0.0f, 0.0f, 0.0f, 0.40f) : ImVec4(0.12f, 0.14f, 0.18f, 0.16f);
    c[ImGuiCol_NavWindowingHighlight] = ArgbToImVec4(theme.primary, 0.7f);
    c[ImGuiCol_NavWindowingDimBg]     = ImVec4(0, 0, 0, 0.2f);

    // Input cursor
    c[ImGuiCol_InputTextCursor]  = ArgbToImVec4(theme.primary);
}

inline void ApplyDefault() {
    Apply(kThemeTable[0]);
}

inline void ApplyForModelColor(uint8_t modelColor) {
    Apply(ThemeForModelColor(modelColor));
}

} // namespace MaterialYouTheme
