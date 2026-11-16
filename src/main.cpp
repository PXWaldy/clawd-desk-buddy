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
#include "improv_serial.h"
#include "net.h"
#include "settings.h"
#include "sleep.h"
#include "touch.h"
#include "web.h"

namespace {

volatile bool factoryResetRequested = false;

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
        net::requestSetup();
        break;
    case touch::Gesture::FactoryReset:
        hint::show("Factory reset", "", "Restarting...", 0);
        factoryResetRequested = true;
        break;
    default:
        break;
    }
}

// The animation runs in its own task so it stays smooth no matter what the
// rest of the firmware (WiFi, web server) is doing.
void animationTask(void*) {
    bool backlightOn = false;
    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        const uint32_t now = millis();

        const touch::Gesture gesture = touch::poll(now);
        updateHoldHint(now);
        handleGesture(gesture);
        behavior::update(now);

        // While asleep nothing is drawn and the backlight is off. A hint
        // (long press, WiFi setup) still lights the screen.
        const bool hintShown = hint::active(now);
        const bool lit = hintShown || behavior::screenLit();
        if (lit) {
            if (hintShown) hint::draw();
            else eyes::draw(now);
            display::present();
        }
        if (lit != backlightOn) {
            backlightOn = lit;
            display::setBacklight(lit);
        }
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(cfg::FRAME_MS));
    }
}

// The serial port carries Improv packets (WiFi setup from the web flasher)
// and a small console for testing: 0-9 and A-N select an expression, r
// switches random mode on, t simulates a tap.
void handleSerial() {
    while (Serial.available()) {
        const int ch = Serial.read();
        if (improv::handleByte((uint8_t)ch)) continue;
        int index = -1;
        if (ch >= '0' && ch <= '9') index = ch - '0';
        else if (ch >= 'A' && ch < 'A' + EXPRESSION_COUNT - 10) index = 10 + ch - 'A';
        if (index >= 0) {
            behavior::post(behavior::EventType::SetExpression, index);
            Serial.printf("Expression: %s\n", expressionName((Expression)index));
        } else if (ch == 'r') {
            behavior::post(behavior::EventType::SetRandom, 1);
            Serial.println("Random mode on");
        } else if (ch == 't') {
            behavior::post(behavior::EventType::Tap);
            Serial.println("Tap");
        }
    }
}

// Hands the stored settings to the modules that use them.
void applySettings() {
    using behavior::EventType;
    const Settings s = settings::get();
    behavior::post(EventType::SetBackground, (int32_t)s.colorBackground);
    behavior::post(EventType::SetEyeColor, (int32_t)s.colorEye);
    behavior::post(EventType::SetBrightness, s.brightness);
    behavior::post(EventType::SetDisplayOn, s.displayOn);
    behavior::post(EventType::SetEnabledMask, s.enabledMask);
    behavior::post(EventType::SetRandomInterval, s.randomIntervalS);
    behavior::post(EventType::SetRandom, s.randomMode);
    behavior::post(EventType::SetWakeSeconds, s.wakeSeconds);
}

void restart() {
    settings::flush();
    delay(600);   // let the last HTTP response and the hint get out
    ESP.restart();
}

}  // namespace

void setup() {
    Serial.begin(115200);
    Serial.printf("%s %s starting\n", cfg::FIRMWARE_NAME, cfg::FIRMWARE_VERSION);
    randomSeed(esp_random());

    settings::begin();
    display::begin();
    eyes::begin();
    behavior::begin();
    applySettings();
    if (!touch::begin()) Serial.println("Touch controller not found");

    xTaskCreate(animationTask, "anim", 8192, nullptr, 3, nullptr);

    net::begin();
    web::begin();
}

void loop() {
    const uint32_t now = millis();
    handleSerial();
    improv::loop();
    net::loop(now);
    web::loop(now);
    settings::loop(now);
    sleep_schedule::loop(now);

    if (factoryResetRequested) {
        settings::factoryReset();
        restart();
    }
    if (web::rebootRequested()) restart();

    delay(10);
}
