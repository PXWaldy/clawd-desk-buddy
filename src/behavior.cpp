#include "behavior.h"

#include <Arduino.h>

#include <atomic>

#include "config.h"
#include "display.h"
#include "eyes.h"

namespace {

QueueHandle_t queue = nullptr;

Expression base = Expression::Normal;   // the style to return to after a reaction
uint16_t enabled = ALL_EXPRESSIONS_MASK;
bool randomOn = true;
uint32_t randomIntervalSeconds = cfg::RANDOM_INTERVAL_DEFAULT_S;
uint32_t nextRandomMs = 0;
uint32_t reactionUntilMs = 0;           // 0 = no tap reaction running
uint32_t colorBackground = cfg::COLOR_BACKGROUND;
uint32_t colorEye = cfg::COLOR_EYE;
bool screenOn = true;                   // display switch in the web UI

// Sleeping: the schedule or the display switch send the buddy to sleep. A tap
// wakes it for a while.
enum class Power : uint8_t { Awake, FallingAsleep, Asleep, Waking };
Power power = Power::Awake;
uint32_t powerSinceMs = 0;
bool wakeSurprisedShown = false;
bool scheduleSleep = false;
uint32_t stayAwakeUntilMs = 0;          // 0 = no temporary wake up running
uint32_t wakeSeconds = cfg::WAKE_SECONDS_DEFAULT;
std::atomic<uint32_t> stateVersion{0};

bool isEnabled(Expression e) { return enabled & (1u << (uint8_t)e); }

void scheduleRandom(uint32_t now) {
    nextRandomMs = now + randomIntervalSeconds * 1000;
}

void showBase(uint32_t now) {
    reactionUntilMs = 0;
    eyes::setExpression(base);
    scheduleRandom(now);
    stateVersion++;
}

// The next enabled expression after `from`, in list order.
Expression nextEnabled(Expression from) {
    for (uint8_t step = 1; step <= EXPRESSION_COUNT; step++) {
        const Expression e = (Expression)(((uint8_t)from + step) % EXPRESSION_COUNT);
        if (isEnabled(e)) return e;
    }
    return from;
}

Expression randomEnabled(Expression notThis) {
    uint8_t candidates[EXPRESSION_COUNT];
    uint8_t n = 0;
    for (uint8_t i = 0; i < EXPRESSION_COUNT; i++) {
        if (isEnabled((Expression)i) && (Expression)i != notThis) candidates[n++] = i;
    }
    return n ? (Expression)candidates[random(n)] : notThis;
}

bool wantAsleep(uint32_t now) {
    if (stayAwakeUntilMs != 0) {
        if ((int32_t)(now - stayAwakeUntilMs) < 0) return false;
        stayAwakeUntilMs = 0;
    }
    return scheduleSleep || !screenOn;
}

// Any interaction keeps the buddy awake for a while if it should be asleep.
void stayAwake(uint32_t now) {
    if (!scheduleSleep && screenOn) return;
    stayAwakeUntilMs = now + wakeSeconds * 1000;
    if (stayAwakeUntilMs == 0) stayAwakeUntilMs = 1;
}

void setPower(Power next, uint32_t now) {
    power = next;
    powerSinceMs = now;
    stateVersion++;
}

void updatePower(uint32_t now) {
    const bool sleepy = wantAsleep(now);
    switch (power) {
    case Power::Awake:
        if (sleepy) {
            // Going to sleep: sleepy eyes for a moment, then lights off.
            reactionUntilMs = 0;
            eyes::setExpression(Expression::Sleepy);
            setPower(Power::FallingAsleep, now);
        }
        break;
    case Power::FallingAsleep:
        if (!sleepy) {
            showBase(now);
            setPower(Power::Awake, now);
        } else if (now - powerSinceMs >= cfg::SLEEP_ANIMATION_MS) {
            setPower(Power::Asleep, now);
        }
        break;
    case Power::Asleep:
        if (!sleepy) {
            // Waking up: sleepy eyes, then wide open, then back to normal.
            eyes::setExpression(Expression::Sleepy);
            wakeSurprisedShown = false;
            setPower(Power::Waking, now);
        }
        break;
    case Power::Waking: {
        const uint32_t t = now - powerSinceMs;
        if (!wakeSurprisedShown && t >= cfg::WAKE_SLEEPY_MS) {
            wakeSurprisedShown = true;
            eyes::setExpression(Expression::Surprised);
        }
        if (t >= cfg::WAKE_SLEEPY_MS + cfg::WAKE_SURPRISED_MS) {
            showBase(now);
            setPower(Power::Awake, now);
        }
        break;
    }
    }
}

void onTap(uint32_t now) {
    stayAwake(now);
    if (power != Power::Awake) return;   // this tap only wakes the buddy up
    if (reactionUntilMs != 0) {
        // Tapped again while still happy: go to the next style.
        base = nextEnabled(base);
        showBase(now);
        return;
    }
    // First tap: look happy for a moment, then return to the previous style.
    reactionUntilMs = now + cfg::HAPPY_REACTION_MS;
    if (reactionUntilMs == 0) reactionUntilMs = 1;
    if (isEnabled(Expression::Happy)) {
        eyes::setExpression(Expression::Happy);
    } else {
        eyes::setExpression(Expression::Normal);
        eyes::triggerWiggle();
    }
}

void handle(const behavior::Event& ev, uint32_t now) {
    using behavior::EventType;
    switch (ev.type) {
    case EventType::Tap:
        onTap(now);
        break;
    case EventType::SetExpression:
        if (ev.value >= 0 && ev.value < EXPRESSION_COUNT) {
            randomOn = false;
            base = (Expression)ev.value;
            stayAwake(now);
            if (power == Power::Awake) showBase(now);
        }
        break;
    case EventType::SetRandom:
        randomOn = ev.value != 0;
        scheduleRandom(now);
        if (randomOn) stayAwake(now);
        stateVersion++;
        break;
    case EventType::SetEnabledMask: {
        const uint16_t mask = (uint16_t)ev.value & ALL_EXPRESSIONS_MASK;
        if (mask == 0) break;               // at least one must stay enabled
        enabled = mask;
        if (!isEnabled(base)) {
            base = nextEnabled(base);
            if (reactionUntilMs == 0 && power == Power::Awake) showBase(now);
        }
        break;
    }
    case EventType::SetBackground:
        colorBackground = (uint32_t)ev.value & 0xFFFFFF;
        eyes::setColors(colorBackground, colorEye);
        break;
    case EventType::SetEyeColor:
        colorEye = (uint32_t)ev.value & 0xFFFFFF;
        eyes::setColors(colorBackground, colorEye);
        break;
    case EventType::SetBrightness:
        display::setBrightness((uint8_t)constrain(ev.value, 0, 100));
        break;
    case EventType::SetDisplayOn:
        screenOn = ev.value != 0;
        stayAwakeUntilMs = 0;
        break;
    case EventType::SetSleepWanted:
        scheduleSleep = ev.value != 0;
        if (!scheduleSleep) stayAwakeUntilMs = 0;
        break;
    case EventType::SetWakeSeconds:
        wakeSeconds = constrain((uint32_t)ev.value, (uint32_t)cfg::WAKE_SECONDS_MIN,
                                (uint32_t)cfg::WAKE_SECONDS_MAX);
        break;
    case EventType::SetRandomInterval:
        randomIntervalSeconds = constrain((uint32_t)ev.value, cfg::RANDOM_INTERVAL_MIN_S,
                                          cfg::RANDOM_INTERVAL_MAX_S);
        scheduleRandom(now);
        break;
    }
}

}  // namespace

namespace behavior {

void begin() {
    queue = xQueueCreate(16, sizeof(Event));
    scheduleRandom(millis());
}

void post(EventType type, int32_t value) {
    if (queue == nullptr) return;
    const Event ev = {type, value};
    xQueueSend(queue, &ev, 0);
}

void update(uint32_t now) {
    Event ev;
    while (xQueueReceive(queue, &ev, 0) == pdTRUE) handle(ev, now);

    updatePower(now);
    if (power != Power::Awake) return;

    if (reactionUntilMs != 0 && (int32_t)(now - reactionUntilMs) >= 0) showBase(now);

    if (randomOn && reactionUntilMs == 0 && (int32_t)(now - nextRandomMs) >= 0) {
        base = randomEnabled(base);
        showBase(now);
    }
}

Expression baseExpression() { return base; }
bool randomMode() { return randomOn; }
uint16_t enabledMask() { return enabled; }
uint32_t randomIntervalS() { return randomIntervalSeconds; }
bool screenLit() { return power != Power::Asleep; }
uint32_t version() { return stateVersion; }

}  // namespace behavior
