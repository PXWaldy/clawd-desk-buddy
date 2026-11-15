// Clawd Desk Buddy
// A small desk companion with animated pixel eyes.
//
// Fan project, not affiliated with Anthropic.

#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "eyes.h"

namespace {

// The animation runs in its own task so it stays smooth no matter what the
// rest of the firmware (WiFi, web server) is doing.
void animationTask(void*) {
    bool firstFrame = true;
    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        if (eyes::update(millis())) {
            display::present();
            if (firstFrame) {
                firstFrame = false;
                display::setBacklight(true);
            }
        }
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(cfg::FRAME_MS));
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    Serial.println("Clawd Desk Buddy starting");

    display::begin();
    display::setBrightness(cfg::DEFAULT_BRIGHTNESS);
    eyes::begin();

    xTaskCreate(animationTask, "anim", 4096, nullptr, 3, nullptr);
}

void loop() {
    delay(1000);
}
