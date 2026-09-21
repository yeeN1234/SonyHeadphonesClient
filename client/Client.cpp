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
    return ImGui::Button(label, ImVec2{std::max(1.0f, width), 0});
}

// The discovery and connecting screens are drawn straight on the window surface as a centred,
// scrollable column. They used to be modals, which put a second rounded card (with its own
// background and a dark gutter) inside the already custom-framed window.
bool ImBeginScreenColumn(const char* id)
{
    const float avail = ImGui::GetContentRegionAvail().x;
    const float width = std::max(1.0f, std::min(avail, ImGui::GetFontSize() * 44));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - width) * 0.5f);
    return ImGui::BeginChild(id, {width, 0}, ImGuiChildFlags_AlwaysUseWindowPadding,
                             ImGuiWindowFlags_NoBackground);
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
    auto& style = ImGui::GetStyle();
    ImVec2 size = ImGui::CalcTextSize(text);
    auto [offset, region, draw] = ImWindowDrawOffsetRegionList();
    ImVec2 pad = style.FramePadding / 2;
    ImGui::Text("%s", text);
    offset.y += style.FramePadding.y;
    draw->AddRect(offset - pad, offset + size + pad, color, rounding, ImDrawFlags_None, thickness);
    ImGui::Dummy({pad.x, 0});
}

template <typename T, size_t Extent, typename Formatter>
bool ImComboBoxItems(const char* label, std::span<const T, Extent> items, T& selection, Formatter format)
{
    bool changed = false;
    if (ImGui::BeginCombo(label, format(selection)))
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
    float bandHeight = std::max(region.y, 160.0f);
    if (numBands == 5)
        ImGui::SeparatorText(tr("5-Band EQ"));
    if (numBands == 10)
        ImGui::SeparatorText(tr("10-Band EQ"));
    for (int i = 0; i < numBands; ++i)
    {
        ImGui::BeginGroup();
        ImGui::PushID(i);
        changed |= ImGui::VSliderInt("##v", ImVec2{bandWidth, bandHeight}, &bands[i], mn, mx);
        ImGui::PopID();

        float textWidth = ImGui::CalcTextSize(kBands[i]).x;
        float textOffset = (bandWidth - textWidth) * 0.5f;
        if (textOffset > 0.0f)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + textOffset);
        ImGui::TextUnformatted(kBands[i]);

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

// A small vector illustration stays crisp with the UI's DPI scale and model palette.
void DrawListeningHero(const char* title, const char* subtitle)
{
    const float unit = ImGui::GetFontSize();
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const bool compact = connState == CONN_STATE_CONNECTED;
    const float height = unit * (compact ? 4.8f : 8.0f);
    auto* draw = ImGui::GetWindowDrawList();
    const ImU32 accent = ImGui::GetColorU32(ImGuiCol_CheckMark);
    draw->AddRectFilled(start, start + ImVec2(width, height),
                        MaterialYouTheme::ArgbToImU32(0xFFFFFFFF), unit * 1.5f);
    // Hide the illustration on narrow windows to leave room for the heading.
    const bool illustrated = width > unit * 25;
    if (illustrated)
    {
        const float floatY = !compact && clientSettings().animations
            ? std::sin(static_cast<float>(ImGui::GetTime()) * 2.0f) * unit * 0.18f : 0.0f;
        const ImVec2 center = start + ImVec2(width - unit * 4, height * 0.5f + floatY);
        draw->AddCircleFilled(center, unit * 2.8f, MaterialYouTheme::ArgbToImU32(0xFFF5F5F7));
        if (connState == CONN_STATE_CONNECTING)
        {
            const float phase = clientSettings().animations ? static_cast<float>(ImGui::GetTime()) * 3.0f : 0.0f;
            draw->PathArcTo(center, unit * 3.1f, phase, phase + 4.6f, 48);
            draw->PathStroke(accent, 0, unit * 0.12f);
        }
        draw->PathArcTo(center, unit * 1.6f, 3.14159265f, 6.2831853f, 32);
        draw->PathStroke(accent, 0, unit * 0.22f);
        for (float side : {-1.0f, 1.0f})
        {
            const ImVec2 cup = center + ImVec2(side * unit * 1.55f, unit * 0.3f);
            draw->AddRectFilled(cup - ImVec2(unit * 0.35f, unit * 0.75f),
                                cup + ImVec2(unit * 0.35f, unit * 0.75f), accent, unit * 0.3f);
        }
    }
    ImGui::SetCursorScreenPos(start + ImVec2(unit * 1.2f, unit * (compact ? 0.5f : 1.2f)));
    ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), tr("PERSONAL AUDIO"));
    ImGui::PushFont(nullptr, unit * 1.6f);
    // Fit longer model names without colliding with the illustration.
    const float titleWidth = std::max(unit, width - unit * (illustrated ? 9.0f : 2.4f));
    const float measured = ImGui::CalcTextSize(title).x;
    if (measured > titleWidth)
    {
        ImGui::PopFont();
        ImGui::PushFont(nullptr, unit * 1.6f * titleWidth / measured);
    }
    ImGui::SetCursorScreenPos(start + ImVec2(unit * 1.2f, unit * (compact ? 1.6f : 2.8f)));
    ImGui::TextUnformatted(title);
    ImGui::PopFont();
    ImGui::SetCursorScreenPos(start + ImVec2(unit * 1.2f, unit * (compact ? 3.5f : 5.4f)));
    ImGui::TextDisabled("%s", subtitle);
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

// Inline spinner that occupies one frame height and advances the cursor like a widget.
void ImInlineSpinner(ImU32 color)
{
    const float h = ImGui::GetFrameHeight();
    const float radius = ImGui::GetFontSize() * 0.4f;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawSpinnerAt(pos + ImVec2(radius + 2.0f, h * 0.5f), radius, color);
    ImGui::Dummy(ImVec2(radius * 2.0f + 4.0f, h));
}

void DrawLanguageCombo(const char* id, float width)
{
    ClientSettings& settings = clientSettings();
    constexpr ClientLanguage kLanguages[] = {ClientLanguage::Auto, ClientLanguage::English,
                                             ClientLanguage::ChineseTraditional};
    int current = 0;
    for (int i = 0; i < 3; ++i)
        if (static_cast<int>(kLanguages[i]) == settings.language)
            current = i;
    ImGui::SetNextItemWidth(width);
    if (ImGui::BeginCombo(id, clientLanguageName(kLanguages[current])))
    {
        for (int i = 0; i < 3; ++i)
        {
            if (ImGui::Selectable(clientLanguageName(kLanguages[i]), i == current))
            {
                settings.language = static_cast<int>(kLanguages[i]);
                clientLocalizationSetLanguage(kLanguages[i]);
                clientSettingsSave();
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

    auto* draw = ImGui::GetWindowDrawList();
    const float x = start.x + indent;
    draw->AddText({x, start.y + style.FramePadding.y}, ImGui::GetColorU32(ImGuiCol_Text), heading.c_str());
    draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                  {x, start.y + style.FramePadding.y + headingHeight + style.ItemInnerSpacing.y},
                  ImGui::GetColorU32(ImGuiCol_TextDisabled), hint, nullptr, wrapWidth);
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
        const float spacing = ImGui::GetStyle().ItemSpacing.y;
        {
            ImStylesRAII styles;
            styles.PushFont(nullptr, ImGui::GetFontSize() * 1.35f);
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
        ImGui::Checkbox(tr("Remember my choice"), &gClosePromptRemember);
        ImGui::SameLine();
        const float cancelWidth = ImGui::CalcTextSize(tr("Cancel")).x + ImGui::GetStyle().FramePadding.x * 4;
        const float lineAvail = ImGui::GetContentRegionAvail().x;
        if (lineAvail > cancelWidth)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + lineAvail - cancelWidth);
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

void DrawAppSettings()
{
    ClientSettings& settings = clientSettings();
    if (ImGui::Checkbox(tr("Interface animations"), &settings.animations))
        clientSettingsSave();
    if (ImGui::Checkbox(tr("Connection notifications"), &settings.notifications))
        clientSettingsSave();
    DrawLanguageCombo(tr("Language"), ImGui::GetFontSize() * 12.0f);
    {
        constexpr int kCloseActions[] = {CLIENT_CLOSE_ASK, CLIENT_CLOSE_MINIMIZE, CLIENT_CLOSE_EXIT};
        auto closeActionName = [](int action)
        {
            return action == CLIENT_CLOSE_MINIMIZE ? tr("Minimize to the system tray")
                : action == CLIENT_CLOSE_EXIT ? tr("Exit the app")
                : tr("Ask every time");
        };
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 14.0f);
        if (ImGui::BeginCombo(tr("Close button"), closeActionName(settings.closeAction)))
        {
            for (const int action : kCloseActions)
                if (ImGui::Selectable(closeActionName(action), action == settings.closeAction))
                    settings.closeAction = action, clientSettingsSave();
            ImGui::EndCombo();
        }
    }
    ImGui::BeginDisabled(!clientPlatformAutoStartSupported());
    if (ImGui::Checkbox(tr("Start with Windows (minimized to the tray)"), &settings.autoStart))
    {
        if (!clientPlatformAutoStartSet(settings.autoStart ? 1 : 0))
            settings.autoStart = clientPlatformAutoStartGet() != 0;
        clientSettingsSave();
    }
    ImGui::EndDisabled();
    if (!settings.lastDeviceAddress.empty())
    {
        ImGui::TextDisabled(tr("Auto-connect: %s (%s)"),
                            settings.lastDeviceName.empty() ? "last device" : settings.lastDeviceName.c_str(),
                            settings.lastDeviceAddress.c_str());
        ImGui::SameLine();
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
        static MDRDeviceInfo* pDeviceInfo = nullptr;
        static int nDeviceInfo = 0;
        DrawListeningHero(reconnecting ? connectionAttempt.name.c_str() : tr("Your sound. Your space."),
                          reconnecting ? tr("Connecting...") : tr("Connect your Sony headphones."));
        ImGui::SeparatorText(tr("Connection"));
        // Chose, and have the GATT backend active
        static bool usingBLE = false;
        static DEVICE_TYPE deviceType = DEVICE_TYPE_AUTO;
        static int connInitResult = MDR_RESULT_INPROGRESS;
        // BLE / Classic toggle
        bool needSwitchClientPlatform = clientPlatformConnectionGet() == nullptr;
        {
            ImStylesRAII styles;
            {
                ImStylesRAII styles;
                if (usingBLE)
                    styles.PushCol(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                if (ImModalButton(tri(PSI_BLUETOOTH, "Classic"), 0, 2))
                    usingBLE = false, needSwitchClientPlatform = true;
            }
            {
                ImStylesRAII styles;
                if (!usingBLE)
                    styles.PushCol(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                if (ImModalButton(tri(PSI_BLUETOOTH_ALT, "BLE (GATT)"), 1, 2))
                    usingBLE = true, needSwitchClientPlatform = true;
            }
        }
        ImGui::BeginDisabled(usingBLE);
        {
            ImStylesRAII styles;
            const std::array<const char*, 3> labels{tri(PSI_PLUS_SIGN, "Auto"), tri(PSI_FAST_FORWARD, "V2"), tri(PSI_FORWARD, "V1")};
            constexpr std::array tooltips{
                "Auto-detect: tries the V2 (XM5+) service first and falls back to the legacy V1 service if it can't connect.",
                "V2 only: connects to devices exposing the V2 MDR service (XM5+) - newer models like WH/WF-1000XM5.",
                "V1 only: connects to devices exposing the legacy V1 MDR service - older models.",
            };
            for (int i = 0; i < static_cast<int>(labels.size()); ++i)
            {
                ImStylesRAII buttonStyles;
                if (deviceType != i)
                    buttonStyles.PushCol(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                if (ImModalButton(labels[i], i, static_cast<int>(labels.size())))
                    deviceType = static_cast<DEVICE_TYPE>(i);
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                {
                    ImGui::BeginTooltip();
                    ImGui::TextUnformatted(tr(tooltips[i]));
                    ImGui::EndTooltip();
                }
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
        constexpr uint64_t kAutoRefreshIntervalMs = 2000;
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
            ImGui::SeparatorText(tr("Available Devices"));
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
                ImGui::BeginChild("##DiscoveredDevices", {0, ImGui::GetFrameHeightWithSpacing() * std::min(3, nDeviceInfo)},
                                  ImGuiChildFlags_None);
                for (const auto& device : devices)
                {
                    ImGui::PushID(device.szDeviceMacAddress);
                    ImStylesRAII rowStyles;
                    rowStyles.PushVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
                    rowStyles.PushCol(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
                    rowStyles.PushCol(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered));
                    rowStyles.PushCol(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
                    // Clicking a device connects to it straight away.
                    const mdr::String rowLabel = mdr::Format("{}  {}", PSI_LINK, device.szDeviceName);
                    if (ImGui::Button(rowLabel.c_str(), {ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight()}))
                    {
                        gAutoConnectSuppressed = false;
                        gAutoConnectFailures = 0;
                        const int res = StartConnection(clientPlatformConnectionGet(), device.szDeviceMacAddress,
                                                        device.szDeviceName, usingBLE, deviceType);
                        connState = (res != MDR_RESULT_OK && res != MDR_RESULT_INPROGRESS)
                            ? CONN_STATE_DISCONNECTED : CONN_STATE_CONNECTING;
                    }
                    ImGui::PopID();
                }
                ImGui::EndChild();
            }
            else
            {
                ImGui::TextUnformatted(tri(PSI_BLUETOOTH, "Ready when you are"));
                ImGui::TextWrapped(tr("Turn on Bluetooth and connect your headphones in system settings. They will appear here automatically."));
            }
            // The scan itself is instant, so give the click a short, visible acknowledgement.
            static uint64_t refreshFeedbackUntilMs = 0;
            const bool refreshing = SDL_GetTicks() < refreshFeedbackUntilMs;
            ImGui::BeginDisabled(refreshing);
            if (ImModalButton(refreshing ? tri(PSI_REFRESH, "Refreshing...") : tri(PSI_REFRESH, "Refresh")))
            {
                RefreshDeviceList();
                refreshFeedbackUntilMs = SDL_GetTicks() + 900;
            }
            ImGui::EndDisabled(); // refreshing
            if (refreshing)
            {
                const ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
                const float radius = ImGui::GetFontSize() * 0.4f;
                ImDrawSpinnerAt(ImVec2(min.x + (max.y - min.y) * 0.5f, (min.y + max.y) * 0.5f), radius,
                                ImGui::GetColorU32(ImGuiCol_Text));
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
        ImGui::TextWrapped(tr("Use Classic for most devices. Choose BLE (GATT) for LE Audio connections."));
        if (ImGui::TreeNodeEx(tr("App Settings")))
        {
            DrawAppSettings();
            ImGui::TreePop();
        }
        ImGui::Separator();
        ImGui::TextDisabled(tr("SonyHeadphonesClient  /  %s"), CLIENT_VERSION);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(tr("Branch: %s\nCommit: %s\n%s (%s)"), MDR_GIT_BRANCH_NAME,
                              MDR_GIT_COMMIT_HASH, MDR_PLATFORM_OS, MDR_PLATFORM_PROCESSOR);
        // Language picker right on the first screen so it is discoverable without digging.
        {
            const float comboWidth = ImGui::GetFontSize() * 8.0f;
            ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - comboWidth);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 2.0f));
            DrawLanguageCombo("##FooterLanguage", comboWidth);
            ImGui::PopStyleVar();
        }
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled(tr("Independent client. Not affiliated with Sony. Use at your own risk."));
        ImGui::PopTextWrapPos();
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

void DrawDeviceControlsHeader()
{
    MDRConnection* conn = clientPlatformConnectionGet();
    const mdr::String modelName = GetText(MDR_TEXT_MODEL_NAME);
    if (ImGui::BeginMenuBar())
    {
        auto& style = ImGui::GetStyle();
        /* Disconnect & Shutdown */
        if (ImGui::BeginMenu(mdr::Format("{} {}", PSI_CHEVRON_DOWN, modelName).c_str()))
        {
            if (ImGui::MenuItem(tri(PSI_UNLINK, "Disconnect")))
            {
                gAutoConnectSuppressed = true;
                CloseDevice();
                mdrConnectionDisconnect(conn);
                connState = CONN_STATE_NO_CONNECTION;
            }
            if (FeatureAvailable(MDR_FEATURE_SHUTDOWN))
            {
                if (ImGui::MenuItem(tri(PSI_OFF, "Shutdown")))
                {
                    MDRPower power{};
                    if (mdrHeadphonesGetPower(gDevice, &power) == MDR_RESULT_OK)
                    {
                        power.shutdown_requested = MDR_TRUE;
                        mdrHeadphonesSetPower(gDevice, &power);
                    }
                }
            }
#ifdef MDR_CLIENT_DEBUGGER
            ImGui::Separator();
            ImGui::MenuItem(tr("Protocol Debugger"), nullptr, &gDebuggerOpen);
            if (ImGui::MenuItem(tri(PSI_BUG, "Trigger disconnect error")))
                DisconnectWithModal("Disconnect error manually triggered from the debug menu");
#endif
            ImGui::EndMenu();
        }
        if (!gDevice)
        {
            ImGui::EndMenuBar();
            return;
        }
        if (!mdrHeadphonesIsReady(gDevice))
            ImSpinner(1000, style.FontSizeBase * 0.5f,
                      MaterialYouTheme::ArgbToImU32(MaterialYouTheme::FixedSurfaceColors::onSurface, 0.5f), 2.0f, false,
                      true, 1.0f, ImEaseInOutCubic);
        /* Cool Badges */
        // Title, Border Color, Text Color
        using Badge = std::tuple<const char*, int, int>;
        std::array<Badge, 4> badges4;
        Badge *badgeFirst = &badges4[0], *badgeLast = &badges4[0];
        /* Codec */
        if (gState.mModelAvailable && gState.mModel.audio_codec != MDR_AUDIO_CODEC_UNKNOWN)
        {
            *(badgeLast++) = {FormatAudioCodec(gState.mModel.audio_codec), ~0u, ~0u};
        }
        /* DSEE */
        if (FeatureAvailable(MDR_FEATURE_DSEE) && gState.mEqualizerAvailable && gState.mEqualizer.dsee_enabled)
        {
            *(badgeLast++) = {FormatDseeType(gState.mEqualizer.dsee_type), ~0u, ~0u};
        }
        std::span<Badge> badges{badgeFirst, static_cast<size_t>(badgeLast - badgeFirst)};
        // Right-align and draw them
        // XXX: This is surprisingly painful to do.
        ImVec2 padding = style.FramePadding;
        float badgeRegionX = 0, badgeRegionY = 0;
        ImGui::PushFont(ImGui::GetFont(), style.FontSizeBase - padding.y / 2);
        for (auto& [s, border, text] : badges)
        {
            ImVec2 size = ImGui::CalcTextSize(s);
            badgeRegionX += size.x + padding.x * 2, badgeRegionY = std::max(badgeRegionY, size.y);
        }
        ImGui::SameLine(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - badgeRegionX - padding.x * 2);
        float rounding = style.FrameRounding;
        float offsetY = padding.y / 2;
        for (auto& [s, border, text] : badges)
        {
            ImGui::SetCursorPosY(offsetY);
            ImTextWithBorder(s, border, rounding, 2.0f);
        }
        ImGui::PopFont();
        ImGui::EndMenuBar();
    }
    DrawListeningHero(modelName.empty() ? tr("Your headphones") : modelName.c_str(),
                      mdrHeadphonesIsReady(gDevice) ? tr("Connected / Ready to listen") : tr("Connected / Syncing settings"));
    const int columns = ImGui::GetContentRegionAvail().x > ImGui::GetFontSize() * 32 ? 2 : 1;
    if (ImGui::BeginTable("##Stats", columns, ImGuiTableFlags_SizingStretchSame))
    {
        ImGui::TableNextColumn();
        ImGui::PushStyleColor(ImGuiCol_ChildBg,
            MaterialYouTheme::ArgbToImVec4(MaterialYouTheme::FixedSurfaceColors::surfaceContainerLow));
        ImGui::BeginChild("##BatteryCard", {0, 0}, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
        ImGui::TextDisabled(tr("BATTERY"));
        bool hasBattery = false;
        for (const MDRBattery& battery : gState.mBatteries)
        {
            if (!battery.present || !battery.update_threshold_percent)
                continue;
            hasBattery = true;
            const char* label = battery.part == MDR_BATTERY_LEFT ? tr("Left") :
                battery.part == MDR_BATTERY_RIGHT ? tr("Right") :
                battery.part == MDR_BATTERY_CASE ? tr("Case") : tr("Headphones");
            ImGui::Text("%s  %u%%", label, static_cast<unsigned>(battery.level_percent));
            const char* charging = FormatChargingState(battery.charging);
            if (*charging)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("%s", charging);
            }
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, battery.level_percent <= 20
                ? MaterialYouTheme::ArgbToImVec4(MaterialYouTheme::FixedSurfaceColors::error)
                : ImGui::GetStyleColorVec4(ImGuiCol_CheckMark));
            ImGui::ProgressBar(std::clamp(battery.level_percent / 100.0f, 0.0f, 1.0f),
                               {-1, ImGui::GetFontSize() * 0.35f}, "");
            ImGui::PopStyleColor();
        }
        if (!hasBattery)
            ImGui::TextDisabled(tr("Waiting for battery status"));
        ImGui::EndChild();
        ImGui::TableNextColumn();
        ImGui::BeginChild("##PlayingCard", {0, 0}, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
        ImGui::TextDisabled(tr("NOW PLAYING"));
        const auto title = GetText(MDR_TEXT_TRACK_TITLE);
        const auto artist = GetText(MDR_TEXT_TRACK_ARTIST);
        const auto album = GetText(MDR_TEXT_TRACK_ALBUM);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(title.empty() ? tr("Nothing playing yet") : title.c_str());
        if (!artist.empty())
            ImGui::TextDisabled("%s", artist.c_str());
        if (!album.empty())
            ImGui::TextDisabled("%s", album.c_str());
        if (title.empty())
            ImGui::TextDisabled(tr("Play something on your connected device."));
        ImGui::PopTextWrapPos();
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::EndTable();
    }
}

// The device reports volume as 0..30. Windows shows the AVRCP absolute volume (0..127) as a
// percentage, so mirror that two-step rounding to display the same number the OS does.
constexpr int kDeviceVolumeMax = 30;
constexpr int kAvrcpVolumeMax = 127;

int DeviceVolumeToPercent(int volume)
{
    const int avrcp = (volume * kAvrcpVolumeMax + kDeviceVolumeMax / 2) / kDeviceVolumeMax;
    return (avrcp * 100 + kAvrcpVolumeMax / 2) / kAvrcpVolumeMax;
}

void DrawDeviceControlsPlayback()
{
    ImGui::SeparatorText(tr("Volume"));
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    // Slider steps are the device's own 0..30 levels; the label adds the percentage Windows shows.
    // "%%" because ImGui parses the slider label as a printf format string.
    int volume = gState.mPlayback.volume;
    const mdr::String label = mdr::Format("{}/{}  ({}%%)", volume, kDeviceVolumeMax, DeviceVolumeToPercent(volume));
    bool changed = ImGui::SliderInt("##Volume", &volume, 0, kDeviceVolumeMax, label.c_str());
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
    if (changed)
    {
        MDRPlayback playback = gState.mPlayback;
        playback.volume = static_cast<uint8_t>(volume);
        if (mdrHeadphonesSetPlayback(gDevice, &playback) == MDR_RESULT_OK)
        {
            gState.mPlayback = playback;
            gState.mPlaybackVolumeStaged = true;
        }
    }
    ImGui::SeparatorText(tr("Controls"));
    if (ImModalButton(tri(PSI_STEP_BACKWARD, "Prev"), 0, 3))
    {
        MDRPlaybackCommand command{};
        command.action = MDR_PLAYBACK_PREVIOUS;
        mdrHeadphonesPlayback(gDevice, &command);
    }
    if (gState.mPlayback.status == MDR_PLAYBACK_PLAYING)
    {
        if (ImModalButton(tri(PSI_PAUSE, "Pause"), 1, 3))
        {
            MDRPlaybackCommand command{};
            command.action = MDR_PLAYBACK_PAUSE;
            mdrHeadphonesPlayback(gDevice, &command);
        }
    }
    else
    {
        if (ImModalButton(tri(PSI_PLAY, "Play"), 1, 3))
        {
            MDRPlaybackCommand command{};
            command.action = MDR_PLAYBACK_PLAY;
            mdrHeadphonesPlayback(gDevice, &command);
        }
    }
    if (ImModalButton(tri(PSI_STEP_FORWARD, "Next"), 2, 3))
    {
        MDRPlaybackCommand command{};
        command.action = MDR_PLAYBACK_NEXT;
        mdrHeadphonesPlayback(gDevice, &command);
    }
}

void DrawDeviceControlsSound()
{
    const bool supportNC = FeatureAvailable(MDR_FEATURE_NOISE_CANCELLING);
    const bool supportASM = FeatureAvailable(MDR_FEATURE_AMBIENT_SOUND);
    const bool supportAutoASM = FeatureAvailable(MDR_FEATURE_ADAPTIVE_AMBIENT_SOUND);
    /* NC/ASM */
    if (supportASM || supportNC)
    {
        if (ImGui::TreeNodeEx(tr("Ambient Sound"), ImGuiTreeNodeFlags_DefaultOpen))
        {
            bool changed = false;

            MDRProtocolVersion protocolVersion = ConnectionProtocolVersion();
            if (protocolVersion == MDR_PROTOCOL_V1)
            {
                bool ncAsmEnabled = gState.mNoise.mode != MDR_NOISE_MODE_OFF;
                if (ImGui::Checkbox(tr("Enabled"), &ncAsmEnabled))
                    gState.mNoise.mode = ncAsmEnabled ? MDR_NOISE_MODE_V1_ON : MDR_NOISE_MODE_OFF, changed = true;

                ImGui::BeginDisabled(!ncAsmEnabled);

                // -1: Noise Cancelling
                // 0: Wind Noise Reduction
                // 1-20: Ambient Sound
                bool sliderChanged;
                int sliderLevel = static_cast<int8_t>(gState.mNoise.ambient_level);
                if (sliderLevel == -1)
                    sliderChanged = ImGui::SliderInt("##AmbStrength", &sliderLevel, -1, 20, tr("Noise Cancelling"));
                else if (sliderLevel == 0)
                    sliderChanged = ImGui::SliderInt("##AmbStrength", &sliderLevel, -1, 20, tr("Wind Noise Reduction"));
                else
                    sliderChanged = ImGui::SliderInt("##AmbStrength", &sliderLevel, -1, 20, fmt::format("Ambient Sound {}", sliderLevel).c_str());
                if (sliderChanged)
                    gState.mNoise.ambient_level = static_cast<uint8_t>(sliderLevel), changed = true;
                gState.mNoise.changing_asm_level = sliderChanged && ImGui::IsItemActive();
                if (ImGui::IsItemDeactivatedAfterEdit())
                    changed = true;

                ImGui::BeginDisabled(sliderLevel < 1);
                bool focusOnVoice = gState.mNoise.focus_on_voice != MDR_FALSE;
                if (ImGui::Checkbox(tr("Voice Passthrough"), &focusOnVoice))
                    gState.mNoise.focus_on_voice = focusOnVoice ? MDR_TRUE : MDR_FALSE, changed = true;
                ImGui::EndDisabled(); // sliderLevel < 1

                ImGui::EndDisabled(); // !ncAsmEnabled
            }
            else if (protocolVersion == MDR_PROTOCOL_V2)
            {
                if (supportNC)
                {
                    if (ImGui::RadioButton(tr("Noise Cancelling"), gState.mNoise.mode == MDR_NOISE_MODE_CANCELLING))
                    {
                        gState.mNoise.mode = MDR_NOISE_MODE_CANCELLING;
                        changed = true;
                    }
                    ImGui::SameLine();
                }
                if (supportASM)
                {
                    if (ImGui::RadioButton(tr("Ambient Sound"), gState.mNoise.mode == MDR_NOISE_MODE_AMBIENT))
                    {
                        gState.mNoise.mode = MDR_NOISE_MODE_AMBIENT;
                        if (gState.mNoise.ambient_level == 0)
                            gState.mNoise.ambient_level = 20;
                        changed = true;
                    }
                    ImGui::SameLine();
                }
                if (ImGui::RadioButton(tr("Off"), gState.mNoise.mode == MDR_NOISE_MODE_OFF))
                    gState.mNoise.mode = MDR_NOISE_MODE_OFF, changed = true;
                ImGui::SeparatorText(tr("Ambient Strength"));
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                {
                    // Only works with AMB enabled
                    ImGui::BeginDisabled(gState.mNoise.mode != MDR_NOISE_MODE_AMBIENT);
                    bool ambientChanged = false;
                    int ambientLevel = gState.mNoise.ambient_level;
                    if (ImGui::SliderInt("##AmbStrength", &ambientLevel, 1, 20))
                        gState.mNoise.ambient_level = static_cast<uint8_t>(ambientLevel), ambientChanged = changed = true;
                    gState.mNoise.changing_asm_level = ambientChanged && ImGui::IsItemActive();
                    if (ImGui::IsItemDeactivatedAfterEdit())
                        changed = true;
                    if (supportAutoASM)
                    {
                        bool adaptive = gState.mNoise.adaptive_ambient != MDR_FALSE;
                        if (ImGui::Checkbox(tr("Auto Ambient Sound"), &adaptive))
                            gState.mNoise.adaptive_ambient = adaptive ? MDR_TRUE : MDR_FALSE, changed = true;
                        ImGui::BeginDisabled(!adaptive);
                        constexpr MDRAdaptiveSensitivity kSelections[] = {
                            MDR_ADAPTIVE_SENSITIVITY_STANDARD, MDR_ADAPTIVE_SENSITIVITY_HIGH, MDR_ADAPTIVE_SENSITIVITY_LOW};
                        changed |= ImComboBoxItems(tr("Sensitivity"), std::span{kSelections},
                                                   gState.mNoise.adaptive_sensitivity, FormatAdaptiveSensitivity);
                        ImGui::EndDisabled(); // !adaptive
                    }
                    bool focusOnVoice = gState.mNoise.focus_on_voice != MDR_FALSE;
                    if (ImGui::Checkbox(tr("Voice Passthrough"), &focusOnVoice))
                        gState.mNoise.focus_on_voice = focusOnVoice ? MDR_TRUE : MDR_FALSE, changed = true;
                    ImGui::EndDisabled(); // gState.mNoise.mode != MDR_NOISE_MODE_AMBIENT
                }
            }

            if (changed && gState.mNoiseAvailable)
                mdrHeadphonesSetNoiseControl(gDevice, &gState.mNoise);
            ImGui::TreePop();
        }
    }
    /* STC */
    if (FeatureAvailable(MDR_FEATURE_SPEAK_TO_CHAT))
    {
        if (ImGui::TreeNodeEx(tr("Speak To Chat"), ImGuiTreeNodeFlags_DefaultOpen))
        {
            bool changed = false;
            bool enabled = gState.mSpeakToChat.enabled != MDR_FALSE;
            if (ImGui::Checkbox(tr("Enabled"), &enabled))
                gState.mSpeakToChat.enabled = enabled ? MDR_TRUE : MDR_FALSE, changed = true;
            ImGui::BeginDisabled(!enabled);
            constexpr MDRSpeechSensitivity kSensitivity[] = {
                MDR_SPEECH_SENSITIVITY_AUTO, MDR_SPEECH_SENSITIVITY_HIGH, MDR_SPEECH_SENSITIVITY_LOW};
            changed |= ImComboBoxItems(tr("Sensitivity"), std::span{kSensitivity}, gState.mSpeakToChat.sensitivity, FormatSpeechSensitivity);
            constexpr MDRSpeakTimeout kTimeout[] = {
                MDR_SPEAK_TIMEOUT_SHORT, MDR_SPEAK_TIMEOUT_MEDIUM, MDR_SPEAK_TIMEOUT_LONG,
                MDR_SPEAK_TIMEOUT_MANUAL};
            changed |= ImComboBoxItems(tr("Mode Duration"), std::span{kTimeout}, gState.mSpeakToChat.timeout, FormatSpeakTimeout);
            ImGui::EndDisabled();
            if (changed && gState.mSpeakToChatAvailable)
                mdrHeadphonesSetSpeakToChat(gDevice, &gState.mSpeakToChat);
            ImGui::TreePop();
        }
    }
    /* Listening Mode */
    if (FeatureAvailable(MDR_FEATURE_LISTENING_MODE))
    {
        if (ImGui::TreeNodeEx(tr("Listening Mode"), ImGuiTreeNodeFlags_DefaultOpen))
        {
            bool changed = false;
            if (ImGui::RadioButton(tr("Standard"), gState.mListening.mode == MDR_LISTENING_STANDARD))
                gState.mListening.mode = MDR_LISTENING_STANDARD, changed = true;
            if (ImGui::RadioButton(tr("BGM"), gState.mListening.mode == MDR_LISTENING_BACKGROUND_MUSIC))
                gState.mListening.mode = MDR_LISTENING_BACKGROUND_MUSIC, changed = true;

            ImGui::Indent();
            ImGui::BeginDisabled(gState.mListening.mode != MDR_LISTENING_BACKGROUND_MUSIC);
            static const std::pair<MDRRoomSize, const char*> kBGMDistanceModes[] = {
                {MDR_ROOM_SMALL, "My Room"},
                {MDR_ROOM_MEDIUM, "Living Room"},
                {MDR_ROOM_LARGE, "Cafe"},
            };
            const char* currentDistStr = tr("Unknown");
            for (auto const& [k, v] : kBGMDistanceModes)
                if (k == gState.mListening.background_room)
                    currentDistStr = tr(v);
            if (ImGui::BeginCombo(tr("Distance"), currentDistStr))
            {
                for (auto const& [k, v] : kBGMDistanceModes)
                {
                    bool is_selected = k == gState.mListening.background_room;
                    if (ImGui::Selectable(tr(v), is_selected))
                        gState.mListening.background_room = k, changed = true;
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::EndDisabled();
            ImGui::Unindent();

            if (ImGui::RadioButton(tr("Cinema"), gState.mListening.mode == MDR_LISTENING_CINEMA))
                gState.mListening.mode = MDR_LISTENING_CINEMA, changed = true;

            if (changed && gState.mListeningAvailable)
                mdrHeadphonesSetListening(gDevice, &gState.mListening);
            ImGui::TreePop();
        }
    }
    /* EQ & DSEE */
    if (ImGui::TreeNodeEx(tr("Equalizer & DSEE"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        bool changed = false;
        constexpr MDREqualizerPreset kSelections[] = {
            MDR_EQ_OFF, MDR_EQ_ROCK, MDR_EQ_POP, MDR_EQ_JAZZ, MDR_EQ_DANCE, MDR_EQ_EDM,
            MDR_EQ_R_AND_B_HIP_HOP, MDR_EQ_ACOUSTIC, MDR_EQ_BRIGHT, MDR_EQ_EXCITED, MDR_EQ_MELLOW,
            MDR_EQ_RELAXED, MDR_EQ_VOCAL, MDR_EQ_TREBLE, MDR_EQ_BASS, MDR_EQ_SPEECH, MDR_EQ_HEAVY,
            MDR_EQ_CLEAR, MDR_EQ_HARD, MDR_EQ_SOFT, MDR_EQ_GAMING, MDR_EQ_FPS_1, MDR_EQ_FPS_2,
            MDR_EQ_FPS_3, MDR_EQ_CUSTOM, MDR_EQ_USER_1, MDR_EQ_USER_2, MDR_EQ_USER_3, MDR_EQ_USER_4,
            MDR_EQ_USER_5};
        changed |= ImComboBoxItems(tr("Preset"), std::span{kSelections}, gState.mEqualizer.preset, FormatEqualizerPreset);
        if (ImEqualizer(gState.mEqualizerBands))
            SetEqualizerBands(gState.mEqualizerBands);
        if (gState.mEqualizerBands.size() == 5)
        {
            ImGui::SeparatorText(tr("Clear Bass"));
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            int clearBass = gState.mEqualizer.clear_bass;
            if (ImGui::SliderInt("##", &clearBass, -10, 10))
                gState.mEqualizer.clear_bass = static_cast<int8_t>(clearBass), changed = true;
        }
        ImGui::SeparatorText(tr("DSEE"));
        ImGui::BeginDisabled(!FeatureAvailable(MDR_FEATURE_DSEE));
        if (ImGui::RadioButton(tr("Off"), gState.mEqualizer.dsee_enabled == MDR_FALSE))
            gState.mEqualizer.dsee_enabled = MDR_FALSE, changed = true;
        if (ImGui::RadioButton(tr("On (Auto)"), gState.mEqualizer.dsee_enabled != MDR_FALSE))
            gState.mEqualizer.dsee_enabled = MDR_TRUE, changed = true;
        ImGui::EndDisabled();
        if (changed && gState.mEqualizerAvailable)
            mdrHeadphonesSetEqualizer(gDevice, &gState.mEqualizer);
        ImGui::TreePop();
    }
}

void DrawDeviceControlsDevices()
{
    const bool supportDeviceMgmt = FeatureAvailable(MDR_FEATURE_PAIRED_DEVICE_MANAGEMENT);
    if (!supportDeviceMgmt)
        ImGui::TextWrapped("%s", tr("Please enable \"Connect to 2 devices simultaneously\" in System settings to manage devices."));
    ImGui::BeginDisabled(!supportDeviceMgmt);
    struct DeviceView
    {
        MDRPairedDevice state;
        mdr::String mac;   // Colonated MAC (17 chars); stable addressing key
        mdr::String name;
    };
    mdr::Vector<DeviceView> devices;
    for (const MDRPairedDevice& state : gState.mPairedDevices)
        devices.emplace_back(state, mdr::String{state.macAddress},
                           mdr::String{state.name});
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
    auto DrawDeviceElement = [&](const DeviceView& device, bool selected) -> bool
    {
        ImGui::PushID(device.mac.c_str());
        ImGui::BeginGroup();
        if (device.state.playback_device)
        {
            ImGui::Text(tri(PSI_VOLUME_DOWN, "")), ImGui::SameLine();
            if (playbackFixed)
                ImGui::Text(tri(PSI_LOCK, "")), ImGui::SameLine();
        }
        bool res = ImGui::Selectable(device.name.c_str(), selected);
        if (device.state.connected && ImGui::IsItemHovered() &&
            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            StageDeviceAction(MDR_PAIRED_DEVICE_SELECT_PLAYBACK, device.mac.c_str());
        if (selected)
        {
            ImGui::Separator();
            if (device.state.connected)
            {
                const bool canFix = supportFix && device.state.playback_device;
                const int columns = canFix ? 3 : 2;
                if (ImModalButton(tri(PSI_UNLINK, "Disconnect"), 0, columns))
                    StageDeviceAction(MDR_PAIRED_DEVICE_DISCONNECT, device.mac.c_str());
                if (ImModalButton(tri(PSI_VOLUME_DOWN, "Switch Playback"), 1, columns))
                    StageDeviceAction(MDR_PAIRED_DEVICE_SELECT_PLAYBACK, device.mac.c_str());
                if (canFix &&
                    ImModalButton(playbackFixed ? tri(PSI_UNLOCK, "Unfix Playback") : tri(PSI_LOCK, "Fix Playback"), 2, columns))
                    mdrHeadphonesSetSourceSwitchControl(gDevice, playbackFixed ? MDR_TRUE : MDR_FALSE);
            }
            else
            {
                if (ImModalButton(tri(PSI_LINK, "Connect"), 0, 2))
                    StageDeviceAction(MDR_PAIRED_DEVICE_CONNECT, device.mac.c_str());
            }
            if (ImModalButton(tri(PSI_BLUETOOTH_ALT, "Unpair"), 1, 2))
                StageDeviceAction(MDR_PAIRED_DEVICE_UNPAIR, device.mac.c_str());
            // After the button rows: Unpair shares a row, so an inline message would land beside it.
            if (supportFix && device.state.connected && device.state.playback_device)
            {
                MDRSourceSwitchControlResult fixResult = MDR_SOURCE_SWITCH_CONTROL_SUCCESS;
                mdrHeadphonesGetSourceSwitchControlResult(gDevice, &fixResult);
                if (fixResult != MDR_SOURCE_SWITCH_CONTROL_SUCCESS)
                    ImGui::TextWrapped(tri(PSI_INFO_SIGN_ALT, "%s"), FormatSourceSwitchControlResult(fixResult));
            }
        }
        ImGui::EndGroup();
        ImGui::PopID();
        return res;
    };
    static mdr::String connectSelectedMac;
    if (ImGui::TreeNodeEx(tr("Connected"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        for (auto& device : devices)
            if (device.state.connected && DrawDeviceElement(device, connectSelectedMac == device.mac))
                connectSelectedMac = connectSelectedMac == device.mac ? "" : device.mac;
        ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx(tr("Paired"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        for (auto& device : devices)
            if (!device.state.connected && DrawDeviceElement(device, connectSelectedMac == device.mac))
                connectSelectedMac = connectSelectedMac == device.mac ? "" : device.mac;
        ImGui::TreePop();
    }
    if (gState.mPairing.enabled)
    {
        ImTextCentered(tr("Pairing..."));
        ImSpinner(1000.0f, 16.0f,
                  MaterialYouTheme::ArgbToImU32(MaterialYouTheme::ThemeForModelColor(GetModelColor()).primary),
                  2.0f, true, false, 1.0f, ImEaseInOutCubic);
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
        ImGui::TextWrapped("%s", tri(PSI_INFO_SIGN_ALT, "For TWS (Earbuds) devices, you may need to take both of your headphones out from your case to enter Pairing Mode."));
    }
    ImGui::EndDisabled();
}

void DrawDeviceControlsSystem()
{
    /* General Settings */
    if (ImGui::TreeNodeEx(tr("General Setting"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        using StringPair = std::pair<const char*, const char*>;
        constexpr auto kFormatGSString = [](const char* key, std::span<const StringPair> strings) -> const char*
        {
            auto it = std::lower_bound(strings.begin(), strings.end(), key, [](const StringPair& lhs, const char* rhs)
                                       { return strcmp(lhs.first, rhs) < 0; });
            if (it == strings.end() || strcmp(it->first, key) != 0)
                return tr("<Unknown>");
            return tr(it->second);
        };
        constexpr StringPair kGSSubjectStrings[] = {{"MULTIPOINT_SETTING", "Connect to 2 devices simultaneously"},
                                                    {"SIDETONE_SETTING", "Capture Voice During a Phone Call"},
                                                    {"TOUCH_PANEL_SETTING", "Touch sensor control panel"}};
        constexpr StringPair kGSSummaryStrings[] = {
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
        for (auto& [info, setting] : gState.mGeneralSettings)
        {
            if (info.type != MDR_GENERAL_SETTING_BOOLEAN)
                continue;
            const mdr::String subjectKey = GetText(MDR_TEXT_GENERAL_SETTING_SUBJECT, info.index);
            const mdr::String summaryKey = GetText(MDR_TEXT_GENERAL_SETTING_SUMMARY, info.index);
            const char* subject = kFormatGSString(subjectKey.c_str(), kGSSubjectStrings);
            const char* summary = kFormatGSString(summaryKey.c_str(), kGSSummaryStrings);
            bool value = setting.boolean_value != MDR_FALSE;
            ImGui::PushID(static_cast<int>(info.index));
            ImGui::BeginDisabled(subjectKey.empty() || !info.writable);
            if (ImGui::Checkbox(subject, &value))
            {
                setting.boolean_value = value ? MDR_TRUE : MDR_FALSE;
                mdrHeadphonesSetGeneralSetting(gDevice, &setting);
            }
            if (!summaryKey.empty())
            {
                ImGui::Bullet();
                ImGui::SameLine();
                ImGui::TextWrapped("%s", summary);
            }
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        ImGui::TreePop();
    }
    /* Assignable Settings */
    if (FeatureAvailable(MDR_FEATURE_ASSIGNABLE_CONTROLS))
    {
        if (ImGui::TreeNodeEx(tr("Assignable Controls"), ImGuiTreeNodeFlags_DefaultOpen))
        {
            bool changed = false;

            auto controls = GetAssignableControls();
            for (MDRAssignableControl& control : controls)
            {
                mdr::Vector<MDRAssignableAction> actions = GetAssignableControlActions(control.location);
                std::erase(actions, MDR_ASSIGNABLE_GOOGLE_ASSISTANT);
                changed |= ImComboBoxItems<MDRAssignableAction, std::dynamic_extent>(
                    FormatAssignableActionKeyLocation(control), std::span{actions}, control.action,
                    FormatAssignableAction);
            }

            if (changed)
                mdrHeadphonesSetAssignableControls(gDevice, controls.data(), static_cast<uint32_t>(controls.size()));

            ImGui::TreePop();
        }
    }
    /* NC/ASM Button Settings */
    if (FeatureAvailable(MDR_FEATURE_NOISE_CONTROL_BUTTON) &&
        ImGui::TreeNodeEx(tr("NC/AMB Button Function"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (gState.mNoiseAvailable)
        {
            constexpr MDRNoiseButtonMode kSelections[] = {
                MDR_NOISE_BUTTON_NONE, MDR_NOISE_BUTTON_NOISE_AMBIENT_OFF, MDR_NOISE_BUTTON_NOISE_AMBIENT,
                MDR_NOISE_BUTTON_NOISE_OFF, MDR_NOISE_BUTTON_AMBIENT_OFF};
            if (ImComboBoxItems(tr("Function"), std::span{kSelections}, gState.mNoise.button_mode, FormatNoiseButtonMode))
                mdrHeadphonesSetNoiseControl(gDevice, &gState.mNoise);
        }
        ImGui::TreePop();
    }
    MDRPower power{};
    const bool havePower = mdrHeadphonesGetPower(gDevice, &power) == MDR_RESULT_OK;
    /* Head Gesture */
    if (FeatureAvailable(MDR_FEATURE_HEAD_GESTURE) &&
        ImGui::TreeNodeEx(tr("Head Gesture"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        bool enabled = power.head_gesture != MDR_FALSE;
        if (ImGui::Checkbox(tr("Enabled"), &enabled) && havePower)
        {
            power.head_gesture = enabled ? MDR_TRUE : MDR_FALSE;
            mdrHeadphonesSetPower(gDevice, &power);
        }
        ImGui::TreePop();
    }
    /* Auto Power Off */
    if (FeatureAvailable(MDR_FEATURE_AUTO_POWER_OFF) &&
        ImGui::TreeNodeEx(tr("Auto Power Off"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        constexpr uint32_t kSelections[] = {0, 5, 15, 30, 60, 180};
        bool changed = ImComboBoxItems(tr("Time"), std::span{kSelections}, power.auto_power_off_minutes, FormatAutoPowerOff);
        if (FeatureAvailable(MDR_FEATURE_WEARING_DETECTION) &&
            power.wearing_power != MDR_WEARING_POWER_UNAVAILABLE)
        {
            bool whenRemoved = power.wearing_power == MDR_WEARING_POWER_WHEN_REMOVED;
            if (ImGui::Checkbox(tr("Power off when removed"), &whenRemoved))
                power.wearing_power =
                    whenRemoved ? MDR_WEARING_POWER_WHEN_REMOVED : MDR_WEARING_POWER_DISABLED, changed = true;
        }
        if (changed && havePower)
            mdrHeadphonesSetPower(gDevice, &power);
        ImGui::TreePop();
    }
    /* Auto Pause */
    if (FeatureAvailable(MDR_FEATURE_AUTO_PAUSE) &&
        ImGui::TreeNodeEx(tr("Pause when removed"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        bool enabled = power.auto_pause != MDR_FALSE;
        if (ImGui::Checkbox(tr("Enabled"), &enabled) && havePower)
        {
            power.auto_pause = enabled ? MDR_TRUE : MDR_FALSE;
            mdrHeadphonesSetPower(gDevice, &power);
        }
        ImGui::TreePop();
    }
    /* Voice Guidance */
    if (FeatureAvailable(MDR_FEATURE_VOICE_GUIDANCE) &&
        ImGui::TreeNodeEx(tr("Voice Guidance"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        MDRVoiceGuidance voice{};
        if (mdrHeadphonesGetVoiceGuidance(gDevice, &voice) == MDR_RESULT_OK)
        {
            bool changed = false;
            bool enabled = voice.enabled != MDR_FALSE;
            if (ImGui::Checkbox(tr("Enabled"), &enabled))
                voice.enabled = enabled ? MDR_TRUE : MDR_FALSE, changed = true;
            if (FeatureAvailable(MDR_FEATURE_VOICE_GUIDANCE_VOLUME))
            {
                ImGui::SeparatorText(tr("Volume"));
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                int volume = voice.volume;
                if (ImGui::SliderInt("##Volume", &volume, -2, 2))
                    voice.volume = static_cast<int8_t>(volume), changed = true;
            }
            if (changed)
                mdrHeadphonesSetVoiceGuidance(gDevice, &voice);
        }
        ImGui::TreePop();
    }
    /* App behaviour (this client, not the headphones) */
    if (ImGui::TreeNodeEx(tr("App Settings"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        DrawAppSettings();
        ImGui::TreePop();
    }
}
void DrawDeviceControlsAbout()
{
    if (ImGui::TreeNodeEx(tr("Model"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::BeginTable("##ModelTable", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text(tr("Model:"));
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", GetText(MDR_TEXT_MODEL_NAME).c_str());

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text(tr("MAC:"));
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", GetText(MDR_TEXT_UNIQUE_ID).c_str());

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text(tr("Firmware Version:"));
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", GetText(MDR_TEXT_FIRMWARE_VERSION).c_str());

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text(tr("Series:"));
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", GetText(MDR_TEXT_MODEL_SERIES).c_str());

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text(tr("Color:"));
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", GetText(MDR_TEXT_MODEL_COLOR).c_str());

            ImGui::EndTable();
        }
        ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx(tr("Features"), ImGuiTreeNodeFlags_DefaultOpen))
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
        if (ImGui::BeginTable("##Features", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
        {
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
void DrawDeviceControlsTabs()
{
    if (ImGui::BeginTabBar("##Controls"))
    {
        if (ImGui::BeginTabItem(tr("Playback")))
        {
            DrawDeviceControlsPlayback();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(tr("Sound")))
        {
            DrawDeviceControlsSound();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(tr("Devices")))
        {
            DrawDeviceControlsDevices();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(tr("System")))
        {
            DrawDeviceControlsSystem();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(tr("About")))
        {
            DrawDeviceControlsAbout();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

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
    DrawDeviceControlsHeader();
    if (!gDevice)
        return;
    ImGui::Separator();
    ImGui::BeginChild("##ControlTabs", {0, 0}, ImGuiChildFlags_Borders);
    DrawDeviceControlsTabs();
    ImScrollWhenDraggingAnywhere(ImGui::GetIO().MouseDelta, ImGuiMouseButton_Left);
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
void DrawConnectionNotification()
{
    static bool wasReady = false;
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
    if (ready && !wasReady)
    {
        notify(tr("Connected. Ready to listen."), true);
        lowBatteryReported = false;
    }
    else if (wasReady && !ready)
    {
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
    wasReady = ready;
    wasConnecting = connState == CONN_STATE_CONNECTING;
    if (!clientSettings().notifications || now - shownAt >= 4.0)
        return;
    const float age = static_cast<float>(now - shownAt);
    const float progress = clientSettings().animations ? std::clamp(age / 0.3f, 0.0f, 1.0f) : 1.0f;
    const float alpha = clientSettings().animations ? std::min(progress, (4.0f - age) / 0.3f) : 1.0f;
    const auto display = ImGui::GetIO().DisplaySize;
    const float unit = ImGui::GetFontSize();
    const float width = std::min(display.x - 24.0f, unit * 27.0f);
    const float height = unit * 3.6f;
    const ImVec2 min((display.x - width) * 0.5f, display.y - height - 20.0f + (1.0f - progress) * 16.0f);
    auto* draw = ImGui::GetForegroundDrawList();
    draw->AddRectFilled(min + ImVec2(0, 3), min + ImVec2(width, height + 3),
                        IM_COL32(0, 0, 0, static_cast<int>(20 * alpha)), 18.0f);
    draw->AddRectFilled(min, min + ImVec2(width, height),
                        IM_COL32(255, 255, 255, static_cast<int>(255 * alpha)), 18.0f);
    draw->AddCircleFilled(min + ImVec2(unit * 1.5f, height * 0.5f), unit * 0.7f,
                          IM_COL32(0, 113, 227, static_cast<int>(255 * alpha)));
    draw->AddText(min + ImVec2(unit * 1.15f, height * 0.5f - unit * 0.5f),
                  IM_COL32(255, 255, 255, static_cast<int>(255 * alpha)), success ? PSI_OK : PSI_INFO_SIGN_ALT);
    draw->AddText(ImGui::GetFont(), unit, min + ImVec2(unit * 3.0f, unit * 0.8f),
                  IM_COL32(29, 29, 31, static_cast<int>(255 * alpha)), message.c_str(), nullptr, width - unit * 4.0f);
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
    ImGui::SetNextWindowPos({0, clientWindowChromeHeight()});
    ImGui::SetNextWindowSize({io.DisplaySize.x, io.DisplaySize.y - clientWindowChromeHeight()});
    ImGuiWindowFlags flags = kImWindowFlagsTopMost;
    switch (connState)
    {
    case CONN_STATE_CONNECTED:
        flags |= ImGuiWindowFlags_MenuBar;
        break;
    default:
        break;
    }
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
    DrawConnectionNotification();
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
