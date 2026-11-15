// Web server: serves the web UI, the REST API and a WebSocket for live control.
#pragma once

#include <stdint.h>

namespace web {

void begin();

// Housekeeping and state broadcasts. Call from the main loop.
void loop(uint32_t nowMs);

// True once a reboot was requested through the API.
bool rebootRequested();

}  // namespace web
