// Display module: panel driver, off-screen framebuffer and backlight.
#pragma once

#include <LovyanGFX.hpp>

namespace display {

// Initializes panel, framebuffer and backlight. The backlight stays off until
// the first frame was pushed, so no garbage is visible during boot.
void begin();

// The off-screen framebuffer (320 x 172, 4 bit palette). Draw with the
// cfg::PaletteIndex values as colors, then call present().
lgfx::LGFX_Sprite& canvas();

// Changes one palette color (RGB888). Takes effect on the next present().
void setPaletteColor(uint8_t index, uint32_t rgb888);

// Pushes the framebuffer to the panel. Only the area that changed since the
// last call is transferred. Returns false if nothing changed.
bool present();

// Backlight brightness in percent (0..100). Remembered while the light is off.
void setBrightness(uint8_t percent);
uint8_t brightness();

// Switches the backlight on or off without losing the brightness value.
void setBacklight(bool on);
bool backlightOn();

}  // namespace display
