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
    SetBackground,     // value = RGB888
    SetEyeColor,       // value = RGB888
    SetBrightness,     // value = percent
    SetDisplayOn,      // value = 0 / 1
    SetSleepWanted,    // value = 0 / 1, from the sleep schedule
    SetWakeSeconds,    // value = seconds to stay awake after a tap while asleep
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
uint32_t enabledMask();
uint32_t randomIntervalS();

// False while the buddy sleeps (schedule or display switched off). The
// backlight follows this.
bool screenLit();

// Increases whenever the visible state (expression, random mode) changes, so
// the web UI knows when to refresh.
uint32_t version();

}  // namespace behavior
