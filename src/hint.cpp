#include "hint.h"

#include <Arduino.h>

#include "config.h"
#include "display.h"

namespace {

constexpr int LINE_LENGTH = 27;   // characters that fit on one line
char lines[3][LINE_LENGTH + 1] = {};
bool shown = false;
uint32_t untilMs = 0;             // 0 = no timeout

// Hints are shown from several tasks (network, touch) and drawn by the
// animation task.
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

}  // namespace

namespace hint {

void show(const char* line1, const char* line2, const char* line3, uint32_t durationMs) {
    uint32_t until = durationMs ? millis() + durationMs : 0;
    if (durationMs && until == 0) until = 1;
    portENTER_CRITICAL(&mux);
    strlcpy(lines[0], line1, sizeof(lines[0]));
    strlcpy(lines[1], line2, sizeof(lines[1]));
    strlcpy(lines[2], line3, sizeof(lines[2]));
    shown = true;
    untilMs = until;
    portEXIT_CRITICAL(&mux);
}

void clear() { shown = false; }

bool active(uint32_t now) {
    if (shown && untilMs != 0 && (int32_t)(now - untilMs) >= 0) shown = false;
    return shown;
}

void draw() {
    auto& c = display::canvas();
    c.fillScreen(cfg::PAL_BACKGROUND);
    c.setFont(&fonts::Font0);          // blocky 6 x 8 font, scaled up
    c.setTextSize(2);
    c.setTextDatum(textdatum_t::middle_center);
    c.setTextColor(cfg::PAL_TEXT);
    char text[3][LINE_LENGTH + 1];
    portENTER_CRITICAL(&mux);
    memcpy(text, lines, sizeof(text));
    portEXIT_CRITICAL(&mux);
    const int lineHeight = 30;
    for (int i = 0; i < 3; i++) {
        c.drawString(text[i], cfg::SCREEN_W / 2,
                     cfg::SCREEN_H / 2 + (i - 1) * lineHeight);
    }
}

}  // namespace hint
