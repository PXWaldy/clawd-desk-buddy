// Clawd Desk Buddy - central configuration
//
// Everything that describes the hardware or the look of the eyes lives here,
// so it can be tuned in one place.
#pragma once

#include <stdint.h>

// Set by scripts/version.py from the git tag.
#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif

namespace cfg {

constexpr char FIRMWARE_NAME[]    = "Clawd Desk Buddy";
constexpr char FIRMWARE_VERSION[] = FW_VERSION;

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

// Touch controller AXS5106L (facts from the official Waveshare examples)
constexpr uint8_t  TOUCH_I2C_ADDR = 0x63;
constexpr uint8_t  TOUCH_REG_DATA = 0x01;
constexpr uint32_t I2C_HZ = 400000;

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
constexpr uint32_t TRANSITION_MS = 280;      // morph between two expressions

// ---------------------------------------------------------------------------
// Behaviour
// ---------------------------------------------------------------------------
constexpr uint32_t HAPPY_REACTION_MS = 2000;     // happy look after a tap
constexpr uint32_t RANDOM_INTERVAL_DEFAULT_S = 20;
constexpr uint32_t RANDOM_INTERVAL_MIN_S = 5;
constexpr uint32_t RANDOM_INTERVAL_MAX_S = 300;

// ---------------------------------------------------------------------------
// Network
// ---------------------------------------------------------------------------
constexpr char DEFAULT_HOSTNAME[] = "clawd";             // http://clawd.local
constexpr char SETUP_AP_SSID[]    = "Clawd-Setup";
constexpr uint32_t SETUP_AP_TIMEOUT_MS   = 10 * 60 * 1000; // close AP without clients
constexpr uint32_t SETUP_AP_LINGER_MS    = 20 * 1000;      // keep AP after success
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15 * 1000;    // per attempt
constexpr uint8_t  WIFI_CONNECT_ATTEMPTS = 3;              // then fall back to setup
constexpr uint32_t WIFI_LOST_GRACE_MS    = 30 * 1000;      // before reconnecting by hand
constexpr uint32_t WIFI_RETRY_PAUSE_MS   = 60 * 1000;      // retry while offline
constexpr uint32_t IP_HINT_MS            = 6000;           // show IP after connecting
constexpr char DEFAULT_TIMEZONE_NAME[] = "Europe/Berlin";
constexpr char DEFAULT_TIMEZONE[] = "CET-1CEST,M3.5.0,M10.5.0/3";  // POSIX rule for it
constexpr char NTP_SERVER[]       = "pool.ntp.org";

constexpr uint32_t SETTINGS_SAVE_DELAY_MS = 2000;  // collect changes before writing flash

// Sleep
constexpr uint32_t SLEEP_ANIMATION_MS  = 3500;   // sleepy eyes before the light goes off
constexpr uint32_t WAKE_SLEEPY_MS      = 800;    // wake up: sleepy eyes ...
constexpr uint32_t WAKE_SURPRISED_MS   = 900;    // ... then wide open, then normal
constexpr uint16_t WAKE_SECONDS_DEFAULT = 30;    // tap while asleep: stay awake this long
constexpr uint16_t WAKE_SECONDS_MIN = 5;
constexpr uint16_t WAKE_SECONDS_MAX = 600;
constexpr uint16_t SLEEP_FROM_DEFAULT_MIN = 22 * 60;   // 22:00
constexpr uint16_t SLEEP_TO_DEFAULT_MIN   = 7 * 60;    // 07:00

// Password protection
constexpr uint32_t PASSWORD_PBKDF2_ROUNDS = 4000;
constexpr size_t   PASSWORD_MIN_LENGTH = 4;
constexpr uint32_t LOGIN_RETRY_PAUSE_MS = 1500;  // slows down password guessing

// Touch gestures
constexpr uint32_t TAP_MAX_MS        = 800;      // longer presses are no tap
constexpr uint32_t HOLD_WIFI_MS      = 5000;     // release after this: WiFi setup
constexpr uint32_t HOLD_FACTORY_MS   = 15000;    // keep holding: factory reset
constexpr uint32_t TOUCH_RELEASE_MS  = 80;       // debounce for lifting the finger

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
// Claude orange is #D97757. This panel shows colors paler and cooler than a
// phone or monitor, so the original value looks pink on it. The default below
// was picked by eye on the real display to look like Claude orange.
constexpr uint32_t COLOR_CLAUDE_ORANGE = 0xD97757;
constexpr uint32_t COLOR_BACKGROUND    = 0xE05500;
constexpr uint32_t COLOR_EYE        = 0x000000;

// Accent colors. They are independent of the eye color and are picked to
// stay visible on the orange background.
constexpr uint32_t COLOR_BLUSH   = 0xFFA0A0;
constexpr uint32_t COLOR_HEART   = 0xFFD1E0;
constexpr uint32_t COLOR_SPARKLE = 0xFFF4D6;
constexpr uint32_t COLOR_ANGRY   = 0xA80010;
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
