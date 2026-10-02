// A replay transport for design work and screenshots.
//
// The recorded RX packets of a real session (the .bin files a `--record` run writes) are handed to
// libmdr one frame per poll, in the order they were captured, and everything the library sends is
// swallowed. Because libmdr issues its requests in the same order every time, the recorded answers
// line up and the session reaches the ready state exactly as it did with the real headphones. The
// whole connected UI then runs without a device, with the feature table of the captured model.
// Started with `--demo <capture-folder>`; nothing here is used otherwise.
#include "DemoConnection.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace
{
    struct DemoTransport
    {
        std::vector<std::vector<unsigned char>> frames;
        size_t nextFrame = 0;
        size_t frameOffset = 0;        // Partial delivery when the caller's buffer is small
        uint64_t lastDeliveryMs = 0;   // A few milliseconds between frames keeps the pacing close to a real link
        MDRDeviceInfo device{};
        MDRConnection connection{};
    };

    MDRResult Connect(void*, const char*, const char*) { return MDR_RESULT_OK; }
    void Disconnect(void*) {}

    MDRResult Receive(void* user, char* destination, int size, int* received)
    {
        auto& self = *static_cast<DemoTransport*>(user);
        *received = 0;
        if (self.nextFrame >= self.frames.size() || size <= 0)
            return MDR_RESULT_INPROGRESS;
        const uint64_t now = SDL_GetTicks();
        if (self.frameOffset == 0 && now - self.lastDeliveryMs < 8)
            return MDR_RESULT_INPROGRESS; // Nothing new "arrived" yet
        const std::vector<unsigned char>& frame = self.frames[self.nextFrame];
        const size_t remaining = frame.size() - self.frameOffset;
        const size_t count = std::min(remaining, static_cast<size_t>(size));
        std::memcpy(destination, frame.data() + self.frameOffset, count);
        self.frameOffset += count;
        self.lastDeliveryMs = now;
        if (self.frameOffset >= frame.size())
        {
            self.frameOffset = 0;
            ++self.nextFrame;
        }
        *received = static_cast<int>(count);
        return MDR_RESULT_OK;
    }

    MDRResult Send(void*, const char*, int size, int* sent)
    {
        *sent = size;
        return MDR_RESULT_OK;
    }

    MDRResult Poll(void*, int) { return MDR_RESULT_OK; }

    MDRResult GetDevices(void* user, MDRDeviceInfo** devices, int* count)
    {
        auto& self = *static_cast<DemoTransport*>(user);
        *devices = new MDRDeviceInfo(self.device);
        *count = 1;
        return MDR_RESULT_OK;
    }

    MDRResult FreeDevices(void*, MDRDeviceInfo** devices)
    {
        delete *devices;
        *devices = nullptr;
        return MDR_RESULT_OK;
    }

    const char* GetLastError(void*) { return "demo transport"; }
}

MDRConnection* clientDemoConnectionCreate(const char* folder, const char* macAddress, const char* name)
{
    int count = 0;
    char** paths = SDL_GlobDirectory(folder, "*-rx.*.bin", SDL_GLOB_CASEINSENSITIVE, &count);
    if (!paths || count == 0)
    {
        SDL_Log("Demo: no recorded RX packets in %s", folder);
        SDL_free(paths);
        return nullptr;
    }
    std::vector<std::string> names(paths, paths + count);
    SDL_free(paths);
    std::sort(names.begin(), names.end()); // Timestamp then sequence number: capture order

    auto* transport = new DemoTransport();
    for (const std::string& relative : names)
    {
        const bool absolute = relative.size() > 1 && (relative[1] == ':' || relative[0] == '/' || relative[0] == '\\');
        const std::string path = absolute ? relative : std::string(folder) + "/" + relative;
        size_t size = 0;
        void* data = SDL_LoadFile(path.c_str(), &size);
        if (!data)
            continue;
        transport->frames.emplace_back(static_cast<unsigned char*>(data), static_cast<unsigned char*>(data) + size);
        SDL_free(data);
    }
    SDL_strlcpy(transport->device.szDeviceName, name && *name ? name : "Demo headphones", sizeof(transport->device.szDeviceName));
    SDL_strlcpy(transport->device.szDeviceMacAddress, macAddress && *macAddress ? macAddress : "00:11:22:33:44:55",
                sizeof(transport->device.szDeviceMacAddress));
    transport->connection = MDRConnection{transport, Connect, Disconnect, Receive, Send, Poll, GetDevices, FreeDevices, GetLastError};
    SDL_Log("Demo: replaying %zu recorded packets from %s", transport->frames.size(), folder);
    return &transport->connection;
}
