// Improv Serial: lets the web flasher set the WiFi credentials over USB right
// after flashing. Protocol: https://www.improv-wifi.com/serial/
#pragma once

#include <stdint.h>

namespace improv {

// Feeds one byte from the serial port. Returns true if the byte belongs to
// an Improv packet (and must not be interpreted as anything else).
bool handleByte(uint8_t byte);

// Reports the outcome of a running provisioning. Call from the main loop.
void loop();

}  // namespace improv
