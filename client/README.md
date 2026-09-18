Client
===
Reference client interface implementation for every platform `libmdr` supports.

## Command-line options

```text
SonyHeadphonesClient [-con] [--record <capture-folder>]
SonyHeadphonesClient [-con] [--replay <packet-file-or-folder>]
```

- `-con` opens a console on Windows so diagnostic logs are visible, including in release builds.
- `--record <capture-folder>` records MDR packets for replay tests. See the
  [capture guide](../tests/README.md#capturing--contributing) for privacy considerations.
- `--replay <packet-file-or-folder>` opens the protocol debugger without initializing a
  headphones connection. It loads either one `.bin` MDR packet or a folder's recorded
  TX/RX packets in filename order. Either can also be dropped onto a running client window.

Replay mode exposes only the protocol debugger. It is available in Debug builds and
in other configurations built with `-DMDR_CLIENT_DEBUGGER=ON`; clients built without
the debugger reject replay arguments. Debugger exports use native save dialogs on
desktop platforms with `.bin` and `.zip` default filenames. A ZIP export stores the
complete packet history without compression. Emscripten exports start a browser
download directly. On other platforms where dialogs are unavailable, exports fall
back to their default filenames in the current directory.

## Credits
The following third-party libraries are used in the implementation.

- https://github.com/fmtlib/fmt

- https://github.com/ocornut/imgui
- https://github.com/libsdl-org/SDL

The custom font `PlexSansIcon` is created with the following source fonts.

- https://github.com/IBM/plex

- https://github.com/FortAwesome/Font-Awesome

...with the help of FontForge.

- https://fontforge.org/

The font `NeoXiHei-Code` is graciously provided by @lxgw, and is the default font for non Latin-1 or icon characters in the Web client.

- https://github.com/lxgw/NeoXiHei-Code

## Interface and connection feedback

The client uses neutral light surfaces, rounded device cards and a consistent blue accent.
The connected device header is compact to leave more room for playback and sound controls.
The precomputed Material You palette table is retained for future theme options; the light
interface does not apply its dark accent values.

App Settings includes persistent **Interface animations** and **Connection notifications**
preferences. The device illustration floats gently and displays an animated arc while a
connection is pending. Disabling animations keeps these new visuals static.

Notifications follow actual device readiness and connection transitions. A focused window
shows a four-second confirmation card; otherwise the existing platform tray notification
API is used (Windows respects the system's quiet-time setting). Intentional disconnects
are silent. Failed attempts are limited to one alert per 30 seconds. Low headphone battery
alerts trigger at 20% and rearm after charging or recovery above 25%; case battery is excluded.
Unsupported platforms retain foreground cards but may not display background notifications.

### Manual verification

- Connect a supported headset and check that confirmation appears only after initialization.
- Repeat with the client in the background and verify one system notification.
- Cancel an attempt, disconnect deliberately, and simulate an unexpected link loss.
- Check a low-battery headset, charging, and repeated updates for duplicate alerts.
- Disable animations/notifications, restart, and check that preferences persist.
- Inspect long track titles, narrow windows, DPI scaling, and both interface languages.
