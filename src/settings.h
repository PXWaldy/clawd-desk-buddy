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

    // Sleep schedule (minutes since midnight, local time)
    bool sleepEnabled;
    uint16_t sleepFromMin;
    uint16_t sleepToMin;
    uint16_t wakeSeconds;      // tap while asleep: stay awake this long

    char timezoneName[40];     // e.g. "Europe/Berlin", shown in the web UI
    char timezonePosix[48];    // matching POSIX TZ rule, used by the clock

    // Optional password for the web UI. Only a salted hash is stored.
    bool passwordEnabled;
    uint8_t passwordSalt[16];
    uint8_t passwordHash[32];
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
