// Touch input: AXS5106L capacitive touch controller on I2C.
//
// The firmware only needs to know whether a finger is on the screen, so this
// driver turns the raw data into a few simple gestures.
#pragma once

#include <stdint.h>

namespace touch {

enum class Gesture : uint8_t {
    None,
    Tap,            // short press and release
    WifiSetup,      // released after holding for cfg::HOLD_WIFI_MS
    FactoryReset,   // held for cfg::HOLD_FACTORY_MS (fires while still pressed)
};

// Returns false if the controller does not answer.
bool begin();

// Reads the controller. Call this regularly (every frame).
Gesture poll(uint32_t nowMs);

// How long the finger has been down, 0 if nothing touches the screen.
uint32_t holdMs(uint32_t nowMs);

}  // namespace touch
