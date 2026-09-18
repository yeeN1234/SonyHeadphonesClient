#include "../Platform.hpp"
#include <mdr-bt/ConnectionMacOS.h>

extern "C" {
int clientPlatformLocateFontBinary(const char** outData)
{
    *outData = nullptr;
    return 0;
}

static MDRConnectionMacOS* gConn = nullptr;

int clientPlatformConnectionInit(int flags)
{
    if (flags & MDR_INIT_BT_BLE)
        return MDR_RESULT_ERROR_NOT_SUPPORTED;
    gConn = mdrConnectionMacOSCreate();
    return MDR_RESULT_OK;
}

void clientPlatformConnectionDestroy()
{
    if (gConn) { mdrConnectionMacOSDestroy(gConn); gConn = nullptr; }
}

MDRConnection* clientPlatformConnectionGet()
{
    if (gConn) return mdrConnectionMacOSGet(gConn);
    [[unlikely]] return nullptr;
}

void clientPlatformDestroy()
{
    clientPlatformConnectionDestroy();
    // TODO
}

int clientPlatformLocateEmojiFontBinary(const char** outData) { *outData = nullptr; return 0; }
int clientPlatformLocateLatinFontBinary(const char** outData) { *outData = nullptr; return 0; }

int clientPlatformBluetoothResetSupported(void) { return 0; }
int clientPlatformBluetoothResetStart(void) { return 0; }
int clientPlatformBluetoothResetInProgress(void) { return 0; }
int clientPlatformAutoStartSupported(void) { return 0; }
int clientPlatformAutoStartGet(void) { return 0; }
int clientPlatformAutoStartSet(int) { return 0; }
void clientPlatformTrayNotify(const char*, const char*) {}

/* System tray is not implemented on this platform. */
int clientPlatformTrayInit(void) { return 0; }
void clientPlatformTrayUpdate(const ClientTrayStatus*) {}
int clientPlatformTrayPollEvent(ClientTrayEvent*) { return 0; }
void clientPlatformTrayDestroy(void) {}
}
