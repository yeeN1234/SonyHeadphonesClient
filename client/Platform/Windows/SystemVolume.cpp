// Windows system volume for the headphones' own audio endpoint.
//
// Driving the OS endpoint instead of the MDR volume command keeps the app's slider and the
// Windows volume slider the same number: Windows pushes the change to the headphones as AVRCP
// absolute volume (the same path the keyboard volume keys use), and the headphones report it
// back over MDR. Setting only the MDR side left Windows showing a different value.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <initguid.h> // Defines CLSID_MMDeviceEnumerator and PKEY_Device_FriendlyName in this TU
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <functiondiscoverykeys_devpkey.h>
#include <cwchar>
#include <cstring>

#include "../Platform.hpp"

namespace
{
    IAudioEndpointVolume* gEndpoint = nullptr;
    bool gComInitialized = false;

    void Release()
    {
        if (gEndpoint)
        {
            gEndpoint->Release();
            gEndpoint = nullptr;
        }
    }

    // UTF-8 model name -> wide, for matching the endpoint's friendly name.
    bool ToWide(const char* utf8, wchar_t* out, int outCount)
    {
        return MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out, outCount) > 0;
    }

    bool NameMatches(IMMDevice* device, const wchar_t* needle)
    {
        IPropertyStore* store = nullptr;
        if (FAILED(device->OpenPropertyStore(STGM_READ, &store)) || !store)
            return false;
        PROPVARIANT name;
        PropVariantInit(&name);
        bool matches = false;
        if (SUCCEEDED(store->GetValue(PKEY_Device_FriendlyName, &name)) && name.vt == VT_LPWSTR && name.pwszVal)
        {
            // The A2DP endpoint carries the model name; the Hands-Free one is the call audio profile.
            matches = wcsstr(name.pwszVal, needle) != nullptr && wcsstr(name.pwszVal, L"Hands-Free") == nullptr;
        }
        PropVariantClear(&name);
        store->Release();
        return matches;
    }

    IAudioEndpointVolume* Activate(IMMDevice* device)
    {
        IAudioEndpointVolume* endpoint = nullptr;
        if (FAILED(device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr,
                                    reinterpret_cast<void**>(&endpoint))))
            return nullptr;
        return endpoint;
    }
}

extern "C" {
int clientPlatformSystemVolumeBind(const char* deviceName)
{
    Release();
    if (!deviceName || !*deviceName)
        return 0;
    if (!gComInitialized)
    {
        // The UI thread may already hold an apartment (SDL, WinRT); either answer is fine.
        const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        gComInitialized = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
        if (!gComInitialized)
            return 0;
    }
    wchar_t needle[128];
    if (!ToWide(deviceName, needle, 128))
        return 0;
    IMMDeviceEnumerator* enumerator = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator))) || !enumerator)
        return 0;
    // Prefer the default output when it is the headphones: that is the slider Windows shows.
    IMMDevice* device = nullptr;
    if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device)) && device)
    {
        if (NameMatches(device, needle))
            gEndpoint = Activate(device);
        device->Release();
        device = nullptr;
    }
    if (!gEndpoint)
    {
        IMMDeviceCollection* devices = nullptr;
        if (SUCCEEDED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devices)) && devices)
        {
            UINT count = 0;
            devices->GetCount(&count);
            for (UINT i = 0; i < count && !gEndpoint; ++i)
            {
                if (FAILED(devices->Item(i, &device)) || !device)
                    continue;
                if (NameMatches(device, needle))
                    gEndpoint = Activate(device);
                device->Release();
                device = nullptr;
            }
            devices->Release();
        }
    }
    enumerator->Release();
    return gEndpoint ? 1 : 0;
}

void clientPlatformSystemVolumeUnbind(void)
{
    Release();
}

int clientPlatformSystemVolumeGet(float* outScalar)
{
    if (!gEndpoint || !outScalar)
        return 0;
    float scalar = 0.0f;
    if (FAILED(gEndpoint->GetMasterVolumeLevelScalar(&scalar)))
    {
        Release(); // The endpoint went away (headphones dropped as an audio device); rebind later
        return 0;
    }
    *outScalar = scalar;
    return 1;
}

int clientPlatformSystemVolumeSet(float scalar)
{
    if (!gEndpoint)
        return 0;
    scalar = scalar < 0.0f ? 0.0f : scalar > 1.0f ? 1.0f : scalar;
    if (FAILED(gEndpoint->SetMasterVolumeLevelScalar(scalar, nullptr)))
    {
        Release();
        return 0;
    }
    // Like the volume keys: raising the volume also lifts a mute.
    if (scalar > 0.0f)
        gEndpoint->SetMute(FALSE, nullptr);
    return 1;
}
}
