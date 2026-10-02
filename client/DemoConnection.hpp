#pragma once
#include <mdr-c/Connection.h>

/**
 * A transport that replays the recorded RX packets of a real session, so the connected UI can run
 * without headphones (`--demo <capture-folder>`). The device it reports uses @p macAddress and
 * @p name; pass the remembered device's address to make auto-connect pick it up.
 * @return The connection, or nullptr when the folder holds no recorded packets.
 */
MDRConnection* clientDemoConnectionCreate(const char* folder, const char* macAddress, const char* name);
