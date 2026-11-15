#include "touch.h"

#include <Arduino.h>
#include <Wire.h>

#include "config.h"

namespace {

bool present = false;
bool pressed = false;
bool factoryFired = false;
uint32_t pressStartMs = 0;
uint32_t lastContactMs = 0;

// Reads the number of fingers on the screen. Returns -1 on a bus error.
int readTouchCount() {
    Wire.beginTransmission(cfg::TOUCH_I2C_ADDR);
    Wire.write(cfg::TOUCH_REG_DATA);
    if (Wire.endTransmission() != 0) return -1;

    // Layout of the data block: [0] gesture, [1] number of points, then
    // 6 bytes per point. Only the first point is requested.
    const uint8_t length = 8;
    if (Wire.requestFrom(cfg::TOUCH_I2C_ADDR, length) != length) return -1;
    uint8_t data[length];
    Wire.readBytes(data, length);

    const uint8_t count = data[1];
    return count <= 5 ? count : 0;
}

}  // namespace

namespace touch {

bool begin() {
    pinMode(cfg::PIN_TOUCH_INT, INPUT_PULLUP);
    pinMode(cfg::PIN_TOUCH_RST, OUTPUT);
    digitalWrite(cfg::PIN_TOUCH_RST, LOW);
    delay(200);
    digitalWrite(cfg::PIN_TOUCH_RST, HIGH);
    delay(300);

    Wire.begin(cfg::PIN_I2C_SDA, cfg::PIN_I2C_SCL, cfg::I2C_HZ);
    Wire.beginTransmission(cfg::TOUCH_I2C_ADDR);
    present = Wire.endTransmission() == 0;
    return present;
}

Gesture poll(uint32_t now) {
    if (!present) return Gesture::None;

    const int count = readTouchCount();
    if (count > 0) {
        lastContactMs = now;
        if (!pressed) {
            pressed = true;
            factoryFired = false;
            pressStartMs = now;
        }
        if (!factoryFired && now - pressStartMs >= cfg::HOLD_FACTORY_MS) {
            factoryFired = true;
            return Gesture::FactoryReset;
        }
        return Gesture::None;
    }

    // No contact (or a read error). Wait a moment before treating it as a
    // release, the controller sometimes drops a single report.
    if (pressed && now - lastContactMs >= cfg::TOUCH_RELEASE_MS) {
        pressed = false;
        const uint32_t held = lastContactMs - pressStartMs;
        if (factoryFired) return Gesture::None;
        if (held >= cfg::HOLD_WIFI_MS) return Gesture::WifiSetup;
        if (held <= cfg::TAP_MAX_MS) return Gesture::Tap;
    }
    return Gesture::None;
}

uint32_t holdMs(uint32_t now) {
    return pressed ? now - pressStartMs : 0;
}

}  // namespace touch
