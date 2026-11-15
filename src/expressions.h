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
    Count
};

constexpr uint8_t EXPRESSION_COUNT = (uint8_t)Expression::Count;
constexpr uint16_t ALL_EXPRESSIONS_MASK = (1u << EXPRESSION_COUNT) - 1;

// Human readable name, e.g. "In love"
const char* expressionName(Expression e);

// Stable identifier for the API and settings, e.g. "in_love"
const char* expressionId(Expression e);
