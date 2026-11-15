// Persistent settings, stored in NVS (Preferences).
#pragma once

#include <functional>
#include <stdint.h>

struct Settings {
    char wifiSsid[33];
    char wifiPass[65];
    char hostname[33];
    uint32_t colorBackground;
    uint32_t colorEye;
    uint8_t brightness;        // percent
    bool displayOn;
    uint16_t enabledMask;      // one bit per expression
    bool randomMode;
    uint16_t randomIntervalS;
};

namespace settings {

// Loads the settings, falling back to defaults for everything not stored yet.
void begin();

// Returns a copy of the current settings. Safe to call from any task.
Settings get();

// Changes settings. The change is written to flash a moment later, so quick
// successive changes (sliders) cause only one write.
void update(const std::function<void(Settings&)>& change);

// Writes pending changes. Call regularly from the main loop.
void loop(uint32_t nowMs);

// Writes pending changes immediately (before a reboot).
void flush();

// Erases everything, including the WiFi credentials.
void factoryReset();

// True if the name is usable as hostname: a-z, 0-9 and '-', 1 to 32 chars.
bool validHostname(const char* name);

}  // namespace settings
