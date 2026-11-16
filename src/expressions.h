// The list of eye expressions.
#pragma once

#include <stdint.h>

enum class Expression : uint8_t {
    Normal = 0,
    Happy,
    InLove,
    Excited,
    Curious,
    Surprised,
    Suspicious,
    Sleepy,
    Sad,
    Grumpy,
    // Added later. New expressions go at the end so stored settings and the
    // API keep their meaning.
    Bored,
    Shy,
    Wink,
    Thinking,
    Confused,
    Dizzy,
    Content,
    Vibing,
    Cool,
    Ko,
    Sweat,
    Money,
    Touched,
    Starstruck,
    Count
};

constexpr uint8_t EXPRESSION_COUNT = (uint8_t)Expression::Count;
// One bit per expression, so there is room for 32 of them.
constexpr uint32_t ALL_EXPRESSIONS_MASK = (1u << EXPRESSION_COUNT) - 1;
static_assert(EXPRESSION_COUNT < 32, "the expression mask has 32 bits");

// The expressions of the first firmware versions, which stored a 16 bit mask.
constexpr uint32_t LEGACY_EXPRESSIONS_MASK = (1u << 10) - 1;

// Human readable name, e.g. "In love"
const char* expressionName(Expression e);

// Stable identifier for the API and settings, e.g. "in_love"
const char* expressionId(Expression e);
