#pragma once

// Firmware version, shown on the Device page and the restart screen.
// Raise it with every release.
constexpr const char* FIRMWARE_VERSION = "v1.0.0";

namespace DeviceReset {

// "Reset and unpair": erases everything kept in flash (NVS): the home Wi-Fi,
// calendar link, tokens, widgets, city and habits. After the restart that
// follows, the device starts with the Wi-Fi setup page like a new one.
void eraseAll();

}
