// Clawd Desk Buddy
// A small desk companion with animated pixel eyes.
//
// Fan project, not affiliated with Anthropic.

#include <Arduino.h>

#include "behavior.h"
#include "config.h"
#include "display.h"
#include "eyes.h"
#include "hint.h"
#include "touch.h"

namespace {

// Shows what a long press will do while the finger is still down.
void updateHoldHint(uint32_t now) {
    static bool holdHintShown = false;
    const uint32_t held = touch::holdMs(now);
    if (held >= cfg::HOLD_WIFI_MS && held < cfg::HOLD_FACTORY_MS) {
        char line[28];
        const uint32_t secondsLeft = (cfg::HOLD_FACTORY_MS - held + 999) / 1000;
        snprintf(line, sizeof(line), "reset in %u s", (unsigned)secondsLeft);
        hint::show("Release: WiFi setup", "Keep holding: factory", line, 0);
        holdHintShown = true;
    } else if (holdHintShown && held == 0) {
        holdHintShown = false;
        hint::clear();
    }
}

void handleGesture(touch::Gesture gesture) {
    switch (gesture) {
    case touch::Gesture::Tap:
        behavior::post(behavior::EventType::Tap);
        break;
    case touch::Gesture::WifiSetup:
        // WiFi setup itself follows with the network milestone.
        Serial.println("Gesture: WiFi setup");
        hint::show("WiFi setup", "", "(coming soon)", 2500);
        break;
    case touch::Gesture::FactoryReset:
        // The reset itself follows with the settings milestone.
        Serial.println("Gesture: factory reset");
        hint::show("Factory reset", "", "(coming soon)", 2500);
        break;
    default:
        break;
    }
}

// The animation runs in its own task so it stays smooth no matter what the
// rest of the firmware (WiFi, web server) is doing.
void animationTask(void*) {
    bool firstFrame = true;
    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        const uint32_t now = millis();

        const touch::Gesture gesture = touch::poll(now);
        updateHoldHint(now);
        handleGesture(gesture);
        behavior::update(now);

        if (hint::active(now)) hint::draw();
        else eyes::draw(now);

        if (display::present() && firstFrame) {
            firstFrame = false;
            display::setBacklight(true);
        }
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(cfg::FRAME_MS));
    }
}

// Small serial console for testing: 0-9 selects an expression, r switches
// random mode on, t simulates a tap.
void handleSerialConsole() {
    while (Serial.available()) {
        const int ch = Serial.read();
        if (ch >= '0' && ch <= '9') {
            behavior::post(behavior::EventType::SetExpression, ch - '0');
            Serial.printf("Expression: %s\n", expressionName((Expression)(ch - '0')));
        } else if (ch == 'r') {
            behavior::post(behavior::EventType::SetRandom, 1);
            Serial.println("Random mode on");
        } else if (ch == 't') {
            behavior::post(behavior::EventType::Tap);
            Serial.println("Tap");
        }
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    Serial.println("Clawd Desk Buddy starting");
    randomSeed(esp_random());

    display::begin();
    display::setBrightness(cfg::DEFAULT_BRIGHTNESS);
    eyes::begin();
    behavior::begin();
    if (!touch::begin()) Serial.println("Touch controller not found");

    xTaskCreate(animationTask, "anim", 8192, nullptr, 3, nullptr);
}

void loop() {
    handleSerialConsole();
    delay(20);
}
