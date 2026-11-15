// Clawd Desk Buddy - central configuration
//
// Everything that describes the hardware or the look of the eyes lives here,
// so it can be tuned in one place.
#pragma once

#include <stdint.h>

namespace cfg {

// ---------------------------------------------------------------------------
// Hardware: Waveshare ESP32-C6-Touch-LCD-1.47
// Pin mapping taken from the official Waveshare documentation.
// ---------------------------------------------------------------------------
constexpr int PIN_LCD_SCK  = 1;   // shared with the TF card slot
constexpr int PIN_LCD_MOSI = 2;   // shared with the TF card slot
constexpr int PIN_LCD_CS   = 14;
constexpr int PIN_LCD_DC   = 15;
constexpr int PIN_LCD_RST  = 22;
constexpr int PIN_LCD_BL   = 23;  // backlight, active high, PWM capable

constexpr int PIN_TF_CS    = 4;   // TF card is unused, CS is kept inactive

constexpr int PIN_I2C_SDA  = 18;  // shared by touch and IMU
constexpr int PIN_I2C_SCL  = 19;
constexpr int PIN_TOUCH_RST = 20;
constexpr int PIN_TOUCH_INT = 21;

// Panel geometry (JD9853, 172 x 320 visible inside a 240 x 320 controller RAM)
constexpr int PANEL_WIDTH    = 172;
constexpr int PANEL_HEIGHT   = 320;
constexpr int PANEL_OFFSET_X = 34;
constexpr uint32_t LCD_SPI_HZ = 40000000;

// Mounting: the display sits in landscape inside the figure with the USB-C
// port pointing to the right (seen from the front).
// Valid landscape values are 1 and 3. If the picture is upside down, swap them.
constexpr uint8_t DISPLAY_ROTATION = 1;

// Screen size as seen by the user (landscape)
constexpr int SCREEN_W = 320;
constexpr int SCREEN_H = 172;

// Backlight PWM
constexpr uint32_t BL_PWM_HZ   = 5000;
constexpr uint8_t  BL_PWM_BITS = 10;
constexpr uint8_t  DEFAULT_BRIGHTNESS = 80;  // percent

// ---------------------------------------------------------------------------
// Animation
// ---------------------------------------------------------------------------
constexpr uint32_t FRAME_MS = 33;  // ~30 fps

// ---------------------------------------------------------------------------
// Eye look
// All eye geometry snaps to this grid, which gives the chunky pixel-art feel.
// ---------------------------------------------------------------------------
constexpr int GRID = 4;            // size of one "pixel" block in real pixels

constexpr int EYE_W       = 44;    // width of one eye
constexpr int EYE_H       = 84;    // height of one open eye
constexpr int EYE_SPACING = 148;   // distance between the two eye centers
constexpr int EYE_CENTER_Y = SCREEN_H / 2;

// Idle behaviour of the normal expression
constexpr int GAZE_MAX_X = 12;     // how far the eyes drift sideways
constexpr int GAZE_MAX_Y = 8;      // how far the eyes drift up and down
constexpr uint32_t GAZE_MIN_MS  = 1800;   // pause between gaze changes
constexpr uint32_t GAZE_MAX_MS  = 5000;
constexpr uint32_t BLINK_MIN_MS = 2500;   // pause between blinks
constexpr uint32_t BLINK_MAX_MS = 6500;
constexpr uint32_t BLINK_CLOSE_MS = 70;
constexpr uint32_t BLINK_HOLD_MS  = 50;
constexpr uint32_t BLINK_OPEN_MS  = 100;

// ---------------------------------------------------------------------------
// Colors (RGB888)
// ---------------------------------------------------------------------------
constexpr uint32_t COLOR_BACKGROUND = 0xD97757;  // Claude orange
constexpr uint32_t COLOR_EYE        = 0x000000;

// Accent colors. They are independent of the eye color and are picked to
// stay visible on the orange background.
constexpr uint32_t COLOR_BLUSH   = 0xF2A6A0;
constexpr uint32_t COLOR_HEART   = 0xC2185B;
constexpr uint32_t COLOR_SPARKLE = 0xFFF4D6;
constexpr uint32_t COLOR_ANGRY   = 0xB00020;
constexpr uint32_t COLOR_TEXT    = 0xFFFFFF;

// Palette slots of the 4 bit framebuffer
enum PaletteIndex : uint8_t {
    PAL_BACKGROUND = 0,
    PAL_EYE,
    PAL_BLUSH,
    PAL_HEART,
    PAL_SPARKLE,
    PAL_ANGRY,
    PAL_TEXT,
    PAL_COUNT
};

}  // namespace cfg
