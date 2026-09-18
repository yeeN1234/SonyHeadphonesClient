// Bluetooth radio reset for Windows.
//
// When the headphones stop answering on an otherwise "connected" link (a zombie ACL link,
// typically after an abrupt disconnect), the only reliable recovery from the PC side is to
// switch the Bluetooth radio off and on. That is what the quick-settings toggle does; the
// Windows.Devices.Radios API lets a desktop app do the same without elevation.
//
// Raw WinRT ABI + WRL (no C++/WinRT) because the project builds without C++ exceptions.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <roapi.h>
#include <wrl/client.h>
#include <wrl/event.h>
#include <wrl/wrappers/corewrappers.h>
#include <windows.foundation.h>
#include <windows.foundation.collections.h>
#include <windows.devices.radios.h>
#include <atomic>
#include <thread>

#include <mdr/Protocol.hpp>
#include "../Platform.hpp"

#pragma comment(lib, "runtimeobject.lib")

namespace
{
    using namespace ABI::Windows::Devices::Radios;
    using namespace ABI::Windows::Foundation;
    using namespace ABI::Windows::Foundation::Collections;
    using Microsoft::WRL::Callback;
    using Microsoft::WRL::ComPtr;
    using Microsoft::WRL::Wrappers::HStringReference;

    std::atomic<bool> gInProgress{false};
    std::atomic<int> gLastResult{0};

    // Blocks the calling (worker) thread until the async operation completes. Returns false on
    // failure or timeout.
    template <typename TResult>
    bool Await(IAsyncOperation<TResult>* op, DWORD timeoutMs)
    {
        HANDLE done = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!done)
            return false;
        auto handler = Callback<IAsyncOperationCompletedHandler<TResult>>(
            [done](IAsyncOperation<TResult>*, AsyncStatus) -> HRESULT
            {
                SetEvent(done);
                return S_OK;
            });
        bool ok = handler && SUCCEEDED(op->put_Completed(handler.Get())) &&
            WaitForSingleObject(done, timeoutMs) == WAIT_OBJECT_0;
        if (ok)
        {
            ComPtr<IAsyncInfo> info;
            AsyncStatus status = AsyncStatus::Started;
            if (SUCCEEDED(op->QueryInterface(IID_PPV_ARGS(&info))) && SUCCEEDED(info->get_Status(&status)))
                ok = status == AsyncStatus::Completed;
        }
        CloseHandle(done);
        return ok;
    }

    bool SetRadioState(IRadio* radio, RadioState state)
    {
        ComPtr<IAsyncOperation<RadioAccessStatus>> op;
        if (FAILED(radio->SetStateAsync(state, &op)) || !Await(op.Get(), 15000))
            return false;
        RadioAccessStatus access = RadioAccessStatus_Unspecified;
        return SUCCEEDED(op->GetResults(&access)) && access == RadioAccessStatus_Allowed;
    }

    // Runs on its own thread: off, pause, on.
    void ResetWorker()
    {
        int result = 0;
        const HRESULT init = RoInitialize(RO_INIT_MULTITHREADED);
        MDR_LOG("[BT-Reset] worker start, RoInitialize=0x{:08X}", static_cast<unsigned>(init));
        ComPtr<IRadioStatics> statics;
        const HRESULT factory = RoGetActivationFactory(
            HStringReference(RuntimeClass_Windows_Devices_Radios_Radio).Get(), IID_PPV_ARGS(&statics));
        MDR_LOG("[BT-Reset] activation factory=0x{:08X}", static_cast<unsigned>(factory));
        if (SUCCEEDED(factory))
        {
            ComPtr<IAsyncOperation<RadioAccessStatus>> accessOp;
            RadioAccessStatus access = RadioAccessStatus_Unspecified;
            if (SUCCEEDED(statics->RequestAccessAsync(&accessOp)) && Await(accessOp.Get(), 15000))
                accessOp->GetResults(&access);
            MDR_LOG("[BT-Reset] access={}", static_cast<int>(access));

            ComPtr<IAsyncOperation<IVectorView<Radio*>*>> radiosOp;
            ComPtr<IVectorView<Radio*>> radios;
            if (access == RadioAccessStatus_Allowed && SUCCEEDED(statics->GetRadiosAsync(&radiosOp)) &&
                Await(radiosOp.Get(), 15000) && SUCCEEDED(radiosOp->GetResults(&radios)))
            {
                unsigned int count = 0;
                radios->get_Size(&count);
                MDR_LOG("[BT-Reset] radios={}", count);
                for (unsigned int i = 0; i < count; ++i)
                {
                    ComPtr<IRadio> radio;
                    RadioKind kind = RadioKind_Other;
                    if (FAILED(radios->GetAt(i, &radio)) || FAILED(radio->get_Kind(&kind)) || kind != RadioKind_Bluetooth)
                        continue;
                    const bool off = SetRadioState(radio.Get(), RadioState_Off);
                    MDR_LOG("[BT-Reset] radio off -> {}", off);
                    if (off)
                    {
                        // Long enough for the headphones to see the link go down and drop their
                        // side of it (link supervision); a 3 s blip is not noticed and the stuck
                        // session survives.
                        Sleep(10000);
                        result = SetRadioState(radio.Get(), RadioState_On) ? 1 : 0;
                        MDR_LOG("[BT-Reset] radio on -> {}", result);
                    }
                    break;
                }
            }
            else
                MDR_LOG("[BT-Reset] could not enumerate radios");
        }
        if (SUCCEEDED(init))
            RoUninitialize();
        gLastResult = result;
        gInProgress = false;
        MDR_LOG("[BT-Reset] worker done, result={}", result);
    }
}

extern "C" {
int clientPlatformBluetoothResetSupported(void)
{
    return 1;
}

int clientPlatformBluetoothResetStart(void)
{
    bool expected = false;
    if (!gInProgress.compare_exchange_strong(expected, true))
        return 0; // already running
    std::thread(ResetWorker).detach();
    return 1;
}

int clientPlatformBluetoothResetInProgress(void)
{
    return gInProgress ? 1 : 0;
}
}
