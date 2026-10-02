#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string>
#include <tuple>
#include <utility>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <mdr-c/Headphones.h>
#include <mdr/Protocol.hpp>
#include "Fonts/PlexSansIcon.h"
#include "MaterialYouTheme.hpp"
#include "PacketObserver.hpp"
#include "Platform/Platform.hpp"
#include "Settings.hpp"
#include "Localization.hpp"
#include "Headphones3D.hpp"
#ifdef MDR_CLIENT_DEBUGGER
#include "Debugger.hpp"
#endif

MDRHeadphones* gDevice;
mdr::String gHeadphonesError;
#ifdef MDR_CLIENT_DEBUGGER
bool gDebuggerOpen{};
bool gDebuggerOnlyMode{};
#endif

#pragma region Enum Names
const char* FormatAudioCodec(MDRAudioCodec codec)
{
    switch (codec)
    {
    case MDR_AUDIO_CODEC_UNKNOWN:
        return tr("<unsettled>");
    case MDR_AUDIO_CODEC_SBC:
        return tr("SBC");
    case MDR_AUDIO_CODEC_AAC:
        return tr("AAC");
    case MDR_AUDIO_CODEC_LDAC:
        return tr("LDAC");
    case MDR_AUDIO_CODEC_APTX:
        return tr("aptX");
    case MDR_AUDIO_CODEC_APTX_HD:
        return tr("aptX HD");
    case MDR_AUDIO_CODEC_LC3:
        return tr("LC3");
    default:
    case MDR_AUDIO_CODEC_OTHER:
        return tr("Unknown");
    }
}

const char* FormatDseeType(MDRDSEEType type)
{
    switch (type)
    {
    case MDR_DSEE_HX:
        return tr("DSEE HX");
    case MDR_DSEE_STANDARD:
        return tr("DSEE");
    case MDR_DSEE_HX_AI:
        return tr("DSEE HX AI");
    case MDR_DSEE_ULTIMATE:
        return tr("DSEE ULTIMATE");
    default:
        return tr("DSEE Unknown");
    }
}

const char* FormatChargingState(MDRChargingState status)
{
    switch (status)
    {
    case MDR_CHARGING_YES:
        return tr("Charging");
    case MDR_CHARGING_COMPLETE:
        return tr("Charged");
    case MDR_CHARGING_NO:
        return ""; // Hidden
    default:
    case MDR_CHARGING_UNKNOWN:
        return tr("Unknown");
    }
}

const char* FormatAdaptiveSensitivity(MDRAdaptiveSensitivity status)
{
    switch (status)
    {
    case MDR_ADAPTIVE_SENSITIVITY_STANDARD:
        return tr("Standard");
    case MDR_ADAPTIVE_SENSITIVITY_HIGH:
        return tr("High");
    case MDR_ADAPTIVE_SENSITIVITY_LOW:
        return tr("Low");
    default:
        return tr("Unknown");
    }
}

const char* FormatSpeechSensitivity(MDRSpeechSensitivity status)
{
    switch (status)
    {
    case MDR_SPEECH_SENSITIVITY_AUTO:
        return tr("Auto");
    case MDR_SPEECH_SENSITIVITY_HIGH:
        return tr("High");
    case MDR_SPEECH_SENSITIVITY_LOW:
        return tr("Low");
    default:
        return tr("Unknown");
    }
}

const char* FormatSpeakTimeout(MDRSpeakTimeout status)
{
    switch (status)
    {
    case MDR_SPEAK_TIMEOUT_SHORT:
        return tr("Short (~5s)");
    case MDR_SPEAK_TIMEOUT_MEDIUM:
        return tr("Standard (~15s)");
    case MDR_SPEAK_TIMEOUT_LONG:
        return tr("Long (~30s)");
    case MDR_SPEAK_TIMEOUT_MANUAL:
        return tr("Don't end automatically");
    default:
        return tr("Unknown");
    }
}

const char* FormatSourceSwitchControlResult(MDRSourceSwitchControlResult result)
{
    switch (result)
    {
    case MDR_SOURCE_SWITCH_CONTROL_FAILED_ON_CALL:
        return tr("The headphones refused: a call is in progress.");
    case MDR_SOURCE_SWITCH_CONTROL_FAILED_NOT_CONNECTED:
        return tr("The headphones refused: the device has no audio connection.");
    case MDR_SOURCE_SWITCH_CONTROL_FAILED_VOICE_ASSISTANT:
        return tr("The headphones refused: the voice assistant has priority.");
    default:
        return tr("The headphones refused the request.");
    }
}

const char* FormatEqualizerPreset(MDREqualizerPreset id)
{
    switch (id)
    {
    case MDR_EQ_OFF:
        return tr("Off");
    case MDR_EQ_ROCK:
        return tr("Rock");
    case MDR_EQ_POP:
        return tr("Pop");
    case MDR_EQ_JAZZ:
        return tr("Jazz");
    case MDR_EQ_DANCE:
        return tr("Dance");
    case MDR_EQ_EDM:
        return tr("EDM");
    case MDR_EQ_R_AND_B_HIP_HOP:
        return tr("R&B/Hip-Hop");
    case MDR_EQ_ACOUSTIC:
        return tr("Acoustic");
    case MDR_EQ_BRIGHT:
        return tr("Bright");
    case MDR_EQ_EXCITED:
        return tr("Excited");
    case MDR_EQ_MELLOW:
        return tr("Mellow");
    case MDR_EQ_RELAXED:
        return tr("Relaxed");
    case MDR_EQ_VOCAL:
        return tr("Vocal");
    case MDR_EQ_TREBLE:
        return tr("Treble");
    case MDR_EQ_BASS:
        return tr("Bass");
    case MDR_EQ_SPEECH:
        return tr("Speech");
    case MDR_EQ_HEAVY:
        return tr("Heavy");
    case MDR_EQ_CLEAR:
        return tr("Clear");
    case MDR_EQ_HARD:
        return tr("Hard");
    case MDR_EQ_SOFT:
        return tr("Soft");
    case MDR_EQ_GAMING:
        return tr("Gaming");
    case MDR_EQ_FPS_1:
        return tr("FPS 1");
    case MDR_EQ_FPS_2:
        return tr("FPS 2");
    case MDR_EQ_FPS_3:
        return tr("FPS 3");
    case MDR_EQ_CUSTOM:
        return tr("Custom");
    case MDR_EQ_USER_1:
        return tr("User Setting 1");
    case MDR_EQ_USER_2:
        return tr("User Setting 2");
    case MDR_EQ_USER_3:
        return tr("User Setting 3");
    case MDR_EQ_USER_4:
        return tr("User Setting 4");
    case MDR_EQ_USER_5:
        return tr("User Setting 5");
    default:
        return tr("Unknown");
    }
}

const char* FormatAssignableActionKeyLocation(const MDRAssignableControl& control)
{
    switch (control.location)
    {
    case MDR_ASSIGNABLE_ACTION_KEY_LEFT:
        return control.type == MDR_ASSIGNABLE_ACTION_KEY_TYPE_TOUCH_SENSOR ? tr("Left Touch") : tr("Left Button");
    case MDR_ASSIGNABLE_ACTION_KEY_RIGHT:
        return control.type == MDR_ASSIGNABLE_ACTION_KEY_TYPE_TOUCH_SENSOR ? tr("Right Touch") : tr("Right Button");
    case MDR_ASSIGNABLE_ACTION_KEY_CUSTOM:
        return tr("[CUSTOM] Button");
    default:
        return tr("Unknown");
    }
}

const char* FormatAssignableAction(MDRAssignableAction action)
{
    switch (action)
    {
    case MDR_ASSIGNABLE_NOISE_CONTROL:
        return tr("Ambient Sound Control");
    case MDR_ASSIGNABLE_PLAYBACK:
        return tr("Playback Control");
    case MDR_ASSIGNABLE_TRACK_CONTROL:
        return tr("Track Control");
    case MDR_ASSIGNABLE_VOICE_RECOGNITION:
        return tr("Voice Recognition");
    case MDR_ASSIGNABLE_GOOGLE_ASSISTANT:
        return tr("Google Assistant");
    case MDR_ASSIGNABLE_AMAZON_ALEXA:
        return tr("Amazon Alexa");
    case MDR_ASSIGNABLE_TENCENT_XIAOWEI:
        return tr("Tencent Xiaowei");
    case MDR_ASSIGNABLE_MICROSOFT_CORTANA:
        return tr("Microsoft Cortana");
    case MDR_ASSIGNABLE_NOISE_CONTROL_QUICK_ACCESS:
        return tr("Ambient Sound Control");
    case MDR_ASSIGNABLE_QUICK_ACCESS:
        return tr("Quick Access");
    case MDR_ASSIGNABLE_NONE:
        return tr("No Function");
    default:
        return tr("Unknown");
    }
}

const char* FormatNoiseButtonMode(MDRNoiseButtonMode function)
{
    switch (function)
    {
    case MDR_NOISE_BUTTON_NONE:
        return tr("No Function");
    case MDR_NOISE_BUTTON_NOISE_AMBIENT_OFF:
        return tr("NC-ASM-OFF");
    case MDR_NOISE_BUTTON_NOISE_AMBIENT:
        return tr("NC-ASM");
    case MDR_NOISE_BUTTON_NOISE_OFF:
        return tr("NC-OFF");
    case MDR_NOISE_BUTTON_AMBIENT_OFF:
        return tr("ASM-OFF");
    default:
        return tr("Unknown");
    }
}

const char* FormatAutoPowerOff(uint32_t minutes)
{
    switch (minutes)
    {
    case 5:
        return tr("5 minutes of no Bluetooth connection");
    case 15:
        return tr("15 minutes of no Bluetooth connection");
    case 30:
        return tr("30 minutes of no Bluetooth connection");
    case 60:
        return tr("1 hour of no Bluetooth connection");
    case 180:
        return tr("3 hours of no Bluetooth connection");
    case 0:
        return tr("Do not turn off");
    default:
        return tr("Unknown");
    }
}

const char* FormatFeatureAvailability(MDRFeatureAvailability availability)
{
    switch (availability)
    {
    case MDR_AVAILABILITY_AVAILABLE:
        return PSI_OK;
    case MDR_AVAILABILITY_UNAVAILABLE:
        return PSI_REMOVE;
    default:
        return "?";
    }
}
#pragma endregion

bool FeatureAvailable(MDRFeature feature)
{
    MDRFeatureAvailability availability = MDR_AVAILABILITY_UNKNOWN;
    return gDevice && mdrHeadphonesGetFeature(gDevice, feature, &availability) == MDR_RESULT_OK &&
        availability == MDR_AVAILABILITY_AVAILABLE;
}

mdr::String GetText(MDRText text, uint32_t index = 0)
{
    if (!gDevice)
        return {};
    uint32_t size = 0;
    if (mdrHeadphonesGetText(gDevice, text, index, nullptr, &size) != MDR_RESULT_OK || size == 0)
        return {};
    mdr::Vector<char> buffer(size);
    if (mdrHeadphonesGetText(gDevice, text, index, buffer.data(), &size) != MDR_RESULT_OK)
        return {};
    return buffer.data();
}

uint8_t GetModelColor()
{
    MDRModel identity{};
    return gDevice && mdrHeadphonesGetModel(gDevice, &identity) == MDR_RESULT_OK ? identity.model_color : 0;
}

mdr::Vector<MDRBattery> GetBatteries()
{
    mdr::Vector<MDRBattery> values(4);
    uint32_t count = static_cast<uint32_t>(values.size());
    if (!gDevice || mdrHeadphonesGetBatteries(gDevice, values.data(), &count) != MDR_RESULT_OK)
        return {};
    values.resize(count);
    return values;
}

mdr::Vector<MDRPairedDevice> GetPairedDevices()
{
    mdr::Vector<MDRPairedDevice> values(16);
    uint32_t count = static_cast<uint32_t>(values.size());
    if (!gDevice || mdrHeadphonesGetPairedDevices(gDevice, values.data(), &count) != MDR_RESULT_OK)
        return {};
    values.resize(count);
    return values;
}

mdr::Vector<MDRGeneralSettingInfo> GetGeneralSettingInfos()
{
    mdr::Vector<MDRGeneralSettingInfo> values(4);
    uint32_t count = static_cast<uint32_t>(values.size());
    if (!gDevice || mdrHeadphonesGetGeneralSettingInfo(gDevice, values.data(), &count) != MDR_RESULT_OK)
        return {};
    values.resize(count);
    return values;
}

mdr::Vector<std::pair<MDRGeneralSettingInfo, MDRGeneralSetting>> GetGeneralSettings(const mdr::Vector<MDRGeneralSettingInfo>& infos)
{
    mdr::Vector<std::pair<MDRGeneralSettingInfo, MDRGeneralSetting>> values;
    values.reserve(infos.size());
    for (const MDRGeneralSettingInfo& info : infos)
    {
        MDRGeneralSetting setting{};
        if (!gDevice || mdrHeadphonesGetGeneralSetting(gDevice, info.index, &setting) != MDR_RESULT_OK)
            continue;
        values.emplace_back(info, setting);
    }
    return values;
}

mdr::Vector<MDRAssignableControl> GetAssignableControls()
{
    mdr::Vector<MDRAssignableControl> values(2); // Custom only, or Left+Right
    uint32_t count = static_cast<uint32_t>(values.size());
    if (!gDevice || mdrHeadphonesGetAssignableControls(gDevice, values.data(), &count) != MDR_RESULT_OK)
        return {};
    values.resize(count);
    return values;
}

mdr::Vector<MDRAssignableAction> GetAssignableControlActions(MDRAssignableActionKeyLocation key)
{
    mdr::Vector<MDRAssignableAction> values(7); // Number of MDRAssignableAction enum values
    uint32_t count = static_cast<uint32_t>(values.size());
    if (!gDevice || mdrHeadphonesGetAssignableControlActions(gDevice, key, values.data(), &count) != MDR_RESULT_OK)
        return {};
    values.resize(count);
    return values;
}

mdr::Vector<int> GetEqualizerBands()
{
    mdr::Vector<int8_t> bytes(16);
    uint32_t count = static_cast<uint32_t>(bytes.size());
    if (!gDevice || mdrHeadphonesGetEqualizerBands(gDevice, bytes.data(), &count) != MDR_RESULT_OK)
        return {};
    bytes.resize(count);
    mdr::Vector<int> values;
    values.reserve(count);
    for (const int8_t value : bytes)
        values.emplace_back(value);
    return values;
}

void SetEqualizerBands(const mdr::Vector<int>& values)
{
    mdr::Vector<int8_t> bytes;
    bytes.reserve(values.size());
    for (const int value : values)
        bytes.emplace_back(static_cast<int8_t>(value));
    if (!bytes.empty())
        mdrHeadphonesSetEqualizerBands(gDevice, bytes.data(), static_cast<uint32_t>(bytes.size()));
}

struct ClientState
{
    MDRModel mModel{};
    mdr::Vector<MDRBattery> mBatteries;
    MDRPlayback mPlayback{};
    MDRNoiseControl mNoise{};
    MDRSpeakToChat mSpeakToChat{};
    MDRListening mListening{};
    MDREqualizer mEqualizer{};
    mdr::Vector<int> mEqualizerBands;
    mdr::Vector<MDRPairedDevice> mPairedDevices;
    MDRPairing mPairing{};
    mdr::Vector<std::pair<MDRGeneralSettingInfo, MDRGeneralSetting>> mGeneralSettings;
    bool mModelAvailable;
    bool mNoiseAvailable;
    bool mSpeakToChatAvailable;
    bool mListeningAvailable;
    bool mEqualizerAvailable;
    bool mPairingAvailable;
    bool mPlaybackVolumeStaged;
    // Set to true to update batteries, etc for V2 and  playback vol/metadata once headphones become available.
    bool mPendingSync;
} gState;

void RefreshPlaybackState()
{
    MDRPlayback playback{};
    if (mdrHeadphonesGetPlayback(gDevice, &playback) != MDR_RESULT_OK)
        return;
    gState.mPlayback.status = playback.status;
    if (gState.mPlaybackVolumeStaged && playback.volume == gState.mPlayback.volume)
        gState.mPlaybackVolumeStaged = false;
    if (!gState.mPlaybackVolumeStaged)
        gState.mPlayback.volume = playback.volume;
}

void RefreshClientState()
{
    gState = {};
    gState.mModelAvailable = mdrHeadphonesGetModel(gDevice, &gState.mModel) == MDR_RESULT_OK;
    gState.mBatteries = GetBatteries();
    RefreshPlaybackState();
    gState.mNoiseAvailable = mdrHeadphonesGetNoiseControl(gDevice, &gState.mNoise) == MDR_RESULT_OK;
    gState.mSpeakToChatAvailable = mdrHeadphonesGetSpeakToChat(gDevice, &gState.mSpeakToChat) == MDR_RESULT_OK;
    gState.mListeningAvailable = mdrHeadphonesGetListening(gDevice, &gState.mListening) == MDR_RESULT_OK;
    gState.mEqualizerAvailable = mdrHeadphonesGetEqualizer(gDevice, &gState.mEqualizer) == MDR_RESULT_OK;
    gState.mEqualizerBands = GetEqualizerBands();
    gState.mPairedDevices = GetPairedDevices();
    gState.mPairingAvailable = mdrHeadphonesGetPairing(gDevice, &gState.mPairing) == MDR_RESULT_OK;
    gState.mGeneralSettings = GetGeneralSettings(GetGeneralSettingInfos());
}

void CloseDevice()
{
    if (!gDevice)
        return;
    gHeadphonesError = GetText(MDR_TEXT_LAST_ERROR);
    clientPacketObserverDetach();
    mdrHeadphonesDestroy(gDevice);
    gDevice = nullptr;
    gState = {};
    clientPlatformSystemVolumeUnbind();
}

#pragma region ImGui Extra
constexpr ImGuiWindowFlags kImWindowFlagsTopMost =
    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar;

// -- https://github.com/ocornut/imgui/issues/3379#issuecomment-2943903877
void ImScrollWhenDraggingOnVoid(const ImVec2& delta, ImGuiMouseButton mouse_button)
{
    using namespace ImGui;

    ImGuiContext& g = *GetCurrentContext();
    ImGuiWindow* window = g.CurrentWindow;
    ImGuiID id = window->GetID("##scrolldraggingoverlay");
    KeepAliveID(id);

    // Passing 0 to ItemHoverable means it doesn't set HoveredId, which is what we want.
    if (g.ActiveId == 0 && ItemHoverable(window->Rect(), 0, g.CurrentItemFlags) &&
        IsMouseClicked(mouse_button, ImGuiInputFlags_None, id))
        SetActiveID(id, window);
    if (g.ActiveId == id && !g.IO.MouseDown[mouse_button])
        ClearActiveID();

    // Set keep underlying highlight. However, mouse not necessarily hovering same item creates a weird disconnect.
    // if (g.ActiveId == id)
    //    g.ActiveIdAllowOverlap = true;

    // if (g.ActiveId == id && delta.x != 0.0f)
    //     SetScrollX(window, window->Scroll.x + delta.x);
    if (g.ActiveId == id && delta.y != 0.0f)
        SetScrollY(window, window->Scroll.y - delta.y);
}

void ImScrollWhenDraggingAnywhere(const ImVec2& delta, ImGuiMouseButton mouse_button)
{
    ImGuiContext& g = *ImGui::GetCurrentContext();
    const bool backup_hovered_id_allow_overlap = g.HoveredIdAllowOverlap;
    g.HoveredIdAllowOverlap = true;
    ImScrollWhenDraggingOnVoid(delta, mouse_button);
    g.HoveredIdAllowOverlap = backup_hovered_id_allow_overlap; // As we know ScrollWhenDraggingOnVoid() doesn't changed
                                                               // HoveredId we can unconditionally restore.
}
// --

// Only useful if you're manipulating the DrawList which has positions
// that are _NOT_ window local
std::tuple<ImVec2, ImVec2, ImDrawList*> ImWindowDrawOffsetRegionList()
{
    ImVec2 offset = ImGui::GetCursorScreenPos();
    ImVec2 region = ImGui::GetContentRegionAvail();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    return {offset, region, drawList};
}

// Centered text.
void ImTextCentered(const char* text)
{
    ImVec2 size = ImGui::CalcTextSize(text);
    ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x / 2 - size.x / 2 + ImGui::GetStyle().FramePadding.x);
    ImGui::Text("%s", text);
}

// Generate linear, monotonous ints of [0, count - 1] at interval of intervalMS
int ImBlink(int intervalMS, int count)
{
    size_t time = ImGui::GetTime() * 1000;
    time = time % (intervalMS * count);
    return time / intervalMS;
}

// Generate linear, monotonous float in range of [0, 1] at interval of intervalMS
float ImBlinkF(float intervalMS)
{
    float time = ImGui::GetTime();
    intervalMS /= 1000.0f;
    time = fmod(time, intervalMS);
    return time / intervalMS;
}

// CSS linear easing function on x of range [0,1]
constexpr float ImEaseLinear(float x) { return x; }

// CSS easeInOutCubic easing function on x of range [0,1]
constexpr float ImEaseInOutCubic(float x)
{
    return x < 0.5f ? 4 * pow(x, 3.0f) : 1.0f - pow(-2.0f * x + 2.0f, 3.0f) / 2.0f;
}

// Your next favourite spinner
void ImSpinner(float interval, float size, int color, float thickness = 1.0f, bool centerX = false,
               bool centerY = false, float cycles = 1.0f, float (*easing)(float) = ImEaseLinear)
{
    constexpr ImVec2 kPoints[] = {{-1, 1}, {-1, -1}, {1, -1}, {1, 1}};
    auto& style = ImGui::GetStyle();
    ImVec2 points[std::size(kPoints)];
    if (centerX)
        ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x / 2 - size / 2);
    if (centerY)
        ImGui::SetCursorPosY((ImGui::GetTextLineHeight() + style.FramePadding.y * 2 - size) / 2);
    auto [offset, region, draw] = ImWindowDrawOffsetRegionList();
    float t = ImBlinkF(interval), theta = easing(t) * acos(-1) * cycles;
    for (int i = 0; auto p : kPoints)
    {
        auto& pp = points[i++] = {
            p.x * cos(theta) - p.y * sin(theta),
            p.x * sin(theta) + p.y * cos(theta),
        };
        pp *= size, pp += offset, pp.x += size, pp.y += size;
    }
    draw->AddPolyline(points, std::size(kPoints), color, ImDrawFlags_Closed, thickness);
    ImGui::Dummy({sqrt(2.0f) * size, sqrt(2.0f) * size + style.FramePadding.y * 2.0f});
}

// A checkbox is sized from the frame padding, so the 40pt control height would turn it into a
// block. Apple keeps selection controls smaller than buttons; this does the same.
bool ImCheckbox(const char* label, bool* value)
{
    const ImVec2 padding = ImGui::GetStyle().FramePadding;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(padding.x, padding.y * 0.5f));
    // A box this small needs more contrast than the wide control frames: the default frame tone
    // vanishes against the glass surface.
    ImGui::PushStyleColor(ImGuiCol_FrameBg,
                          MaterialYouTheme::ArgbToImVec4(MaterialYouTheme::FixedSurfaceColors::outlineVariant));
    const bool changed = ImGui::Checkbox(label, value);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    return changed;
}

extern ImFont* clientHeadingFont(); // SDLMain.cpp: the body faces, emboldened

// Heading weight at a size relative to the body size; the body font stands in when the heading
// font is not available yet.
void ImPushHeading(float scale = 1.0f) { ImGui::PushFont(clientHeadingFont(), ImGui::GetFontSize() * scale); }
void ImPopHeading() { ImGui::PopFont(); }

// Section heading that sits close to the block it introduces: roomier above, tight below.
void ImSectionHeading(const char* label)
{
    ImPushHeading(0.95f);
    ImGui::SeparatorText(label);
    ImPopHeading();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().ItemSpacing.y * 0.75f);
}


#pragma region Motion & controls
// Every animation in the app runs through these, so "Interface animations" off means instant.
float ImEaseOutCubic(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    const float u = 1.0f - t;
    return 1.0f - u * u * u;
}

float ImEaseOutBack(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    constexpr float c1 = 1.70158f, c3 = c1 + 1.0f;
    const float u = t - 1.0f;
    return 1.0f + c3 * u * u * u + c1 * u * u;
}

// Frame-rate independent exponential approach: `speed` is how quickly the remaining distance
// collapses (about two thirds of it per 1/speed seconds).
float ImApproach(float current, float target, float speed)
{
    if (!clientSettings().animations)
        return target;
    const float k = 1.0f - std::exp(-speed * ImGui::GetIO().DeltaTime);
    const float next = current + (target - current) * k;
    return std::fabs(next - target) < 0.0005f ? target : next;
}

// A per-widget animated value kept in the window's storage under `id`.
float ImAnimValue(ImGuiID id, float target, float speed, float initial)
{
    float* value = ImGui::GetStateStorage()->GetFloatRef(id, clientSettings().animations ? initial : target);
    *value = ImApproach(*value, target, speed);
    return *value;
}
float ImAnimValue(ImGuiID id, float target, float speed) { return ImAnimValue(id, target, speed, target); }

// Reveal: content fades in and slides up when a screen (clock 0) or a tab (clock 1) first shows.
// Blocks with a higher index start a beat later. Always pair Begin with End.
namespace
{
    double gRevealStart[2] = {-10.0, -10.0};
}
void ImRevealRestart(int clock) { gRevealStart[clock] = ImGui::GetTime(); }
float ImRevealProgress(int clock, int index)
{
    if (!clientSettings().animations)
        return 1.0f;
    const double age = ImGui::GetTime() - gRevealStart[clock] - index * 0.07;
    return ImEaseOutCubic(static_cast<float>(age / 0.36));
}
void ImRevealBegin(int clock, int index)
{
    const float progress = ImRevealProgress(clock, index);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * progress);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (1.0f - progress) * ImGui::GetFontSize() * 0.9f);
}
void ImRevealEnd() { ImGui::PopStyleVar(); }

// Button whose hover tint eases in and out instead of snapping.
bool ImButtonSmooth(const char* label, const ImVec2& size)
{
    const ImGuiID id = ImGui::GetID(label);
    ImGuiStorage* storage = ImGui::GetStateStorage();
    const bool wasHovered = storage->GetBool(id + 1, false);
    const float t = ImAnimValue(id + 2, wasHovered ? 1.0f : 0.0f, 14.0f);
    const ImVec4 blended = ImLerp(ImGui::GetStyleColorVec4(ImGuiCol_Button),
                                  ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), t);
    ImGui::PushStyleColor(ImGuiCol_Button, blended);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, blended);
    const bool pressed = ImGui::Button(label, size);
    ImGui::PopStyleColor(2);
    storage->SetBool(id + 1, ImGui::IsItemHovered());
    return pressed;
}

// Settings rows: label on the left, the control at the right edge. Draw the control right after
// ImRowLabel. One shared control width keeps the column of controls lined up.
float ImRowControlWidth() { return ImGui::GetFontSize() * 12.0f; }
// Rows remember where they expect to end; a row that starts exactly there draws a hairline above
// itself, so consecutive rows separate the way an inset list does and headings stay clean.
void ImRowDivider()
{
    float* expectedTop = ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("##rowNext"), -1.0f);
    const float y = ImGui::GetCursorPosY();
    if (std::fabs(*expectedTop - y) < 0.5f)
    {
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const float half = ImGui::GetStyle().ItemSpacing.y * 0.5f;
        ImGui::GetWindowDrawList()->AddLine({pos.x, pos.y - half}, {pos.x + ImGui::GetContentRegionAvail().x, pos.y - half},
                                            MaterialYouTheme::ArgbToImU32(MaterialYouTheme::FixedSurfaceColors::outlineVariant,
                                                                          ImGui::GetStyle().Alpha * 0.8f));
    }
    *expectedTop = y + ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y;
}
void ImRowLabel(const char* label, float controlWidth)
{
    ImRowDivider();
    const float right = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
    ImGui::AlignTextToFramePadding();
    ImGui::PushTextWrapPos(right - controlWidth - ImGui::GetStyle().ItemSpacing.x);
    ImGui::TextUnformatted(label);
    ImGui::PopTextWrapPos();
    ImGui::SameLine(right - controlWidth);
}
// A muted explanation under a row.
void ImRowHint(const char* text)
{
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("%s", text);
    ImGui::PopTextWrapPos();
    // The hint belongs to the row above; the next row separates from it.
    *ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("##rowNext"), -1.0f) = ImGui::GetCursorPosY();
}

// iOS-style switch: the knob glides and the track colour cross-fades.
bool ImToggle(const char* id, bool* value)
{
    const float unit = ImGui::GetFontSize();
    const float h = unit * 1.45f, w = h * 1.72f;
    const float rowHeight = std::max(h, ImGui::GetFrameHeight());
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton(id, ImVec2(w, rowHeight));
    if (pressed)
        *value = !*value;
    const float t = ImAnimValue(ImGui::GetItemID(), *value ? 1.0f : 0.0f, 16.0f);
    const float alpha = ImGui::GetStyle().Alpha;
    ImVec4 track = ImLerp(MaterialYouTheme::ArgbToImVec4(MaterialYouTheme::FixedSurfaceColors::toggleOff),
                          MaterialYouTheme::ArgbToImVec4(MaterialYouTheme::AccentTheme().primary), t);
    track.w *= alpha;
    const ImVec2 min(origin.x, origin.y + (rowHeight - h) * 0.5f);
    const ImVec2 max = min + ImVec2(w, h);
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(min, max, ImGui::ColorConvertFloat4ToU32(track), h * 0.5f);
    const float knobRadius = h * 0.5f - unit * 0.12f;
    const ImVec2 knob(min.x + h * 0.5f + t * (w - h), min.y + h * 0.5f);
    draw->AddCircleFilled(knob + ImVec2(0.0f, unit * 0.06f), knobRadius, IM_COL32(0, 0, 0, static_cast<int>(45 * alpha)));
    draw->AddCircleFilled(knob, knobRadius, IM_COL32(255, 255, 255, static_cast<int>(255 * alpha)));
    return pressed;
}

// A settings row with a switch on the right.
bool ImToggleRow(const char* label, bool* value)
{
    ImGui::PushID(label);
    ImRowLabel(label, ImGui::GetFontSize() * 1.45f * 1.72f);
    const bool changed = ImToggle("##toggle", value);
    ImGui::PopID();
    return changed;
}

// Segmented control whose pill slides to the chosen segment. Returns the (new) index; pass -1 for
// "nothing selected" (the pill fades out). Hairlines separate the idle segments the way iOS does,
// and a label that would not fit its segment is drawn smaller rather than spilling over the edge.
int ImSegmented(const char* id, std::span<const char* const> labels, int selected, float width = 0.0f)
{
    const int count = static_cast<int>(labels.size());
    if (count == 0)
        return selected;
    ImGui::PushID(id);
    if (width <= 0.0f)
        width = ImGui::GetContentRegionAvail().x;
    width = std::max(width, static_cast<float>(count));
    const float h = ImGui::GetFrameHeight();
    const float segment = width / count;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float alpha = ImGui::GetStyle().Alpha;
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(pos, pos + ImVec2(width, h),
                        MaterialYouTheme::ArgbToImU32(MaterialYouTheme::FixedSurfaceColors::segmentTrack, alpha), h * 0.5f);
    int result = selected < 0 ? -1 : std::clamp(selected, 0, count - 1);
    for (int i = 0; i < count; ++i)
    {
        ImGui::SetCursorScreenPos(pos + ImVec2(i * segment, 0.0f));
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("##segment", ImVec2(segment, h)))
            result = i;
        ImGui::PopID();
    }
    ImGuiStorage* storage = ImGui::GetStateStorage();
    int* pillIndex = storage->GetIntRef(ImGui::GetID("pillIndex"), std::max(result, 0));
    if (result >= 0)
        *pillIndex = result;
    const float shown = ImAnimValue(ImGui::GetID("pillShown"), result >= 0 ? 1.0f : 0.0f, 14.0f);
    const float x = ImAnimValue(ImGui::GetID("pill"), *pillIndex * segment, 18.0f, *pillIndex * segment);
    const float inset = ImGui::GetFontSize() * 0.12f;
    for (int i = 1; i < count; ++i)
    {
        const float dx = i * segment;
        const bool underPill = shown > 0.5f && dx > x - 1.0f && dx < x + segment + 1.0f;
        if (!underPill)
            draw->AddLine({pos.x + dx, pos.y + h * 0.28f}, {pos.x + dx, pos.y + h * 0.72f},
                          MaterialYouTheme::ArgbToImU32(MaterialYouTheme::FixedSurfaceColors::outline, alpha * 0.5f));
    }
    if (shown > 0.001f)
    {
        const ImVec2 pillMin = pos + ImVec2(x + inset, inset), pillMax = pos + ImVec2(x + segment - inset, h - inset);
        const float pillRounding = (h - inset * 2.0f) * 0.5f;
        draw->AddRectFilled(pillMin + ImVec2(0.0f, inset * 0.5f), pillMax + ImVec2(0.0f, inset * 0.5f),
                            IM_COL32(0, 0, 0, static_cast<int>(28 * alpha * shown)), pillRounding);
        draw->AddRectFilled(pillMin, pillMax,
                            MaterialYouTheme::ArgbToImU32(MaterialYouTheme::FixedSurfaceColors::segmentPill, alpha * shown), pillRounding);
    }
    const float maxLabelWidth = segment - inset * 4.0f;
    for (int i = 0; i < count; ++i)
    {
        // The label darkens and gains weight as the pill arrives under it.
        const float onPill = std::clamp(1.0f - std::fabs(x - i * segment) / segment, 0.0f, 1.0f) * shown;
        ImFont* font = onPill > 0.5f && clientHeadingFont() ? clientHeadingFont() : ImGui::GetFont();
        float fontSize = ImGui::GetFontSize();
        ImVec2 size = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, labels[i]);
        if (size.x > maxLabelWidth)
        {
            fontSize *= std::max(0.6f, maxLabelWidth / size.x);
            size = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, labels[i]);
        }
        const ImVec2 at = pos + ImVec2(i * segment + (segment - size.x) * 0.5f, (h - size.y) * 0.5f);
        ImVec4 colour = ImLerp(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled), ImGui::GetStyleColorVec4(ImGuiCol_Text), onPill);
        colour.w *= alpha;
        draw->PushClipRect(pos + ImVec2(i * segment, 0.0f), pos + ImVec2((i + 1) * segment, h), true);
        draw->AddText(font, fontSize, at, ImGui::ColorConvertFloat4ToU32(colour), labels[i]);
        draw->PopClipRect();
    }
    ImGui::SetCursorScreenPos(pos);
    ImGui::Dummy(ImVec2(width, h));
    ImGui::PopID();
    return result;
}

// Smooth wheel scrolling for a child created with ImGuiWindowFlags_NoScrollWithMouse: the wheel
// moves a target and the view eases towards it. Call just before EndChild.
void ImSmoothScroll()
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImGuiStorage* storage = ImGui::GetStateStorage();
    float* target = storage->GetFloatRef(ImGui::GetID("##scrollTarget"), window->Scroll.y);
    float* applied = storage->GetFloatRef(ImGui::GetID("##scrollApplied"), window->Scroll.y);
    if (window->Scroll.y != *applied) // Something else moved it (a drag, a layout change): follow it
        *target = window->Scroll.y;
    const float wheel = ImGui::GetIO().MouseWheel;
    if (wheel != 0.0f && ImGui::TestKeyOwner(ImGuiKey_MouseWheelY, ImGuiKeyOwner_NoOwner))
    {
        // Ours unless a scrollable child under the mouse takes it first.
        bool ours = false;
        for (ImGuiWindow* w = ImGui::GetCurrentContext()->HoveredWindow; w; w = w->ParentWindow)
        {
            if (w == window)
            {
                ours = true;
                break;
            }
            if (w->ScrollMax.y > 0.0f && !(w->Flags & ImGuiWindowFlags_NoScrollWithMouse))
                break;
        }
        if (ours)
            *target -= wheel * ImGui::GetFontSize() * 4.0f;
    }
    *target = std::clamp(*target, 0.0f, window->ScrollMax.y);
    const float next = std::fabs(*target - window->Scroll.y) < 0.5f ? *target : ImApproach(window->Scroll.y, *target, 16.0f);
    if (next != window->Scroll.y)
        ImGui::SetScrollY(next);
    *applied = next;
}

// Three bars bouncing to nothing in particular: "something is playing".
void ImPlayingBars(ImU32 colour)
{
    const float unit = ImGui::GetFontSize();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float h = ImGui::GetTextLineHeight(), w = unit * 0.18f, gap = unit * 0.1f;
    const bool animations = clientSettings().animations;
    const float time = animations ? static_cast<float>(ImGui::GetTime()) : 0.0f;
    auto* draw = ImGui::GetWindowDrawList();
    for (int i = 0; i < 3; ++i)
    {
        const float level = animations ? 0.35f + 0.65f * (0.5f + 0.5f * std::sin(time * (5.0f + i * 1.3f) + i * 1.9f)) : 0.6f;
        const float x = pos.x + i * (w + gap);
        draw->AddRectFilled({x, pos.y + h * (1.0f - level)}, {x + w, pos.y + h}, colour, w * 0.5f);
    }
    ImGui::Dummy({w * 3.0f + gap * 2.0f, h});
}
#pragma endregion

// A slider drawn the iOS way: a thin track, the travelled part in the accent colour, a round knob
// with a shadow. Same contract as ImGui::SliderInt; the optional printf-style format is shown
// above the track, right-aligned. The knob follows the pointer while dragging and eases to values
// that arrive from the wheel, the keys or the device.
bool ImSliderInt(const char* label, int* value, int min, int max, const char* format = nullptr)
{
    const float unit = ImGui::GetFontSize();
    const float width = std::max(ImGui::CalcItemWidth(), unit * 4.0f);
    const float trackH = unit * 0.35f, knobR = unit * 0.62f;
    const bool hasLabel = format && *format;
    const float labelH = hasLabel ? ImGui::GetTextLineHeight() + unit * 0.15f : 0.0f;
    const float h = labelH + std::max(ImGui::GetFrameHeight(), knobR * 2.0f + unit * 0.3f);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(label, ImVec2(width, h));
    const ImGuiID id = ImGui::GetItemID();
    const bool active = ImGui::IsItemActive(), hovered = ImGui::IsItemHovered();
    const float trackY = origin.y + labelH + (h - labelH) * 0.5f;
    const float x0 = origin.x + knobR, x1 = origin.x + width - knobR;
    const int span = std::max(1, max - min);
    bool changed = false;
    if (active)
    {
        const float t = std::clamp((ImGui::GetIO().MousePos.x - x0) / std::max(1.0f, x1 - x0), 0.0f, 1.0f);
        const int next = min + static_cast<int>(std::lround(t * span));
        if (next != *value)
        {
            *value = next;
            changed = true;
            ImGui::MarkItemEdited(id);
        }
    }
    const float target = static_cast<float>(std::clamp(*value, min, max) - min) / static_cast<float>(span);
    float* eased = ImGui::GetStateStorage()->GetFloatRef(id + 1, target);
    if (active)
        *eased = target; // Under the pointer: no easing, and nothing to catch up on after release
    const float t = active ? target : (*eased = ImApproach(*eased, target, 22.0f));
    const float alpha = ImGui::GetStyle().Alpha;
    auto* draw = ImGui::GetWindowDrawList();
    const ImVec2 trackMin(origin.x, trackY - trackH * 0.5f), trackMax(origin.x + width, trackY + trackH * 0.5f);
    draw->AddRectFilled(trackMin, trackMax,
                        MaterialYouTheme::ArgbToImU32(MaterialYouTheme::FixedSurfaceColors::segmentTrack, alpha), trackH * 0.5f);
    const float knobX = x0 + t * (x1 - x0);
    draw->AddRectFilled(trackMin, ImVec2(std::max(knobX, trackMin.x + trackH), trackMax.y),
                        MaterialYouTheme::ArgbToImU32(MaterialYouTheme::AccentTheme().primary, alpha), trackH * 0.5f);
    const float grow = ImAnimValue(id + 2, (hovered || active) ? 1.0f : 0.0f, 16.0f);
    const float r = knobR * (1.0f + 0.12f * grow);
    draw->AddCircleFilled({knobX, trackY + unit * 0.08f}, r, IM_COL32(0, 0, 0, static_cast<int>(55 * alpha)), 32);
    draw->AddCircleFilled({knobX, trackY}, r, IM_COL32(255, 255, 255, static_cast<int>(255 * alpha)), 32);
    draw->AddCircle({knobX, trackY}, r, IM_COL32(0, 0, 0, static_cast<int>(28 * alpha)), 32);
    if (hasLabel)
    {
        char text[160];
        std::snprintf(text, sizeof(text), format, *value);
        const ImVec2 size = ImGui::CalcTextSize(text);
        draw->AddText({origin.x + width - size.x, origin.y}, ImGui::GetColorU32(ImGuiCol_TextDisabled), text);
    }
    return changed;
}

// A combo with a quiet chevron in place of ImGui's arrow button. Same use as BeginCombo/EndCombo.
bool ImBeginComboRow(const char* id, const char* preview, float width)
{
    const float unit = ImGui::GetFontSize();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float h = ImGui::GetFrameHeight();
    ImDrawList* draw = ImGui::GetWindowDrawList(); // The row's list, whatever window the popup opens
    ImGui::SetNextItemWidth(width);
    const bool open = ImGui::BeginCombo(id, preview, ImGuiComboFlags_NoArrowButton);
    const ImVec2 size = ImGui::CalcTextSize(PSI_CHEVRON_DOWN);
    draw->AddText({pos.x + width - size.x - unit * 0.7f, pos.y + (h - size.y) * 0.5f},
                  ImGui::GetColorU32(ImGuiCol_TextDisabled), PSI_CHEVRON_DOWN);
    return open;
}

// Fill the available horizontal region with lineTotal amount of buttons
// This is used for modal dialogues
bool ImModalButton(const char* label, int lineIndex = 0, int lineTotal = 1)
{
    assert(lineIndex < lineTotal);
    auto& style = ImGui::GetStyle();
    if (lineIndex)
        ImGui::SameLine();
    const int remaining = lineTotal - lineIndex;
    const float width = (ImGui::GetContentRegionAvail().x - style.ItemSpacing.x * (remaining - 1)) / remaining;
    return ImButtonSmooth(label, ImVec2{std::max(1.0f, width), 0});
}

// The discovery and connecting screens are drawn straight on the window surface as a centred,
// scrollable column. They used to be modals, which put a second rounded card (with its own
// background and a dark gutter) inside the already custom-framed window.
extern bool gContentScrolled; // SDLMain.cpp
bool ImBeginScreenColumn(const char* id)
{
    const float avail = ImGui::GetContentRegionAvail().x;
    const float width = std::max(1.0f, std::min(avail, ImGui::GetFontSize() * 44));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - width) * 0.5f);
    // No padding of its own: the main window already provides it, and doubling it up pushed the
    // content 40px down and 48px in from each side.
    const bool open = ImGui::BeginChild(id, {width, 0}, ImGuiChildFlags_None,
                                        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollWithMouse);
    if (open)
        gContentScrolled = ImGui::GetScrollY() > 1.0f;
    return open;
}

extern float clientWindowChromeHeight();
void ImSetNextWindowCentered()
{
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float margin = std::max(ImGui::GetStyle().WindowPadding.x, clientWindowChromeHeight());
    const float width = std::max(1.0f, std::min(display.x - margin * 2, ImGui::GetFontSize() * 44));
    ImGui::SetNextWindowPos(display * 0.5f, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({width, 0});
    ImGui::SetNextWindowSizeConstraints({width, 0}, {width, std::max(1.0f, display.y - margin * 2)});
}

void ImTextWithBorder(const char* text, int color, float rounding = 0.0f, float thickness = 1.0f)
{
    // The box is fitted around the text item itself, so the text sits inside it whatever the font.
    const ImVec2 pad = ImGui::GetStyle().FramePadding / 2;
    ImGui::Text("%s", text);
    ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin() - pad, ImGui::GetItemRectMax() + pad, color,
                                        rounding, ImDrawFlags_None, thickness);
    ImGui::Dummy({pad.x, 0});
}

template <typename T, size_t Extent, typename Formatter>
bool ImComboBoxItems(const char* label, std::span<const T, Extent> items, T& selection, Formatter format)
{
    bool changed = false;
    ImGui::PushID(label);
    ImRowLabel(label, ImRowControlWidth());
    if (ImBeginComboRow("##combo", format(selection), ImRowControlWidth()))
    {
        for (T const& i : items)
        {
            bool selected = i == selection;
            if (ImGui::Selectable(format(i), selected))
                selection = i, changed = true;
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::PopID();
    return changed;
}

// A vertical slider for the equalizer: a thin track with the zero line marked, the travelled part
// in the accent colour from the centre, and a round knob. Same contract as ImGui::VSliderInt.
bool ImVSliderInt(const char* label, const ImVec2& size, int* value, int min, int max)
{
    const float unit = ImGui::GetFontSize();
    const float knobR = unit * 0.5f, trackW = unit * 0.3f;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(label, size);
    const ImGuiID id = ImGui::GetItemID();
    const bool active = ImGui::IsItemActive(), hovered = ImGui::IsItemHovered();
    const float labelH = ImGui::GetTextLineHeight() + unit * 0.2f;
    const float y0 = origin.y + labelH + knobR, y1 = origin.y + size.y - knobR; // y0 = max, y1 = min
    const int span = std::max(1, max - min);
    bool changed = false;
    if (active)
    {
        const float t = std::clamp((y1 - ImGui::GetIO().MousePos.y) / std::max(1.0f, y1 - y0), 0.0f, 1.0f);
        const int next = min + static_cast<int>(std::lround(t * span));
        if (next != *value)
        {
            *value = next;
            changed = true;
            ImGui::MarkItemEdited(id);
        }
    }
    const float target = static_cast<float>(std::clamp(*value, min, max) - min) / static_cast<float>(span);
    float* eased = ImGui::GetStateStorage()->GetFloatRef(id + 1, target);
    if (active)
        *eased = target;
    const float t = active ? target : (*eased = ImApproach(*eased, target, 22.0f));
    const float alpha = ImGui::GetStyle().Alpha;
    auto* draw = ImGui::GetWindowDrawList();
    const float cx = origin.x + size.x * 0.5f;
    const float knobY = y1 - t * (y1 - y0);
    const float zeroT = std::clamp(static_cast<float>(0 - min) / static_cast<float>(span), 0.0f, 1.0f);
    const float zeroY = y1 - zeroT * (y1 - y0);
    draw->AddRectFilled({cx - trackW * 0.5f, y0}, {cx + trackW * 0.5f, y1},
                        MaterialYouTheme::ArgbToImU32(MaterialYouTheme::FixedSurfaceColors::outlineVariant, alpha * 0.7f), trackW * 0.5f);
    draw->AddRectFilled({cx - trackW * 0.5f, std::min(knobY, zeroY)}, {cx + trackW * 0.5f, std::max(knobY, zeroY)},
                        MaterialYouTheme::ArgbToImU32(MaterialYouTheme::AccentTheme().primary, alpha), trackW * 0.5f);
    draw->AddLine({cx - unit * 0.55f, zeroY}, {cx + unit * 0.55f, zeroY},
                  MaterialYouTheme::ArgbToImU32(MaterialYouTheme::FixedSurfaceColors::outline, alpha * 0.6f));
    const float grow = ImAnimValue(id + 2, (hovered || active) ? 1.0f : 0.0f, 16.0f);
    const float r = knobR * (1.0f + 0.12f * grow);
    draw->AddCircleFilled({cx, knobY + unit * 0.08f}, r, IM_COL32(0, 0, 0, static_cast<int>(55 * alpha)), 32);
    draw->AddCircleFilled({cx, knobY}, r, IM_COL32(255, 255, 255, static_cast<int>(255 * alpha)), 32);
    draw->AddCircle({cx, knobY}, r, IM_COL32(0, 0, 0, static_cast<int>(28 * alpha)), 32);
    // The value above the track, signed like the device shows it.
    char text[16];
    std::snprintf(text, sizeof(text), "%+d", *value);
    ImFont* font = clientHeadingFont() ? clientHeadingFont() : ImGui::GetFont();
    const ImVec2 textSize = font->CalcTextSizeA(unit * 0.9f, FLT_MAX, 0.0f, text);
    draw->AddText(font, unit * 0.9f, {cx - textSize.x * 0.5f, origin.y}, ImGui::GetColorU32(ImGuiCol_Text), text);
    return changed;
}

bool ImEqualizer(std::span<int> bands)
{
    constexpr const char* kBand5[] = {"400", "1k", "2.5k", "6.3k", "16k"};
    constexpr const char* kBand10[] = {"31", "63", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"};
    const char* const* kBands = nullptr;
    int numBands = static_cast<int>(bands.size());
    int mn = 0, mx = 0;
    if (numBands == 10)
        kBands = kBand10, mn = -6, mx = 6;
    if (numBands == 5)
        kBands = kBand5, mn = -10, mx = 10;
    if (!kBands)
    {
        ImGui::Text(tr("EQ Unavailable (bands=%d)"), numBands);
        return false;
    }
    bool changed = false;
    auto& style = ImGui::GetStyle();
    float padding = style.FramePadding.x;
    auto [offset, region, draw] = ImWindowDrawOffsetRegionList();
    float bandWidth = region.x / numBands - padding;
    const float bandHeight = ImGui::GetFontSize() * 9.5f;
    if (numBands == 5)
        ImSectionHeading(tr("5-Band EQ"));
    if (numBands == 10)
        ImSectionHeading(tr("10-Band EQ"));
    for (int i = 0; i < numBands; ++i)
    {
        ImGui::BeginGroup();
        ImGui::PushID(i);
        changed |= ImVSliderInt("##v", ImVec2{bandWidth, bandHeight}, &bands[i], mn, mx);
        ImGui::PopID();

        float textWidth = ImGui::CalcTextSize(kBands[i]).x;
        float textOffset = (bandWidth - textWidth) * 0.5f;
        if (textOffset > 0.0f)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + textOffset);
        ImGui::TextDisabled("%s", kBands[i]);

        ImGui::EndGroup();
        if (i != numBands - 1)
            ImGui::SameLine(0.0f, padding);
    }
    return changed;
}
struct ImStylesRAII
{
    size_t numVars = 0, numColors = 0, numFonts = 0;
    template <typename... Args>
    void PushVar(ImGuiStyleVar idx, Args&&... args)
    {
        ImGui::PushStyleVar(idx, args...), numVars++;
    }
    template <typename... Args>
    void PushCol(ImGuiCol idx, Args&&... args)
    {
        ImGui::PushStyleColor(idx, args...), numColors++;
    }
    template <typename... Args>
    void PushFont(ImFont* font, Args&&... args)
    {
        ImGui::PushFont(font, args...), numFonts++;
    }
    ~ImStylesRAII()
    {
        ImGui::PopStyleVar(numVars);
        ImGui::PopStyleColor(numColors);
        while (numFonts--)
            ImGui::PopFont();
    }
};
#pragma endregion

#pragma region States
enum CONN_STATE
{
    CONN_STATE_NO_CONNECTION,
    CONN_STATE_CONNECTING,
    CONN_STATE_CONNECTED,
    CONN_STATE_DISCONNECTED // Passive, or from errors
} connState{};

enum DEVICE_TYPE
{
    DEVICE_TYPE_AUTO,
    DEVICE_TYPE_V2,
    DEVICE_TYPE_V1
};

struct ConnectionAttemptState
{
    // Longer than the Windows RFCOMM driver's own connect timeout (15 s). Abandoning a pending
    // connect earlier leaves a half-open channel behind, and the next attempt then fails with
    // "address in use" until the driver gives up on it, which cascades into endless failures.
    static constexpr uint64_t kAttemptTimeoutMs = 20'000;

    std::string address;
    std::string name;      // Display name from discovery, remembered on success
    bool automatic{};      // Started by auto-connect rather than a click
    std::array<const char*, 2> services{};
    std::string lastError;
    size_t serviceCount{};
    size_t serviceIndex{};
    uint64_t deadlineMs{};
    bool ble{};
};

ConnectionAttemptState connectionAttempt;

// Auto-connect bookkeeping. Suppressed after a manual disconnect/cancel until the next manual connect.
bool gAutoConnectSuppressed = false;
uint64_t gNextAutoConnectMs = 0;
int gAutoConnectFailures = 0;       // Consecutive failed automatic attempts, drives the backoff
bool gAutoConnectDeviceSeen = false; // Whether the remembered device was in the last scan
// Bluetooth reset: offered after repeated silent failures, done automatically once per session.
bool gBluetoothResetAutoDone = false;
uint64_t gBluetoothResetStartedMs = 0;
constexpr int kOfferResetAfterFailures = 2;
constexpr int kAutoResetAfterFailures = 3;
// Why the last session ended; formatted at draw time so it follows language switches.
bool gHasDisconnectMessage = false;
std::string gLastDisconnectReason;
constexpr uint64_t kAutoConnectRetryMs = 15'000;
constexpr uint64_t kAutoConnectRetryMaxMs = 120'000;
constexpr uint64_t kReconnectGraceMs = 3'000;

uint64_t AutoConnectBackoffMs()
{
    uint64_t delay = kAutoConnectRetryMs;
    for (int i = 0; i < gAutoConnectFailures && delay < kAutoConnectRetryMaxMs; ++i)
        delay *= 2;
    return std::min(delay, kAutoConnectRetryMaxMs);
}

const char* ConnectionAttemptName()
{
    if (connectionAttempt.ble)
        return "BLE";
    if (connectionAttempt.serviceCount == 1)
        return std::strcmp(connectionAttempt.services[0], MDR_SERVICE_UUID_LEGACY) == 0 ? tr("V1") : tr("V2");
    return connectionAttempt.serviceIndex == 0 ? tr("V2") : tr("V1");
}

MDRProtocolVersion ConnectionProtocolVersion()
{
    if (connectionAttempt.ble)
        return MDR_PROTOCOL_V2;
    const char* service = connectionAttempt.services[connectionAttempt.serviceIndex];
    if (std::strcmp(service, MDR_SERVICE_UUID_LEGACY) == 0)
        return MDR_PROTOCOL_V1;
    return MDR_PROTOCOL_V2;
}

void CaptureConnectionError(MDRConnection* conn, MDRResult result)
{
    const char* error = mdrConnectionGetLastError(conn);
    connectionAttempt.lastError = error && *error ? error : mdrResultString(result);
}

MDRResult TryConnectionAttempt(MDRConnection* conn)
{
    while (connectionAttempt.serviceIndex < connectionAttempt.serviceCount)
    {
        const MDRResult result =
            mdrConnectionConnect(conn, connectionAttempt.address.c_str(),
                                 connectionAttempt.services[connectionAttempt.serviceIndex]);
        if (result == MDR_RESULT_OK || result == MDR_RESULT_INPROGRESS)
        {
            connectionAttempt.deadlineMs = SDL_GetTicks() + ConnectionAttemptState::kAttemptTimeoutMs;
            return result;
        }

        CaptureConnectionError(conn, result);
        mdrConnectionDisconnect(conn);
        ++connectionAttempt.serviceIndex;
    }
    return MDR_RESULT_ERROR_NO_CONNECTION;
}

MDRResult AdvanceConnectionAttempt(MDRConnection* conn, MDRResult reason)
{
    CaptureConnectionError(conn, reason);
    mdrConnectionDisconnect(conn);
    ++connectionAttempt.serviceIndex;
    return TryConnectionAttempt(conn);
}

MDRResult StartConnection(
    MDRConnection* conn,
    const char* address,
    const char* name,
    bool usingBLE,
    DEVICE_TYPE deviceType,
    bool automatic = false)
{
    connectionAttempt = {};
    connectionAttempt.address = address;
    connectionAttempt.name = name ? name : "";
    connectionAttempt.automatic = automatic;
    connectionAttempt.ble = usingBLE;
    if (usingBLE)
    {
        connectionAttempt.services[0] = MDR_BLE_SERVICE_UUID_TANDEM_OVER_BLE_HPC;
        connectionAttempt.serviceCount = 1;
    }
    else if (deviceType == DEVICE_TYPE_AUTO)
    {
        connectionAttempt.services = {MDR_SERVICE_UUID_XM5, MDR_SERVICE_UUID_LEGACY};
        connectionAttempt.serviceCount = 2;
    }
    else
    {
        connectionAttempt.services[0] =
            deviceType == DEVICE_TYPE_V2 ? MDR_SERVICE_UUID_XM5 : MDR_SERVICE_UUID_LEGACY;
        connectionAttempt.serviceCount = 1;
    }
    return TryConnectionAttempt(conn);
}
#pragma endregion

// A pane of Liquid Glass: a diffused fill, a darkened outer edge that separates it from whatever
// shows through, and a bright specular highlight along the lit (top) edge. The edge pair is what
// iOS 27 added to keep glass readable over busy content, and it is what stops a translucent
// rectangle from looking like a flat wash of colour.
void ImGlassEdges(ImVec2 min, ImVec2 max, float rounding)
{
    auto* draw = ImGui::GetWindowDrawList();
    rounding = std::min(rounding, std::min(max.x - min.x, max.y - min.y) * 0.5f);
    draw->AddRect(min, max, MaterialYouTheme::glassEdgeShadow(), rounding, 0, 1.0f);
    if (rounding <= 0.0f)
    {
        draw->AddLine({min.x, min.y + 0.5f}, {max.x, min.y + 0.5f}, MaterialYouTheme::glassEdgeHighlight());
        return;
    }
    draw->PathArcTo({min.x + rounding, min.y + rounding}, rounding, IM_PI, IM_PI * 1.5f, 12);
    draw->PathArcTo({max.x - rounding, min.y + rounding}, rounding, IM_PI * 1.5f, IM_PI * 2.0f, 12);
    draw->PathStroke(MaterialYouTheme::glassEdgeHighlight(), 0, 1.5f);
}

void ImGlassPanel(ImVec2 min, ImVec2 max, float rounding, ImU32 fill)
{
    ImGui::GetWindowDrawList()->AddRectFilled(
        min, max, fill, std::min(rounding, std::min(max.x - min.x, max.y - min.y) * 0.5f));
    ImGlassEdges(min, max, rounding);
}

// Pills: Liquid Glass controls are capsules, so push a rounding the widget will clamp to half its height.
constexpr float kImCapsuleRounding = 1000.0f;

// A device in the discovery list: a disc with the headphone glyph, the name in heading weight,
// the address underneath and a chevron. The whole row is the button; its tint eases in and out.
constexpr float kImDeviceRowUnits = 3.0f;
bool ImDeviceRow(const char* name, const char* address)
{
    const float unit = ImGui::GetFontSize();
    const float h = unit * kImDeviceRowUnits;
    const float width = std::max(ImGui::GetContentRegionAvail().x, unit * 8.0f);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::PushID(address);
    const bool pressed = ImGui::InvisibleButton("##row", ImVec2(width, h));
    const ImGuiID id = ImGui::GetItemID();
    const bool hovered = ImGui::IsItemHovered(), active = ImGui::IsItemActive();
    const float hot = ImAnimValue(id + 1, hovered ? 1.0f : 0.0f, 14.0f);
    const float alpha = ImGui::GetStyle().Alpha;
    auto* draw = ImGui::GetWindowDrawList();
    if (hot > 0.001f || active)
        draw->AddRectFilled(pos, pos + ImVec2(width, h),
                            MaterialYouTheme::ArgbToImU32(MaterialYouTheme::AccentTheme().primary, alpha * (active ? 0.16f : 0.09f * hot)),
                            unit * 0.8f);
    const ImVec2 disc(pos.x + unit * 1.7f, pos.y + h * 0.5f);
    draw->AddCircleFilled(disc, unit * 1.1f, MaterialYouTheme::ArgbToImU32(MaterialYouTheme::AccentTheme().primaryContainer, alpha), 32);
    const ImVec2 glyph = ImGui::CalcTextSize(PSI_HEADPHONES);
    draw->AddText(disc - glyph * 0.5f, ImGui::GetColorU32(ImGuiCol_CheckMark), PSI_HEADPHONES);
    const float textX = pos.x + unit * 3.5f;
    ImFont* heading = clientHeadingFont() ? clientHeadingFont() : ImGui::GetFont();
    draw->AddText(heading, unit, {textX, pos.y + unit * 0.55f}, ImGui::GetColorU32(ImGuiCol_Text), name);
    draw->AddText(ImGui::GetFont(), unit * 0.85f, {textX, pos.y + unit * 1.7f}, ImGui::GetColorU32(ImGuiCol_TextDisabled), address);
    const ImVec2 chevron = ImGui::CalcTextSize(PSI_CHEVRON_RIGHT);
    draw->AddText({pos.x + width - unit * 1.2f - chevron.x, pos.y + (h - chevron.y) * 0.5f},
                  ImGui::GetColorU32(ImGuiCol_TextDisabled), PSI_CHEVRON_RIGHT);
    ImGui::PopID();
    return pressed;
}

// The edge pair for a card, drawn from inside the child so it sits above the card's own
// background. (A child's draw list renders after its parent's, so edges drawn after EndChild would
// end up underneath.)
void ImCardEdges()
{
    ImGlassEdges(ImGui::GetWindowPos(), ImGui::GetWindowPos() + ImGui::GetWindowSize(), ImGui::GetStyle().ChildRounding);
}

// Soft drop shadow below a card, light appearance only: dark cards are lighter than the surface
// and separate on their own. Drawn on the parent after EndChild, which renders underneath the card.
void ImCardShadow(ImVec2 min, ImVec2 max, float rounding)
{
    if (MaterialYouTheme::darkMode)
        return;
    auto* draw = ImGui::GetWindowDrawList();
    const float alpha = ImGui::GetStyle().Alpha;
    draw->AddRectFilled(min + ImVec2(0.0f, 2.0f), max + ImVec2(0.0f, 2.0f), IM_COL32(0, 0, 0, static_cast<int>(16 * alpha)), rounding);
    draw->AddRectFilled(min + ImVec2(-1.0f, 4.0f), max + ImVec2(1.0f, 7.0f), IM_COL32(0, 0, 0, static_cast<int>(9 * alpha)), rounding + 2.0f);
}

// A card's title line: an accent-coloured glyph and the title in heading weight.
void ImCardTitle(const char* icon, const char* title)
{
    if (icon && *icon)
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), "%s", icon);
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
    }
    ImPushHeading(1.0f);
    ImGui::TextUnformatted(title);
    ImPopHeading();
    ImGui::Spacing();
}

// A card: the grouping unit of the control screens. Always "open"; pair with ImEndCard.
bool ImBeginCard(const char* title, const char* icon = nullptr)
{
    ImGui::PushID(title);
    const float unit = ImGui::GetFontSize();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(unit, unit * 0.75f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                          MaterialYouTheme::ArgbToImVec4(MaterialYouTheme::FixedSurfaceColors::surfaceContainerLow,
                                                         MaterialYouTheme::glassAlpha(0.92f, 0.80f)));
    ImGui::BeginChild("##card", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImCardEdges();
    if (title && *title)
        ImCardTitle(icon, title);
    return true;
}
void ImEndCard()
{
    ImGui::EndChild();
    ImCardShadow(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImGui::GetStyle().ChildRounding);
    ImGui::PopID();
}

// The hero: one line that says what matters (the model, or what to do next), one muted line
// under it, and a small illustration on a tinted disc that stays inside the card. Everything is
// drawn with the UI's DPI scale, so it stays crisp at any size.
ImU32 GenericProductColour(); // Defined with the connected screen

void DrawListeningHero(const char* title, const char* subtitle)
{
    const float unit = ImGui::GetFontSize();
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const bool compact = connState == CONN_STATE_CONNECTED;
    const float height = unit * (compact ? 4.6f : 6.6f);
    const ImVec2 end = start + ImVec2(width, height);
    auto* draw = ImGui::GetWindowDrawList();
    const ImU32 accent = ImGui::GetColorU32(ImGuiCol_CheckMark);
    ImGlassPanel(start, end, unit * 1.5f,
                 MaterialYouTheme::ArgbToImU32(MaterialYouTheme::AccentTheme().primaryContainer,
                                               MaterialYouTheme::glassAlpha(0.88f, 0.74f)));
    // Hide the illustration on narrow windows to leave room for the heading.
    const bool illustrated = width > unit * 22;
    if (illustrated)
    {
        // The headphones last used (headphones in general before any), in 3D at rest on a glowing
        // disc that breathes, ringed by a spinning arc while connecting.
        const bool animations = clientSettings().animations;
        const float time = static_cast<float>(ImGui::GetTime());
        // The ring is sized to stay inside the card, with a margin; the disc and the model follow it.
        const float ring = std::min(unit * 3.75f, height * 0.5f - unit * 0.45f);
        const float size = ring / 1.5f;
        const ImVec2 center = start + ImVec2(width - ring - unit * 0.9f, height * 0.5f);
        draw->PushClipRect(start, end, true);
        const float breath = animations ? 0.5f + 0.5f * std::sin(time * 1.6f) : 0.5f;
        draw->AddCircleFilled(center, ring * (0.94f + 0.05f * breath),
                              MaterialYouTheme::ArgbToImU32(MaterialYouTheme::AccentTheme().primary, 0.05f + 0.05f * breath), 48);
        if (connState == CONN_STATE_CONNECTING)
        {
            const float phase = animations ? time * 3.0f : 0.0f;
            draw->PathArcTo(center, ring, phase, phase + 4.6f, 48);
            draw->PathStroke(accent, 0, unit * 0.1f);
        }
        const char* product = clientSettings().lastDeviceName.c_str();
        Headphones3D::View view;
        view.yaw = Headphones3D::RestYaw(product);
        view.pitch = 0.2f;
        view.size = size;
        Headphones3D::Draw(draw, center, view, product, GenericProductColour());
        draw->PopClipRect();
    }
    const float textLeft = unit * 1.4f;
    ImGui::PushFont(clientHeadingFont(), unit * 1.5f);
    // Fit longer model names without colliding with the illustration.
    const float titleWidth = std::max(unit, width - unit * (illustrated ? 9.4f : 2.8f));
    const float measured = ImGui::CalcTextSize(title).x;
    if (measured > titleWidth)
    {
        ImGui::PopFont();
        ImGui::PushFont(clientHeadingFont(), unit * 1.5f * titleWidth / measured);
    }
    ImGui::SetCursorScreenPos(start + ImVec2(textLeft, unit * (compact ? 0.95f : 1.9f)));
    ImGui::TextUnformatted(title);
    ImGui::PopFont();
    ImGui::SetCursorScreenPos(start + ImVec2(textLeft, unit * (compact ? 2.75f : 3.9f)));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + titleWidth);
    ImGui::TextDisabled("%s", subtitle);
    ImGui::PopTextWrapPos();
    ImGui::SetCursorScreenPos(start);
    ImGui::Dummy({width, height});
}

// Compact language picker, used in App Settings and on the discovery screen footer.
// A small rotating arc, drawn at `center`. Cheap enough to run every frame.
void ImDrawSpinnerAt(ImVec2 center, float radius, ImU32 color, float thickness = 2.0f)
{
    const float angle = static_cast<float>(ImGui::GetTime()) * 6.0f;
    auto* draw = ImGui::GetWindowDrawList();
    draw->PathArcTo(center, radius, angle, angle + 4.4f, 24);
    draw->PathStroke(color, 0, thickness);
}

// Draws text centered on `center`, turned by `angle` radians. The glyph quads are rotated in the
// draw list after the fact, which lets a button's own icon spin instead of gaining a second spinner.
void ImDrawTextRotated(ImVec2 center, ImU32 color, const char* text, float angle)
{
    auto* draw = ImGui::GetWindowDrawList();
    const int first = draw->VtxBuffer.Size;
    const ImVec2 size = ImGui::CalcTextSize(text);
    draw->AddText(center - size * 0.5f, color, text);
    if (angle == 0.0f)
        return;
    const float sn = std::sin(angle), cs = std::cos(angle);
    for (int i = first; i < draw->VtxBuffer.Size; ++i)
    {
        ImVec2& p = draw->VtxBuffer[i].pos;
        const float x = p.x - center.x, y = p.y - center.y;
        p = ImVec2(x * cs - y * sn + center.x, x * sn + y * cs + center.y);
    }
}

// Inline spinner that occupies one frame height and advances the cursor like a widget.
// A spinner on the current line. `height` is the line it centres on: a frame by default (next to
// buttons and text aligned to frame padding), or the text line height next to plain text.
void ImInlineSpinner(ImU32 color, float height = 0.0f)
{
    const float h = height > 0.0f ? height : ImGui::GetFrameHeight();
    const float radius = ImGui::GetFontSize() * 0.4f;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawSpinnerAt(pos + ImVec2(radius + 2.0f, h * 0.5f), radius, color);
    ImGui::Dummy(ImVec2(radius * 2.0f + 4.0f, h));
}

extern void clientRequestFontRebuild(); // SDLMain.cpp
void DrawLanguageCombo(const char* id, float width)
{
    ClientSettings& settings = clientSettings();
    constexpr ClientLanguage kLanguages[] = {ClientLanguage::Auto, ClientLanguage::English,
                                             ClientLanguage::ChineseTraditional, ClientLanguage::Japanese};
    constexpr int kCount = static_cast<int>(std::size(kLanguages));
    int current = 0;
    for (int i = 0; i < kCount; ++i)
        if (static_cast<int>(kLanguages[i]) == settings.language)
            current = i;
    if (ImBeginComboRow(id, clientLanguageName(kLanguages[current]), width))
    {
        for (int i = 0; i < kCount; ++i)
        {
            if (ImGui::Selectable(clientLanguageName(kLanguages[i]), i == current))
            {
                const ClientLanguage before = clientLocalizationEffectiveLanguage();
                settings.language = static_cast<int>(kLanguages[i]);
                clientLocalizationSetLanguage(kLanguages[i]);
                clientSettingsSave();
                // Japanese and Chinese draw many of the same ideographs differently.
                if ((before == ClientLanguage::Japanese) != (clientLocalizationEffectiveLanguage() == ClientLanguage::Japanese))
                    clientRequestFontRebuild();
            }
        }
        ImGui::EndCombo();
    }
}

#pragma region Close Prompt
// Whether the close button minimizes or quits. Drawn as a modal stacked on top of whichever
// modal the current screen already owns, so it is opened from inside that modal's block:
// opening it at the top level would close the screen underneath instead of layering over it.
extern bool gShouldClose;       // SDLMain.cpp
extern bool gCloseAskPending;   // SDLMain.cpp
extern void clientHideToTray(); // SDLMain.cpp

namespace
{
    constexpr const char* kClosePromptId = "##ClosePrompt";
    bool gClosePromptDrawn = false;    // Reset each frame; the first call site owns the prompt
    bool gClosePromptOpened = false;
    bool gClosePromptRemember = false;
}

void ClosePromptFinish(int action)
{
    if (action != CLIENT_CLOSE_ASK && gClosePromptRemember)
    {
        clientSettings().closeAction = action;
        clientSettingsSave();
    }
    gCloseAskPending = false;
    gClosePromptOpened = false;
    gClosePromptRemember = false;
    ImGui::CloseCurrentPopup();
    if (action == CLIENT_CLOSE_EXIT)
        gShouldClose = true;
    else if (action == CLIENT_CLOSE_MINIMIZE)
        clientHideToTray();
}

// One choice: icon and title on the first line, a muted explanation wrapped underneath.
bool ClosePromptOption(const char* icon, const char* title, const char* hint, bool suggested)
{
    const ImGuiStyle& style = ImGui::GetStyle();
    const float width = ImGui::GetContentRegionAvail().x;
    const float indent = style.FramePadding.x * 2;
    const float wrapWidth = std::max(1.0f, width - indent * 2);
    const mdr::String heading = mdr::Format("{}  {}", icon, title);
    const float headingHeight = ImGui::GetTextLineHeight();
    const ImVec2 hintSize = ImGui::CalcTextSize(hint, nullptr, false, wrapWidth);
    const float height = headingHeight + style.ItemInnerSpacing.y + hintSize.y + style.FramePadding.y * 2;

    const ImVec2 start = ImGui::GetCursorScreenPos();
    ImGui::PushID(title);
    bool pressed;
    {
        ImStylesRAII styles;
        styles.PushCol(ImGuiCol_Button,
                       ImGui::GetStyleColorVec4(suggested ? ImGuiCol_ButtonHovered : ImGuiCol_FrameBg));
        pressed = ImGui::Button("##option", {width, height});
    }
    ImGui::PopID();

    // Centred: the heading, then each wrapped line of the hint on its own.
    auto* draw = ImGui::GetWindowDrawList();
    const float centre = start.x + width * 0.5f;
    const float headingWidth = ImGui::CalcTextSize(heading.c_str()).x;
    draw->AddText({centre - headingWidth * 0.5f, start.y + style.FramePadding.y}, ImGui::GetColorU32(ImGuiCol_Text),
                  heading.c_str());
    ImFont* font = ImGui::GetFont();
    const float size = ImGui::GetFontSize();
    const ImU32 muted = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    float y = start.y + style.FramePadding.y + headingHeight + style.ItemInnerSpacing.y;
    const char* end = hint + std::strlen(hint);
    for (const char* line = hint; line < end;)
    {
        const char* next = font->CalcWordWrapPosition(size, line, end, wrapWidth);
        if (next == line) // A glyph wider than the whole box: take it anyway
            ++next;
        const char* lineEnd = next;
        while (lineEnd > line && (lineEnd[-1] == ' ' || lineEnd[-1] == '\n'))
            --lineEnd;
        const float lineWidth = font->CalcTextSizeA(size, FLT_MAX, 0.0f, line, lineEnd).x;
        draw->AddText(font, size, {centre - lineWidth * 0.5f, y}, muted, line, lineEnd);
        y += ImGui::GetTextLineHeight();
        line = next;
        while (line < end && (*line == ' ' || *line == '\n'))
            ++line;
    }
    return pressed;
}

void DrawClosePrompt()
{
    if (!gCloseAskPending || gClosePromptDrawn)
        return;
    gClosePromptDrawn = true;
    if (!gClosePromptOpened)
    {
        ImGui::OpenPopup(kClosePromptId);
        gClosePromptOpened = true;
    }
    // Narrower than the screen-sized modals, and opaque: this one stacks over another sheet,
    // and two translucent layers make the text underneath bleed through.
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float margin = std::max(ImGui::GetStyle().WindowPadding.x, clientWindowChromeHeight());
    const float promptWidth = std::max(1.0f, std::min(display.x - margin * 2, ImGui::GetFontSize() * 30));
    ImGui::SetNextWindowPos(display * 0.5f, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({promptWidth, 0});
    ImGui::PushStyleColor(ImGuiCol_PopupBg,
                          MaterialYouTheme::ArgbToImVec4(MaterialYouTheme::FixedSurfaceColors::surfaceContainerLow));
    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.0f, 0.0f, 0.0f, 0.45f));
    const bool promptVisible = ImGui::BeginPopupModal(kClosePromptId, nullptr, kImWindowFlagsTopMost);
    ImGui::PopStyleColor(2); // Both are only read while the window is being begun
    if (promptVisible)
    {
        // The sheet is drawn by ImGui; add the edge pair that makes it read as glass.
        ImGlassEdges(ImGui::GetWindowPos(), ImGui::GetWindowPos() + ImGui::GetWindowSize(),
                     ImGui::GetStyle().PopupRounding);
        const float spacing = ImGui::GetStyle().ItemSpacing.y;
        {
            ImStylesRAII styles;
            styles.PushFont(clientHeadingFont(), ImGui::GetFontSize() * 1.35f);
            ImTextCentered(tr("Close the window?"));
        }
        ImGui::Dummy({0, spacing});
        // Decide first, act once: each option closes the popup, so two of them must not both fire.
        int chosen = -1;
        if (ClosePromptOption(PSI_RESIZE_SMALL, tr("Minimize to the system tray"),
                              tr("Keeps running in the background and stays connected to your headphones."), true))
            chosen = CLIENT_CLOSE_MINIMIZE;
        if (ClosePromptOption(PSI_OFF, tr("Exit the app"),
                              tr("Disconnects from your headphones and closes the app."), false))
            chosen = CLIENT_CLOSE_EXIT;
        ImGui::Dummy({0, spacing});
        // Centred like the options above: the label and its switch as one group, then Cancel.
        {
            const char* remember = tr("Remember my choice");
            const float gap = ImGui::GetFontSize() * 0.8f;
            const float toggleWidth = ImGui::GetFontSize() * 1.45f * 1.72f;
            const float groupWidth = ImGui::CalcTextSize(remember).x + gap + toggleWidth;
            const float avail = ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (avail - groupWidth) * 0.5f));
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(remember);
            ImGui::SameLine(0.0f, gap);
            ImToggle("##remember", &gClosePromptRemember);
        }
        ImGui::Spacing();
        const float cancelWidth = ImGui::CalcTextSize(tr("Cancel")).x + ImGui::GetStyle().FramePadding.x * 4;
        const float lineAvail = ImGui::GetContentRegionAvail().x;
        if (lineAvail > cancelWidth)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (lineAvail - cancelWidth) * 0.5f);
        // ImGui only closes a modal on Escape while it owns keyboard nav, so handle it here too.
        if (ImGui::Button(tr("Cancel"), {cancelWidth, 0}) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            chosen = CLIENT_CLOSE_ASK; // Back out: the window stays as it is
        if (chosen >= 0)
            ClosePromptFinish(chosen);
        ImGui::EndPopup();
    }
    else if (gClosePromptOpened)
    {
        // Escape, or the screen underneath changed and took the popup stack with it: do nothing.
        gClosePromptOpened = false;
        gClosePromptRemember = false;
        gCloseAskPending = false;
    }
}
#pragma endregion

extern void clientRefreshWindowBackdrop(); // SDLMain.cpp
extern void clientRequestFontRebuild();     // SDLMain.cpp
extern void clientApplyAppearance();       // SDLMain.cpp
void ShowConnectOverlay(bool preview);
void DrawAppSettings()
{
    ClientSettings& settings = clientSettings();
    const float controlWidth = ImRowControlWidth();
    if (ImToggleRow(tr("Interface animations"), &settings.animations))
        clientSettingsSave();
    if (ImToggleRow(tr("Connection notifications"), &settings.notifications))
        clientSettingsSave();

    ImRowLabel(tr("Language"), controlWidth);
    DrawLanguageCombo("##Language", controlWidth);

    ImRowLabel(tr("Appearance"), controlWidth);
    {
        constexpr int kAppearances[] = {CLIENT_APPEARANCE_AUTO, CLIENT_APPEARANCE_LIGHT, CLIENT_APPEARANCE_DARK};
        auto appearanceName = [](int appearance)
        {
            return appearance == CLIENT_APPEARANCE_LIGHT ? tr("Light")
                 : appearance == CLIENT_APPEARANCE_DARK ? tr("Dark") : tr("Follow system");
        };
        if (ImBeginComboRow("##Appearance", appearanceName(settings.appearance), controlWidth))
        {
            for (int appearance : kAppearances)
                if (ImGui::Selectable(appearanceName(appearance), settings.appearance == appearance))
                {
                    settings.appearance = appearance;
                    clientSettingsSave();
                    clientApplyAppearance();
                }
            ImGui::EndCombo();
        }
    }

    ImRowLabel(tr("Window glass"), controlWidth);
    {
        // iOS 27 exposes the same choice: glass can be dialled from frosted to nearly clear, or off.
        constexpr int kGlassLevels[] = {CLIENT_GLASS_OFF, CLIENT_GLASS_REGULAR, CLIENT_GLASS_CLEAR};
        auto glassName = [](int level)
        {
            return level == CLIENT_GLASS_OFF ? tr("Off")
                 : level == CLIENT_GLASS_CLEAR ? tr("See-through") : tr("Frosted");
        };
        if (ImBeginComboRow("##Glass", glassName(settings.glassLevel), controlWidth))
        {
            for (int level : kGlassLevels)
                if (ImGui::Selectable(glassName(level), settings.glassLevel == level))
                {
                    settings.glassLevel = level;
                    clientSettingsSave();
                    clientRefreshWindowBackdrop();
                }
            ImGui::EndCombo();
        }
    }

    ImRowLabel(tr("Close button"), controlWidth);
    {
        constexpr int kCloseActions[] = {CLIENT_CLOSE_ASK, CLIENT_CLOSE_MINIMIZE, CLIENT_CLOSE_EXIT};
        auto closeActionName = [](int action)
        {
            return action == CLIENT_CLOSE_MINIMIZE ? tr("Minimize to the system tray")
                : action == CLIENT_CLOSE_EXIT ? tr("Exit the app")
                : tr("Ask every time");
        };
        if (ImBeginComboRow("##CloseAction", closeActionName(settings.closeAction), controlWidth))
        {
            for (const int action : kCloseActions)
                if (ImGui::Selectable(closeActionName(action), action == settings.closeAction))
                    settings.closeAction = action, clientSettingsSave();
            ImGui::EndCombo();
        }
    }

    {
        const float previewWidth = ImGui::CalcTextSize(tr("Preview")).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImRowLabel(tr("Connection animation"), previewWidth);
        if (ImGui::SmallButton(tr("Preview")))
            ShowConnectOverlay(true);
    }

    ImGui::BeginDisabled(!clientPlatformAutoStartSupported());
    if (ImToggleRow(tr("Start with Windows (minimized to the tray)"), &settings.autoStart))
    {
        if (!clientPlatformAutoStartSet(settings.autoStart ? 1 : 0))
            settings.autoStart = clientPlatformAutoStartGet() != 0;
        clientSettingsSave();
    }
    ImGui::EndDisabled();

    if (!settings.lastDeviceAddress.empty())
    {
        char autoConnect[256];
        std::snprintf(autoConnect, sizeof(autoConnect), tr("Auto-connect: %s (%s)"),
                      settings.lastDeviceName.empty() ? "last device" : settings.lastDeviceName.c_str(),
                      settings.lastDeviceAddress.c_str());
        const float forgetWidth = ImGui::CalcTextSize(tr("Forget")).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImRowLabel(autoConnect, forgetWidth);
        if (ImGui::SmallButton(tr("Forget")))
        {
            settings.lastDeviceAddress.clear();
            settings.lastDeviceName.clear();
            settings.lastDeviceProtocol = 0;
            clientSettingsSave();
        }
    }
}

void DrawDeviceDiscovery()
{
    // Also drawn while an *automatic* attempt is in flight, so reconnecting stays inline.
    assert(connState == CONN_STATE_NO_CONNECTION || (connState == CONN_STATE_CONNECTING && connectionAttempt.automatic));
    const bool reconnecting = connState == CONN_STATE_CONNECTING;
    if (ImBeginScreenColumn("##DeviceDiscovery"))
    {
        ImRevealBegin(0, 0);
        static MDRDeviceInfo* pDeviceInfo = nullptr;
        static int nDeviceInfo = 0;
        DrawListeningHero(reconnecting ? connectionAttempt.name.c_str() : tr("Connect your Sony headphones"),
                          reconnecting ? tr("Connecting...") : tr("Turn the headphones on and they will appear below."));
        ImSectionHeading(tr("Connection"));
        // Chose, and have the GATT backend active
        static bool usingBLE = false;
        static DEVICE_TYPE deviceType = DEVICE_TYPE_AUTO;
        static int connInitResult = MDR_RESULT_INPROGRESS;
        // BLE / Classic toggle
        bool needSwitchClientPlatform = clientPlatformConnectionGet() == nullptr;
        {
            const mdr::String classic = mdr::Format("{} {}", PSI_BLUETOOTH, tr("Classic"));
            const mdr::String ble = mdr::Format("{} {}", PSI_BLUETOOTH_ALT, tr("BLE (GATT)"));
            const char* const transports[] = {classic.c_str(), ble.c_str()};
            const int transport = ImSegmented("##Transport", transports, usingBLE ? 1 : 0);
            if (ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()))
                ImGui::SetTooltip("%s", tr("Use Classic for most devices. Choose BLE (GATT) for LE Audio connections."));
            if ((transport == 1) != usingBLE)
                usingBLE = transport == 1, needSwitchClientPlatform = true;
        }
        ImGui::BeginDisabled(usingBLE);
        {
            // Advanced option: a caption and a small segmented control on one line.
            ImStylesRAII styles;
            styles.PushVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 4.0f));
            const float controlWidth = ImGui::GetFontSize() * 12.0f;
            ImRowLabel(tr("Protocol"), controlWidth);
            const char* const protocols[] = {tr("Auto"), "V2", "V1"};
            constexpr const char* kProtocolHints[] = {
                "Auto-detect: tries the V2 (XM5+) service first and falls back to the legacy V1 service if it can't connect.",
                "V2 only: connects to devices exposing the V2 MDR service (XM5+) - newer models like WH/WF-1000XM5.",
                "V1 only: connects to devices exposing the legacy V1 MDR service - older models.",
            };
            const int protocol = ImSegmented("##Protocol", protocols, static_cast<int>(deviceType), controlWidth);
            deviceType = static_cast<DEVICE_TYPE>(protocol);
            const ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
            if (ImGui::IsMouseHoveringRect(min, max))
            {
                const int hovered = std::clamp(static_cast<int>((ImGui::GetMousePos().x - min.x) / ((max.x - min.x) / 3.0f)), 0, 2);
                ImGui::SetTooltip("%s", tr(kProtocolHints[hovered]));
            }
        }
        ImGui::EndDisabled();
        static uint64_t lastRefreshMs = 0;
        auto RefreshDeviceList = [&]()
        {
            MDRConnection* conn = clientPlatformConnectionGet();
            if (!conn)
                return;
            if (pDeviceInfo)
                mdrConnectionFreeDevicesList(conn, &pDeviceInfo), pDeviceInfo = nullptr, nDeviceInfo = 0;
            mdrConnectionGetDevicesList(conn, &pDeviceInfo, &nDeviceInfo); // TODO: Error modals
            lastRefreshMs = SDL_GetTicks();
        };
        if (needSwitchClientPlatform)
        {
            int flags = 0;
            if (usingBLE)
                flags |= MDR_INIT_BT_BLE;
            MDRConnection* conn = clientPlatformConnectionGet();
            if (conn && pDeviceInfo)
                mdrConnectionFreeDevicesList(conn, &pDeviceInfo), pDeviceInfo = nullptr, nDeviceInfo = 0;
            CloseDevice();
            clientPlatformConnectionDestroy();
            connInitResult = clientPlatformConnectionInit(flags);
            RefreshDeviceList();
        }
        // Devices that pair/connect after the app started should show up on their own. Enumeration
        // does not issue a Bluetooth inquiry (it lists what the OS already knows), so polling is cheap.
        constexpr uint64_t kAutoRefreshIntervalMs = 3000;
        if (connInitResult == MDR_RESULT_OK && SDL_GetTicks() - lastRefreshMs >= kAutoRefreshIntervalMs)
            RefreshDeviceList();
        // Bluetooth reset bookkeeping: while the radio is down, hold off; when it is back, retry at once.
        static bool resetWasRunning = false;
        const bool resetRunning = clientPlatformBluetoothResetInProgress() != 0;
        if (resetWasRunning && !resetRunning)
        {
            gAutoConnectFailures = 0;
            gNextAutoConnectMs = SDL_GetTicks() + kReconnectGraceMs;
        }
        resetWasRunning = resetRunning;
        // Auto-connect: the last successfully connected device shows up -> connect without a click.
        if (!reconnecting && !resetRunning)
        {
            const ClientSettings& settings = clientSettings();
            const MDRDeviceInfo* remembered = nullptr;
            if (!settings.lastDeviceAddress.empty())
                for (const MDRDeviceInfo& device : std::span<MDRDeviceInfo>{pDeviceInfo, static_cast<size_t>(nDeviceInfo)})
                    if (SDL_strcasecmp(device.szDeviceMacAddress, settings.lastDeviceAddress.c_str()) == 0)
                        remembered = &device;
            // The device (re)appearing after being absent is the "headphones just turned on" moment:
            // forget any backoff and try right away.
            if (remembered && !gAutoConnectDeviceSeen)
            {
                gAutoConnectFailures = 0;
                gNextAutoConnectMs = std::min(gNextAutoConnectMs, SDL_GetTicks() + kReconnectGraceMs);
            }
            gAutoConnectDeviceSeen = remembered != nullptr;
            if (remembered && connInitResult == MDR_RESULT_OK && !gAutoConnectSuppressed &&
                settings.lastDeviceBLE == usingBLE && SDL_GetTicks() >= gNextAutoConnectMs)
            {
                // Use the protocol that worked last time so we skip Auto's 10 s fallback wait.
                DEVICE_TYPE type = deviceType;
                if (type == DEVICE_TYPE_AUTO && settings.lastDeviceProtocol == 1) type = DEVICE_TYPE_V1;
                if (type == DEVICE_TYPE_AUTO && settings.lastDeviceProtocol == 2) type = DEVICE_TYPE_V2;
                MDR_LOG("[Client] Auto-connecting to {} ({}), attempt {}", remembered->szDeviceName,
                        remembered->szDeviceMacAddress, gAutoConnectFailures + 1)
                const int res = StartConnection(clientPlatformConnectionGet(), remembered->szDeviceMacAddress,
                                                remembered->szDeviceName, usingBLE, type, true);
                connState = (res != MDR_RESULT_OK && res != MDR_RESULT_INPROGRESS)
                    ? CONN_STATE_DISCONNECTED : CONN_STATE_CONNECTING;
                gNextAutoConnectMs = SDL_GetTicks() + AutoConnectBackoffMs();
            }
        }
        auto DrawDeviceList = [&]()
        {
            // The heading carries its own small action. Scanning is instant and already runs every
            // two seconds, so a full-width button oversold what "refresh" does.
            static uint64_t refreshFeedbackUntilMs = 0;
            const bool refreshing = SDL_GetTicks() < refreshFeedbackUntilMs;
            const float lineRight = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
            const float headingY = ImGui::GetCursorPosY();
            ImSectionHeading(tr("Available Devices"));
            {
                ImStylesRAII styles;
                styles.PushVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x * 0.5f, 2.0f));
                styles.PushVar(ImGuiStyleVar_FrameRounding, kImCapsuleRounding);
                styles.PushCol(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                const char* label = tr(refreshing ? "Refreshing..." : "Refresh");
                const float iconWidth = ImGui::CalcTextSize(PSI_REFRESH).x;
                const float gap = ImGui::GetStyle().ItemInnerSpacing.x;
                const ImVec2 labelSize = ImGui::CalcTextSize(label);
                const float width = iconWidth + gap + labelSize.x + ImGui::GetStyle().FramePadding.x * 2.0f;
                ImGui::SameLine(lineRight - width);
                ImGui::SetCursorPosY(headingY + ImGui::GetStyle().SeparatorTextPadding.y +
                                     (ImGui::GetTextLineHeight() - ImGui::GetFrameHeight()) * 0.5f);
                ImGui::BeginDisabled(refreshing || reconnecting);
                if (ImGui::Button("##Refresh", {width, 0}))
                {
                    RefreshDeviceList();
                    refreshFeedbackUntilMs = SDL_GetTicks() + 900; // Visible acknowledgement of the click
                }
                // Label drawn by hand: while a refresh is in flight its icon turns in place.
                const ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
                const float left = min.x + ImGui::GetStyle().FramePadding.x;
                const float centerY = (min.y + max.y) * 0.5f;
                const ImU32 color = ImGui::GetColorU32(ImGuiCol_TextDisabled);
                const float angle = refreshing && clientSettings().animations
                    ? static_cast<float>(ImGui::GetTime()) * 6.0f : 0.0f;
                ImDrawTextRotated({left + iconWidth * 0.5f, centerY}, color, PSI_REFRESH, angle);
                ImGui::GetWindowDrawList()->AddText({left + iconWidth + gap, centerY - labelSize.y * 0.5f}, color, label);
                ImGui::EndDisabled();
            }
            if (reconnecting)
            {
                ImInlineSpinner(ImGui::GetColorU32(ImGuiCol_CheckMark));
                ImGui::SameLine();
                ImGui::AlignTextToFramePadding();
                ImGui::Text(tr("Reconnecting to %s (%s)..."), connectionAttempt.name.c_str(), ConnectionAttemptName());
                ImGui::SameLine();
                if (ImGui::SmallButton(tr("Cancel")))
                {
                    gAutoConnectSuppressed = true;
                    CloseDevice();
                    mdrConnectionDisconnect(clientPlatformConnectionGet());
                    connectionAttempt = {};
                    connState = CONN_STATE_NO_CONNECTION;
                }
            }
            else if (gHasDisconnectMessage)
            {
                const mdr::String message = gLastDisconnectReason.empty()
                    ? mdr::String(tr("Connection lost. Waiting for the headphones to come back."))
                    : mdr::Format(fmt::runtime(tr("Connection lost: {}. Waiting for the headphones to come back.")),
                                  gLastDisconnectReason);
                ImGui::PushStyleColor(ImGuiCol_Text,
                                      MaterialYouTheme::ArgbToImVec4(MaterialYouTheme::FixedSurfaceColors::error));
                ImGui::TextWrapped(tri(PSI_EXCLAMATION_SIGN, "%s"), message.c_str());
                ImGui::PopStyleColor();
                const uint64_t now = SDL_GetTicks();
                if (clientPlatformBluetoothResetInProgress())
                {
                    ImInlineSpinner(ImGui::GetColorU32(ImGuiCol_CheckMark));
                    ImGui::SameLine();
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(tr("Resetting Bluetooth..."));
                }
                else if (gAutoConnectDeviceSeen && !gAutoConnectSuppressed && gNextAutoConnectMs > now)
                    ImGui::TextDisabled(tr("Retrying automatically in %llu s (attempt %d)."),
                                        static_cast<unsigned long long>((gNextAutoConnectMs - now + 999) / 1000),
                                        gAutoConnectFailures + 1);
                else if (gAutoConnectSuppressed)
                    ImGui::TextDisabled(tr("Automatic reconnect paused. Click the device to connect."));
                // The headphones are listed but never answer: a stuck link. Offer the fix, and do it
                // once by ourselves after one more failure.
                if (clientPlatformBluetoothResetSupported() && !clientPlatformBluetoothResetInProgress() &&
                    gAutoConnectDeviceSeen && gAutoConnectFailures >= kOfferResetAfterFailures)
                {
                    if (!gBluetoothResetAutoDone && gAutoConnectFailures >= kAutoResetAfterFailures)
                    {
                        gBluetoothResetAutoDone = true;
                        if (clientPlatformBluetoothResetStart())
                            gBluetoothResetStartedMs = now;
                    }
                    ImGui::TextWrapped("%s", tr("The headphones are not answering on this link. Resetting Bluetooth usually fixes it; every Bluetooth device reconnects, which takes a few seconds."));
                    if (ImGui::SmallButton(tri(PSI_BLUETOOTH, "Reset Bluetooth")))
                        if (clientPlatformBluetoothResetStart())
                            gBluetoothResetStartedMs = now;
                    if (gBluetoothResetAutoDone)
                    {
                        ImGui::SameLine();
                        ImGui::TextDisabled("%s", tr("Bluetooth was already reset once automatically."));
                    }
                }
            }
            ImGui::BeginDisabled(reconnecting);
            std::span<MDRDeviceInfo> devices{pDeviceInfo, static_cast<size_t>(nDeviceInfo)};
            if (!devices.empty())
            {
                const float listHeight = (ImGui::GetFontSize() * kImDeviceRowUnits + ImGui::GetStyle().ItemSpacing.y) * std::min(3, nDeviceInfo)
                    + ImGui::GetStyle().ItemSpacing.y;
                const ImVec2 listMin = ImGui::GetCursorScreenPos();
                ImGlassPanel(listMin, listMin + ImVec2(ImGui::GetContentRegionAvail().x, listHeight),
                             ImGui::GetStyle().ChildRounding,
                             MaterialYouTheme::ArgbToImU32(MaterialYouTheme::FixedSurfaceColors::surfaceContainerLow,
                                                           MaterialYouTheme::glassAlpha(0.55f, 0.34f)));
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ImGui::GetFontSize() * 0.5f, ImGui::GetStyle().ItemSpacing.y * 0.5f));
                ImGui::BeginChild("##DiscoveredDevices", {0, listHeight}, ImGuiChildFlags_AlwaysUseWindowPadding);
                ImGui::PopStyleVar();
                for (const auto& device : devices)
                {
                    // Clicking a device connects to it straight away.
                    if (ImDeviceRow(device.szDeviceName, device.szDeviceMacAddress))
                    {
                        gAutoConnectSuppressed = false;
                        gAutoConnectFailures = 0;
                        const int res = StartConnection(clientPlatformConnectionGet(), device.szDeviceMacAddress,
                                                        device.szDeviceName, usingBLE, deviceType);
                        connState = (res != MDR_RESULT_OK && res != MDR_RESULT_INPROGRESS)
                            ? CONN_STATE_DISCONNECTED : CONN_STATE_CONNECTING;
                    }
                }
                ImGui::EndChild();
            }
            else
            {
                // One muted line says it all; "ready" was restating the empty list.
                ImGui::PushTextWrapPos(0.0f);
                ImGui::TextDisabled("%s", tr("Turn on Bluetooth and connect your headphones in system settings. They will appear here automatically."));
                ImGui::PopTextWrapPos();
            }
            ImGui::EndDisabled(); // reconnecting
        };
        if (connInitResult != MDR_RESULT_OK && connInitResult != MDR_RESULT_INPROGRESS)
        {
            ImTextCentered(mdr::Format("{} {}", PSI_EXCLAMATION_SIGN,
                                       mdr::Format(fmt::runtime(tr("Failed to initialize connection: {}")),
                                                   mdrResultString(connInitResult)))
                               .c_str());
        }
        DrawDeviceList();
        if (ImGui::TreeNodeEx(tr("App Settings")))
        {
            DrawAppSettings();
            ImGui::TreePop();
        }
        ImGui::Separator();
        {
            ImStylesRAII styles;
            styles.PushFont(nullptr, ImGui::GetFontSize() * 0.85f);
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextDisabled("%s", mdr::Format("SonyHeadphonesClient {}  \u00b7  {}", CLIENT_VERSION,
                                                  tr("Independent client. Not affiliated with Sony. Use at your own risk."))
                                          .c_str());
            ImGui::PopTextWrapPos();
        }
#ifdef MDR_CLIENT_DEBUGGER
        ImGui::Separator();
        if (ImModalButton(tr("Protocol Debugger")))
            gDebuggerOpen = true;
        if (clientDebuggerHasPackets())
        {
            ImGui::BeginDisabled(clientDebuggerExportInProgress());
            if (ImModalButton(tri(PSI_SAVE, "Export latest"), 0, 2))
                clientDebuggerExportLatestPacket();
            if (ImModalButton(tri(PSI_SAVE, "Export ZIP"), 1, 2))
                clientDebuggerExportPacketCollection();
            ImGui::EndDisabled();
            const char* exportStatus = clientDebuggerGetExportStatus();
            if (*exportStatus)
                ImGui::TextWrapped(tr("Packet export: %s"), exportStatus);
        }
#endif
        ImRevealEnd();
        ImSmoothScroll();
    }
    ImGui::EndChild(); // DrawApp stacks the close prompt on the main window
}

// NOTE: Only CONN_STATE_DISCONNECTED state shows the modal
void DisconnectWithModal(const char* manualError = nullptr)
{
    MDRConnection* conn = clientPlatformConnectionGet();
    connState = CONN_STATE_DISCONNECTED;
    CloseDevice();
    if (manualError)
        gHeadphonesError = manualError;
    mdrConnectionDisconnect(conn);
}

void DrawDeviceConnecting()
{
    assert(connState == CONN_STATE_CONNECTING);
    MDRConnection* conn = clientPlatformConnectionGet();
    MDRResult pollResult = mdrConnectionPoll(conn, 0);
    if (pollResult != MDR_RESULT_OK)
    {
        const bool attemptTimedOut =
            (pollResult == MDR_RESULT_INPROGRESS || pollResult == MDR_RESULT_ERROR_TIMEOUT) &&
            SDL_GetTicks() >= connectionAttempt.deadlineMs;
        const bool attemptFailed =
            pollResult != MDR_RESULT_INPROGRESS && pollResult != MDR_RESULT_ERROR_TIMEOUT;
        if (attemptTimedOut || attemptFailed)
        {
            pollResult =
                AdvanceConnectionAttempt(conn, attemptTimedOut ? MDR_RESULT_ERROR_TIMEOUT : pollResult);
            if (pollResult != MDR_RESULT_OK && pollResult != MDR_RESULT_INPROGRESS)
            {
                connState = CONN_STATE_DISCONNECTED;
                CloseDevice();
                MaterialYouTheme::ApplyDefault();
                return;
            }
        }
    }
    switch (pollResult)
    {
    case MDR_RESULT_OK:
        connState = CONN_STATE_CONNECTED;
        connectionAttempt.lastError.clear();
        CloseDevice();
        if (mdrHeadphonesCreate(MDR_ABI_VERSION, conn, ConnectionProtocolVersion(), &gDevice) != MDR_RESULT_OK)
        {
            DisconnectWithModal();
            return;
        }
        {
            // Remember what worked so the next launch (or the next time the link drops) reconnects by itself.
            ClientSettings& settings = clientSettings();
            settings.lastDeviceAddress = connectionAttempt.address;
            if (!connectionAttempt.name.empty())
                settings.lastDeviceName = connectionAttempt.name;
            settings.lastDeviceBLE = connectionAttempt.ble;
            settings.lastDeviceProtocol = ConnectionProtocolVersion() == MDR_PROTOCOL_V1 ? 1 : 2;
            clientSettingsSave();
            gHasDisconnectMessage = false;
            gLastDisconnectReason.clear();
            gAutoConnectFailures = 0;
        }
        clientPacketObserverAttach(gDevice);
        if (mdrHeadphonesRequestInit(gDevice) != MDR_RESULT_OK)
            DisconnectWithModal();

        return;
    case MDR_RESULT_ERROR_TIMEOUT:
    case MDR_RESULT_INPROGRESS:
        {
            if (connectionAttempt.automatic)
                return; // DrawApp draws the discovery screen with an inline status instead
            if (ImBeginScreenColumn("##Connection"))
            {
                DrawListeningHero(connectionAttempt.name.empty() ? tr("Your headphones") : connectionAttempt.name.c_str(),
                                  tr("Connecting..."));
                ImGui::TextWrapped("%s", tr("Keep your headphones nearby."));
                const char* error = mdrConnectionGetLastError(conn);
                if (error && *error)
                    ImGui::TextWrapped("%s", error);
                if (ImModalButton(tri(PSI_REMOVE, "Cancel")))
                {
                    gAutoConnectSuppressed = true;
                    CloseDevice();
                    mdrConnectionDisconnect(conn);
                    connectionAttempt = {};
                    connState = CONN_STATE_NO_CONNECTION;
                }
            }
            ImGui::EndChild();
            return;
        }
    default:
        {
            CaptureConnectionError(conn, pollResult);
            connState = CONN_STATE_DISCONNECTED;
            CloseDevice();
            mdrConnectionDisconnect(conn);
            MaterialYouTheme::ApplyDefault();
            break;
        }
    }
}

#pragma region Connected screen
// Laid out after Sony's own app: the device at the top (picture, battery, codec, power), a few
// shortcuts for what people change most, and everything else grouped on an "all device
// settings" page, one collapsible section per topic. The app's own preferences have a page of
// their own, so it is always clear which side of the link a setting lives on.


// The device reports volume as 0..30. Windows shows the AVRCP absolute volume (0..127) as a
// percentage, so mirror that two-step rounding to display the same number the OS does.
constexpr int kDeviceVolumeMax = 30;
constexpr int kAvrcpVolumeMax = 127;

int DeviceVolumeToPercent(int volume)
{
    const int avrcp = (volume * kAvrcpVolumeMax + kDeviceVolumeMax / 2) / kDeviceVolumeMax;
    return (avrcp * 100 + kAvrcpVolumeMax / 2) / kAvrcpVolumeMax;
}

constexpr MDREqualizerPreset kEqualizerPresets[] = {
    MDR_EQ_OFF, MDR_EQ_ROCK, MDR_EQ_POP, MDR_EQ_JAZZ, MDR_EQ_DANCE, MDR_EQ_EDM,
    MDR_EQ_R_AND_B_HIP_HOP, MDR_EQ_ACOUSTIC, MDR_EQ_BRIGHT, MDR_EQ_EXCITED, MDR_EQ_MELLOW,
    MDR_EQ_RELAXED, MDR_EQ_VOCAL, MDR_EQ_TREBLE, MDR_EQ_BASS, MDR_EQ_SPEECH, MDR_EQ_HEAVY,
    MDR_EQ_CLEAR, MDR_EQ_HARD, MDR_EQ_SOFT, MDR_EQ_GAMING, MDR_EQ_FPS_1, MDR_EQ_FPS_2,
    MDR_EQ_FPS_3, MDR_EQ_CUSTOM, MDR_EQ_USER_1, MDR_EQ_USER_2, MDR_EQ_USER_3, MDR_EQ_USER_4,
    MDR_EQ_USER_5};

constexpr ImU32 kImChargingGreen = IM_COL32(52, 199, 89, 255);

const char* OnOff(bool on) { return on ? tr("On") : tr("Off"); }

const char* FormatNoiseMode(MDRNoiseMode mode)
{
    if (ConnectionProtocolVersion() == MDR_PROTOCOL_V1)
        return mode == MDR_NOISE_MODE_OFF ? tr("Off") : tr("On");
    switch (mode)
    {
    case MDR_NOISE_MODE_CANCELLING: return tr("Noise Cancelling");
    case MDR_NOISE_MODE_AMBIENT: return tr("Ambient Sound");
    default: return tr("Off");
    }
}

// Midnight blue: headphones before one connects, and until they report their own colour. A touch
// lighter on dark surfaces so the shape still reads.
ImU32 GenericProductColour()
{
    return MaterialYouTheme::ArgbToImU32(MaterialYouTheme::darkMode ? 0xFF46557A : 0xFF2F3B57);
}

// The colour the model is sold in, for the illustration. The headphones report it.
ImU32 ProductColour()
{
    static constexpr MaterialYouTheme::Argb kColours[] = {
        0,          // DEFAULT: the accent
        0xFF2C2C30, // BLACK
        0xFFE9E6E0, // WHITE
        0xFFC8C4BC, // SILVER (platinum)
        0xFFB5332F, // RED
        0xFF2F3B57, // BLUE (midnight)
        0xFFD8B2B7, // PINK
        0xFFE6D36A, // YELLOW
        0xFF8FB5A6, // GREEN
        0xFF7B7D82, // GRAY
        0xFFC7A86C, // GOLD
        0xFFE5DAC0, // CREAM
        0xFFE68C6F, // ORANGE
        0xFF6B5643, // BROWN
        0xFFA69BCB, // VIOLET
    };
    const uint8_t colour = gDevice && gState.mModelAvailable ? gState.mModel.model_color : 0;
    if (colour == 0 || colour >= std::size(kColours))
        return GenericProductColour();
    return MaterialYouTheme::ArgbToImU32(kColours[colour]);
}

// The product to illustrate: the model the headphones report, or until they have (while
// connecting and syncing) the name of the device being connected, so that the illustration
// is the right model from the first frame instead of switching over from the generic one.
mdr::String IllustrationProduct()
{
    mdr::String product = gDevice ? GetText(MDR_TEXT_MODEL_NAME) : mdr::String{};
    if (product.empty())
        product = clientSettings().lastDeviceName.c_str();
    return product;
}

// The 3D model with its motion: it turns in from the side when the headphones connect (the way
// Sony's app presents them), then holds still, and follows the mouse when dragged, coasting after
// a flick and then easing back to its resting angle. Holding still matters: every other angle is
// a new render, a still one is free.
struct ModelSpin
{
    float offset = 0.0f;   // Yaw the user added, radians
    float velocity = 0.0f; // Radians per second after a flick
    double releasedAt = -10.0;
};

void DrawInteractiveModel(const char* id, ImVec2 center, float size, const char* product, float introAge,
                          ImU32 colour, float fade, ModelSpin& spin)
{
    const bool animations = clientSettings().animations;
    const float time = static_cast<float>(ImGui::GetTime());
    const float dt = ImGui::GetIO().DeltaTime;
    const ImVec2 half(size * std::max(Headphones3D::Aspect(product), 0.8f), size);
    ImGui::SetCursorScreenPos(center - half);
    ImGui::InvisibleButton(id, half * 2.0f);
    if (ImGui::IsItemHovered() || ImGui::IsItemActive())
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    if (ImGui::IsItemActive())
    {
        const float delta = ImGui::GetIO().MouseDelta.x * 0.012f;
        spin.offset += delta;
        spin.velocity = dt > 0.0f ? delta / dt : 0.0f;
        spin.releasedAt = time;
    }
    else
    {
        spin.offset += spin.velocity * dt;
        spin.velocity *= std::exp(-4.0f * dt);
        if (time - spin.releasedAt > 0.9f)
        {
            // Back to rest the short way round.
            spin.offset = std::remainder(spin.offset, 2.0f * IM_PI);
            spin.offset = ImApproach(spin.offset, 0.0f, 3.0f);
        }
    }
    const float turnIn = animations ? (1.0f - ImEaseOutCubic(introAge / 1.6f)) * 1.35f : 0.0f;
    const float appear = animations ? ImEaseOutCubic(introAge / 0.7f) : 1.0f;
    Headphones3D::View view;
    view.yaw = Headphones3D::RestYaw(product) + turnIn + spin.offset;
    view.pitch = 0.2f;
    view.size = size * (0.92f + 0.08f * appear);
    Headphones3D::Draw(ImGui::GetWindowDrawList(), center, view, product, colour, fade * appear);
}

// A battery like a phone's status bar shows it, filled to `level` (0..1).
void ImBatteryGlyph(float level, bool charging, bool low)
{
    const float unit = ImGui::GetFontSize();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float h = unit * 0.62f, w = unit * 1.25f;
    const float y = pos.y + (ImGui::GetTextLineHeight() - h) * 0.5f;
    auto* draw = ImGui::GetWindowDrawList();
    const ImU32 outline = ImGui::GetColorU32(ImGuiCol_Text);
    draw->AddRect({pos.x, y}, {pos.x + w, y + h}, outline, h * 0.25f, 0, 1.2f);
    draw->AddRectFilled({pos.x + w + 1.0f, y + h * 0.3f}, {pos.x + w + unit * 0.12f, y + h * 0.7f}, outline, 1.0f);
    const ImU32 fill = low ? MaterialYouTheme::ArgbToImU32(MaterialYouTheme::FixedSurfaceColors::error)
                           : charging ? kImChargingGreen : outline;
    const float inset = 2.5f;
    const float fillW = (w - inset * 2.0f) * std::clamp(level, 0.0f, 1.0f);
    if (fillW > 0.5f)
        draw->AddRectFilled({pos.x + inset, y + inset}, {pos.x + inset + fillW, y + h - inset}, fill, h * 0.12f);
    ImGui::Dummy({w + unit * 0.2f, ImGui::GetTextLineHeight()});
}

// A small outlined pill with a short label, for the codec and DSEE.
void ImBadge(const char* text)
{
    const float unit = ImGui::GetFontSize();
    ImFont* font = clientHeadingFont() ? clientHeadingFont() : ImGui::GetFont();
    const float size = unit * 0.75f;
    const ImVec2 textSize = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
    const ImVec2 pad(unit * 0.5f, unit * 0.2f);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float lineH = ImGui::GetTextLineHeight();
    const float h = textSize.y + pad.y * 2.0f;
    const ImVec2 min(pos.x, pos.y + (lineH - h) * 0.5f), max(pos.x + textSize.x + pad.x * 2.0f, min.y + h);
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRect(min, max, ImGui::GetColorU32(ImGuiCol_TextDisabled), h * 0.5f, 0, 1.2f);
    draw->AddText(font, size, min + pad, ImGui::GetColorU32(ImGuiCol_Text), text);
    ImGui::Dummy({max.x - min.x, lineH});
}

// A chevron centred on `center`; angle 0 points down, -pi/2 right, pi up.
void ImDrawChevron(ImDrawList* draw, ImVec2 center, float size, float angle, ImU32 colour, float thickness = 1.7f)
{
    const float c = std::cos(angle), s = std::sin(angle);
    auto at = [&](float x, float y) { return ImVec2(center.x + x * c - y * s, center.y + x * s + y * c); };
    const ImVec2 points[3] = {at(-size, -size * 0.5f), at(0.0f, size * 0.5f), at(size, -size * 0.5f)};
    draw->AddPolyline(points, 3, colour, 0, thickness);
}

// Glyphs drawn by hand where the icon font has none.
constexpr const char* kImGlyphMore = "\x01more"; // Three dots
constexpr const char* kImGlyphMenu = "\x01menu"; // Three lines

// A round, borderless icon button; the tint behind it eases in on hover. `filled` gives the
// accent disc used for the main action of a group (play/pause).
bool ImRoundIconButton(const char* id, const char* glyph, float diameter, const char* tooltip = nullptr,
                       bool filled = false)
{
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton(id, ImVec2(diameter, diameter));
    const ImGuiID itemId = ImGui::GetItemID();
    const bool active = ImGui::IsItemActive();
    const float hot = ImAnimValue(itemId + 1, ImGui::IsItemHovered() ? 1.0f : 0.0f, 14.0f);
    const float alpha = ImGui::GetStyle().Alpha;
    const ImVec2 c = pos + ImVec2(diameter, diameter) * 0.5f;
    auto* draw = ImGui::GetWindowDrawList();
    ImU32 ink = ImGui::GetColorU32(ImGuiCol_Text);
    if (filled)
    {
        draw->AddCircleFilled(c, diameter * 0.5f,
                              MaterialYouTheme::ArgbToImU32(MaterialYouTheme::AccentTheme().primary,
                                                            alpha * (active ? 0.8f : 1.0f - 0.12f * hot)), 40);
        ink = IM_COL32(255, 255, 255, static_cast<int>(255 * alpha));
    }
    else if (hot > 0.001f || active)
        draw->AddCircleFilled(c, diameter * 0.5f,
                              MaterialYouTheme::ArgbToImU32(MaterialYouTheme::FixedSurfaceColors::onSurface,
                                                            alpha * (active ? 0.16f : 0.08f * hot)), 40);
    const float unit = ImGui::GetFontSize();
    if (glyph == kImGlyphMore)
    {
        for (int i = -1; i <= 1; ++i)
            draw->AddCircleFilled(c + ImVec2(i * unit * 0.36f, 0.0f), unit * 0.11f, ink, 12);
    }
    else if (glyph == kImGlyphMenu)
    {
        for (int i = -1; i <= 1; ++i)
            draw->AddLine(c + ImVec2(-unit * 0.45f, i * unit * 0.3f), c + ImVec2(unit * 0.45f, i * unit * 0.3f), ink, 1.6f);
    }
    else
    {
        const ImVec2 size = ImGui::CalcTextSize(glyph);
        draw->AddText(c - size * 0.5f, ink, glyph);
    }
    if (tooltip && *tooltip)
        ImGui::SetItemTooltip("%s", tooltip);
    return pressed;
}

// A card-sized row that leads somewhere else: icon, title, optional subtitle, chevron.
bool ImNavRow(const char* id, const char* icon, const char* title, const char* subtitle)
{
    const float unit = ImGui::GetFontSize();
    const float h = unit * (subtitle && *subtitle ? 3.5f : 2.9f);
    const float width = ImGui::GetContentRegionAvail().x;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 max = pos + ImVec2(width, h);
    const float rounding = ImGui::GetStyle().ChildRounding;
    ImCardShadow(pos, max, rounding);
    ImGlassPanel(pos, max, rounding,
                 MaterialYouTheme::ArgbToImU32(MaterialYouTheme::FixedSurfaceColors::surfaceContainerLow,
                                               MaterialYouTheme::glassAlpha(0.92f, 0.80f)));
    const bool pressed = ImGui::InvisibleButton(id, ImVec2(width, h));
    const float hot = ImAnimValue(ImGui::GetItemID() + 1, ImGui::IsItemHovered() ? 1.0f : 0.0f, 14.0f);
    auto* draw = ImGui::GetWindowDrawList();
    if (hot > 0.001f)
        draw->AddRectFilled(pos, max, MaterialYouTheme::ArgbToImU32(MaterialYouTheme::AccentTheme().primary, 0.06f * hot), rounding);
    float x = pos.x + unit;
    if (icon && *icon)
    {
        const ImVec2 size = ImGui::CalcTextSize(icon);
        draw->AddText({x, pos.y + (h - size.y) * 0.5f}, ImGui::GetColorU32(ImGuiCol_CheckMark), icon);
        x += unit * 1.7f;
    }
    ImFont* heading = clientHeadingFont() ? clientHeadingFont() : ImGui::GetFont();
    const bool twoLines = subtitle && *subtitle;
    const float titleY = twoLines ? pos.y + unit * 0.7f : pos.y + (h - unit) * 0.5f;
    draw->AddText(heading, unit, {x, titleY}, ImGui::GetColorU32(ImGuiCol_Text), title);
    if (twoLines)
        draw->AddText(ImGui::GetFont(), unit * 0.9f, {x, titleY + unit * 1.3f}, ImGui::GetColorU32(ImGuiCol_TextDisabled), subtitle);
    ImDrawChevron(draw, {max.x - unit * 1.2f, pos.y + h * 0.5f}, unit * 0.32f, -IM_PI * 0.5f,
                  ImGui::GetColorU32(ImGuiCol_TextDisabled));
    return pressed;
}

// A card without a title of its own (the caller draws a link header). Pair with ImEndCard.
void ImBeginCardId(const char* id)
{
    ImGui::PushID(id);
    const float unit = ImGui::GetFontSize();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(unit, unit * 0.75f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                          MaterialYouTheme::ArgbToImVec4(MaterialYouTheme::FixedSurfaceColors::surfaceContainerLow,
                                                         MaterialYouTheme::glassAlpha(0.92f, 0.80f)));
    ImGui::BeginChild("##card", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImCardEdges();
}

// A card's title that leads to the full settings for it: the whole line is the link.
bool ImCardLink(const char* title)
{
    const float unit = ImGui::GetFontSize();
    const float width = ImGui::GetContentRegionAvail().x;
    const float h = ImGui::GetTextLineHeight() + unit * 0.4f;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton("##link", ImVec2(width, h));
    const float hot = ImAnimValue(ImGui::GetItemID() + 1, ImGui::IsItemHovered() ? 1.0f : 0.0f, 14.0f);
    auto* draw = ImGui::GetWindowDrawList();
    ImFont* heading = clientHeadingFont() ? clientHeadingFont() : ImGui::GetFont();
    const ImVec4 colour = ImLerp(ImGui::GetStyleColorVec4(ImGuiCol_Text), ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), hot);
    draw->AddText(heading, unit, {pos.x, pos.y + unit * 0.1f}, ImGui::ColorConvertFloat4ToU32(colour), title);
    ImDrawChevron(draw, {pos.x + width - unit * 0.5f, pos.y + unit * 0.6f}, unit * 0.3f, -IM_PI * 0.5f,
                  ImGui::GetColorU32(ImGuiCol_TextDisabled));
    return pressed;
}

// Collapsible sections for the settings page. The header shows a one-line summary of the
// current values while the section is closed, so nothing needs opening just to check a value.
namespace
{
    ImGuiID gSectionFocus = 0;              // A section to open and scroll to (from a shortcut)
    mdr::Vector<bool> gSectionContentStack; // Whether each open ImBeginSection pushed content styling
}

void ImFocusSection(const char* key) { gSectionFocus = ImHashStr(key); }

bool ImBeginSection(const char* key, const char* icon, const char* title, const char* summary)
{
    const float unit = ImGui::GetFontSize();
    ImGuiStorage* storage = ImGui::GetStateStorage();
    ImGui::PushID(key);
    const ImGuiID openId = ImGui::GetID("##open"), openedAtId = ImGui::GetID("##openedAt");
    ImGui::PopID();
    bool* open = storage->GetBoolRef(openId, false);
    if (gSectionFocus == ImHashStr(key))
    {
        gSectionFocus = 0;
        *open = true;
        storage->SetFloat(openedAtId, static_cast<float>(ImGui::GetTime()));
        ImGui::SetScrollHereY(0.0f);
    }
    ImBeginCardId(key);
    // Header.
    const float width = ImGui::GetContentRegionAvail().x;
    const float h = unit * 2.4f;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    if (ImGui::InvisibleButton("##header", ImVec2(width, h)))
    {
        *open = !*open;
        if (*open)
            storage->SetFloat(openedAtId, static_cast<float>(ImGui::GetTime()));
    }
    const float hot = ImAnimValue(ImGui::GetItemID() + 1, ImGui::IsItemHovered() ? 1.0f : 0.0f, 14.0f);
    const float turn = ImAnimValue(ImGui::GetItemID() + 2, *open ? 1.0f : 0.0f, 16.0f);
    auto* draw = ImGui::GetWindowDrawList();
    const float midY = pos.y + h * 0.5f;
    float x = pos.x;
    if (icon && *icon)
    {
        const ImVec2 size = ImGui::CalcTextSize(icon);
        draw->AddText({x, midY - size.y * 0.5f}, ImGui::GetColorU32(ImGuiCol_CheckMark), icon);
        x += unit * 1.7f;
    }
    ImFont* heading = clientHeadingFont() ? clientHeadingFont() : ImGui::GetFont();
    const ImVec4 titleColour = ImLerp(ImGui::GetStyleColorVec4(ImGuiCol_Text), ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), hot * 0.6f);
    const float titleSize = unit * 1.15f;
    draw->AddText(heading, titleSize, {x, midY - titleSize * 0.52f}, ImGui::ColorConvertFloat4ToU32(titleColour), title);
    const float chevronX = pos.x + width - unit * 0.6f;
    if (summary && *summary && turn < 0.99f)
    {
        // The summary fades out as the section opens; it would only repeat what is shown below.
        const float left = x + heading->CalcTextSizeA(titleSize, FLT_MAX, 0.0f, title).x + unit * 1.0f;
        const float right = chevronX - unit * 1.0f;
        if (right > left + unit * 2.0f)
        {
            const float size = unit * 1.0f;
            const float textW = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, summary).x;
            const float tx = std::max(left, right - textW);
            ImVec4 colour = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
            colour.w *= 1.0f - turn;
            draw->PushClipRect({left, pos.y}, {right, pos.y + h}, true);
            draw->AddText(ImGui::GetFont(), size, {tx, midY - size * 0.55f}, ImGui::ColorConvertFloat4ToU32(colour), summary);
            draw->PopClipRect();
        }
    }
    ImDrawChevron(draw, {chevronX, midY}, unit * 0.32f, IM_PI * turn, ImGui::GetColorU32(ImGuiCol_TextDisabled));
    if (!*open)
    {
        gSectionContentStack.push_back(false);
        return false;
    }
    // Content: a hairline under the header, then everything fades and rises in.
    const ImVec2 line = ImGui::GetCursorScreenPos();
    draw->AddLine({line.x, line.y}, {line.x + width, line.y},
                  MaterialYouTheme::ArgbToImU32(MaterialYouTheme::FixedSurfaceColors::outlineVariant, 0.8f));
    ImGui::Dummy({0.0f, unit * 0.2f});
    const float openedAt = storage->GetFloat(openedAtId, -10.0f);
    const float progress = clientSettings().animations
        ? ImEaseOutCubic((static_cast<float>(ImGui::GetTime()) - openedAt) / 0.3f) : 1.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * progress);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (1.0f - progress) * unit * 0.6f);
    gSectionContentStack.push_back(true);
    return true;
}

void ImEndSection()
{
    const bool content = !gSectionContentStack.empty() && gSectionContentStack.back();
    if (!gSectionContentStack.empty())
        gSectionContentStack.pop_back();
    if (content)
    {
        ImGui::PopStyleVar();
        ImGui::Dummy({0.0f, ImGui::GetFontSize() * 0.2f});
    }
    ImEndCard();
}

// A page title with a back button. Returns true when the user asked to go back.
bool ImPageHeader(const char* title)
{
    const float unit = ImGui::GetFontSize();
    const float d = unit * 2.2f;
    const bool back = ImRoundIconButton("##Back", PSI_CHEVRON_LEFT, d, tr("Back"));
    ImGui::SameLine(0.0f, unit * 0.4f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (d - unit * 1.35f) * 0.5f);
    ImPushHeading(1.35f);
    ImGui::TextUnformatted(title);
    ImPopHeading();
    ImGui::Spacing();
    return back;
}

// Label on the left, a read-only value on the right.
void ImInfoRow(const char* label, const char* value)
{
    const char* shown = value && *value ? value : "-";
    ImRowLabel(label, ImGui::CalcTextSize(shown).x);
    ImGui::TextDisabled("%s", shown);
}

bool ImToggleRowHint(const char* label, bool* value, const char* hint)
{
    const bool changed = ImToggleRow(label, value);
    if (hint && *hint)
        ImRowHint(hint);
    return changed;
}

// ---------------------------------------------------------------------------------------------
// Noise control: the three round mode buttons, shared by the home shortcut and the settings page.

enum class ImModeIcon { NoiseCancelling, Ambient, Off };

void ImDrawModeIcon(ImDrawList* draw, ImVec2 c, float s, ImModeIcon icon, ImU32 ink)
{
    const float t = s * 0.1f;
    if (icon == ImModeIcon::Off)
    {
        draw->AddCircle(c, s * 0.42f, ink, 32, t);
        return;
    }
    // A head and shoulders; noise cancelling wraps it in a closed ring, ambient in a dotted one.
    draw->AddCircle(c + ImVec2(0.0f, -s * 0.2f), s * 0.2f, ink, 24, t);
    draw->PathArcTo(c + ImVec2(0.0f, s * 0.52f), s * 0.42f, IM_PI * 1.12f, IM_PI * 1.88f, 20);
    draw->PathStroke(ink, 0, t);
    if (icon == ImModeIcon::NoiseCancelling)
        draw->AddCircle(c, s * 0.78f, ink, 40, t);
    else
        for (int i = 0; i < 16; ++i)
        {
            const float a = i * IM_PI * 2.0f / 16.0f;
            draw->AddCircleFilled(c + ImVec2(std::cos(a), std::sin(a)) * (s * 0.78f), t * 0.6f, ink, 8);
        }
}

bool ImModeButton(const char* id, ImModeIcon icon, const char* label, bool selected, float width)
{
    const float unit = ImGui::GetFontSize();
    const float r = unit * 1.45f;
    const float h = r * 2.0f + unit * 0.45f + ImGui::GetTextLineHeight();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton(id, ImVec2(width, h));
    const ImGuiID itemId = ImGui::GetItemID();
    const float on = ImAnimValue(itemId + 1, selected ? 1.0f : 0.0f, 14.0f);
    const float hot = ImAnimValue(itemId + 2, ImGui::IsItemHovered() ? 1.0f : 0.0f, 14.0f);
    const float alpha = ImGui::GetStyle().Alpha;
    const ImVec2 c(pos.x + width * 0.5f, pos.y + r);
    auto* draw = ImGui::GetWindowDrawList();
    const ImVec4 accent = MaterialYouTheme::ArgbToImVec4(MaterialYouTheme::AccentTheme().primary);
    ImVec4 idle = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
    idle.w *= hot;
    ImVec4 fill = ImLerp(idle, accent, on);
    fill.w *= alpha;
    if (fill.w > 0.001f)
        draw->AddCircleFilled(c, r, ImGui::ColorConvertFloat4ToU32(fill), 48);
    if (on < 0.999f)
        draw->AddCircle(c, r - 0.75f, MaterialYouTheme::ArgbToImU32(MaterialYouTheme::FixedSurfaceColors::outline, alpha * (1.0f - on)), 48, 1.4f);
    ImVec4 ink = ImLerp(ImGui::GetStyleColorVec4(ImGuiCol_Text), ImVec4(1, 1, 1, 1), on);
    ink.w *= alpha;
    ImDrawModeIcon(draw, c, r * 0.62f, icon, ImGui::ColorConvertFloat4ToU32(ink));
    ImFont* font = selected && clientHeadingFont() ? clientHeadingFont() : ImGui::GetFont();
    // Long labels (Japanese "ノイズキャンセリング") shrink to their column instead of running into the next.
    float fontSize = unit;
    ImVec2 size = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label);
    if (size.x > width - unit * 0.3f)
    {
        fontSize *= std::max(0.65f, (width - unit * 0.3f) / size.x);
        size = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label);
    }
    ImVec4 labelColour = ImLerp(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled), ImGui::GetStyleColorVec4(ImGuiCol_Text), on);
    labelColour.w *= alpha;
    draw->AddText(font, fontSize, {c.x - size.x * 0.5f, pos.y + r * 2.0f + unit * 0.35f + (unit - fontSize) * 0.5f},
                  ImGui::ColorConvertFloat4ToU32(labelColour), label);
    return pressed;
}

// Returns true when the mode changed.
bool DrawNoiseModeButtons()
{
    const bool supportNC = FeatureAvailable(MDR_FEATURE_NOISE_CANCELLING);
    const bool supportASM = FeatureAvailable(MDR_FEATURE_AMBIENT_SOUND);
    const char* labels[3];
    ImModeIcon icons[3];
    MDRNoiseMode modes[3];
    int count = 0;
    if (supportNC)
        labels[count] = tr("Noise Cancelling"), icons[count] = ImModeIcon::NoiseCancelling, modes[count++] = MDR_NOISE_MODE_CANCELLING;
    if (supportASM)
        labels[count] = tr("Ambient Sound"), icons[count] = ImModeIcon::Ambient, modes[count++] = MDR_NOISE_MODE_AMBIENT;
    labels[count] = tr("Off"), icons[count] = ImModeIcon::Off, modes[count++] = MDR_NOISE_MODE_OFF;
    const float unit = ImGui::GetFontSize();
    const float width = ImGui::GetContentRegionAvail().x;
    const float cell = std::min(width / count, unit * 7.5f);
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float left = (width - cell * count) * 0.5f;
    float height = 0.0f;
    bool changed = false;
    for (int i = 0; i < count; ++i)
    {
        ImGui::SetCursorScreenPos(start + ImVec2(left + i * cell, 0.0f));
        ImGui::PushID(i);
        if (ImModeButton("##mode", icons[i], labels[i], gState.mNoise.mode == modes[i], cell) && gState.mNoise.mode != modes[i])
        {
            gState.mNoise.mode = modes[i];
            if (modes[i] == MDR_NOISE_MODE_AMBIENT && gState.mNoise.ambient_level == 0)
                gState.mNoise.ambient_level = 20;
            changed = true;
        }
        height = ImGui::GetItemRectSize().y;
        ImGui::PopID();
    }
    ImGui::SetCursorScreenPos(start + ImVec2(0.0f, height));
    ImGui::Dummy({width, 0.0f});
    return changed;
}

// The noise controls. The home shortcut shows the modes (and the level while ambient sound is
// on); the settings page adds every secondary option with an explanation.
void DrawNoiseControls(bool detailed)
{
    const bool supportAutoASM = FeatureAvailable(MDR_FEATURE_ADAPTIVE_AMBIENT_SOUND);
    bool changed = false;
    const MDRProtocolVersion protocolVersion = ConnectionProtocolVersion();
    if (protocolVersion == MDR_PROTOCOL_V1)
    {
        bool ncAsmEnabled = gState.mNoise.mode != MDR_NOISE_MODE_OFF;
        if (ImToggleRowHint(tr("Ambient Sound Control"), &ncAsmEnabled,
                            detailed ? tr("Noise cancelling blocks out the world around you; ambient sound lets it in through the microphones, for commutes or a quick chat.") : nullptr))
            gState.mNoise.mode = ncAsmEnabled ? MDR_NOISE_MODE_V1_ON : MDR_NOISE_MODE_OFF, changed = true;
        ImGui::BeginDisabled(!ncAsmEnabled);
        // -1: Noise Cancelling, 0: Wind Noise Reduction, 1-20: Ambient Sound
        int sliderLevel = static_cast<int8_t>(gState.mNoise.ambient_level);
        const mdr::String label = sliderLevel == -1 ? mdr::String(tr("Noise Cancelling"))
            : sliderLevel == 0 ? mdr::String(tr("Wind Noise Reduction"))
            : mdr::Format("{} %d", tr("Ambient Sound"));
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        const bool sliderChanged = ImSliderInt("##AmbStrength", &sliderLevel, -1, 20, label.c_str());
        if (sliderChanged)
            gState.mNoise.ambient_level = static_cast<uint8_t>(sliderLevel), changed = true;
        gState.mNoise.changing_asm_level = sliderChanged && ImGui::IsItemActive();
        if (ImGui::IsItemDeactivatedAfterEdit())
            changed = true;
        ImGui::BeginDisabled(sliderLevel < 1);
        bool focusOnVoice = gState.mNoise.focus_on_voice != MDR_FALSE;
        if (ImToggleRowHint(tr("Voice Passthrough"), &focusOnVoice,
                            detailed ? tr("Lets voices through while keeping other surrounding noise down.") : nullptr))
            gState.mNoise.focus_on_voice = focusOnVoice ? MDR_TRUE : MDR_FALSE, changed = true;
        ImGui::EndDisabled();
        ImGui::EndDisabled();
    }
    else if (protocolVersion == MDR_PROTOCOL_V2)
    {
        if (detailed)
            ImRowHint(tr("Noise cancelling blocks out the world around you; ambient sound lets it in through the microphones, for commutes or a quick chat."));
        ImGui::Spacing();
        changed |= DrawNoiseModeButtons();
        const bool ambient = gState.mNoise.mode == MDR_NOISE_MODE_AMBIENT;
        if (ambient || detailed)
        {
            ImGui::Spacing();
            ImGui::BeginDisabled(!ambient);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            bool ambientChanged = false;
            int ambientLevel = gState.mNoise.ambient_level;
            const mdr::String label = mdr::Format("{}  %d", tr("Ambient Strength"));
            if (ImSliderInt("##AmbStrength", &ambientLevel, 1, 20, label.c_str()))
                gState.mNoise.ambient_level = static_cast<uint8_t>(ambientLevel), ambientChanged = changed = true;
            gState.mNoise.changing_asm_level = ambientChanged && ImGui::IsItemActive();
            if (ImGui::IsItemDeactivatedAfterEdit())
                changed = true;
            if (detailed)
            {
                ImRowHint(tr("The higher the level, the more of your surroundings you hear."));
                if (supportAutoASM)
                {
                    bool adaptive = gState.mNoise.adaptive_ambient != MDR_FALSE;
                    if (ImToggleRowHint(tr("Auto Ambient Sound"), &adaptive,
                                        tr("Adjusts the ambient sound level automatically to the noise around you.")))
                        gState.mNoise.adaptive_ambient = adaptive ? MDR_TRUE : MDR_FALSE, changed = true;
                    ImGui::BeginDisabled(!adaptive);
                    constexpr MDRAdaptiveSensitivity kSelections[] = {
                        MDR_ADAPTIVE_SENSITIVITY_STANDARD, MDR_ADAPTIVE_SENSITIVITY_HIGH, MDR_ADAPTIVE_SENSITIVITY_LOW};
                    changed |= ImComboBoxItems(tr("Sensitivity"), std::span{kSelections},
                                               gState.mNoise.adaptive_sensitivity, FormatAdaptiveSensitivity);
                    ImGui::EndDisabled();
                }
                bool focusOnVoice = gState.mNoise.focus_on_voice != MDR_FALSE;
                if (ImToggleRowHint(tr("Voice Passthrough"), &focusOnVoice,
                                    tr("Lets voices through while keeping other surrounding noise down.")))
                    gState.mNoise.focus_on_voice = focusOnVoice ? MDR_TRUE : MDR_FALSE, changed = true;
            }
            ImGui::EndDisabled();
        }
    }
    if (changed && gState.mNoiseAvailable)
        mdrHeadphonesSetNoiseControl(gDevice, &gState.mNoise);
}

// ---------------------------------------------------------------------------------------------
// General settings: a list the headphones publish. Each known one is shown in the section it
// belongs to (Sony's grouping); anything unrecognised falls through to "System".

namespace
{
    using StringPair = std::pair<const char*, const char*>;
    constexpr StringPair kGeneralSubjects[] = {{"MULTIPOINT_SETTING", "Connect to 2 devices simultaneously"},
                                               {"SIDETONE_SETTING", "Capture Voice During a Phone Call"},
                                               {"TOUCH_PANEL_SETTING", "Touch sensor control panel"}};
    constexpr StringPair kGeneralSummaries[] = {
        {"MULTIPOINT_SETTING_SUMMARY",
         "For example, when using the audio device with both a PC and a smartphone, you can use it comfortably "
         "without needing to switch connections. During simultaneous connections, playback with the LDAC codec "
         "is not possible even if Prioritize Sound Quality is selected."},
        {"MULTIPOINT_SETTING_SUMMARY_LDAC_AVAILABLE",
         "For example, when using the audio device with both a PC and a smartphone, you can use it comfortably "
         "without needing to switch connections."},
        {"SIDETONE_SETTING_SUMMARY",
         "Your own voice will be easier to hear during calls. If your voice sounds too loud or background "
         "noise is distracting, please turn off this feature."},
    };

    const char* LookupGeneralString(const char* key, std::span<const StringPair> strings)
    {
        auto it = std::lower_bound(strings.begin(), strings.end(), key,
                                   [](const StringPair& lhs, const char* rhs) { return strcmp(lhs.first, rhs) < 0; });
        if (it == strings.end() || strcmp(it->first, key) != 0)
            return nullptr;
        return tr(it->second);
    }

    bool IsClaimedGeneralSetting(const mdr::String& key)
    {
        return key == "MULTIPOINT_SETTING" || key == "SIDETONE_SETTING" || key == "TOUCH_PANEL_SETTING";
    }
}

// -1 when the headphones do not publish the setting.
int GeneralSettingValue(const char* key)
{
    for (auto& [info, setting] : gState.mGeneralSettings)
        if (info.type == MDR_GENERAL_SETTING_BOOLEAN && GetText(MDR_TEXT_GENERAL_SETTING_SUBJECT, info.index) == key)
            return setting.boolean_value != MDR_FALSE ? 1 : 0;
    return -1;
}

// Draws the boolean general settings: only `onlyKey` when given, otherwise the unclaimed ones.
void DrawGeneralSettingRows(const char* onlyKey)
{
    for (auto& [info, setting] : gState.mGeneralSettings)
    {
        if (info.type != MDR_GENERAL_SETTING_BOOLEAN)
            continue;
        const mdr::String subjectKey = GetText(MDR_TEXT_GENERAL_SETTING_SUBJECT, info.index);
        if (onlyKey ? subjectKey != onlyKey : IsClaimedGeneralSetting(subjectKey))
            continue;
        const mdr::String summaryKey = GetText(MDR_TEXT_GENERAL_SETTING_SUMMARY, info.index);
        const char* subject = LookupGeneralString(subjectKey.c_str(), kGeneralSubjects);
        const char* summary = summaryKey.empty() ? nullptr : LookupGeneralString(summaryKey.c_str(), kGeneralSummaries);
        if (!summary && subjectKey == "TOUCH_PANEL_SETTING")
            summary = tr("Turning this off disables the touch controls on the outside of the headphones, to avoid accidental touches.");
        bool value = setting.boolean_value != MDR_FALSE;
        ImGui::PushID(static_cast<int>(info.index));
        ImGui::BeginDisabled(subjectKey.empty() || !info.writable);
        if (ImToggleRowHint(subject ? subject : tr("<Unknown>"), &value, summary))
        {
            setting.boolean_value = value ? MDR_TRUE : MDR_FALSE;
            mdrHeadphonesSetGeneralSetting(gDevice, &setting);
        }
        ImGui::EndDisabled();
        ImGui::PopID();
    }
}

// ---------------------------------------------------------------------------------------------
// Volume, shared by the player card.

void DrawVolumeRow()
{
    // When the headphones are this PC's audio output, the slider is the OS volume for that output:
    // Windows then shows the same number and forwards the change to the headphones itself (AVRCP
    // absolute volume). Setting only the MDR side left the two sliders disagreeing. The endpoint
    // can appear a little after the MDR link, so keep looking for it while connected.
    static uint64_t systemVolumeProbeMs = 0;
    float systemScalar = 0.0f;
    bool systemVolume = clientPlatformSystemVolumeGet(&systemScalar) != 0;
    if (!systemVolume && SDL_GetTicks() - systemVolumeProbeMs >= 10000) // Enumerating endpoints is a visible hitch
    {
        systemVolumeProbeMs = SDL_GetTicks();
        const mdr::String model = GetText(MDR_TEXT_MODEL_NAME);
        if (!model.empty() && clientPlatformSystemVolumeBind(model.c_str()))
            systemVolume = clientPlatformSystemVolumeGet(&systemScalar) != 0;
    }
    // Slider steps are the device's own 0..30 levels; the label adds the percentage Windows shows.
    int volume = systemVolume
        ? (static_cast<int>(std::lround(systemScalar * kAvrcpVolumeMax)) * kDeviceVolumeMax + kAvrcpVolumeMax / 2) / kAvrcpVolumeMax
        : gState.mPlayback.volume;
    const mdr::String value = mdr::Format("{}/{}  ({}%)", volume, kDeviceVolumeMax, DeviceVolumeToPercent(volume));
    const float valueWidth = ImGui::CalcTextSize(value.c_str()).x;
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", PSI_VOLUME_DOWN);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(std::max(1.0f, ImGui::GetContentRegionAvail().x - valueWidth - ImGui::GetStyle().ItemSpacing.x));
    bool changed = ImSliderInt("##Volume", &volume, 0, kDeviceVolumeMax);
    // Like the Windows volume flyout: mouse wheel while hovering, and Left/Right arrows while
    // hovering or focused, step one device level. Owning the keys keeps the wheel from scrolling
    // the surrounding panel and stops the arrows from moving keyboard focus elsewhere.
    if (ImGui::IsItemHovered() || ImGui::IsItemFocused())
    {
        int step = 0;
        // Wheel input can arrive as fractions of a notch spread over frames; accumulate so one
        // notch is exactly one level.
        static float wheelAccumulator = 0.0f;
        if (ImGui::IsItemHovered())
        {
            ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
            wheelAccumulator += ImGui::GetIO().MouseWheel;
            const int notches = static_cast<int>(wheelAccumulator);
            wheelAccumulator -= static_cast<float>(notches);
            step += notches;
        }
        else
            wheelAccumulator = 0.0f;
        ImGui::SetItemKeyOwner(ImGuiKey_LeftArrow);
        ImGui::SetItemKeyOwner(ImGuiKey_RightArrow);
        step -= ImGui::IsKeyPressed(ImGuiKey_LeftArrow) ? 1 : 0;
        step += ImGui::IsKeyPressed(ImGuiKey_RightArrow) ? 1 : 0;
        if (step != 0)
        {
            const int stepped = std::clamp(volume + step, 0, kDeviceVolumeMax);
            if (stepped != volume)
                volume = stepped, changed = true;
        }
    }
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", value.c_str());
    if (changed && systemVolume)
    {
        // Land exactly on the AVRCP step Windows will send, so the headphones end up on `volume`.
        const int avrcp = (volume * kAvrcpVolumeMax + kDeviceVolumeMax / 2) / kDeviceVolumeMax;
        clientPlatformSystemVolumeSet(static_cast<float>(avrcp) / kAvrcpVolumeMax);
    }
    else if (changed)
    {
        MDRPlayback playback = gState.mPlayback;
        playback.volume = static_cast<uint8_t>(volume);
        if (mdrHeadphonesSetPlayback(gDevice, &playback) == MDR_RESULT_OK)
        {
            gState.mPlayback = playback;
            gState.mPlaybackVolumeStaged = true;
        }
    }
}

// ---------------------------------------------------------------------------------------------
// Pages.

enum class ConnectedPage { Home, Settings, AppSettings };
namespace
{
    ConnectedPage gConnectedPage = ConnectedPage::Home;
    int gDemoTab = -1;        // --demo-tab: 0 home, 1-6 a settings section, 7 app settings, 8 connection sheet
    float gDemoScroll = -1.0f; // --demo-scroll: scroll the connected page this far once it has settled
}
void clientDemoSelectTab(int tab) { gDemoTab = tab; }
void clientDemoScroll(float pixels) { gDemoScroll = pixels; }
void ShowConnectOverlay(bool preview);

void OpenSettingsSection(const char* key)
{
    gConnectedPage = ConnectedPage::Settings;
    ImFocusSection(key);
}

// The card's corner buttons (and the same ones on the sticky bar) only record what was asked;
// the popups open at page level, where both places share one ID stack.
enum class DeviceAction { None, More, Shutdown };
namespace
{
    DeviceAction gPendingDeviceAction = DeviceAction::None;
}

void DrawDeviceActionButtons(float right, float y, float d)
{
    const float unit = ImGui::GetFontSize();
    float x = right - d;
    ImGui::SetCursorScreenPos({x, y});
    if (ImRoundIconButton("##More", kImGlyphMore, d, tr("More")))
        gPendingDeviceAction = DeviceAction::More;
    x -= d + unit * 0.2f;
    ImGui::SetCursorScreenPos({x, y});
    if (ImRoundIconButton("##AppSettings", kImGlyphMenu, d, tr("App Settings")))
        gConnectedPage = ConnectedPage::AppSettings;
    if (FeatureAvailable(MDR_FEATURE_SHUTDOWN))
    {
        x -= d + unit * 0.2f;
        ImGui::SetCursorScreenPos({x, y});
        if (ImRoundIconButton("##Power", PSI_OFF, d, tr("Turn off")))
            gPendingDeviceAction = DeviceAction::Shutdown;
    }
}

void DrawDeviceActionPopups()
{
    const float unit = ImGui::GetFontSize();
    if (gPendingDeviceAction == DeviceAction::More)
        ImGui::OpenPopup("##DeviceMore");
    else if (gPendingDeviceAction == DeviceAction::Shutdown)
        ImGui::OpenPopup("##ShutdownConfirm");
    gPendingDeviceAction = DeviceAction::None;
    if (ImGui::BeginPopup("##DeviceMore"))
    {
        if (ImGui::MenuItem(tri(PSI_UNLINK, "Disconnect")))
        {
            MDRConnection* conn = clientPlatformConnectionGet();
            gAutoConnectSuppressed = true;
            CloseDevice();
            mdrConnectionDisconnect(conn);
            connState = CONN_STATE_NO_CONNECTION;
        }
#ifdef MDR_CLIENT_DEBUGGER
        ImGui::Separator();
        ImGui::MenuItem(tr("Protocol Debugger"), nullptr, &gDebuggerOpen);
        if (ImGui::MenuItem(tri(PSI_BUG, "Trigger disconnect error")))
            DisconnectWithModal("Disconnect error manually triggered from the debug menu");
#endif
        ImGui::EndPopup();
    }
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(display * 0.5f, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({std::min(display.x - unit * 3.0f, unit * 24.0f), 0.0f});
    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.0f, 0.0f, 0.0f, 0.45f));
    const bool confirmOpen = ImGui::BeginPopupModal("##ShutdownConfirm", nullptr, kImWindowFlagsTopMost);
    ImGui::PopStyleColor();
    if (confirmOpen)
    {
        ImGlassEdges(ImGui::GetWindowPos(), ImGui::GetWindowPos() + ImGui::GetWindowSize(), ImGui::GetStyle().PopupRounding);
        {
            ImStylesRAII styles;
            styles.PushFont(clientHeadingFont(), unit * 1.3f);
            ImTextCentered(tr("Turn off the headphones?"));
        }
        ImGui::Spacing();
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("%s", tr("The headphones switch off and disconnect. Turn them on again with their power button."));
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
        ImStylesRAII buttons;
        buttons.PushVar(ImGuiStyleVar_FrameRounding, kImCapsuleRounding);
        if (ImModalButton(tr("Cancel"), 0, 2) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            ImGui::CloseCurrentPopup();
        buttons.PushCol(ImGuiCol_Button, MaterialYouTheme::ArgbToImVec4(MaterialYouTheme::FixedSurfaceColors::error, 0.16f));
        buttons.PushCol(ImGuiCol_ButtonHovered, MaterialYouTheme::ArgbToImVec4(MaterialYouTheme::FixedSurfaceColors::error, 0.26f));
        buttons.PushCol(ImGuiCol_ButtonActive, MaterialYouTheme::ArgbToImVec4(MaterialYouTheme::FixedSurfaceColors::error, 0.36f));
        if (ImModalButton(tri(PSI_OFF, "Turn off"), 1, 2))
        {
            MDRPower power{};
            if (gDevice && mdrHeadphonesGetPower(gDevice, &power) == MDR_RESULT_OK)
            {
                power.shutdown_requested = MDR_TRUE;
                mdrHeadphonesSetPower(gDevice, &power);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// A compact summary of the batteries: "90%", or "L 0%  R 100%  Case 53%".
mdr::String BatterySummary()
{
    mdr::String text;
    for (const MDRBattery& battery : gState.mBatteries)
    {
        if (!battery.present || !battery.update_threshold_percent)
            continue;
        if (!text.empty())
            text += "   ";
        if (battery.part != MDR_BATTERY_MAIN)
            text += mdr::Format("{} ", battery.part == MDR_BATTERY_LEFT ? tr("Left") : battery.part == MDR_BATTERY_RIGHT ? tr("Right") : tr("Case"));
        text += mdr::Format("{}%", static_cast<unsigned>(battery.level_percent));
    }
    return text;
}

constexpr float kDeviceHeroUnits = 11.0f;

// The device card at the top: the model in 3D in its own colour, name, battery, codec, and the
// power / app settings / more buttons in the corner. `scrollY` drives the parallax as the page
// scrolls: the model rises faster than the card and fades out, as in Sony's app.
void DrawDeviceHero(float scrollY)
{
    const mdr::String modelName = GetText(MDR_TEXT_MODEL_NAME);
    const char* name = modelName.empty() ? tr("Your headphones") : modelName.c_str();
    const float unit = ImGui::GetFontSize();
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const float height = unit * kDeviceHeroUnits;
    const ImVec2 end = start + ImVec2(width, height);
    const float rounding = unit * 1.5f;
    auto* draw = ImGui::GetWindowDrawList();
    const MaterialYouTheme::Argb tint = MaterialYouTheme::AccentTheme().primaryContainer;
    ImCardShadow(start, end, rounding);
    ImGlassPanel(start, end, rounding, MaterialYouTheme::ArgbToImU32(tint, MaterialYouTheme::glassAlpha(0.9f, 0.76f)));
    const bool illustrated = width > unit * 26;
    if (illustrated)
    {
        const bool animations = clientSettings().animations;
        const float lift = scrollY * 0.35f;
        const float fade = 1.0f - std::clamp(scrollY / (height * 0.55f), 0.0f, 1.0f);
        const ImVec2 center = start + ImVec2(width - unit * 6.2f, height * 0.56f - lift);
        draw->PushClipRect(start, end, true);
        const float breath = animations ? 0.5f + 0.5f * std::sin(static_cast<float>(ImGui::GetTime()) * 1.6f) : 0.5f;
        draw->AddCircleFilled(center, unit * 4.1f,
                              MaterialYouTheme::ArgbToImU32(MaterialYouTheme::AccentTheme().primary, (0.05f + 0.04f * breath) * fade), 64);
        static ModelSpin spin;
        DrawInteractiveModel("##HeroModel", center, unit * 3.2f, IllustrationProduct().c_str(),
                             static_cast<float>(ImGui::GetTime() - gRevealStart[0]), ProductColour(), fade, spin);
        draw->PopClipRect();
    }
    // The text and buttons fade as they scroll up under the sticky bar that takes over from them.
    const float textFade = 1.0f - std::clamp(scrollY / (height * 0.5f), 0.0f, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * textFade);
    // Name.
    const float textLeft = start.x + unit * 1.4f;
    const float textWidth = std::max(unit, (illustrated ? end.x - unit * 10.4f : end.x - unit * 1.4f) - textLeft);
    {
        float size = unit * 1.7f;
        ImFont* font = clientHeadingFont() ? clientHeadingFont() : ImGui::GetFont();
        const float measured = font->CalcTextSizeA(size, FLT_MAX, 0.0f, name).x;
        if (measured > textWidth)
            size *= textWidth / measured;
        draw->AddText(font, size, {textLeft, start.y + unit * 2.6f}, ImGui::GetColorU32(ImGuiCol_Text), name);
    }
    // Battery, one entry per part.
    ImGui::SetCursorScreenPos({textLeft, start.y + unit * 5.2f});
    bool anyBattery = false;
    for (const MDRBattery& battery : gState.mBatteries)
    {
        if (!battery.present || !battery.update_threshold_percent)
            continue;
        if (anyBattery)
            ImGui::SameLine(0.0f, unit * 0.9f);
        anyBattery = true;
        if (battery.part != MDR_BATTERY_MAIN)
        {
            const char* part = battery.part == MDR_BATTERY_LEFT ? tr("Left") : battery.part == MDR_BATTERY_RIGHT ? tr("Right") : tr("Case");
            ImGui::TextDisabled("%s", part);
            ImGui::SameLine(0.0f, unit * 0.3f);
        }
        const bool charging = battery.charging == MDR_CHARGING_YES;
        ImBatteryGlyph(battery.level_percent / 100.0f, charging, battery.level_percent <= 20 && !charging);
        ImGui::SameLine(0.0f, unit * 0.15f);
        ImPushHeading(1.0f);
        ImGui::Text("%u%%", static_cast<unsigned>(battery.level_percent));
        ImPopHeading();
        if (charging)
            ImGui::SetItemTooltip("%s", tr("Charging"));
    }
    if (!anyBattery)
        ImGui::TextDisabled("%s", tr("Waiting for battery status"));
    // Codec, DSEE, sync state.
    ImGui::SetCursorScreenPos({textLeft, start.y + unit * 7.3f});
    bool anyBadge = false;
    if (gState.mModelAvailable && gState.mModel.audio_codec != MDR_AUDIO_CODEC_UNKNOWN)
    {
        ImBadge(FormatAudioCodec(gState.mModel.audio_codec));
        anyBadge = true;
    }
    if (FeatureAvailable(MDR_FEATURE_DSEE) && gState.mEqualizerAvailable && gState.mEqualizer.dsee_enabled)
    {
        if (anyBadge)
            ImGui::SameLine(0.0f, unit * 0.4f);
        ImBadge(FormatDseeType(gState.mEqualizer.dsee_type));
        anyBadge = true;
    }
    if (gDevice && !mdrHeadphonesIsReady(gDevice))
    {
        if (anyBadge)
            ImGui::SameLine(0.0f, unit * 0.6f);
        ImInlineSpinner(ImGui::GetColorU32(ImGuiCol_TextDisabled), ImGui::GetTextLineHeight());
        ImGui::SameLine(0.0f, unit * 0.2f);
        ImGui::TextDisabled("%s", tr("Syncing settings"));
    }
    DrawDeviceActionButtons(end.x - unit * 0.6f, start.y + unit * 0.5f, unit * 2.1f);
    ImGui::PopStyleVar();
    ImGui::SetCursorScreenPos(start);
    ImGui::Dummy({width, height});
}

// Once the device card has scrolled away, a slim bar takes its place at the top of the page with
// the name, the battery and the same buttons: Sony's collapsing header.
void DrawStickyHeader(float show)
{
    const float unit = ImGui::GetFontSize();
    ImGuiWindow* column = ImGui::GetCurrentWindow();
    const float width = column->Size.x - (column->ScrollbarY ? ImGui::GetStyle().ScrollbarSize : 0.0f);
    const float h = unit * 3.2f;
    const ImVec2 backup = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(column->Pos);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * show);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("##StickyHeader", ImVec2(width, h), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar(); // Window padding; the alpha stays for the contents
    auto* draw = ImGui::GetWindowDrawList();
    const ImVec2 min = ImGui::GetWindowPos(), max = min + ImVec2(width, h);
    const float slide = (1.0f - show) * unit * 0.6f; // Drops in from the top
    const ImVec2 top(min.x, min.y - slide), bottom(max.x, max.y - slide);
    // The background turns opaque well before the text arrives, so the card scrolling underneath
    // never shows through the name.
    const float cover = std::min(1.0f, show * 2.5f);
    draw->AddRectFilled(top + ImVec2(0.0f, 3.0f), bottom + ImVec2(0.0f, 3.0f), IM_COL32(0, 0, 0, static_cast<int>(22 * cover)),
                        unit, ImDrawFlags_RoundCornersBottom);
    draw->AddRectFilled(top, bottom,
                        MaterialYouTheme::ArgbToImU32(MaterialYouTheme::AccentTheme().primaryContainer, cover), unit,
                        ImDrawFlags_RoundCornersBottom);
    const mdr::String modelName = GetText(MDR_TEXT_MODEL_NAME);
    ImFont* heading = clientHeadingFont() ? clientHeadingFont() : ImGui::GetFont();
    const float nameSize = unit * 1.2f;
    const float y = top.y + (h - nameSize) * 0.5f;
    ImVec4 text = ImGui::GetStyleColorVec4(ImGuiCol_Text);
    text.w *= show;
    draw->AddText(heading, nameSize, {top.x + unit, y}, ImGui::ColorConvertFloat4ToU32(text),
                  modelName.empty() ? tr("Your headphones") : modelName.c_str());
    const mdr::String battery = BatterySummary();
    if (!battery.empty())
    {
        const float nameW = heading->CalcTextSizeA(nameSize, FLT_MAX, 0.0f, modelName.c_str()).x;
        ImVec4 muted = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
        muted.w *= show;
        draw->AddText(ImGui::GetFont(), unit * 0.9f, {top.x + unit * 1.8f + nameW, top.y + (h - unit * 0.9f) * 0.5f},
                      ImGui::ColorConvertFloat4ToU32(muted), battery.c_str());
    }
    const float d = unit * 2.0f;
    DrawDeviceActionButtons(bottom.x - unit * 0.5f, top.y + (h - d) * 0.5f, d);
    ImGui::EndChild();
    ImGui::PopStyleVar(); // Alpha
    ImGui::SetCursorScreenPos(backup);
}

// A small equalizer glyph: five faders at different heights.
void ImDrawEqualizerGlyph(ImDrawList* draw, ImVec2 c, float size, ImU32 ink)
{
    constexpr float kLevels[] = {0.2f, -0.35f, 0.4f, -0.1f, 0.25f};
    for (int i = 0; i < 5; ++i)
    {
        const float x = c.x + (i - 2) * size * 0.28f;
        draw->AddLine({x, c.y - size * 0.5f}, {x, c.y + size * 0.5f}, ink, 1.2f);
        draw->AddCircleFilled({x, c.y - kLevels[i] * size}, size * 0.1f, ink, 8);
    }
}

void DrawNoiseShortcut()
{
    if (!FeatureAvailable(MDR_FEATURE_NOISE_CANCELLING) && !FeatureAvailable(MDR_FEATURE_AMBIENT_SOUND))
        return;
    ImBeginCardId("##NoiseShortcut");
    if (ImCardLink(tr("Ambient Sound Control")))
        OpenSettingsSection("noise");
    DrawNoiseControls(false);
    ImEndCard();
}

void DrawEqualizerShortcut()
{
    ImBeginCardId("##EqShortcut");
    if (ImCardLink(tr("Equalizer")))
        OpenSettingsSection("sound");
    // The current preset in a pill; clicking it switches presets right here.
    const float unit = ImGui::GetFontSize();
    const float width = ImGui::GetContentRegionAvail().x;
    const float h = ImGui::GetFrameHeight();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    if (ImGui::InvisibleButton("##Preset", ImVec2(width, h)))
        ImGui::OpenPopup("##EqPresets");
    const float hot = ImAnimValue(ImGui::GetItemID() + 1, ImGui::IsItemHovered() ? 1.0f : 0.0f, 14.0f);
    auto* draw = ImGui::GetWindowDrawList();
    const ImU32 accent = ImGui::GetColorU32(ImGuiCol_CheckMark);
    if (hot > 0.001f)
        draw->AddRectFilled(pos, pos + ImVec2(width, h), MaterialYouTheme::ArgbToImU32(MaterialYouTheme::AccentTheme().primary, 0.08f * hot), h * 0.5f);
    draw->AddRect(pos, pos + ImVec2(width, h), accent, h * 0.5f, 0, 1.6f);
    const char* preset = gState.mEqualizerAvailable ? FormatEqualizerPreset(gState.mEqualizer.preset) : tr("Unknown");
    ImFont* heading = clientHeadingFont() ? clientHeadingFont() : ImGui::GetFont();
    const float textW = heading->CalcTextSizeA(unit, FLT_MAX, 0.0f, preset).x;
    const float contentW = unit * 1.6f + textW;
    const float left = pos.x + (width - contentW) * 0.5f;
    ImDrawEqualizerGlyph(draw, {left + unit * 0.6f, pos.y + h * 0.5f}, unit * 0.9f, accent);
    draw->AddText(heading, unit, {left + unit * 1.6f, pos.y + (h - unit) * 0.5f}, ImGui::GetColorU32(ImGuiCol_Text), preset);
    if (ImGui::BeginPopup("##EqPresets"))
    {
        for (const MDREqualizerPreset option : kEqualizerPresets)
            if (ImGui::Selectable(FormatEqualizerPreset(option), option == gState.mEqualizer.preset) &&
                option != gState.mEqualizer.preset && gState.mEqualizerAvailable)
            {
                gState.mEqualizer.preset = option;
                mdrHeadphonesSetEqualizer(gDevice, &gState.mEqualizer);
            }
        ImGui::EndPopup();
    }
    // The shape of the current curve, small, under the pill.
    const auto& bands = gState.mEqualizerBands;
    if (!bands.empty())
    {
        ImGui::Spacing();
        const ImVec2 area = ImGui::GetCursorScreenPos();
        const float graphH = unit * 1.6f, limit = bands.size() == 10 ? 6.0f : 10.0f;
        ImVec2 points[16];
        const int count = std::min<int>(static_cast<int>(bands.size()), 16);
        for (int i = 0; i < count; ++i)
        {
            const float t = count > 1 ? static_cast<float>(i) / (count - 1) : 0.5f;
            const float v = std::clamp(bands[i] / limit, -1.0f, 1.0f);
            points[i] = {area.x + unit * 0.4f + t * (width - unit * 0.8f), area.y + graphH * 0.5f - v * graphH * 0.45f};
        }
        draw->AddLine({area.x, area.y + graphH * 0.5f}, {area.x + width, area.y + graphH * 0.5f},
                      MaterialYouTheme::ArgbToImU32(MaterialYouTheme::FixedSurfaceColors::outlineVariant, 0.8f));
        draw->AddPolyline(points, count, accent, 0, 2.0f);
        for (int i = 0; i < count; ++i)
            draw->AddCircleFilled(points[i], unit * 0.14f, accent, 12);
        ImGui::Dummy({width, graphH});
    }
    ImEndCard();
}

void DrawMultipointShortcut()
{
    ImBeginCardId("##MultipointShortcut");
    if (ImCardLink(tr("Connect to 2 devices simultaneously")))
        OpenSettingsSection("connection");
    const float unit = ImGui::GetFontSize();
    int number = 0;
    for (const MDRPairedDevice& device : gState.mPairedDevices)
    {
        if (!device.connected)
            continue;
        ++number;
        ImGui::PushID(number);
        ImPushHeading(1.0f);
        ImGui::Text("%d.", number);
        ImPopHeading();
        ImGui::SameLine(0.0f, unit * 0.5f);
        const float glyphW = ImGui::CalcTextSize(PSI_VOLUME_UP).x;
        if (device.playback_device)
        {
            ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), "%s", PSI_VOLUME_UP);
            ImGui::SetItemTooltip("%s", tr("Playing on this device"));
        }
        else
            ImGui::Dummy({glyphW, ImGui::GetTextLineHeight()});
        ImGui::SameLine(0.0f, unit * 0.4f);
        ImPushHeading(1.0f);
        ImGui::TextUnformatted(device.name);
        ImPopHeading();
        ImGui::PopID();
    }
    if (number == 0)
    {
        const int multipoint = GeneralSettingValue("MULTIPOINT_SETTING");
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("%s", multipoint == 0 ? tr("Off") : tr("No other devices connected"));
        ImGui::PopTextWrapPos();
    }
    ImEndCard();
}

void DrawPlayerCard()
{
    ImBeginCardId("##Player");
    const float unit = ImGui::GetFontSize();
    const float small = unit * 2.2f, big = unit * 2.8f, gap = unit * 0.3f;
    const float buttonsW = small * 2.0f + big + gap * 2.0f;
    const float left = ImGui::GetCursorPosX();
    const float avail = ImGui::GetContentRegionAvail().x;
    const float textW = std::max(unit * 4.0f, avail - buttonsW - unit);
    const auto title = GetText(MDR_TEXT_TRACK_TITLE);
    const auto artist = GetText(MDR_TEXT_TRACK_ARTIST);
    const auto album = GetText(MDR_TEXT_TRACK_ALBUM);
    const float top = ImGui::GetCursorPosY();
    ImGui::BeginGroup();
    ImGui::PushTextWrapPos(left + textW);
    if (gState.mPlayback.status == MDR_PLAYBACK_PLAYING)
    {
        ImPlayingBars(ImGui::GetColorU32(ImGuiCol_CheckMark));
        ImGui::SameLine(0.0f, unit * 0.4f);
    }
    ImPushHeading(1.05f);
    ImGui::TextUnformatted(title.empty() ? tr("Nothing playing yet") : title.c_str());
    ImPopHeading();
    mdr::String byline = artist;
    if (!album.empty())
        byline = byline.empty() ? album : byline + "  \xC2\xB7  " + album;
    ImGui::TextDisabled("%s", byline.empty() ? tr("Play something on your connected device.") : byline.c_str());
    ImGui::PopTextWrapPos();
    ImGui::EndGroup();
    const float textBottom = ImGui::GetCursorPosY();
    // Transport, top right.
    ImGui::SetCursorPos({left + avail - buttonsW, top + (big - small) * 0.5f});
    auto Send = [](MDRPlaybackAction action)
    {
        MDRPlaybackCommand command{};
        command.action = action;
        mdrHeadphonesPlayback(gDevice, &command);
    };
    if (ImRoundIconButton("##Prev", PSI_STEP_BACKWARD, small, tr("Prev")))
        Send(MDR_PLAYBACK_PREVIOUS);
    ImGui::SameLine(0.0f, gap);
    ImGui::SetCursorPosY(top);
    const bool playing = gState.mPlayback.status == MDR_PLAYBACK_PLAYING;
    if (ImRoundIconButton("##PlayPause", playing ? PSI_PAUSE : PSI_PLAY, big, playing ? tr("Pause") : tr("Play"), true))
        Send(playing ? MDR_PLAYBACK_PAUSE : MDR_PLAYBACK_PLAY);
    ImGui::SameLine(0.0f, gap);
    ImGui::SetCursorPosY(top + (big - small) * 0.5f);
    if (ImRoundIconButton("##Next", PSI_STEP_FORWARD, small, tr("Next")))
        Send(MDR_PLAYBACK_NEXT);
    ImGui::SetCursorPos({left, std::max(textBottom, top + big + ImGui::GetStyle().ItemSpacing.y)});
    DrawVolumeRow();
    ImEndCard();
}

void DrawHomeScreen()
{
    const float unit = ImGui::GetFontSize();
    const float scrollY = ImGui::GetScrollY();
    ImRevealBegin(0, 0);
    DrawDeviceHero(scrollY);
    ImRevealEnd();
    if (!gDevice)
        return;
    ImGui::Spacing();
    ImRevealBegin(0, 1);
    if (ImNavRow("##AllSettings", PSI_LIST_ALT, tr("All Device Settings"), nullptr))
        gConnectedPage = ConnectedPage::Settings;
    ImRevealEnd();
    ImRevealBegin(0, 2);
    ImSectionHeading(tr("Shortcuts"));
    DrawNoiseShortcut();
    const bool multipoint = FeatureAvailable(MDR_FEATURE_PAIRED_DEVICE_MANAGEMENT);
    if (multipoint && ImGui::GetContentRegionAvail().x > unit * 30.0f)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(unit * 0.25f, 0.0f));
        if (ImGui::BeginTable("##ShortcutGrid", 2, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextColumn();
            DrawEqualizerShortcut();
            ImGui::TableNextColumn();
            DrawMultipointShortcut();
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
    }
    else
    {
        DrawEqualizerShortcut();
        if (multipoint)
            DrawMultipointShortcut();
    }
    ImRevealEnd();
    ImRevealBegin(0, 3);
    DrawPlayerCard();
    ImRevealEnd();
    const float heroHeight = unit * kDeviceHeroUnits;
    const float show = std::clamp((scrollY - heroHeight * 0.5f) / (heroHeight * 0.3f), 0.0f, 1.0f);
    if (show > 0.001f)
        DrawStickyHeader(show);
    DrawDeviceActionPopups();
}

// ---- The settings page, one section per topic in Sony's order. ----

void DrawNoiseSection()
{
    const bool hasNoise = FeatureAvailable(MDR_FEATURE_NOISE_CANCELLING) || FeatureAvailable(MDR_FEATURE_AMBIENT_SOUND);
    const bool hasChat = FeatureAvailable(MDR_FEATURE_SPEAK_TO_CHAT);
    if (!hasNoise && !hasChat)
        return;
    mdr::String summary = hasNoise ? mdr::String(FormatNoiseMode(gState.mNoise.mode)) : mdr::String();
    if (hasChat)
        summary += (summary.empty() ? "" : "  \xC2\xB7  ") + mdr::Format("{} {}", tr("Speak To Chat"), OnOff(gState.mSpeakToChat.enabled != MDR_FALSE));
    if (ImBeginSection("noise", PSI_HEADPHONES, tr("Noise Cancelling / Ambient Sound"), summary.c_str()))
    {
        if (hasNoise)
            DrawNoiseControls(true);
        if (hasChat)
        {
            ImSectionHeading(tr("Speak To Chat"));
            bool changed = false;
            bool enabled = gState.mSpeakToChat.enabled != MDR_FALSE;
            if (ImToggleRowHint(tr("Speak To Chat"), &enabled,
                                tr("When you start talking, playback pauses and ambient sound turns on; it resumes after you stop.")))
                gState.mSpeakToChat.enabled = enabled ? MDR_TRUE : MDR_FALSE, changed = true;
            ImGui::BeginDisabled(!enabled);
            constexpr MDRSpeechSensitivity kSensitivity[] = {
                MDR_SPEECH_SENSITIVITY_AUTO, MDR_SPEECH_SENSITIVITY_HIGH, MDR_SPEECH_SENSITIVITY_LOW};
            changed |= ImComboBoxItems(tr("Sensitivity"), std::span{kSensitivity}, gState.mSpeakToChat.sensitivity, FormatSpeechSensitivity);
            constexpr MDRSpeakTimeout kTimeout[] = {
                MDR_SPEAK_TIMEOUT_SHORT, MDR_SPEAK_TIMEOUT_MEDIUM, MDR_SPEAK_TIMEOUT_LONG, MDR_SPEAK_TIMEOUT_MANUAL};
            changed |= ImComboBoxItems(tr("Mode Duration"), std::span{kTimeout}, gState.mSpeakToChat.timeout, FormatSpeakTimeout);
            ImRowHint(tr("How long to wait after you stop talking before playback resumes."));
            ImGui::EndDisabled();
            if (changed && gState.mSpeakToChatAvailable)
                mdrHeadphonesSetSpeakToChat(gDevice, &gState.mSpeakToChat);
        }
    }
    ImEndSection();
}

void DrawSoundSection()
{
    const bool hasDsee = FeatureAvailable(MDR_FEATURE_DSEE);
    mdr::String summary = mdr::Format("{} {}", tr("Equalizer"),
                                      gState.mEqualizerAvailable ? FormatEqualizerPreset(gState.mEqualizer.preset) : tr("Unknown"));
    if (hasDsee)
        summary += mdr::Format("  \xC2\xB7  DSEE {}", OnOff(gState.mEqualizer.dsee_enabled != MDR_FALSE));
    if (ImBeginSection("sound", PSI_SIGNAL, tr("Sound Quality / Volume"), summary.c_str()))
    {
        bool changed = false;
        ImSectionHeading(tr("Equalizer"));
        changed |= ImComboBoxItems(tr("Preset"), std::span{kEqualizerPresets}, gState.mEqualizer.preset, FormatEqualizerPreset);
        ImRowHint(tr("Pick a preset sound, or choose Custom and drag the bands below."));
        if (ImEqualizer(gState.mEqualizerBands))
            SetEqualizerBands(gState.mEqualizerBands);
        if (gState.mEqualizerBands.size() == 5)
        {
            ImGui::Spacing();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            int clearBass = gState.mEqualizer.clear_bass;
            const mdr::String label = mdr::Format("{}  %+d", tr("Clear Bass"));
            if (ImSliderInt("##ClearBass", &clearBass, -10, 10, label.c_str()))
                gState.mEqualizer.clear_bass = static_cast<int8_t>(clearBass), changed = true;
            ImRowHint(tr("Strengthens the bass while keeping it clear."));
        }
        if (hasDsee)
        {
            ImSectionHeading(tr("DSEE"));
            bool dsee = gState.mEqualizer.dsee_enabled != MDR_FALSE;
            const char* dseeName = gState.mEqualizer.dsee_type != MDR_DSEE_UNKNOWN ? FormatDseeType(gState.mEqualizer.dsee_type) : tr("DSEE");
            if (ImToggleRowHint(dseeName, &dsee,
                                tr("Restores the high-frequency detail lost in compressed music, bringing streams closer to high-resolution sound. Uses a little more battery.")))
                gState.mEqualizer.dsee_enabled = dsee ? MDR_TRUE : MDR_FALSE, changed = true;
        }
        if (changed && gState.mEqualizerAvailable)
            mdrHeadphonesSetEqualizer(gDevice, &gState.mEqualizer);
        if (FeatureAvailable(MDR_FEATURE_LISTENING_MODE))
        {
            bool listeningChanged = false;
            ImSectionHeading(tr("Listening Mode"));
            const char* const modes[] = {tr("Standard"), tr("BGM"), tr("Cinema")};
            constexpr MDRListeningMode kModes[] = {MDR_LISTENING_STANDARD, MDR_LISTENING_BACKGROUND_MUSIC, MDR_LISTENING_CINEMA};
            int selected = 0;
            for (int i = 0; i < 3; ++i)
                if (kModes[i] == gState.mListening.mode)
                    selected = i;
            const int picked = ImSegmented("##ListeningMode", modes, selected);
            if (picked != selected)
                gState.mListening.mode = kModes[picked], listeningChanged = true;
            ImRowHint(tr("Background music makes it sound like a speaker across the room, easy to work to. Cinema makes films more immersive."));
            ImGui::BeginDisabled(gState.mListening.mode != MDR_LISTENING_BACKGROUND_MUSIC);
            static const std::pair<MDRRoomSize, const char*> kRooms[] = {
                {MDR_ROOM_SMALL, "My Room"}, {MDR_ROOM_MEDIUM, "Living Room"}, {MDR_ROOM_LARGE, "Cafe"}};
            const char* current = tr("Unknown");
            for (auto const& [room, label] : kRooms)
                if (room == gState.mListening.background_room)
                    current = tr(label);
            ImRowLabel(tr("Distance"), ImRowControlWidth());
            if (ImBeginComboRow("##Distance", current, ImRowControlWidth()))
            {
                for (auto const& [room, label] : kRooms)
                    if (ImGui::Selectable(tr(label), room == gState.mListening.background_room))
                        gState.mListening.background_room = room, listeningChanged = true;
                ImGui::EndCombo();
            }
            ImGui::EndDisabled();
            if (listeningChanged && gState.mListeningAvailable)
                mdrHeadphonesSetListening(gDevice, &gState.mListening);
        }
        if (GeneralSettingValue("SIDETONE_SETTING") >= 0)
        {
            ImSectionHeading(tr("Calls"));
            DrawGeneralSettingRows("SIDETONE_SETTING");
        }
    }
    ImEndSection();
}

void DrawPairedDevices()
{
    const bool supportDeviceMgmt = FeatureAvailable(MDR_FEATURE_PAIRED_DEVICE_MANAGEMENT);
    if (!supportDeviceMgmt)
    {
        ImRowHint(tr("Turn on \"Connect to 2 devices simultaneously\" above to manage devices."));
        return;
    }
    struct DeviceView
    {
        MDRPairedDevice state;
        mdr::String mac;
        mdr::String name;
    };
    mdr::Vector<DeviceView> devices;
    for (const MDRPairedDevice& state : gState.mPairedDevices)
        devices.emplace_back(state, mdr::String{state.macAddress}, mdr::String{state.name});
    auto StageDeviceAction = [](MDRPairedDeviceCommand command, const char* mac)
    {
        MDRPairedDeviceAction action{};
        action.command = command;
        action.device_id = mac;
        action.device_id_size = static_cast<uint32_t>(std::strlen(mac));
        mdrHeadphonesSetPairedDevice(gDevice, &action);
    };
    const bool supportFix = FeatureAvailable(MDR_FEATURE_SOURCE_SWITCH_CONTROL);
    MDRBoolean switchControlEnabled = MDR_TRUE;
    if (supportFix)
        mdrHeadphonesGetSourceSwitchControl(gDevice, &switchControlEnabled);
    // Sound Connect's "Fixing playback device" is the negation of source switch control.
    const bool playbackFixed = switchControlEnabled == MDR_FALSE;
    static mdr::String selectedMac;
    auto DrawDevice = [&](const DeviceView& device)
    {
        const bool selected = selectedMac == device.mac;
        ImGui::PushID(device.mac.c_str());
        if (device.state.playback_device)
        {
            ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), "%s", playbackFixed ? PSI_LOCK : PSI_VOLUME_UP);
            ImGui::SetItemTooltip("%s", tr("Playing on this device"));
            ImGui::SameLine();
        }
        ImPushHeading(1.0f);
        if (ImGui::Selectable(device.name.c_str(), selected))
            selectedMac = selected ? "" : device.mac;
        ImPopHeading();
        if (device.state.connected && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            StageDeviceAction(MDR_PAIRED_DEVICE_SELECT_PLAYBACK, device.mac.c_str());
        if (selected)
        {
            ImStylesRAII styles;
            styles.PushVar(ImGuiStyleVar_FrameRounding, kImCapsuleRounding);
            if (device.state.connected)
            {
                const bool canFix = supportFix && device.state.playback_device;
                const int columns = canFix ? 3 : 2;
                if (ImModalButton(tri(PSI_UNLINK, "Disconnect"), 0, columns))
                    StageDeviceAction(MDR_PAIRED_DEVICE_DISCONNECT, device.mac.c_str());
                if (ImModalButton(tri(PSI_VOLUME_DOWN, "Switch Playback"), 1, columns))
                    StageDeviceAction(MDR_PAIRED_DEVICE_SELECT_PLAYBACK, device.mac.c_str());
                if (canFix && ImModalButton(playbackFixed ? tri(PSI_UNLOCK, "Unfix Playback") : tri(PSI_LOCK, "Fix Playback"), 2, columns))
                    mdrHeadphonesSetSourceSwitchControl(gDevice, playbackFixed ? MDR_TRUE : MDR_FALSE);
            }
            else if (ImModalButton(tri(PSI_LINK, "Connect"), 0, 2))
                StageDeviceAction(MDR_PAIRED_DEVICE_CONNECT, device.mac.c_str());
            if (ImModalButton(tri(PSI_BLUETOOTH_ALT, "Unpair"), device.state.connected ? 0 : 1, device.state.connected ? 1 : 2))
                StageDeviceAction(MDR_PAIRED_DEVICE_UNPAIR, device.mac.c_str());
            if (supportFix && device.state.connected && device.state.playback_device)
            {
                MDRSourceSwitchControlResult fixResult = MDR_SOURCE_SWITCH_CONTROL_SUCCESS;
                mdrHeadphonesGetSourceSwitchControlResult(gDevice, &fixResult);
                if (fixResult != MDR_SOURCE_SWITCH_CONTROL_SUCCESS)
                    ImRowHint(FormatSourceSwitchControlResult(fixResult));
            }
        }
        ImGui::PopID();
    };
    ImSectionHeading(tr("Connected"));
    for (auto& device : devices)
        if (device.state.connected)
            DrawDevice(device);
    ImSectionHeading(tr("Paired"));
    ImRowHint(tr("Devices that have been paired with these headphones. Click one for more actions."));
    for (auto& device : devices)
        if (!device.state.connected)
            DrawDevice(device);
    ImGui::Spacing();
    ImStylesRAII styles;
    styles.PushVar(ImGuiStyleVar_FrameRounding, kImCapsuleRounding);
    if (gState.mPairing.enabled)
    {
        ImInlineSpinner(ImGui::GetColorU32(ImGuiCol_CheckMark));
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(tr("Pairing..."));
        if (ImModalButton(tr("Stop")))
        {
            gState.mPairing.enabled = MDR_FALSE;
            if (gState.mPairingAvailable)
                mdrHeadphonesSetPairing(gDevice, &gState.mPairing);
        }
    }
    else
    {
        if (ImModalButton(tri(PSI_BLUETOOTH, "Enter Pairing Mode")))
        {
            gState.mPairing.enabled = MDR_TRUE;
            if (gState.mPairingAvailable)
                mdrHeadphonesSetPairing(gDevice, &gState.mPairing);
        }
        ImRowHint(tr("For TWS (Earbuds) devices, you may need to take both of your headphones out from your case to enter Pairing Mode."));
    }
}

void DrawConnectionSection()
{
    const int multipoint = GeneralSettingValue("MULTIPOINT_SETTING");
    MDRConnectionMode mode{};
    const bool hasQuality = FeatureAvailable(MDR_FEATURE_CONNECTION_MODE) &&
                            mdrHeadphonesGetConnectionMode(gDevice, &mode) == MDR_RESULT_OK;
    mdr::String summary;
    if (multipoint >= 0)
        summary = mdr::Format("{} {}", tr("Multipoint"), OnOff(multipoint == 1));
    if (hasQuality && mode.audio_priority != MDR_AUDIO_PRIORITY_UNKNOWN)
        summary += (summary.empty() ? "" : "  \xC2\xB7  ") +
                   mdr::String(mode.audio_priority == MDR_AUDIO_PRIORITY_QUALITY ? tr("Prioritize sound quality") : tr("Prioritize stable connection"));
    if (ImBeginSection("connection", PSI_LINK, tr("Connections"), summary.c_str()))
    {
        DrawGeneralSettingRows("MULTIPOINT_SETTING");
        if (hasQuality)
        {
            ImSectionHeading(tr("Bluetooth connection quality"));
            const char* const options[] = {tr("Prioritize sound quality"), tr("Prioritize stable connection")};
            const int current = mode.audio_priority == MDR_AUDIO_PRIORITY_QUALITY ? 0
                              : mode.audio_priority == MDR_AUDIO_PRIORITY_STABILITY ? 1 : -1;
            const int picked = ImSegmented("##ConnectionQuality", options, current);
            if (picked >= 0 && picked != current)
            {
                MDRConnectionMode next{};
                next.audio_priority = picked == 0 ? MDR_AUDIO_PRIORITY_QUALITY : MDR_AUDIO_PRIORITY_STABILITY;
                mdrHeadphonesSetConnectionMode(gDevice, &next);
            }
            ImRowHint(tr("Sound quality uses LDAC and other high-quality codecs when available. If the sound keeps cutting out, choose stable connection."));
        }
        DrawPairedDevices();
    }
    ImEndSection();
}

void DrawControlsSection()
{
    MDRPower power{};
    const bool havePower = mdrHeadphonesGetPower(gDevice, &power) == MDR_RESULT_OK;
    const bool hasTouch = GeneralSettingValue("TOUCH_PANEL_SETTING") >= 0;
    const bool hasButton = FeatureAvailable(MDR_FEATURE_NOISE_CONTROL_BUTTON) && gState.mNoiseAvailable;
    const bool hasAssignable = FeatureAvailable(MDR_FEATURE_ASSIGNABLE_CONTROLS) && !GetAssignableControls().empty();
    const bool hasPause = FeatureAvailable(MDR_FEATURE_AUTO_PAUSE);
    const bool hasGesture = FeatureAvailable(MDR_FEATURE_HEAD_GESTURE);
    if (!hasTouch && !hasButton && !hasAssignable && !hasPause && !hasGesture)
        return;
    mdr::String summary;
    auto Add = [&](const mdr::String& part) { summary += (summary.empty() ? "" : "  \xC2\xB7  ") + part; };
    if (hasTouch)
        Add(mdr::Format("{} {}", tr("Touch sensor control panel"), OnOff(GeneralSettingValue("TOUCH_PANEL_SETTING") == 1)));
    if (hasPause && havePower)
        Add(mdr::Format("{} {}", tr("Pause when removed"), OnOff(power.auto_pause != MDR_FALSE)));
    if (ImBeginSection("controls", PSI_TASKS, tr("Device Controls"), summary.c_str()))
    {
        DrawGeneralSettingRows("TOUCH_PANEL_SETTING");
        if (hasButton)
        {
            constexpr MDRNoiseButtonMode kSelections[] = {
                MDR_NOISE_BUTTON_NONE, MDR_NOISE_BUTTON_NOISE_AMBIENT_OFF, MDR_NOISE_BUTTON_NOISE_AMBIENT,
                MDR_NOISE_BUTTON_NOISE_OFF, MDR_NOISE_BUTTON_AMBIENT_OFF};
            if (ImComboBoxItems(tr("NC/AMB Button Function"), std::span{kSelections}, gState.mNoise.button_mode, FormatNoiseButtonMode))
                mdrHeadphonesSetNoiseControl(gDevice, &gState.mNoise);
            ImRowHint(tr("The modes the NC/AMB button cycles through when you press it."));
        }
        if (hasAssignable)
        {
            bool changed = false;
            auto controls = GetAssignableControls();
            for (MDRAssignableControl& control : controls)
            {
                mdr::Vector<MDRAssignableAction> actions = GetAssignableControlActions(control.location);
                std::erase(actions, MDR_ASSIGNABLE_GOOGLE_ASSISTANT);
                changed |= ImComboBoxItems<MDRAssignableAction, std::dynamic_extent>(
                    FormatAssignableActionKeyLocation(control), std::span{actions}, control.action, FormatAssignableAction);
            }
            ImRowHint(tr("What the touch sensor or button on each side does."));
            if (changed)
                mdrHeadphonesSetAssignableControls(gDevice, controls.data(), static_cast<uint32_t>(controls.size()));
        }
        if (hasPause)
        {
            bool enabled = power.auto_pause != MDR_FALSE;
            if (ImToggleRowHint(tr("Pause when removed"), &enabled,
                                tr("Pauses playback when you take the headphones off and resumes when you put them back on.")) && havePower)
            {
                power.auto_pause = enabled ? MDR_TRUE : MDR_FALSE;
                mdrHeadphonesSetPower(gDevice, &power);
            }
        }
        if (hasGesture)
        {
            bool enabled = power.head_gesture != MDR_FALSE;
            if (ImToggleRowHint(tr("Head Gesture"), &enabled, tr("Nod to answer a call or shake your head to decline it.")) && havePower)
            {
                power.head_gesture = enabled ? MDR_TRUE : MDR_FALSE;
                mdrHeadphonesSetPower(gDevice, &power);
            }
        }
    }
    ImEndSection();
}

void DrawPowerSection()
{
    MDRPower power{};
    const bool havePower = mdrHeadphonesGetPower(gDevice, &power) == MDR_RESULT_OK;
    const bool hasAutoOff = FeatureAvailable(MDR_FEATURE_AUTO_POWER_OFF);
    const bool hasWearing = FeatureAvailable(MDR_FEATURE_WEARING_DETECTION) && power.wearing_power != MDR_WEARING_POWER_UNAVAILABLE;
    mdr::String summary;
    if (hasAutoOff && havePower)
        summary = hasWearing && power.wearing_power == MDR_WEARING_POWER_WHEN_REMOVED ? mdr::String(tr("Power off when removed"))
                                                                                    : mdr::String(FormatAutoPowerOff(power.auto_power_off_minutes));
    if (ImBeginSection("power", PSI_BOLT, tr("Power / Battery"), summary.c_str()))
    {
        const float unit = ImGui::GetFontSize();
        ImSectionHeading(tr("BATTERY"));
        bool hasBattery = false;
        for (const MDRBattery& battery : gState.mBatteries)
        {
            if (!battery.present || !battery.update_threshold_percent)
                continue;
            hasBattery = true;
            const char* label = battery.part == MDR_BATTERY_LEFT ? tr("Left") :
                battery.part == MDR_BATTERY_RIGHT ? tr("Right") :
                battery.part == MDR_BATTERY_CASE ? tr("Case") : tr("Headphones");
            const float right = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
            ImGui::TextUnformatted(label);
            const char* charging = FormatChargingState(battery.charging);
            if (*charging)
            {
                ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
                ImGui::TextDisabled("%s", charging);
            }
            const mdr::String percent = mdr::Format("{}%", static_cast<unsigned>(battery.level_percent));
            ImPushHeading(1.05f);
            ImGui::SameLine(right - ImGui::CalcTextSize(percent.c_str()).x);
            ImGui::TextUnformatted(percent.c_str());
            ImPopHeading();
            ImGui::PushID(static_cast<int>(battery.part));
            const float fill = ImAnimValue(ImGui::GetID("fill"), std::clamp(battery.level_percent / 100.0f, 0.0f, 1.0f), 3.0f, 0.0f);
            ImGui::PopID();
            const ImVec2 bar = ImGui::GetCursorScreenPos();
            const float barW = ImGui::GetContentRegionAvail().x, barH = unit * 0.3f;
            auto* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(bar, bar + ImVec2(barW, barH),
                                MaterialYouTheme::ArgbToImU32(MaterialYouTheme::FixedSurfaceColors::outlineVariant, 0.6f), barH * 0.5f);
            const ImU32 fillColour = battery.level_percent <= 20
                ? MaterialYouTheme::ArgbToImU32(MaterialYouTheme::FixedSurfaceColors::error)
                : battery.charging == MDR_CHARGING_YES ? kImChargingGreen : ImGui::GetColorU32(ImGuiCol_CheckMark);
            if (fill > 0.0f)
                draw->AddRectFilled(bar, bar + ImVec2(std::max(barH, barW * fill), barH), fillColour, barH * 0.5f);
            ImGui::Dummy({barW, barH});
        }
        if (!hasBattery)
            ImGui::TextDisabled("%s", tr("Waiting for battery status"));
        if (hasAutoOff)
        {
            ImSectionHeading(tr("Auto Power Off"));
            ImRowHint(tr("The audio device turns itself off to save power."));
            bool changed = false;
            if (hasWearing)
            {
                bool whenRemoved = power.wearing_power == MDR_WEARING_POWER_WHEN_REMOVED;
                if (ImToggleRowHint(tr("Power off when removed"), &whenRemoved,
                                    tr("If the headphones are not worn for a while, they turn themselves off.")))
                    power.wearing_power = whenRemoved ? MDR_WEARING_POWER_WHEN_REMOVED : MDR_WEARING_POWER_DISABLED, changed = true;
            }
            constexpr uint32_t kSelections[] = {0, 5, 15, 30, 60, 180};
            changed |= ImComboBoxItems(tr("Turn off when Bluetooth is disconnected"), std::span{kSelections},
                                       power.auto_power_off_minutes, FormatAutoPowerOff);
            if (changed && havePower)
                mdrHeadphonesSetPower(gDevice, &power);
        }
    }
    ImEndSection();
}

void DrawSystemSection()
{
    const mdr::String firmware = GetText(MDR_TEXT_FIRMWARE_VERSION);
    const mdr::String summary = firmware.empty() ? mdr::String() : mdr::Format(fmt::runtime(tr("Firmware {}")), firmware);
    if (ImBeginSection("system", PSI_COG, tr("System"), summary.c_str()))
    {
        MDRVoiceGuidance voice{};
        if (FeatureAvailable(MDR_FEATURE_VOICE_GUIDANCE) && mdrHeadphonesGetVoiceGuidance(gDevice, &voice) == MDR_RESULT_OK)
        {
            bool changed = false;
            bool enabled = voice.enabled != MDR_FALSE;
            if (ImToggleRowHint(tr("Voice Guidance"), &enabled,
                                tr("Voice prompts from the headphones for power on, low battery, mode changes and more.")))
                voice.enabled = enabled ? MDR_TRUE : MDR_FALSE, changed = true;
            if (FeatureAvailable(MDR_FEATURE_VOICE_GUIDANCE_VOLUME))
            {
                ImGui::BeginDisabled(!enabled);
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                int volume = voice.volume;
                const mdr::String label = mdr::Format("{}  %+d", tr("Voice guidance volume"));
                if (ImSliderInt("##VoiceVolume", &volume, -2, 2, label.c_str()))
                    voice.volume = static_cast<int8_t>(volume), changed = true;
                ImGui::EndDisabled();
            }
            if (changed)
                mdrHeadphonesSetVoiceGuidance(gDevice, &voice);
        }
        DrawGeneralSettingRows(nullptr);
        ImSectionHeading(tr("Device information"));
        ImInfoRow(tr("Model"), GetText(MDR_TEXT_MODEL_NAME).c_str());
        ImInfoRow(tr("Firmware version"), firmware.c_str());
        ImInfoRow(tr("MAC"), GetText(MDR_TEXT_UNIQUE_ID).c_str());
        ImInfoRow(tr("Series"), GetText(MDR_TEXT_MODEL_SERIES).c_str());
        ImInfoRow(tr("Color"), GetText(MDR_TEXT_MODEL_COLOR).c_str());
        ImGui::Spacing();
        if (ImGui::TreeNodeEx(tr("Supported features"), ImGuiTreeNodeFlags_SpanAvailWidth))
        {
            struct FeatureRow
            {
                const char* name;
                MDRFeature feature;
            };
            constexpr FeatureRow kFeatures[] = {
                {"Identity", MDR_FEATURE_IDENTITY},
                {"Single battery", MDR_FEATURE_BATTERY_SINGLE},
                {"Left/right battery", MDR_FEATURE_BATTERY_LEFT_RIGHT},
                {"Charging case battery", MDR_FEATURE_BATTERY_CASE},
                {"Playback metadata", MDR_FEATURE_PLAYBACK_METADATA},
                {"Playback control", MDR_FEATURE_PLAYBACK_CONTROL},
                {"Playback volume", MDR_FEATURE_PLAYBACK_VOLUME},
                {"Noise cancelling", MDR_FEATURE_NOISE_CANCELLING},
                {"Ambient sound", MDR_FEATURE_AMBIENT_SOUND},
                {"Adaptive ambient sound", MDR_FEATURE_ADAPTIVE_AMBIENT_SOUND},
                {"Speak to Chat", MDR_FEATURE_SPEAK_TO_CHAT},
                {"Listening mode", MDR_FEATURE_LISTENING_MODE},
                {"Equalizer", MDR_FEATURE_EQUALIZER},
                {"DSEE", MDR_FEATURE_DSEE},
                {"Paired device management", MDR_FEATURE_PAIRED_DEVICE_MANAGEMENT},
                {"Pairing mode", MDR_FEATURE_PAIRING_MODE},
                {"General settings", MDR_FEATURE_GENERAL_SETTINGS},
                {"Assignable controls", MDR_FEATURE_ASSIGNABLE_CONTROLS},
                {"Noise control button", MDR_FEATURE_NOISE_CONTROL_BUTTON},
                {"Auto power off", MDR_FEATURE_AUTO_POWER_OFF},
                {"Wearing detection", MDR_FEATURE_WEARING_DETECTION},
                {"Auto pause", MDR_FEATURE_AUTO_PAUSE},
                {"Head gesture", MDR_FEATURE_HEAD_GESTURE},
                {"Voice guidance", MDR_FEATURE_VOICE_GUIDANCE},
                {"Voice guidance volume", MDR_FEATURE_VOICE_GUIDANCE_VOLUME},
                {"Shutdown", MDR_FEATURE_SHUTDOWN},
                {"Connection mode", MDR_FEATURE_CONNECTION_MODE},
                {"Safe listening", MDR_FEATURE_SAFE_LISTENING},
            };
            if (ImGui::BeginTable("##Features", 2, ImGuiTableFlags_RowBg))
            {
                ImGui::TableSetupColumn("##feature", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("##availability", ImGuiTableColumnFlags_WidthFixed);
                for (const FeatureRow& row : kFeatures)
                {
                    MDRFeatureAvailability availability = MDR_AVAILABILITY_UNKNOWN;
                    mdrHeadphonesGetFeature(gDevice, row.feature, &availability);
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%s", tr(row.name));
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%s", FormatFeatureAvailability(availability));
                }
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
    }
    ImEndSection();
}

void DrawAllSettingsScreen()
{
    if (ImPageHeader(tr("All Device Settings")))
        gConnectedPage = ConnectedPage::Home;
    DrawNoiseSection();
    DrawSoundSection();
    DrawConnectionSection();
    DrawControlsSection();
    DrawPowerSection();
    DrawSystemSection();
}

// The app's own settings, kept apart from everything that is written to the headphones.
void DrawAppSettingsPanel()
{
    ImBeginCard(tr("App Settings"), PSI_COG);
    ImRowHint(tr("These settings belong to this app only; nothing here is sent to the headphones."));
    ImGui::Spacing();
    DrawAppSettings();
    ImEndCard();
}

void DrawAppSettingsScreen()
{
    if (ImPageHeader(tr("App Settings")))
        gConnectedPage = ConnectedPage::Home;
    DrawAppSettingsPanel();
}
#pragma endregion

// Pump one device event. Runs every frame while connected, independent of whether the
// main window is visible, so background tray actions and battery updates keep flowing.
void PollDevice()
{
    MDREvent event = MDR_EVENT_NONE;
    const MDRResult pollResult = mdrHeadphonesPoll(gDevice, &event);
    if (pollResult != MDR_RESULT_OK)
    {
        DisconnectWithModal();
        return;
    }
    switch (event)
    {
    case MDR_EVENT_INITIALIZE_COMPLETE:
        if (mdrHeadphonesRequestSync(gDevice) != MDR_RESULT_OK)
        {
            DisconnectWithModal();
            return;
        }
        break;
    case MDR_EVENT_IDENTITY_CHANGED:
        gState.mModelAvailable = mdrHeadphonesGetModel(gDevice, &gState.mModel) == MDR_RESULT_OK;
        MaterialYouTheme::ApplyForModelColor(GetModelColor());
        break;
    case MDR_EVENT_BATTERY_CHANGED:
        gState.mBatteries = GetBatteries();
        break;
    case MDR_EVENT_PLAYBACK_CHANGED:
        RefreshPlaybackState();
        break;
    case MDR_EVENT_NOISE_CONTROL_CHANGED:
        gState.mNoiseAvailable = mdrHeadphonesGetNoiseControl(gDevice, &gState.mNoise) == MDR_RESULT_OK;
        break;
    case MDR_EVENT_SPEAK_TO_CHAT_CHANGED:
        gState.mSpeakToChatAvailable = mdrHeadphonesGetSpeakToChat(gDevice, &gState.mSpeakToChat) == MDR_RESULT_OK;
        break;
    case MDR_EVENT_LISTENING_MODE_CHANGED:
        gState.mListeningAvailable = mdrHeadphonesGetListening(gDevice, &gState.mListening) == MDR_RESULT_OK;
        break;
    case MDR_EVENT_EQUALIZER_CHANGED:
        gState.mEqualizerAvailable = mdrHeadphonesGetEqualizer(gDevice, &gState.mEqualizer) == MDR_RESULT_OK;
        gState.mEqualizerBands = GetEqualizerBands();
        break;
    case MDR_EVENT_PAIRED_DEVICES_CHANGED:
        gState.mPairedDevices = GetPairedDevices();
        break;
    case MDR_EVENT_PAIRING_CHANGED:
        gState.mPairingAvailable = mdrHeadphonesGetPairing(gDevice, &gState.mPairing) == MDR_RESULT_OK;
        break;
    case MDR_EVENT_GENERAL_SETTINGS_CHANGED:
        gState.mGeneralSettings = GetGeneralSettings(GetGeneralSettingInfos());
        break;
    case MDR_EVENT_SYNC_COMPLETE:
        RefreshClientState();
        MaterialYouTheme::ApplyForModelColor(GetModelColor());
        break;
    case MDR_EVENT_APPLY_COMPLETE:
        RefreshPlaybackState();
        break;
    case MDR_EVENT_NEED_SYNC:
        gState.mPendingSync = true;
        break;
    }
}

// Flush staged settings. Counterpart of PollDevice, also independent of window visibility.
void CommitDevice()
{
    if (!gDevice || !mdrHeadphonesIsReady(gDevice))
        return;
    if (mdrHeadphonesIsDirty(gDevice) && mdrHeadphonesRequestCommit(gDevice) != MDR_RESULT_OK)
        DisconnectWithModal();
    if (gState.mPendingSync){
        gState.mPendingSync = false;
        if (mdrHeadphonesRequestSync(gDevice) != MDR_RESULT_OK)
            DisconnectWithModal();
    }
}

void DrawDeviceControls()
{
    if (gDemoTab >= 0 && gDemoTab != 8)
    {
        static const char* const kSections[] = {"noise", "sound", "connection", "controls", "power", "system"};
        if (gDemoTab >= 1 && gDemoTab <= 6)
            OpenSettingsSection(kSections[gDemoTab - 1]);
        else
            gConnectedPage = gDemoTab >= 7 ? ConnectedPage::AppSettings : ConnectedPage::Home;
        gDemoTab = -1;
    }
    // Back: Escape or the mouse's back button, unless a popup has the keyboard.
    if (gConnectedPage != ConnectedPage::Home &&
        !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel) &&
        (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsMouseClicked(3)))
        gConnectedPage = ConnectedPage::Home;
    static ConnectedPage shownPage = ConnectedPage::Home;
    const bool pageChanged = gConnectedPage != shownPage;
    if (pageChanged)
        shownPage = gConnectedPage, ImRevealRestart(1);
    if (ImBeginScreenColumn("##Connected"))
    {
        if (pageChanged)
            ImGui::SetScrollY(0.0f); // A section jump later in the frame overrides this
        if (gDemoScroll >= 0.0f && ImGui::GetTime() - gRevealStart[0] > 2.5)
        {
            ImGui::SetScrollY(gDemoScroll);
            gDemoScroll = -1.0f;
        }
        if (gConnectedPage == ConnectedPage::Home)
            DrawHomeScreen();
        else
        {
            ImRevealBegin(1, 0);
            if (gConnectedPage == ConnectedPage::Settings)
                DrawAllSettingsScreen();
            else
                DrawAppSettingsScreen();
            ImRevealEnd();
        }
        ImSmoothScroll();
    }
    ImGui::EndChild();
}

void DrawDeviceDisconnect()
{
    // The link dropped (headphones off, switched to another source, or an error). Instead of a
    // modal, record why, return to discovery, and let auto-connect bring the device back.
    MDRConnection* conn = clientPlatformConnectionGet();
#ifdef MDR_CLIENT_DEBUGGER
    clientDebuggerClearExportStatus();
#endif
    MDR_LOG("[Client] Device disconnected")
    mdr::String reason;
    if (!connectionAttempt.lastError.empty())
        reason = connectionAttempt.lastError;
    else if (conn && mdrConnectionGetLastError(conn) && *mdrConnectionGetLastError(conn))
        reason = mdrConnectionGetLastError(conn);
    if (!gHeadphonesError.empty())
        reason = reason.empty() ? gHeadphonesError : reason + " / " + gHeadphonesError;
    if (!reason.empty())
        MDR_LOG("[Client] Reason: {}", reason)
    gHasDisconnectMessage = true;
    gLastDisconnectReason = reason.c_str();

    const bool automaticAttemptFailed = connectionAttempt.automatic;
    CloseDevice();
    if (conn)
        mdrConnectionDisconnect(conn);
    connectionAttempt = {};
    connState = CONN_STATE_NO_CONNECTION;
    if (automaticAttemptFailed)
    {
        // A failed automatic attempt: back off so we do not hammer a device that is not ready.
        ++gAutoConnectFailures;
        gNextAutoConnectMs = SDL_GetTicks() + AutoConnectBackoffMs();
    }
    else
    {
        // An established session dropped: give the Bluetooth link a moment to settle, then retry.
        gNextAutoConnectMs = SDL_GetTicks() + kReconnectGraceMs;
    }
}

// Notifications observe real connection/readiness transitions, independent of the visible tab.
extern SDL_Window* gWindow;
#pragma region Connect overlay
// The moment the headphones come up, a sheet lifts out of the screen: the model turning a full
// circle to face you, its name, the battery counting up. The desktop take on the AirPods card.
namespace
{
    double gConnectOverlayStart = -1.0;
    bool gConnectOverlayPreview = false; // Shown from settings: sample name and battery
    constexpr double kConnectOverlayDuration = 4.8;
    constexpr double kConnectOverlayFadeOut = 0.35;
}

void ShowConnectOverlay(bool preview)
{
    gConnectOverlayStart = ImGui::GetTime();
    gConnectOverlayPreview = preview;
}

ImU32 ImWithAlpha(ImU32 colour, float alpha)
{
    ImVec4 c = ImGui::ColorConvertU32ToFloat4(colour);
    c.w *= alpha;
    return ImGui::ColorConvertFloat4ToU32(c);
}

ImU32 ImScaleColour(ImU32 colour, float factor)
{
    ImVec4 c = ImGui::ColorConvertU32ToFloat4(colour);
    c.x = std::min(c.x * factor, 1.0f), c.y = std::min(c.y * factor, 1.0f), c.z = std::min(c.z * factor, 1.0f);
    return ImGui::ColorConvertFloat4ToU32(c);
}

void DrawConnectOverlay()
{
    if (gConnectOverlayStart < 0.0)
        return;
    const bool animations = clientSettings().animations;
    const double now = ImGui::GetTime();
    const double t = now - gConnectOverlayStart;
    const double total = animations ? kConnectOverlayDuration : 2.8;
    if (t >= total)
    {
        gConnectOverlayStart = -1.0;
        return;
    }
    const float rise = animations ? ImEaseOutBack(static_cast<float>(t / 0.55)) : 1.0f;
    const float fadeIn = animations ? ImEaseOutCubic(static_cast<float>(t / 0.3)) : 1.0f;
    const float fadeOut = static_cast<float>(std::clamp((total - t) / kConnectOverlayFadeOut, 0.0, 1.0));
    const float alpha = std::min(fadeIn, fadeOut);
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float unit = ImGui::GetFontSize();
    auto dismiss = [&]()
    {
        if (t < total - kConnectOverlayFadeOut)
            gConnectOverlayStart = now - (total - kConnectOverlayFadeOut); // Jump to the fade-out
    };

    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(display);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoNav;
    if (ImGui::Begin("##ConnectOverlay", nullptr, kFlags))
    {
        auto* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled({0, 0}, display, IM_COL32(0, 0, 0, static_cast<int>(alpha * (MaterialYouTheme::darkMode ? 150 : 95))));
        ImGui::SetCursorScreenPos({0, 0});
        ImGui::SetNextItemAllowOverlap(); // The Done button sits on top of this
        if (ImGui::InvisibleButton("##dismiss", display))
            dismiss();

        // The sheet.
        const float w = std::min(display.x - unit * 3.0f, unit * 24.0f);
        const float h = unit * 21.0f;
        const ImVec2 min((display.x - w) * 0.5f, (display.y - h) * 0.5f + (1.0f - rise) * unit * 3.0f);
        const ImVec2 max = min + ImVec2(w, h);
        const float rounding = unit * 1.6f;
        for (int i = 3; i >= 1; --i) // Soft shadow: widening, fainter layers
            draw->AddRectFilled(min + ImVec2(-i * 2.0f, i * 3.0f), max + ImVec2(i * 2.0f, i * 3.0f),
                                IM_COL32(0, 0, 0, static_cast<int>(alpha * 22)), rounding + i * 2.0f);
        draw->AddRectFilled(min, max,
                            MaterialYouTheme::ArgbToImU32(MaterialYouTheme::FixedSurfaceColors::surfaceContainerLow, alpha), rounding);
        draw->AddRect(min, max, ImWithAlpha(MaterialYouTheme::glassEdgeShadow(), alpha), rounding);
        draw->PathArcTo({min.x + rounding, min.y + rounding}, rounding, IM_PI, IM_PI * 1.5f, 12);
        draw->PathArcTo({max.x - rounding, min.y + rounding}, rounding, IM_PI * 1.5f, IM_PI * 2.0f, 12);
        draw->PathStroke(ImWithAlpha(MaterialYouTheme::glassEdgeHighlight(), alpha), 0, 1.5f);

        // The headphones on a lit disc: a full turn easing to rest.
        const ImVec2 stage(min.x + w * 0.5f, min.y + unit * 6.2f);
        const float pulse = animations ? 0.5f + 0.5f * std::sin(static_cast<float>(t) * 2.2f) : 0.5f;
        const ImU32 accent = MaterialYouTheme::ArgbToImU32(MaterialYouTheme::AccentTheme().primary, alpha);
        draw->AddCircleFilled(stage, unit * (4.9f + 0.35f * pulse),
                              MaterialYouTheme::ArgbToImU32(MaterialYouTheme::AccentTheme().primary, alpha * (0.07f + 0.07f * pulse)), 64);
        {
            mdr::String shown = IllustrationProduct();
            if (shown.empty() && gConnectOverlayPreview)
                shown = "WH-1000XM5";
            Headphones3D::View view;
            view.yaw = Headphones3D::RestYaw(shown.c_str()) -
                       (animations ? (1.0f - ImEaseOutCubic(static_cast<float>(t / 2.6))) * IM_PI * 2.0f : 0.0f);
            view.pitch = 0.2f;
            view.size = unit * 3.0f * (0.9f + 0.1f * rise);
            Headphones3D::Draw(draw, stage, view, shown.c_str(), ProductColour(), alpha);
        }

        // Name, status, battery.
        const mdr::String model = gDevice ? GetText(MDR_TEXT_MODEL_NAME) : mdr::String{};
        const char* name = !model.empty() ? model.c_str() : gConnectOverlayPreview ? "WH-1000XM5" : tr("Your headphones");
        ImFont* font = ImGui::GetFont();
        ImFont* titleFont = clientHeadingFont() ? clientHeadingFont() : font;
        const ImU32 text = MaterialYouTheme::ArgbToImU32(MaterialYouTheme::FixedSurfaceColors::onSurface, alpha);
        const ImU32 muted = MaterialYouTheme::ArgbToImU32(MaterialYouTheme::FixedSurfaceColors::onSurfaceVariant, alpha);
        const float titleSize = unit * 1.5f;
        ImVec2 size = titleFont->CalcTextSizeA(titleSize, FLT_MAX, 0.0f, name);
        draw->AddText(titleFont, titleSize, {stage.x - size.x * 0.5f, min.y + unit * 11.0f}, text, name);
        const char* status = tr("Connected");
        size = font->CalcTextSizeA(unit, FLT_MAX, 0.0f, status);
        draw->AddText(font, unit, {stage.x - size.x * 0.5f, min.y + unit * 12.9f}, muted, status);
        int level = gConnectOverlayPreview && !gDevice ? 100 : -1;
        for (const MDRBattery& battery : gState.mBatteries)
            if (battery.present && battery.update_threshold_percent && battery.part != MDR_BATTERY_CASE)
            {
                level = battery.level_percent;
                break;
            }
        if (level >= 0)
        {
            // Counts up from zero once the sheet has settled.
            const float count = animations ? ImEaseOutCubic(static_cast<float>((t - 0.5) / 1.0)) : 1.0f;
            const int shown = static_cast<int>(std::lround(level * count));
            const mdr::String label = mdr::Format("{}  {}%", tr("BATTERY"), shown);
            size = font->CalcTextSizeA(unit, FLT_MAX, 0.0f, label.c_str());
            const float barW = unit * 9.0f, barH = unit * 0.4f;
            const ImVec2 barMin(stage.x - barW * 0.5f, min.y + unit * 15.9f);
            draw->AddText(font, unit, {stage.x - size.x * 0.5f, barMin.y - unit * 1.5f}, muted, label.c_str());
            draw->AddRectFilled(barMin, barMin + ImVec2(barW, barH),
                                MaterialYouTheme::ArgbToImU32(MaterialYouTheme::FixedSurfaceColors::surfaceContainerHighest, alpha), barH * 0.5f);
            const ImU32 fill = level <= 20 ? MaterialYouTheme::ArgbToImU32(MaterialYouTheme::FixedSurfaceColors::error, alpha) : accent;
            draw->AddRectFilled(barMin, barMin + ImVec2(barW * (shown / 100.0f), barH), fill, barH * 0.5f);
        }
        // Done.
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, kImCapsuleRounding);
        const float buttonW = unit * 8.0f;
        ImGui::SetCursorScreenPos({stage.x - buttonW * 0.5f, max.y - unit * 0.9f - ImGui::GetFrameHeight()});
        if (ImButtonSmooth(tr("Done"), {buttonW, 0}))
            dismiss();
        ImGui::PopStyleVar(2);
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}
#pragma endregion

void DrawConnectionNotification()
{
    // "Ready" clears while any request is in flight, so it cannot stand in for the link state:
    // every volume change used to announce a loss and a reconnect. Announce once per link.
    static bool announced = false;
    static bool wasConnecting = false;
    static bool lowBatteryReported = false;
    static double lastFailure = -60.0;
    static double shownAt = -10.0;
    static std::string message;
    static bool success = false;
    const bool ready = connState == CONN_STATE_CONNECTED && gDevice && mdrHeadphonesIsReady(gDevice);
    const double now = ImGui::GetTime();
    auto notify = [&](const char* text, bool connected) {
        if (!clientSettings().notifications)
            return;
        success = connected;
        const bool foreground = gWindow && (SDL_GetWindowFlags(gWindow) & SDL_WINDOW_INPUT_FOCUS) &&
            !(SDL_GetWindowFlags(gWindow) & (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED));
        if (foreground)
        {
            message = text;
            shownAt = now;
        }
        else
            clientPlatformTrayNotify("SonyHeadphonesClient", text);
    };
    const bool connected = connState == CONN_STATE_CONNECTED && gDevice;
    if (connected && ready && !announced)
    {
        announced = true;
        lowBatteryReported = false;
        const bool foreground = gWindow && (SDL_GetWindowFlags(gWindow) & SDL_WINDOW_INPUT_FOCUS) &&
            !(SDL_GetWindowFlags(gWindow) & (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED));
        if (foreground)
            ShowConnectOverlay(false); // The sheet says it; no toast on top of it
        else
            notify(tr("Connected. Ready to listen."), true);
    }
    else if (announced && !connected)
    {
        announced = false;
        if (!gAutoConnectSuppressed)
            notify(tr("Connection lost. Reconnecting..."), false);
        lowBatteryReported = false;
    }
    else if (wasConnecting && connState == CONN_STATE_DISCONNECTED && now - lastFailure > 30.0)
    {
        notify(tr("Unable to connect. Please try again."), false);
        lastFailure = now;
    }
    // One low-battery alert per discharge cycle; ignore the charging case.
    if (ready)
    {
        bool low = false, known = false, recovered = true;
        for (const auto& battery : gState.mBatteries)
        {
            if (!battery.present || !battery.update_threshold_percent || battery.part == MDR_BATTERY_CASE)
                continue;
            known = true;
            low |= battery.level_percent <= 20 && battery.charging == MDR_CHARGING_NO;
            recovered &= battery.level_percent > 25 || battery.charging == MDR_CHARGING_YES;
        }
        if (low && !lowBatteryReported)
        {
            // Let the connection confirmation finish before presenting the battery alert.
            if (now - shownAt > 4.0)
            {
                notify(tr("Battery is low. Time to recharge."), false);
                lowBatteryReported = true;
            }
        }
        else if (known && recovered)
            lowBatteryReported = false;
    }
    wasConnecting = connState == CONN_STATE_CONNECTING;
    if (!clientSettings().notifications || now - shownAt >= 4.0)
        return;
    const float age = static_cast<float>(now - shownAt);
    const float progress = clientSettings().animations ? ImEaseOutBack(age / 0.42f) : 1.0f;
    const float alpha = clientSettings().animations
        ? std::min(ImEaseOutCubic(age / 0.25f), std::clamp((4.0f - age) / 0.3f, 0.0f, 1.0f)) : 1.0f;
    const auto display = ImGui::GetIO().DisplaySize;
    const float unit = ImGui::GetFontSize();
    const float width = std::min(display.x - 24.0f, unit * 27.0f);
    const float height = unit * 3.6f;
    const ImVec2 min((display.x - width) * 0.5f, display.y - height - 20.0f + (1.0f - progress) * 16.0f);
    auto* draw = ImGui::GetForegroundDrawList();
    draw->AddRectFilled(min + ImVec2(0, 3), min + ImVec2(width, height + 3),
                        IM_COL32(0, 0, 0, static_cast<int>(20 * alpha)), 18.0f);
    draw->AddRectFilled(min, min + ImVec2(width, height),
                        MaterialYouTheme::ArgbToImU32(MaterialYouTheme::FixedSurfaceColors::surfaceContainerLow, alpha), 18.0f);
    draw->AddCircleFilled(min + ImVec2(unit * 1.5f, height * 0.5f), unit * 0.7f,
                          MaterialYouTheme::ArgbToImU32(MaterialYouTheme::AccentTheme().primary, alpha));
    draw->AddText(min + ImVec2(unit * 1.15f, height * 0.5f - unit * 0.5f),
                  IM_COL32(255, 255, 255, static_cast<int>(255 * alpha)), success ? PSI_OK : PSI_INFO_SIGN_ALT);
    draw->AddText(ImGui::GetFont(), unit, min + ImVec2(unit * 3.0f, unit * 0.8f),
                  MaterialYouTheme::ArgbToImU32(MaterialYouTheme::FixedSurfaceColors::onSurface, alpha),
                  message.c_str(), nullptr, width - unit * 4.0f);
}

// Switching appearance: instead of a hard flip, the old surface colour lingers over the new
// frame for a quarter second. Recorded here, drawn last in the frame from clientDrawAppearanceFade.
namespace
{
    double gAppearanceFadeStart = -10.0;
    MaterialYouTheme::Argb gAppearanceFadeColor = 0;
}

void clientNoteAppearanceSwitch()
{
    gAppearanceFadeStart = ImGui::GetTime();
    gAppearanceFadeColor = MaterialYouTheme::FixedSurfaceColors::surface; // Still the old palette
}

void clientDrawAppearanceFade()
{
    constexpr double kDuration = 0.28;
    const double age = ImGui::GetTime() - gAppearanceFadeStart;
    if (age < 0.0 || age >= kDuration || !clientSettings().animations)
        return;
    const float t = static_cast<float>(age / kDuration);
    const float alpha = (1.0f - t) * (1.0f - t); // Ease-out: quick to go, gentle to finish
    ImGui::GetForegroundDrawList()->AddRectFilled({0, 0}, ImGui::GetIO().DisplaySize,
                                                  MaterialYouTheme::ArgbToImU32(gAppearanceFadeColor, alpha));
}

// The palette depends on the connected model, so the settings screen cannot just call ApplyDefault.
void clientReapplyTheme()
{
    if (connState == CONN_STATE_CONNECTED && gDevice)
        MaterialYouTheme::ApplyForModelColor(GetModelColor());
    else
        MaterialYouTheme::ApplyDefault();
}

void DrawApp()
{
    auto& io = ImGui::GetIO();
    auto& g = *ImGui::GetCurrentContext();
#ifdef MDR_CLIENT_DEBUGGER
    if (gDebuggerOnlyMode)
    {
        ImGui::SetNextWindowPos({0, clientWindowChromeHeight()});
        ImGui::SetNextWindowSize({io.DisplaySize.x, io.DisplaySize.y - clientWindowChromeHeight()});
        if (ImGui::Begin("SonyHeadphonesClient", nullptr, kImWindowFlagsTopMost))
            ImGui::TextDisabled(tr("Packet replay mode"));
        ImGui::End();
        clientDebuggerDraw(&gDebuggerOpen, true);
        if (!gDebuggerOpen)
            gDebuggerOnlyMode = false;
        return;
    }
#endif
    if (connState == CONN_STATE_CONNECTED && gDevice)
        PollDevice();
    {
        // The reveal replays when the screen changes (discovery <-> connected), not on every
        // retry of an automatic connection, which would keep the discovery screen twitching.
        static int shownScreen = -1;
        const int screen = connState == CONN_STATE_CONNECTED ? 1 : 0;
        if (screen != shownScreen)
            shownScreen = screen, ImRevealRestart(0);
    }
    gContentScrolled = false; // Set again by whichever screen owns a scrolling column
    ImGui::SetNextWindowPos({0, clientWindowChromeHeight()});
    ImGui::SetNextWindowSize({io.DisplaySize.x, io.DisplaySize.y - clientWindowChromeHeight()});
    ImGuiWindowFlags flags = kImWindowFlagsTopMost;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    const bool mainVisible = ImGui::Begin("SonyHeadphonesClient", nullptr, flags);
    ImGui::PopStyleVar();
    if (mainVisible)
    {
        switch (connState)
        {
        case CONN_STATE_NO_CONNECTION:
#ifdef MDR_CLIENT_DEBUGGER
            if (!gDebuggerOpen)
#endif
            DrawDeviceDiscovery();
            break;
        case CONN_STATE_CONNECTING:
            DrawDeviceConnecting();
            if (connState == CONN_STATE_CONNECTING && connectionAttempt.automatic)
                DrawDeviceDiscovery();
            break;
        case CONN_STATE_CONNECTED:
            DrawDeviceControls();
            break;
        case CONN_STATE_DISCONNECTED:
            DrawDeviceDisconnect();
            break;
        }
        DrawClosePrompt(); // No-op when a screen above already stacked it
    }
    ImGui::End();
    if (gDemoTab == 8 && ImGui::GetTime() > 2.0)
    {
        gDemoTab = -1;
        ShowConnectOverlay(true);
    }
    DrawConnectionNotification();
    DrawConnectOverlay();
    if (connState == CONN_STATE_CONNECTED && gDevice)
        CommitDevice();
#ifdef MDR_CLIENT_DEBUGGER
    // Error modals replace the debugger popup while preserving its open state.
    // Once the error is dismissed, the debugger reopens with its packet history intact.
    if (connState != CONN_STATE_DISCONNECTED &&
        (gDebuggerOpen || ImGui::IsPopupOpen("Debugger")))
        clientDebuggerDraw(&gDebuggerOpen);
#endif
}

#ifdef MDR_CLIENT_DEBUGGER
void clientEnterDebuggerReplayMode()
{
    CloseDevice();
    clientPlatformConnectionDestroy();
    connectionAttempt = {};
    connState = CONN_STATE_NO_CONNECTION;
    gDebuggerOnlyMode = true;
    gDebuggerOpen = true;
}
#endif

#pragma region System Tray
extern SDL_Window* gWindow; // SDLMain.cpp

namespace
{
    // Battery shown in the tray: the main battery when reported, otherwise the lower earbud.
    // Mirrors the header card filter (present + threshold) so both show the same numbers.
    const MDRBattery* TrayBattery()
    {
        const MDRBattery* main = nullptr;
        const MDRBattery* bud = nullptr;
        for (const MDRBattery& battery : gState.mBatteries)
        {
            if (!battery.present || !battery.update_threshold_percent)
                continue;
            if (battery.part == MDR_BATTERY_MAIN)
                main = &battery;
            else if (battery.part == MDR_BATTERY_LEFT || battery.part == MDR_BATTERY_RIGHT)
                if (!bud || battery.level_percent < bud->level_percent)
                    bud = &battery;
        }
        return main ? main : bud;
    }

    int TrayNoiseMode()
    {
        if (!gState.mNoiseAvailable)
            return CLIENT_TRAY_NOISE_UNAVAILABLE;
        if (gState.mNoise.mode == MDR_NOISE_MODE_OFF)
            return CLIENT_TRAY_NOISE_OFF;
        if (ConnectionProtocolVersion() == MDR_PROTOCOL_V1)
        {
            // V1: mode is just on/off; ambient_level -1 means Noise Cancelling, 0..20 the ambient family.
            return static_cast<int8_t>(gState.mNoise.ambient_level) == -1
                ? CLIENT_TRAY_NOISE_CANCELLING : CLIENT_TRAY_NOISE_AMBIENT;
        }
        return gState.mNoise.mode == MDR_NOISE_MODE_AMBIENT ? CLIENT_TRAY_NOISE_AMBIENT : CLIENT_TRAY_NOISE_CANCELLING;
    }

    // Same mutations as the radio buttons in DrawDeviceControlsSound; CommitDevice flushes them.
    void ApplyTrayNoiseMode(int mode)
    {
        if (connState != CONN_STATE_CONNECTED || !gDevice || !gState.mNoiseAvailable)
            return;
        const bool v1 = ConnectionProtocolVersion() == MDR_PROTOCOL_V1;
        switch (mode)
        {
        case CLIENT_TRAY_NOISE_CANCELLING:
            if (!FeatureAvailable(MDR_FEATURE_NOISE_CANCELLING))
                return;
            if (v1)
            {
                gState.mNoise.mode = MDR_NOISE_MODE_V1_ON;
                gState.mNoise.ambient_level = static_cast<uint8_t>(-1);
            }
            else
                gState.mNoise.mode = MDR_NOISE_MODE_CANCELLING;
            break;
        case CLIENT_TRAY_NOISE_AMBIENT:
            if (!FeatureAvailable(MDR_FEATURE_AMBIENT_SOUND))
                return;
            if (v1)
            {
                gState.mNoise.mode = MDR_NOISE_MODE_V1_ON;
                if (static_cast<int8_t>(gState.mNoise.ambient_level) < 1)
                    gState.mNoise.ambient_level = 20;
            }
            else
            {
                gState.mNoise.mode = MDR_NOISE_MODE_AMBIENT;
                if (gState.mNoise.ambient_level == 0)
                    gState.mNoise.ambient_level = 20;
            }
            break;
        case CLIENT_TRAY_NOISE_OFF:
            gState.mNoise.mode = MDR_NOISE_MODE_OFF;
            break;
        default:
            return;
        }
        gState.mNoise.changing_asm_level = MDR_FALSE;
        mdrHeadphonesSetNoiseControl(gDevice, &gState.mNoise);
    }

    void ProcessTrayEvents(bool& exitRequested)
    {
        ClientTrayEvent event{};
        while (clientPlatformTrayPollEvent(&event))
        {
            switch (event.action)
            {
            case CLIENT_TRAY_ACTION_SHOW_WINDOW:
                if (gWindow)
                {
                    SDL_ShowWindow(gWindow);
                    SDL_RestoreWindow(gWindow);
                    SDL_RaiseWindow(gWindow);
                }
                break;
            case CLIENT_TRAY_ACTION_EXIT:
                exitRequested = true;
                break;
            case CLIENT_TRAY_ACTION_SET_NOISE_MODE:
                ApplyTrayNoiseMode(event.noiseMode);
                break;
            default:
                break;
            }
        }
    }

    void SyncTray()
    {
        const bool connected = connState == CONN_STATE_CONNECTED && gDevice != nullptr;
        const mdr::String name = connected ? GetText(MDR_TEXT_MODEL_NAME) : mdr::String{};
        const MDRBattery* battery = connected ? TrayBattery() : nullptr;
        ClientTrayStatus status{};
        status.connected = connected;
        status.deviceName = name.empty() ? nullptr : name.c_str();
        status.batteryPercent = battery ? battery->level_percent : -1;
        status.charging = battery && battery->charging == MDR_CHARGING_YES;
        status.noiseMode = connected ? TrayNoiseMode() : CLIENT_TRAY_NOISE_UNAVAILABLE;
        status.noiseCancellingAvailable = connected && FeatureAvailable(MDR_FEATURE_NOISE_CANCELLING);
        status.ambientSoundAvailable = connected && FeatureAvailable(MDR_FEATURE_AMBIENT_SOUND);
        status.textNotConnected = tr("Not connected");
        status.textNoiseCancelling = tr("Noise Cancelling");
        status.textAmbientSound = tr("Ambient Sound");
        status.textOff = tr("Off");
        status.textShowWindow = tr("Show Window");
        status.textExit = tr("Exit");
        status.textCharging = tr("charging");
        clientPlatformTrayUpdate(&status);
    }
}
#pragma endregion

// Close the headphone session the same way the Disconnect menu does. Exiting with only the
// socket closed leaves the headphones holding a half-open session, and they then ignore the
// next connection on that link until it times out.
void clientShutdown()
{
    if (gDevice)
        CloseDevice();
    if (MDRConnection* conn = clientPlatformConnectionGet())
        mdrConnectionDisconnect(conn);
    connState = CONN_STATE_NO_CONNECTION;
}

bool clientShouldExit()
{
    // Defines like IMGUI_DISABLE_OBSOLETE_FUNCTIONS changes ImGui struct sizes
    // and can lead to very, very bad results. Check them here too to ensure than this TU got the correct ones.
    IMGUI_CHECKVERSION();
    bool exitRequested = false;
    gClosePromptDrawn = false;
    ProcessTrayEvents(exitRequested); // Before DrawApp so a staged mode change commits this frame
    DrawApp();
    SyncTray();
    return exitRequested;
}
