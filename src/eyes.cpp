#include "eyes.h"

#include <Arduino.h>

#include "config.h"
#include "display.h"

using cfg::GRID;

// ---------------------------------------------------------------------------
// Expression names
// ---------------------------------------------------------------------------

const char* expressionName(Expression e) {
    static const char* const names[EXPRESSION_COUNT] = {
        "Normal", "Happy", "In love", "Excited", "Curious",
        "Surprised", "Suspicious", "Sleepy", "Sad", "Grumpy"};
    return (uint8_t)e < EXPRESSION_COUNT ? names[(uint8_t)e] : "?";
}

const char* expressionId(Expression e) {
    static const char* const ids[EXPRESSION_COUNT] = {
        "normal", "happy", "in_love", "excited", "curious",
        "surprised", "suspicious", "sleepy", "sad", "grumpy"};
    return (uint8_t)e < EXPRESSION_COUNT ? ids[(uint8_t)e] : "?";
}

namespace {

// ---------------------------------------------------------------------------
// Expression definitions
// ---------------------------------------------------------------------------

enum class Shape : uint8_t { Block, Chevron };            // Chevron = "> <"
enum class Pupil : uint8_t { None, Heart, Sparkle };
enum class Gaze : uint8_t { None, Wander, Sides };
enum class Effect : uint8_t { None, Blush, Hearts, Stars, Zzz, Tear };

// Geometry of one eye, relative to its resting position.
struct EyeShape {
    float w, h;      // size
    float dx, dy;    // offset
    float topCut;    // upper lid: pixels hidden from the top
    float botCut;    // lower lid: pixels hidden from the bottom
    float slant;     // slope of the upper edge. >0: inner side lower (angry),
                     //                          <0: outer side lower (sad)
};

struct ExpressionDef {
    Shape shape;
    Pupil pupil;
    Effect effect;
    Gaze gaze;
    EyeShape left, right;
    float gazeX, gazeY;                 // how far the eyes move
    uint16_t blinkMinMs, blinkMaxMs;    // 0 = never blinks
    float redness;                      // 0 = eye color, 1 = angry accent color
};

constexpr float W = cfg::EYE_W;
constexpr float H = cfg::EYE_H;

// Same order as the Expression enum.
const ExpressionDef DEFS[EXPRESSION_COUNT] = {
    // Normal: plain eyes that look around and blink
    {Shape::Block, Pupil::None, Effect::None, Gaze::Wander,
     {W, H, 0, 0, 0, 0, 0}, {W, H, 0, 0, 0, 0, 0},
     (float)cfg::GAZE_MAX_X, (float)cfg::GAZE_MAX_Y,
     (uint16_t)cfg::BLINK_MIN_MS, (uint16_t)cfg::BLINK_MAX_MS, 0},

    // Happy: squinted "> <" eyes with blush marks
    {Shape::Chevron, Pupil::None, Effect::Blush, Gaze::None,
     {52, 72, 0, -4, 0, 0, 0}, {52, 72, 0, -4, 0, 0, 0},
     0, 0, 0, 0, 0},

    // In love: heart shaped pupils, hearts floating around
    {Shape::Block, Pupil::Heart, Effect::Hearts, Gaze::None,
     {60, 84, 0, 0, 0, 0, 0}, {60, 84, 0, 0, 0, 0, 0},
     0, 0, 3000, 6000, 0},

    // Excited: wide eyes with sparkles
    {Shape::Block, Pupil::Sparkle, Effect::Stars, Gaze::None,
     {60, 96, 0, -4, 0, 0, 0}, {60, 96, 0, -4, 0, 0, 0},
     0, 0, 4000, 8000, 0},

    // Curious: one eye wide open, the other half closed, slightly tilted
    {Shape::Block, Pupil::None, Effect::None, Gaze::Sides,
     {48, 96, 0, -6, 0, 0, 0}, {44, 84, 0, 6, 40, 0, 8},
     10, 0, 3000, 6000, 0},

    // Surprised: tall, wide open eyes
    {Shape::Block, Pupil::None, Effect::None, Gaze::None,
     {52, 112, 0, 0, 0, 0, 0}, {52, 112, 0, 0, 0, 0, 0},
     0, 0, 5000, 9000, 0},

    // Suspicious: narrow eyes that look to the side
    {Shape::Block, Pupil::None, Effect::None, Gaze::Sides,
     {52, 84, 0, 0, 36, 20, 0}, {52, 84, 0, 0, 36, 20, 0},
     24, 0, 5000, 9000, 0},

    // Sleepy: half closed eyes, "z z" rising
    {Shape::Block, Pupil::None, Effect::Zzz, Gaze::None,
     {48, 84, 0, 6, 44, 0, 0}, {48, 84, 0, 6, 44, 0, 0},
     0, 0, 0, 0, 0},

    // Sad: droopy eyes looking down
    {Shape::Block, Pupil::None, Effect::Tear, Gaze::Wander,
     {44, 80, 0, 10, 8, 0, -24}, {44, 80, 0, 10, 8, 0, -24},
     6, 0, 4000, 7000, 0},

    // Grumpy: angry slanted eyes that turn red
    {Shape::Block, Pupil::None, Effect::None, Gaze::None,
     {52, 76, 0, 0, 4, 0, 28}, {52, 76, 0, 0, 4, 0, 28},
     0, 0, 4000, 8000, 1},
};

const ExpressionDef& def(Expression e) { return DEFS[(uint8_t)e]; }

// What is drawn in one frame.
struct Pose {
    EyeShape left, right;
    float closed;    // 0 = open, 1 = lids shut (blink)
    bool pulse;      // toggles for pulsing pupils
};

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

Expression current = Expression::Normal;
Expression previous = Expression::Normal;
uint32_t transitionStartMs = 0;     // 0 = no transition running
uint32_t expressionStartMs = 0;

uint32_t colorBackground = cfg::COLOR_BACKGROUND;
uint32_t colorEye = cfg::COLOR_EYE;
uint32_t appliedEyeColor = 0xFFFFFFFF;

uint32_t lastMs = 0;

// Gaze
float gazeX = 0, gazeY = 0, gazeTargetX = 0, gazeTargetY = 0;
uint32_t nextGazeMs = 0;
int8_t gazeSide = 1;

// Wiggle (quick left/right shake)
int8_t wiggleStepsLeft = 0;
uint32_t nextWiggleStepMs = 0;

// Blink
uint32_t blinkStartMs = 0;          // 0 = not blinking
uint32_t nextBlinkMs = 0;
bool doubleBlinkPending = false;

// Expression specific "burst" animation (giggle, shake, ...)
uint32_t burstStartMs = 0;          // 0 = idle
uint32_t nextBurstMs = 0;

// Particles (hearts, stars, z, tears)
enum class ParticleType : uint8_t { Heart, Star, Zzz, Tear };
struct Particle {
    bool alive;
    ParticleType type;
    float x, y, vx, vy;
    uint32_t bornMs;
    uint16_t lifeMs;
    uint8_t cell;                   // size of one block
};
constexpr int MAX_PARTICLES = 10;
Particle particles[MAX_PARTICLES] = {};
uint32_t nextParticleMs = 0;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

int snap(float v) { return (int)lroundf(v / GRID) * GRID; }

float mixf(float a, float b, float t) { return a + (b - a) * t; }

float smooth(float t) { return t * t * (3 - 2 * t); }

uint32_t randomBetween(uint32_t minMs, uint32_t maxMs) {
    return minMs + (uint32_t)random((long)(maxMs - minMs + 1));
}

bool due(uint32_t now, uint32_t when) { return (int32_t)(now - when) >= 0; }

uint32_t mixColor(uint32_t a, uint32_t b, float t) {
    uint32_t out = 0;
    for (int shift = 16; shift >= 0; shift -= 8) {
        const float ca = (a >> shift) & 0xFF, cb = (b >> shift) & 0xFF;
        out |= (uint32_t)lroundf(mixf(ca, cb, t)) << shift;
    }
    return out;
}

EyeShape mixShape(const EyeShape& a, const EyeShape& b, float t) {
    return {mixf(a.w, b.w, t), mixf(a.h, b.h, t), mixf(a.dx, b.dx, t),
            mixf(a.dy, b.dy, t), mixf(a.topCut, b.topCut, t),
            mixf(a.botCut, b.botCut, t), mixf(a.slant, b.slant, t)};
}

// ---------------------------------------------------------------------------
// Idle animation
// ---------------------------------------------------------------------------

float blinkAmount(uint32_t now) {
    if (blinkStartMs == 0) return 0;
    const uint32_t t = now - blinkStartMs;
    if (t < cfg::BLINK_CLOSE_MS) return (float)t / cfg::BLINK_CLOSE_MS;
    if (t < cfg::BLINK_CLOSE_MS + cfg::BLINK_HOLD_MS) return 1;
    const uint32_t openT = t - cfg::BLINK_CLOSE_MS - cfg::BLINK_HOLD_MS;
    if (openT < cfg::BLINK_OPEN_MS) return 1 - (float)openT / cfg::BLINK_OPEN_MS;
    return 0;
}

void updateBlink(const ExpressionDef& d, uint32_t now) {
    const uint32_t blinkLength =
        cfg::BLINK_CLOSE_MS + cfg::BLINK_HOLD_MS + cfg::BLINK_OPEN_MS;

    if (blinkStartMs != 0 && now - blinkStartMs >= blinkLength) {
        blinkStartMs = 0;
        if (doubleBlinkPending) {
            doubleBlinkPending = false;
            nextBlinkMs = now + 90;
        } else {
            nextBlinkMs = now + randomBetween(d.blinkMinMs, d.blinkMaxMs);
        }
    }
    if (d.blinkMaxMs == 0) return;
    if (blinkStartMs == 0 && due(now, nextBlinkMs)) {
        blinkStartMs = now;
        doubleBlinkPending = !doubleBlinkPending && random(6) == 0;
    }
}

void updateGaze(const ExpressionDef& d, uint32_t now, float dt) {
    float speed = 16.0f;

    if (wiggleStepsLeft > 0) {
        if (due(now, nextWiggleStepMs)) {
            wiggleStepsLeft--;
            const int direction = (wiggleStepsLeft % 2) ? 1 : -1;
            gazeTargetX = wiggleStepsLeft == 0 ? 0 : direction * GRID * 2;
            gazeTargetY = 0;
            nextWiggleStepMs = now + 110;
        }
    } else if (d.gaze == Gaze::Wander) {
        if (due(now, nextGazeMs)) {
            nextGazeMs = now + randomBetween(cfg::GAZE_MIN_MS, cfg::GAZE_MAX_MS);
            const long pick = random(10);
            if (pick < 2 && current == Expression::Normal) {
                wiggleStepsLeft = 5;
                nextWiggleStepMs = now;
            } else if (pick < 5) {
                gazeTargetX = 0;
                gazeTargetY = 0;
            } else {
                gazeTargetX = (float)random(-(long)d.gazeX, (long)d.gazeX + 1);
                gazeTargetY = (float)random(-(long)d.gazeY, (long)d.gazeY + 1);
            }
        }
    } else if (d.gaze == Gaze::Sides) {
        // Look to one side, hold, then slowly to the other side.
        speed = 5.0f;
        if (due(now, nextGazeMs)) {
            nextGazeMs = now + randomBetween(1600, 3400);
            gazeSide = -gazeSide;
            gazeTargetX = gazeSide * d.gazeX;
            gazeTargetY = 0;
        }
    } else {
        gazeTargetX = 0;
        gazeTargetY = 0;
    }

    const float k = min(1.0f, dt * speed);
    gazeX += (gazeTargetX - gazeX) * k;
    gazeY += (gazeTargetY - gazeY) * k;
}

// Starts a burst now and then. Returns the time since the burst started, or
// -1 while idle.
int32_t burstTime(uint32_t now, uint32_t lengthMs, uint32_t pauseMinMs, uint32_t pauseMaxMs) {
    if (burstStartMs == 0) {
        if (!due(now, nextBurstMs)) return -1;
        burstStartMs = now;
    }
    const uint32_t t = now - burstStartMs;
    if (t >= lengthMs) {
        burstStartMs = 0;
        nextBurstMs = now + randomBetween(pauseMinMs, pauseMaxMs);
        return -1;
    }
    return (int32_t)t;
}

// The pose of an expression including its idle animation.
Pose idlePose(Expression e, uint32_t now) {
    const ExpressionDef& d = def(e);
    Pose p = {d.left, d.right, blinkAmount(now), false};

    p.left.dx += gazeX;
    p.right.dx += gazeX;
    p.left.dy += gazeY;
    p.right.dy += gazeY;

    switch (e) {
    case Expression::Happy: {
        // Giggle: the eyes squeeze shut twice and hop a little.
        const int32_t t = burstTime(now, 560, 1800, 3500);
        if (t >= 0) {
            const float s = (float)(t % 280) / 280.0f;
            p.closed = max(p.closed, 0.6f * (1 - fabsf(2 * s - 1)));
            const float hop = ((t / 140) % 2) ? -GRID : 0;
            p.left.dy += hop;
            p.right.dy += hop;
        }
        break;
    }
    case Expression::InLove:
        p.pulse = (now / 420) % 2;
        break;
    case Expression::Excited: {
        p.pulse = (now / 300) % 2;
        // Bounce with excitement.
        const int32_t t = burstTime(now, 600, 1400, 3000);
        if (t >= 0 && (t / 100) % 2) {
            p.left.dy -= GRID;
            p.right.dy -= GRID;
        }
        break;
    }
    case Expression::Curious: {
        // The open eye widens for a moment: "hm?"
        const int32_t t = burstTime(now, 450, 2500, 5000);
        if (t >= 0) {
            p.left.h += GRID * 2;
            p.left.dy -= GRID;
        }
        break;
    }
    case Expression::Surprised: {
        // Small tremble
        const int32_t t = burstTime(now, 300, 2000, 4000);
        if (t >= 0 && (t / 60) % 2) {
            p.left.dx += GRID;
            p.right.dx += GRID;
        }
        break;
    }
    case Expression::Sleepy: {
        // The lids sink slowly, then jerk back up.
        const uint32_t t = (now - expressionStartMs) % 4200;
        const float droop = t < 3700 ? (t / 3700.0f) * 20 : 0;
        p.left.topCut += droop;
        p.right.topCut += droop;
        break;
    }
    case Expression::Sad: {
        // Slow sigh
        const float bob = ((now - expressionStartMs) / 1400) % 2 ? GRID : 0;
        p.left.dy += bob;
        p.right.dy += bob;
        break;
    }
    case Expression::Grumpy: {
        // Angry shake
        const int32_t t = burstTime(now, 420, 2500, 5000);
        if (t >= 0) {
            const float shake = ((t / 60) % 2) ? GRID : -GRID;
            p.left.dx += shake;
            p.right.dx += shake;
        }
        break;
    }
    default:
        break;
    }
    return p;
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

// Draws a small bitmap made of square blocks. Each row is a bit mask, the
// most significant used bit is the left column.
void drawBlocks(lgfx::LGFX_Sprite& c, const uint8_t* rows, int rowCount, int columns,
                int centerX, int centerY, int cell, uint8_t color) {
    const int x0 = centerX - columns * cell / 2;
    const int y0 = centerY - rowCount * cell / 2;
    for (int r = 0; r < rowCount; r++) {
        for (int col = 0; col < columns; col++) {
            if (rows[r] & (1 << (columns - 1 - col))) {
                c.fillRect(x0 + col * cell, y0 + r * cell, cell, cell, color);
            }
        }
    }
}

void drawHeart(lgfx::LGFX_Sprite& c, int centerX, int centerY, int cell, uint8_t color) {
    static const uint8_t rows[6] = {0b0110110, 0b1111111, 0b1111111,
                                    0b0111110, 0b0011100, 0b0001000};
    drawBlocks(c, rows, 6, 7, centerX, centerY, cell, color);
}

void drawZ(lgfx::LGFX_Sprite& c, int centerX, int centerY, int cell, uint8_t color) {
    static const uint8_t rows[5] = {0b11111, 0b00010, 0b00100, 0b01000, 0b11111};
    drawBlocks(c, rows, 5, 5, centerX, centerY, cell, color);
}

void drawStar(lgfx::LGFX_Sprite& c, int centerX, int centerY, int cell, bool big, uint8_t color) {
    static const uint8_t plus[3] = {0b010, 0b111, 0b010};
    static const uint8_t dot[1] = {0b1};
    if (big) drawBlocks(c, plus, 3, 3, centerX, centerY, cell, color);
    else drawBlocks(c, dot, 1, 1, centerX, centerY, cell, color);
}

// Draws one block eye. `inner` is +1 if the inner side (towards the nose) is
// on the right, -1 if it is on the left.
void drawBlockEye(lgfx::LGFX_Sprite& c, int restX, const EyeShape& s, float closed,
                  int inner, Pupil pupil, bool pulse) {
    const int w = max(GRID * 2, snap(s.w));
    const int columns = w / GRID;
    const int x0 = restX - w / 2 + snap(s.dx);
    const float centerY = cfg::EYE_CENTER_Y + s.dy;
    const float top0 = centerY - s.h / 2 + s.topCut;
    const float bottom0 = centerY + s.h / 2 - s.botCut;
    // The lids meet a little below the middle.
    const float closedLine = centerY + GRID * 2;

    for (int i = 0; i < columns; i++) {
        float u = (i + 0.5f) / columns - 0.5f;   // -0.5 .. 0.5, left to right
        u *= inner;                              // now: outer .. inner
        float top = top0 + s.slant * u;
        float bottom = bottom0;
        top = mixf(top, closedLine - GRID, closed);
        bottom = mixf(bottom, closedLine + GRID, closed);
        int ys = snap(top), ye = snap(bottom);
        if (ye - ys < GRID) {
            if (closed < 0.5f) continue;         // this column is hidden by a lid
            ys = snap(closedLine) - GRID;
            ye = ys + GRID * 2;
        }
        c.fillRect(x0 + i * GRID, ys, GRID, ye - ys, cfg::PAL_EYE);
    }

    if (closed > 0.3f) return;
    const int midX = x0 + w / 2;
    const int midY = snap((top0 + bottom0) / 2);
    if (pupil == Pupil::Heart) {
        drawHeart(c, midX, midY, pulse ? 5 : 4, cfg::PAL_HEART);
    } else if (pupil == Pupil::Sparkle) {
        // A big and a small highlight, like light reflecting in the eye.
        const int big = pulse ? GRID * 4 : GRID * 3;
        c.fillRect(x0 + GRID * 2, snap(top0) + GRID * 3, big, big, cfg::PAL_SPARKLE);
        c.fillRect(x0 + w - GRID * 5, snap(bottom0) - GRID * 7, GRID * 2, GRID * 2,
                   cfg::PAL_SPARKLE);
    }
}

// Draws one chevron eye: ">" if the tip points right, "<" if it points left.
void drawChevronEye(lgfx::LGFX_Sprite& c, int restX, const EyeShape& s, float closed,
                    int tipDirection) {
    const int thickness = GRID * 5;
    const int w = snap(s.w);
    const int span = w - thickness;
    int h = snap(s.h * (1 - closed));
    if (h < GRID * 2) h = GRID * 2;
    const int rows = h / GRID;
    const int x0 = restX - w / 2 + snap(s.dx);
    const int y0 = snap(cfg::EYE_CENTER_Y + s.dy) - h / 2;

    for (int j = 0; j < rows; j++) {
        const float v = (j + 0.5f) / rows;
        const int offset = snap((1 - fabsf(2 * v - 1)) * span);
        const int x = tipDirection > 0 ? x0 + offset : x0 + span - offset;
        c.fillRect(x, y0 + j * GRID, thickness, GRID, cfg::PAL_EYE);
    }
}

void drawBlush(lgfx::LGFX_Sprite& c) {
    const int y = cfg::EYE_CENTER_Y + 44;
    const int leftX = cfg::SCREEN_W / 2 - cfg::EYE_SPACING / 2;
    const int rightX = cfg::SCREEN_W / 2 + cfg::EYE_SPACING / 2;
    for (int k = 0; k < 3; k++) {
        c.fillRect(leftX - 36 + k * 14, y, GRID * 2, GRID * 2, cfg::PAL_BLUSH);
        c.fillRect(rightX + 28 - k * 14, y, GRID * 2, GRID * 2, cfg::PAL_BLUSH);
    }
}

void drawEyes(lgfx::LGFX_Sprite& c, Expression e, const Pose& p, Pupil pupil) {
    const int leftX = cfg::SCREEN_W / 2 - cfg::EYE_SPACING / 2;
    const int rightX = cfg::SCREEN_W / 2 + cfg::EYE_SPACING / 2;
    if (def(e).shape == Shape::Chevron) {
        drawChevronEye(c, leftX, p.left, p.closed, +1);
        drawChevronEye(c, rightX, p.right, p.closed, -1);
    } else {
        drawBlockEye(c, leftX, p.left, p.closed, +1, pupil, p.pulse);
        drawBlockEye(c, rightX, p.right, p.closed, -1, pupil, p.pulse);
    }
}

// ---------------------------------------------------------------------------
// Particles
// ---------------------------------------------------------------------------

void spawn(ParticleType type, float x, float y, float vx, float vy, uint16_t lifeMs,
           uint8_t cell, uint32_t now) {
    for (auto& p : particles) {
        if (p.alive) continue;
        p = {true, type, x, y, vx, vy, now, lifeMs, cell};
        return;
    }
}

// A random x position that does not cover the eyes.
float freeX() {
    switch (random(3)) {
    case 0:  return (float)random(8, 36);
    case 1:  return (float)random(132, 188);
    default: return (float)random(284, 312);
    }
}

void spawnParticles(Effect effect, uint32_t now) {
    if (!due(now, nextParticleMs)) return;
    const int leftX = cfg::SCREEN_W / 2 - cfg::EYE_SPACING / 2;
    switch (effect) {
    case Effect::Hearts:
        spawn(ParticleType::Heart, freeX(), (float)random(120, 160),
              (float)random(-6, 7), -30, 2400, (uint8_t)random(2, 4), now);
        nextParticleMs = now + randomBetween(450, 900);
        break;
    case Effect::Stars:
        spawn(ParticleType::Star, freeX(), (float)random(20, 150), 0, 0, 520, GRID, now);
        nextParticleMs = now + randomBetween(180, 420);
        break;
    case Effect::Zzz:
        spawn(ParticleType::Zzz, 272, 62, 9, -20, 2600, 2, now);
        nextParticleMs = now + 1300;
        break;
    case Effect::Tear:
        spawn(ParticleType::Tear, (float)(leftX + (random(2) ? 148 : 0)),
              cfg::EYE_CENTER_Y + 56, 0, 46, 1300, GRID, now);
        nextParticleMs = now + randomBetween(3500, 7000);
        break;
    default:
        break;
    }
}

void updateAndDrawParticles(lgfx::LGFX_Sprite& c, uint32_t now, float dt) {
    for (auto& p : particles) {
        if (!p.alive) continue;
        const uint32_t age = now - p.bornMs;
        if (age >= p.lifeMs) {
            p.alive = false;
            continue;
        }
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        const int x = (int)lroundf(p.x / 2) * 2;
        const int y = (int)lroundf(p.y / 2) * 2;
        switch (p.type) {
        case ParticleType::Heart:
            drawHeart(c, x, y, p.cell, cfg::PAL_HEART);
            break;
        case ParticleType::Star:
            drawStar(c, snap(p.x), snap(p.y), p.cell, age > 120 && age < 400,
                     cfg::PAL_SPARKLE);
            break;
        case ParticleType::Zzz:
            // The z grows while it rises.
            drawZ(c, x, y, age > p.lifeMs / 2 ? p.cell + 1 : p.cell, cfg::PAL_TEXT);
            break;
        case ParticleType::Tear:
            c.fillRect(snap(p.x), snap(p.y), p.cell, p.cell * 2, cfg::PAL_SPARKLE);
            break;
        }
    }
}

void clearParticles() {
    for (auto& p : particles) p.alive = false;
}

}  // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

namespace eyes {

void begin() {
    const uint32_t now = millis();
    lastMs = now;
    expressionStartMs = now;
    nextGazeMs = now + 1500;
    nextBlinkMs = now + 1200;
    nextBurstMs = now + 1500;
    setColors(colorBackground, colorEye);
}

void setExpression(Expression e) {
    if ((uint8_t)e >= EXPRESSION_COUNT || e == current) return;
    const uint32_t now = millis();
    previous = current;
    current = e;
    transitionStartMs = now ? now : 1;
    expressionStartMs = now;

    // Restart the idle animation for the new expression.
    blinkStartMs = 0;
    doubleBlinkPending = false;
    const ExpressionDef& d = def(e);
    nextBlinkMs = now + cfg::TRANSITION_MS + randomBetween(d.blinkMinMs, d.blinkMaxMs);
    burstStartMs = 0;
    nextBurstMs = now + cfg::TRANSITION_MS + 700;
    nextGazeMs = now + cfg::TRANSITION_MS + 400;
    gazeTargetX = 0;
    gazeTargetY = 0;
    wiggleStepsLeft = 0;
    clearParticles();
    nextParticleMs = now + cfg::TRANSITION_MS;
}

Expression expression() { return current; }

void triggerWiggle() {
    wiggleStepsLeft = 9;
    nextWiggleStepMs = millis();
}

void setColors(uint32_t background, uint32_t eye) {
    colorBackground = background;
    colorEye = eye;
    display::setPaletteColor(cfg::PAL_BACKGROUND, background);
    appliedEyeColor = 0xFFFFFFFF;   // re-applied in draw()
}

void draw(uint32_t now) {
    float dt = (now - lastMs) / 1000.0f;
    if (dt > 0.2f) dt = 0.2f;
    lastMs = now;

    // Transition progress: 0 = still the previous expression, 1 = done
    float t = 1;
    if (transitionStartMs != 0) {
        t = (float)(now - transitionStartMs) / cfg::TRANSITION_MS;
        if (t >= 1) {
            t = 1;
            transitionStartMs = 0;
            previous = current;
        }
    }

    const ExpressionDef& d = def(current);
    updateBlink(d, now);
    updateGaze(d, now, dt);

    // Eye color: fades to the angry accent color for grumpy eyes.
    const float redness = mixf(def(previous).redness, d.redness, smooth(t));
    const uint32_t eyeColor = mixColor(colorEye, cfg::COLOR_ANGRY, redness);
    if (eyeColor != appliedEyeColor) {
        appliedEyeColor = eyeColor;
        display::setPaletteColor(cfg::PAL_EYE, eyeColor);
    }

    auto& c = display::canvas();
    c.fillScreen(cfg::PAL_BACKGROUND);

    Pose pose = idlePose(current, now);
    Expression shown = current;
    Pupil pupil = d.pupil;

    if (t < 1) {
        const ExpressionDef& from = def(previous);
        if (from.shape == Shape::Block && d.shape == Shape::Block) {
            // Both are block eyes: morph the geometry.
            const float s = smooth(t);
            pose.left = mixShape(from.left, pose.left, s);
            pose.right = mixShape(from.right, pose.right, s);
            pupil = s > 0.5f ? d.pupil : from.pupil;
        } else if (t < 0.5f) {
            // Different shapes: close the old eyes, then open the new ones.
            shown = previous;
            pose = {from.left, from.right, t * 2, false};
            pupil = from.pupil;
        } else {
            pose.closed = max(pose.closed, (1 - t) * 2);
        }
    }

    drawEyes(c, shown, pose, pupil);

    if (def(shown).effect == Effect::Blush && pose.closed < 0.9f) drawBlush(c);
    if (t >= 1) spawnParticles(d.effect, now);
    updateAndDrawParticles(c, now, dt);
}

}  // namespace eyes
