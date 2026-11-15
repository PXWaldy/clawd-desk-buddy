// Sleep schedule: sends the buddy to sleep during the configured hours, based
// on NTP time and the configured timezone.
#pragma once

#include <Arduino.h>

namespace sleep_schedule {

// Checks the clock about once per second. Call from the main loop.
void loop(uint32_t nowMs);

// True once the clock was set by NTP.
bool timeValid();

// Local time as "HH:MM", or an empty string while the clock is not set.
String localTime();

}  // namespace sleep_schedule
