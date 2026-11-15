// Eye renderer and animation.
//
// The eyes are drawn as chunky blocks on a coarse grid (cfg::GRID), which
// gives them the pixel-art look of the figure.
#pragma once

#include <stdint.h>

namespace eyes {

void begin();

// Advances the animation to the given time (millis) and redraws the
// framebuffer if something visible changed. Returns true when the caller
// should push the framebuffer to the panel.
bool update(uint32_t nowMs);

// Forces a redraw on the next update, e.g. after a color change.
void invalidate();

}  // namespace eyes
