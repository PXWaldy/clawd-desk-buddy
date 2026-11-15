#include "settings.h"

#include <Arduino.h>
#include <Preferences.h>

#include "config.h"
#include "expressions.h"

namespace {

constexpr char NVS_NAMESPACE[] = "clawd";

Settings current;
SemaphoreHandle_t lock = nullptr;
bool dirty = false;
uint32_t dirtySinceMs = 0;

void setDefaults(Settings& s) {
    memset(&s, 0, sizeof(s));
    strlcpy(s.hostname, cfg::DEFAULT_HOSTNAME, sizeof(s.hostname));
    s.colorBackground = cfg::COLOR_BACKGROUND;
    s.colorEye = cfg::COLOR_EYE;
    s.brightness = cfg::DEFAULT_BRIGHTNESS;
    s.displayOn = true;
    s.enabledMask = ALL_EXPRESSIONS_MASK;
    s.randomMode = true;
    s.randomIntervalS = cfg::RANDOM_INTERVAL_DEFAULT_S;
    s.sleepEnabled = false;
    s.sleepFromMin = cfg::SLEEP_FROM_DEFAULT_MIN;
    s.sleepToMin = cfg::SLEEP_TO_DEFAULT_MIN;
    s.wakeSeconds = cfg::WAKE_SECONDS_DEFAULT;
    strlcpy(s.timezoneName, cfg::DEFAULT_TIMEZONE_NAME, sizeof(s.timezoneName));
    strlcpy(s.timezonePosix, cfg::DEFAULT_TIMEZONE, sizeof(s.timezonePosix));
    s.passwordEnabled = false;
}

void write(const Settings& s) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) return;
    prefs.putString("ssid", s.wifiSsid);
    prefs.putString("pass", s.wifiPass);
    prefs.putString("host", s.hostname);
    prefs.putUInt("bg", s.colorBackground);
    prefs.putUInt("eye", s.colorEye);
    prefs.putUChar("bright", s.brightness);
    prefs.putBool("dispOn", s.displayOn);
    prefs.putUShort("enabled", s.enabledMask);
    prefs.putBool("random", s.randomMode);
    prefs.putUShort("interval", s.randomIntervalS);
    prefs.putBool("sleepOn", s.sleepEnabled);
    prefs.putUShort("sleepFrom", s.sleepFromMin);
    prefs.putUShort("sleepTo", s.sleepToMin);
    prefs.putUShort("wakeSec", s.wakeSeconds);
    prefs.putString("tzName", s.timezoneName);
    prefs.putString("tzPosix", s.timezonePosix);
    prefs.putBool("pwOn", s.passwordEnabled);
    prefs.putBytes("pwSalt", s.passwordSalt, sizeof(s.passwordSalt));
    prefs.putBytes("pwHash", s.passwordHash, sizeof(s.passwordHash));
    prefs.end();
}

}  // namespace

namespace settings {

void begin() {
    lock = xSemaphoreCreateMutex();
    setDefaults(current);

    Preferences prefs;
    // Opened read/write so the namespace is created on the very first boot.
    if (!prefs.begin(NVS_NAMESPACE, false)) return;
    if (prefs.isKey("ssid")) prefs.getString("ssid", current.wifiSsid, sizeof(current.wifiSsid));
    if (prefs.isKey("pass")) prefs.getString("pass", current.wifiPass, sizeof(current.wifiPass));
    if (prefs.isKey("host")) prefs.getString("host", current.hostname, sizeof(current.hostname));
    current.colorBackground = prefs.getUInt("bg", current.colorBackground);
    current.colorEye = prefs.getUInt("eye", current.colorEye);
    current.brightness = prefs.getUChar("bright", current.brightness);
    current.displayOn = prefs.getBool("dispOn", current.displayOn);
    current.enabledMask = prefs.getUShort("enabled", current.enabledMask) & ALL_EXPRESSIONS_MASK;
    current.randomMode = prefs.getBool("random", current.randomMode);
    current.randomIntervalS = prefs.getUShort("interval", current.randomIntervalS);
    current.sleepEnabled = prefs.getBool("sleepOn", current.sleepEnabled);
    current.sleepFromMin = prefs.getUShort("sleepFrom", current.sleepFromMin) % (24 * 60);
    current.sleepToMin = prefs.getUShort("sleepTo", current.sleepToMin) % (24 * 60);
    current.wakeSeconds = prefs.getUShort("wakeSec", current.wakeSeconds);
    if (prefs.isKey("tzName") && prefs.isKey("tzPosix")) {
        prefs.getString("tzName", current.timezoneName, sizeof(current.timezoneName));
        prefs.getString("tzPosix", current.timezonePosix, sizeof(current.timezonePosix));
    }
    if (prefs.isKey("pwSalt") && prefs.isKey("pwHash")) {
        prefs.getBytes("pwSalt", current.passwordSalt, sizeof(current.passwordSalt));
        prefs.getBytes("pwHash", current.passwordHash, sizeof(current.passwordHash));
        current.passwordEnabled = prefs.getBool("pwOn", false);
    }
    prefs.end();

    if (current.enabledMask == 0) current.enabledMask = ALL_EXPRESSIONS_MASK;
    if (!validHostname(current.hostname)) {
        strlcpy(current.hostname, cfg::DEFAULT_HOSTNAME, sizeof(current.hostname));
    }
}

Settings get() {
    xSemaphoreTake(lock, portMAX_DELAY);
    const Settings copy = current;
    xSemaphoreGive(lock);
    return copy;
}

void update(const std::function<void(Settings&)>& change) {
    xSemaphoreTake(lock, portMAX_DELAY);
    change(current);
    if (!dirty) {
        dirty = true;
        dirtySinceMs = millis();
    }
    xSemaphoreGive(lock);
}

void loop(uint32_t now) {
    if (dirty && now - dirtySinceMs >= cfg::SETTINGS_SAVE_DELAY_MS) flush();
}

void flush() {
    xSemaphoreTake(lock, portMAX_DELAY);
    const bool wasDirty = dirty;
    const Settings copy = current;
    dirty = false;
    xSemaphoreGive(lock);
    if (wasDirty) write(copy);
}

void factoryReset() {
    Preferences prefs;
    if (prefs.begin(NVS_NAMESPACE, false)) {
        prefs.clear();
        prefs.end();
    }
    xSemaphoreTake(lock, portMAX_DELAY);
    setDefaults(current);
    dirty = false;
    xSemaphoreGive(lock);
}

bool validHostname(const char* name) {
    const size_t length = strlen(name);
    if (length < 1 || length > 32) return false;
    if (name[0] == '-' || name[length - 1] == '-') return false;
    for (size_t i = 0; i < length; i++) {
        const char ch = name[i];
        const bool ok = (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-';
        if (!ok) return false;
    }
    return true;
}

}  // namespace settings
