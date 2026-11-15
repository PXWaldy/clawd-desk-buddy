// Eye renderer and animation.
//
// The eyes are drawn as chunky blocks on a coarse grid (cfg::GRID), which
// gives them the pixel-art look of the figure. Every expression has its own
// idle animation, and changing the expression morphs smoothly.
#pragma once

#include <stdint.h>

#include "expressions.h"

namespace eyes {

void begin();

// Switches to another expression with a smooth transition.
void setExpression(Expression e);
Expression expression();

// Plays a short, happy left/right wiggle on top of the current expression.
void triggerWiggle();

// Background and eye color (RGB888). Accent colors are not affected.
void setColors(uint32_t background, uint32_t eye);

// Advances the animation to the given time (millis) and draws the eyes into
// the framebuffer.
void draw(uint32_t nowMs);

}  // namespace eyes
