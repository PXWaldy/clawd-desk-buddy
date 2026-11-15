// Short text messages that temporarily replace the eyes, e.g. the WiFi setup
// hint or the IP address.
#pragma once

#include <stdint.h>

namespace hint {

// Shows up to three centered lines. durationMs = 0 keeps the hint until
// clear() is called. Empty lines can be passed as "".
void show(const char* line1, const char* line2, const char* line3, uint32_t durationMs);
void clear();

bool active(uint32_t nowMs);

// Draws the hint into the framebuffer.
void draw();

}  // namespace hint
