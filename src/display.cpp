#include "display.h"

#include <Arduino.h>

#include "config.h"

namespace {

// JD9853 panel as used on the Waveshare ESP32-C6-Touch-LCD-1.47.
// LovyanGFX ships a JD9853 driver that is tuned for a different (240 x 240)
// glass, so only the command handling is reused here. The register values
// below are the ones Waveshare documents for this 172 x 320 glass in their
// official example package.
class PanelClawd : public lgfx::Panel_JD9853 {
protected:
    const uint8_t* getInitCommands(uint8_t listno) const override {
        static constexpr uint8_t commands[] = {
            // LovyanGFX waits only 64 ms after the hardware reset. Give the
            // panel more time before sleep out by sending a NOP first.
            0x00, 0 + CMD_INIT_DELAY, 120,
            0x11, 0 + CMD_INIT_DELAY, 120,   // sleep out
            0xDF, 2, 0x98, 0x53,             // unlock vendor commands
            0xB2, 1, 0x23,
            0xB7, 4, 0x00, 0x47, 0x00, 0x6F,
            0xBB, 6, 0x1C, 0x1A, 0x55, 0x73, 0x63, 0xF0,
            0xC0, 2, 0x44, 0xA4,
            0xC1, 1, 0x16,
            0xC3, 8, 0x7D, 0x07, 0x14, 0x06, 0xCF, 0x71, 0x72, 0x77,
            0xC4, 12, 0x00, 0x00, 0xA0, 0x79, 0x0B, 0x0A,
                      0x16, 0x79, 0x0B, 0x0A, 0x16, 0x82,
            0xC8, 32,                        // gamma
                0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28,
                0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,
                0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28,
                0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,
            0xD0, 5, 0x04, 0x06, 0x6B, 0x0F, 0x00,
            0xD7, 2, 0x00, 0x30,
            0xE6, 1, 0x14,
            0xDE, 1, 0x01,                   // register page 1
            0xB7, 5, 0x03, 0x13, 0xEF, 0x35, 0x35,
            0xC1, 3, 0x14, 0x15, 0xC0,
            0xC2, 2, 0x06, 0x3A,
            0xC4, 2, 0x72, 0x12,
            0xBE, 1, 0x00,
            0xDE, 1, 0x02,                   // register page 2
            0xE5, 3, 0x00, 0x02, 0x00,
            0xE5, 3, 0x01, 0x02, 0x00,
            0xDE, 1, 0x00,                   // back to page 0
            0x35, 1, 0x00,                   // tearing effect line on
            0x3A, 1, 0x05,                   // 16 bit per pixel
            0xDE, 1, 0x02,                   // register page 2
            0xE5, 3, 0x00, 0x02, 0x00,       // must be written last, see vendor sequence
            0xDE, 1, 0x00,                   // back to page 0
            0x21, 0 + CMD_INIT_DELAY, 10,    // inversion on (IPS)
            0x29, 0 + CMD_INIT_DELAY, 10,    // display on
            0xFF, 0xFF,                      // end of list
        };
        return listno == 0 ? commands : nullptr;
    }
};

class ClawdLcd : public lgfx::LGFX_Device {
public:
    ClawdLcd() {
        {
            auto c = _bus.config();
            c.spi_host    = SPI2_HOST;
            c.spi_mode    = 0;
            c.freq_write  = cfg::LCD_SPI_HZ;
            c.freq_read   = 16000000;
            c.spi_3wire   = false;
            c.use_lock    = true;
            c.dma_channel = SPI_DMA_CH_AUTO;
            c.pin_sclk    = cfg::PIN_LCD_SCK;
            c.pin_mosi    = cfg::PIN_LCD_MOSI;
            c.pin_miso    = -1;
            c.pin_dc      = cfg::PIN_LCD_DC;
            _bus.config(c);
            _panel.setBus(&_bus);
        }
        {
            auto c = _panel.config();
            c.pin_cs        = cfg::PIN_LCD_CS;
            c.pin_rst       = cfg::PIN_LCD_RST;
            c.pin_busy      = -1;
            c.memory_width  = 240;
            c.memory_height = 320;
            c.panel_width   = cfg::PANEL_WIDTH;
            c.panel_height  = cfg::PANEL_HEIGHT;
            c.offset_x      = cfg::PANEL_OFFSET_X;
            c.offset_y      = 0;
            c.offset_rotation = 0;
            c.readable      = false;
            c.invert        = true;   // IPS glass
            c.rgb_order     = true;
            c.dlen_16bit    = false;
            c.bus_shared    = false;
            _panel.config(c);
        }
        setPanel(&_panel);
    }

private:
    lgfx::Bus_SPI _bus;
    PanelClawd _panel;
};

ClawdLcd lcd;
lgfx::LGFX_Sprite frame(&lcd);

// Copy of the frame that is currently on the panel. Comparing against it
// tells which part of the screen really has to be transferred.
constexpr int ROW_BYTES = cfg::SCREEN_W / 2;   // 4 bit per pixel
uint8_t* shadow = nullptr;
bool fullPushNeeded = true;

uint8_t brightnessPercent = cfg::DEFAULT_BRIGHTNESS;
bool lightOn = false;

void applyBacklight() {
    const uint32_t maxDuty = (1u << cfg::BL_PWM_BITS) - 1;
    uint32_t duty = 0;
    if (lightOn && brightnessPercent > 0) {
        // Quadratic curve: perceived brightness follows the slider more evenly.
        const uint32_t p = brightnessPercent;
        duty = (maxDuty * p * p) / (100u * 100u);
        if (duty == 0) duty = 1;
    }
    ledcWrite(cfg::PIN_LCD_BL, duty);
}

}  // namespace

namespace display {

void begin() {
    // The TF card shares the SPI lines with the LCD. Keep it deselected.
    pinMode(cfg::PIN_TF_CS, OUTPUT);
    digitalWrite(cfg::PIN_TF_CS, HIGH);

    ledcAttach(cfg::PIN_LCD_BL, cfg::BL_PWM_HZ, cfg::BL_PWM_BITS);
    ledcWrite(cfg::PIN_LCD_BL, 0);

    lcd.init();
    lcd.setRotation(cfg::DISPLAY_ROTATION);

    frame.setColorDepth(4);
    frame.createSprite(cfg::SCREEN_W, cfg::SCREEN_H);
    frame.createPalette();
    setPaletteColor(cfg::PAL_BACKGROUND, cfg::COLOR_BACKGROUND);
    setPaletteColor(cfg::PAL_EYE, cfg::COLOR_EYE);
    setPaletteColor(cfg::PAL_BLUSH, cfg::COLOR_BLUSH);
    setPaletteColor(cfg::PAL_HEART, cfg::COLOR_HEART);
    setPaletteColor(cfg::PAL_SPARKLE, cfg::COLOR_SPARKLE);
    setPaletteColor(cfg::PAL_ANGRY, cfg::COLOR_ANGRY);
    setPaletteColor(cfg::PAL_TEXT, cfg::COLOR_TEXT);
    setPaletteColor(cfg::PAL_MONEY, cfg::COLOR_MONEY);
    frame.fillScreen(cfg::PAL_BACKGROUND);

    shadow = (uint8_t*)malloc(ROW_BYTES * cfg::SCREEN_H);
}

lgfx::LGFX_Sprite& canvas() { return frame; }

void setPaletteColor(uint8_t index, uint32_t rgb888) {
    frame.setPaletteColor(index, (uint8_t)(rgb888 >> 16), (uint8_t)(rgb888 >> 8),
                          (uint8_t)rgb888);
    fullPushNeeded = true;   // a palette change affects the whole screen
}

bool present() {
    const uint8_t* buf = (const uint8_t*)frame.getBuffer();
    int y0 = -1, y1 = -1, b0 = ROW_BYTES, b1 = -1;

    if (fullPushNeeded || shadow == nullptr) {
        y0 = 0; y1 = cfg::SCREEN_H - 1; b0 = 0; b1 = ROW_BYTES - 1;
    } else {
        for (int y = 0; y < cfg::SCREEN_H; y++) {
            const uint8_t* a = buf + y * ROW_BYTES;
            const uint8_t* b = shadow + y * ROW_BYTES;
            if (memcmp(a, b, ROW_BYTES) == 0) continue;
            int first = 0, last = ROW_BYTES - 1;
            while (a[first] == b[first]) first++;
            while (a[last] == b[last]) last--;
            if (y0 < 0) y0 = y;
            y1 = y;
            if (first < b0) b0 = first;
            if (last > b1) b1 = last;
        }
        if (y0 < 0) return false;
    }

    if (shadow != nullptr) {
        memcpy(shadow + y0 * ROW_BYTES, buf + y0 * ROW_BYTES, (y1 - y0 + 1) * ROW_BYTES);
    }
    fullPushNeeded = false;

    lcd.setClipRect(b0 * 2, y0, (b1 - b0 + 1) * 2, y1 - y0 + 1);
    frame.pushSprite(0, 0);
    lcd.clearClipRect();
    return true;
}

void setBrightness(uint8_t percent) {
    brightnessPercent = percent > 100 ? 100 : percent;
    applyBacklight();
}

uint8_t brightness() { return brightnessPercent; }

void setBacklight(bool on) {
    lightOn = on;
    applyBacklight();
}

bool backlightOn() { return lightOn; }

}  // namespace display
