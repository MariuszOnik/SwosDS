// Tiny hand-authored 3x5 dot-matrix font, digits + a few separators only.
// No SDL_ttf is available on this machine (checked before writing this --
// only SDL2 itself is installed under devkitPro's mingw64), and this is a
// debugging overlay, not a UI, so a small bitmap font for numbers is enough
// to show tick/gameState/score/slot-ordinal directly in the window; the
// full text panel (positions, dest, anim pointers, flags, etc.) goes to the
// console instead -- see main.c's diagDumpStage()/logging and the task's
// own "overlay LUB panel tekstowy" wording.
#pragma once

#include <stdint.h>

// Each glyph: 5 rows, each row's lowest 3 bits = pixels (bit 2 = leftmost).
// Index: '0'..'9' -> 0..9, '-' -> 10, ':' -> 11, '/' -> 12, ' ' -> 13.
#define DIGIT_FONT_GLYPH_COUNT 14
#define DIGIT_FONT_ROWS 5
#define DIGIT_FONT_COLS 3

static const uint8_t kDigitFont[DIGIT_FONT_GLYPH_COUNT][DIGIT_FONT_ROWS] = {
    { 0x7, 0x5, 0x5, 0x5, 0x7 }, // 0
    { 0x2, 0x6, 0x2, 0x2, 0x7 }, // 1
    { 0x7, 0x1, 0x7, 0x4, 0x7 }, // 2
    { 0x7, 0x1, 0x7, 0x1, 0x7 }, // 3
    { 0x5, 0x5, 0x7, 0x1, 0x1 }, // 4
    { 0x7, 0x4, 0x7, 0x1, 0x7 }, // 5
    { 0x7, 0x4, 0x7, 0x5, 0x7 }, // 6
    { 0x7, 0x1, 0x2, 0x2, 0x2 }, // 7
    { 0x7, 0x5, 0x7, 0x5, 0x7 }, // 8
    { 0x7, 0x5, 0x7, 0x1, 0x7 }, // 9
    { 0x0, 0x0, 0x7, 0x0, 0x0 }, // -
    { 0x0, 0x2, 0x0, 0x2, 0x0 }, // :
    { 0x1, 0x1, 0x2, 0x4, 0x4 }, // /
    { 0x0, 0x0, 0x0, 0x0, 0x0 }, // (space)
};

static inline int digitFontGlyphIndex(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c == '-') return 10;
    if (c == ':') return 11;
    if (c == '/') return 12;
    return 13; // anything else -> blank
}
