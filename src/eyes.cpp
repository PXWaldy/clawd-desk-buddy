#include "eyes.h"

#include <Arduino.h>

#include "config.h"
#include "display.h"

namespace {

struct Rect {
    int16_t x, y, w, h;
    bool operator==(const Rect& o) const {
        return x == o.x && y == o.y && w == o.w && h == o.h;
    }
};

struct Frame {
    Rect left, right;
    bool operator==(const Frame& o) const {
        return left == o.left && right == o.right;
    }
};

// --- animation state -------------------------------------------------------

uint32_t lastMs = 0;

// Gaze: the eyes drift to a random spot now and then.
float gazeX = 0, gazeY = 0;        // current offset in pixels
float gazeTargetX = 0, gazeTargetY = 0;
uint32_t nextGazeMs = 0;

// Wiggle: a quick left/right shake that sometimes replaces a gaze change.
int8_t wiggleStepsLeft = 0;
uint32_t nextWiggleStepMs = 0;

// Blink
uint32_t blinkStartMs = 0;         // 0 = not blinking
uint32_t nextBlinkMs = 0;
bool doubleBlinkPending = false;

Frame lastFrame = {};
bool dirty = true;

// --- helpers ---------------------------------------------------------------

int snap(float v) {
    return (int)lroundf(v / cfg::GRID) * cfg::GRID;
}

uint32_t randomBetween(uint32_t minMs, uint32_t maxMs) {
    return minMs + (uint32_t)random((long)(maxMs - minMs + 1));
}

// How far the lids are closed: 0 = open, 1 = fully closed.
float blinkAmount(uint32_t now) {
    if (blinkStartMs == 0) return 0;
    const uint32_t t = now - blinkStartMs;
    if (t < cfg::BLINK_CLOSE_MS) return (float)t / cfg::BLINK_CLOSE_MS;
    if (t < cfg::BLINK_CLOSE_MS + cfg::BLINK_HOLD_MS) return 1;
    const uint32_t openT = t - cfg::BLINK_CLOSE_MS - cfg::BLINK_HOLD_MS;
    if (openT < cfg::BLINK_OPEN_MS) return 1 - (float)openT / cfg::BLINK_OPEN_MS;
    return 0;
}

void updateBlink(uint32_t now) {
    const uint32_t blinkLength =
        cfg::BLINK_CLOSE_MS + cfg::BLINK_HOLD_MS + cfg::BLINK_OPEN_MS;

    if (blinkStartMs != 0 && now - blinkStartMs >= blinkLength) {
        blinkStartMs = 0;
        if (doubleBlinkPending) {
            doubleBlinkPending = false;
            nextBlinkMs = now + 90;
        } else {
            nextBlinkMs = now + randomBetween(cfg::BLINK_MIN_MS, cfg::BLINK_MAX_MS);
        }
    }
    if (blinkStartMs == 0 && (int32_t)(now - nextBlinkMs) >= 0) {
        blinkStartMs = now;
        // Every now and then blink twice in a row.
        if (!doubleBlinkPending && random(6) == 0) doubleBlinkPending = true;
        else doubleBlinkPending = false;
    }
}

void updateGaze(uint32_t now, float dt) {
    if (wiggleStepsLeft > 0) {
        if ((int32_t)(now - nextWiggleStepMs) >= 0) {
            wiggleStepsLeft--;
            const int direction = (wiggleStepsLeft % 2) ? 1 : -1;
            gazeTargetX = wiggleStepsLeft == 0 ? 0 : direction * cfg::GRID * 2;
            gazeTargetY = 0;
            nextWiggleStepMs = now + 110;
        }
    } else if ((int32_t)(now - nextGazeMs) >= 0) {
        nextGazeMs = now + randomBetween(cfg::GAZE_MIN_MS, cfg::GAZE_MAX_MS);
        const long pick = random(10);
        if (pick < 2) {
            // short wiggle
            wiggleStepsLeft = 5;
            nextWiggleStepMs = now;
        } else if (pick < 5) {
            // look straight ahead again
            gazeTargetX = 0;
            gazeTargetY = 0;
        } else {
            gazeTargetX = (float)random(-cfg::GAZE_MAX_X, cfg::GAZE_MAX_X + 1);
            gazeTargetY = (float)random(-cfg::GAZE_MAX_Y, cfg::GAZE_MAX_Y + 1);
        }
    }

    // Ease towards the target. The result is snapped to the grid when drawing,
    // so the motion stays chunky.
    const float k = min(1.0f, dt * 16.0f);
    gazeX += (gazeTargetX - gazeX) * k;
    gazeY += (gazeTargetY - gazeY) * k;
}

Rect eyeRect(int centerX, float closed) {
    int h = snap(cfg::EYE_H * (1.0f - closed));
    if (h < cfg::GRID * 2) h = cfg::GRID * 2;   // a closed eye is a thin bar
    Rect r;
    r.w = cfg::EYE_W;
    r.h = h;
    r.x = centerX - cfg::EYE_W / 2 + snap(gazeX);
    // Lids close towards a line slightly below the middle of the eye.
    const int closedLineY = cfg::EYE_CENTER_Y + cfg::EYE_H / 6;
    const int openTopY = cfg::EYE_CENTER_Y - cfg::EYE_H / 2;
    const float openness = (float)(h - cfg::GRID * 2) / (cfg::EYE_H - cfg::GRID * 2);
    const int closedTopY = closedLineY - cfg::GRID;
    r.y = snap(closedTopY + (openTopY - closedTopY) * openness) + snap(gazeY);
    return r;
}

void drawFrame(const Frame& f) {
    auto& c = display::canvas();
    c.fillScreen(cfg::PAL_BACKGROUND);
    c.fillRect(f.left.x, f.left.y, f.left.w, f.left.h, cfg::PAL_EYE);
    c.fillRect(f.right.x, f.right.y, f.right.w, f.right.h, cfg::PAL_EYE);
}

}  // namespace

namespace eyes {

void begin() {
    const uint32_t now = millis();
    lastMs = now;
    nextGazeMs = now + 1500;
    nextBlinkMs = now + 1200;
    dirty = true;
}

void invalidate() { dirty = true; }

bool update(uint32_t now) {
    float dt = (now - lastMs) / 1000.0f;
    if (dt > 0.2f) dt = 0.2f;
    lastMs = now;

    updateBlink(now);
    updateGaze(now, dt);

    const float closed = blinkAmount(now);
    Frame f;
    f.left = eyeRect(cfg::SCREEN_W / 2 - cfg::EYE_SPACING / 2, closed);
    f.right = eyeRect(cfg::SCREEN_W / 2 + cfg::EYE_SPACING / 2, closed);

    if (!dirty && f == lastFrame) return false;
    drawFrame(f);
    lastFrame = f;
    dirty = false;
    return true;
}

}  // namespace eyes
