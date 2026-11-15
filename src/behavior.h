// Behaviour: decides which expression is shown.
//
// Everything that wants to influence the buddy (touch, web UI, later maybe
// other inputs) posts an event. The events are handled in one place, inside
// the animation task, so no locking is needed elsewhere.
#pragma once

#include <stdint.h>

#include "expressions.h"

namespace behavior {

enum class EventType : uint8_t {
    Tap,               // the screen was tapped
    SetExpression,     // value = expression index, switches random mode off
    SetRandom,         // value = 0 / 1
    SetEnabledMask,    // value = bit mask of enabled expressions
    SetRandomInterval, // value = seconds
};

struct Event {
    EventType type;
    int32_t value;
};

void begin();

// Thread safe, can be called from any task.
void post(EventType type, int32_t value = 0);

// Handles pending events and timers. Called from the animation task.
void update(uint32_t nowMs);

// Current state (for the web UI)
Expression baseExpression();
bool randomMode();
uint16_t enabledMask();
uint32_t randomIntervalS();

}  // namespace behavior
