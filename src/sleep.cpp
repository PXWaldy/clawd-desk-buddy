#include "sleep.h"

#include <time.h>

#include "behavior.h"
#include "settings.h"

namespace {

uint32_t lastCheckMs = 0;
bool sleepWanted = false;
char appliedTimezone[48] = "";

bool readLocalTime(struct tm& out) {
    const time_t now = time(nullptr);
    if (now < 1700000000) return false;   // clock not set yet (before 2023)
    localtime_r(&now, &out);
    return true;
}

// True if `minute` lies in the window. A window like 22:00 to 07:00 wraps
// around midnight.
bool inWindow(uint16_t minute, uint16_t from, uint16_t to) {
    if (from == to) return false;
    if (from < to) return minute >= from && minute < to;
    return minute >= from || minute < to;
}

}  // namespace

namespace sleep_schedule {

void loop(uint32_t now) {
    if (now - lastCheckMs < 1000) return;
    lastCheckMs = now;

    const Settings s = settings::get();

    // Follow timezone changes from the web UI.
    if (strcmp(appliedTimezone, s.timezonePosix) != 0) {
        strlcpy(appliedTimezone, s.timezonePosix, sizeof(appliedTimezone));
        setenv("TZ", appliedTimezone, 1);
        tzset();
    }

    bool wanted = false;
    struct tm local;
    if (s.sleepEnabled && readLocalTime(local)) {
        wanted = inWindow(local.tm_hour * 60 + local.tm_min, s.sleepFromMin, s.sleepToMin);
    }
    if (wanted != sleepWanted) {
        sleepWanted = wanted;
        behavior::post(behavior::EventType::SetSleepWanted, wanted);
    }
}

bool timeValid() {
    struct tm local;
    return readLocalTime(local);
}

String localTime() {
    struct tm local;
    if (!readLocalTime(local)) return String();
    char text[6];
    snprintf(text, sizeof(text), "%02d:%02d", local.tm_hour, local.tm_min);
    return text;
}

}  // namespace sleep_schedule
