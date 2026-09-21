#pragma once
#include <imgui.h>
#include <cstdint>
#include <iterator>

namespace MaterialYouTheme {

using Argb = uint32_t;

// Set only after the native compositor accepts the acrylic backdrop.
inline bool glassEnabled = false;

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

// Neutral light surfaces for the personal-audio interface.
struct FixedSurfaceColors {
    static constexpr Argb surface             = 0xFFF5F5F7;
    static constexpr Argb surfaceContainerLow = 0xFFFFFFFF;
    static constexpr Argb surfaceContainerHigh = 0xFFF0F0F3;
    static constexpr Argb surfaceContainerHighest = 0xFFE5E5EA;
    static constexpr Argb onSurface           = 0xFF1D1D1F;
    static constexpr Argb onSurfaceVariant    = 0xFF636366;
    static constexpr Argb outline             = 0xFF8E8E93;
    static constexpr Argb outlineVariant      = 0xFFD1D1D6;
    static constexpr Argb inverseSurface      = 0xFF1D1D1F;
    static constexpr Argb inverseOnSurface    = 0xFFF5F5F7;
    static constexpr Argb error               = 0xFFB42318;
    static constexpr Argb errorContainer      = 0xFF8C1D18;
};

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

inline void Apply(const Theme&) {
    // Keep controls consistently blue; dark model palettes are unsuitable on white surfaces.
    const Theme theme{0xFF0071E3, 0xFFFFFFFF, 0xFFE5F0FF, 0xFF003366};
    auto& style = ImGui::GetStyle();
    ImVec4* c = style.Colors;

    // Fixed surface colors (Sony standard)
    c[ImGuiCol_WindowBg]        = ArgbToImVec4(FixedSurfaceColors::surface); // Opaque: the chrome shares it
    c[ImGuiCol_ChildBg]         = ArgbToImVec4(FixedSurfaceColors::surface, 0.0f);
    c[ImGuiCol_PopupBg]         = ArgbToImVec4(FixedSurfaceColors::surfaceContainerLow, glassEnabled ? 0.82f : 1.0f);
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
    c[ImGuiCol_Button]           = ArgbToImVec4(theme.primary, 0.10f);
    c[ImGuiCol_ButtonHovered]    = ArgbToImVec4(theme.primary, 0.17f);
    c[ImGuiCol_ButtonActive]     = ArgbToImVec4(theme.primary, 0.24f);

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
    c[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.12f, 0.14f, 0.18f, 0.16f);
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
