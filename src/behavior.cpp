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
bool screenOn = true;
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

void onTap(uint32_t now) {
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
            showBase(now);
        }
        break;
    case EventType::SetRandom:
        randomOn = ev.value != 0;
        scheduleRandom(now);
        stateVersion++;
        break;
    case EventType::SetEnabledMask: {
        const uint16_t mask = (uint16_t)ev.value & ALL_EXPRESSIONS_MASK;
        if (mask == 0) break;               // at least one must stay enabled
        enabled = mask;
        if (!isEnabled(base)) {
            base = nextEnabled(base);
            if (reactionUntilMs == 0) showBase(now);
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
        display::setBacklight(screenOn);
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
bool displayOn() { return screenOn; }
uint32_t version() { return stateVersion; }

}  // namespace behavior
