#pragma once
#include <cstddef>
#include <mdr-c/Connection.h>

extern "C" {
    /**
     * @brief Select and initialize the platform Bluetooth backend.
     *
     * The client is responsible for picking the backend; libmdr-bt only exposes
     * the per-platform entry points (e.g. mdrConnectionWindowsCreate /
     * mdrConnectionWindowsBLECreate). This dispatches to the matching one.
     * @param flags One or more MDR_INIT_* flags (e.g. MDR_INIT_BT_BLE).
     * @return MDR_RESULT_OK on success, or an error code (e.g. NOT_SUPPORTED).
     */
    extern int clientPlatformConnectionInit(int flags);
    extern MDRConnection* clientPlatformConnectionGet();
    extern void clientPlatformConnectionDestroy();

    /**
     * @breif Locate platform-specific font binary data
     * @param outData Pointer to output font data. Must not be freed by caller.
     * @return Size of font data in bytes, 0 if not available - can be retried.
     */
    extern int clientPlatformLocateFontBinary(const char** outData);
    /**
     * @brief Locate a platform font with emoji glyph outlines (rendered monochrome).
     * Same contract as clientPlatformLocateFontBinary.
     */
    extern int clientPlatformLocateEmojiFontBinary(const char** outData);
#ifdef __EMSCRIPTEN__
    /**
     * @brief Download bytes through the browser.
     * @return Non-zero when the browser download was started.
     */
    extern int clientPlatformDownloadFile(
        const char* filename,
        const unsigned char* data,
        size_t dataSize,
        const char* mimeType);
#endif
    /* ---- System tray (status area) integration ---------------------------------------------- */
    /**
     * @brief Ambient sound mode as presented in the tray menu. Mirrors MDRNoiseMode for V2 devices;
     * the client translates for V1 devices where "Noise Cancelling" is an ambient_level of -1.
     */
    enum ClientTrayNoiseMode
    {
        CLIENT_TRAY_NOISE_UNAVAILABLE = -1,
        CLIENT_TRAY_NOISE_OFF = 0,
        CLIENT_TRAY_NOISE_CANCELLING = 1,
        CLIENT_TRAY_NOISE_AMBIENT = 2,
    };
    /**
     * @brief Snapshot of what the tray should display. Strings are copied by the callee.
     */
    typedef struct ClientTrayStatus
    {
        int connected;                  // Non-zero when a device is connected
        const char* deviceName;         // UTF-8 model name, may be NULL
        int batteryPercent;             // 0-100, or -1 when unknown
        int charging;                   // Non-zero when the reported battery is charging
        int noiseMode;                  // ClientTrayNoiseMode
        int noiseCancellingAvailable;   // Non-zero when the device supports Noise Cancelling
        int ambientSoundAvailable;      // Non-zero when the device supports Ambient Sound
    } ClientTrayStatus;
    enum ClientTrayAction
    {
        CLIENT_TRAY_ACTION_NONE = 0,
        CLIENT_TRAY_ACTION_SHOW_WINDOW,
        CLIENT_TRAY_ACTION_EXIT,
        CLIENT_TRAY_ACTION_SET_NOISE_MODE,
    };
    typedef struct ClientTrayEvent
    {
        int action;     // ClientTrayAction
        int noiseMode;  // ClientTrayNoiseMode, valid for CLIENT_TRAY_ACTION_SET_NOISE_MODE
    } ClientTrayEvent;
    /**
     * @brief Create the tray icon. Must be called from the main (UI) thread after the window exists.
     * @return Non-zero when a tray icon is available on this platform and was created.
     */
    extern int clientPlatformTrayInit(void);
    /**
     * @brief Push the latest device status to the tray. Cheap to call every frame; only changes are applied.
     */
    extern void clientPlatformTrayUpdate(const ClientTrayStatus* status);
    /**
     * @brief Pop one pending user action from the tray (menu choice, click).
     * Tray callbacks are delivered on the main thread while the platform event queue is pumped,
     * so this must be polled from the same thread that runs the UI loop.
     * @return Non-zero when @p outEvent was filled.
     */
    extern int clientPlatformTrayPollEvent(ClientTrayEvent* outEvent);
    /**
     * @brief Remove the tray icon and release its resources. Safe to call when never initialized.
     */
    extern void clientPlatformTrayDestroy(void);
    /**
     * @brief Show a transient notification anchored to the tray icon (no-op where unsupported).
     */
    extern void clientPlatformTrayNotify(const char* title, const char* message);

    /* ---- Launch at login ---------------------------------------------------------------------- */
    /** @return Non-zero when this platform can register the app to start at login. */
    extern int clientPlatformAutoStartSupported(void);
    /** @return Non-zero when the app is currently registered to start at login. */
    extern int clientPlatformAutoStartGet(void);
    /**
     * @brief Register (or unregister) the current executable to start at login, minimized.
     * Re-registering refreshes the stored path, so call it at startup when the setting is on.
     * @return Non-zero on success.
     */
    extern int clientPlatformAutoStartSet(int enabled);

    /**
     * @brief Master clean up function.
     * This will destroy all connections, and ensures the client is quit without leaking resources.
     */
    extern void clientPlatformDestroy();
}
