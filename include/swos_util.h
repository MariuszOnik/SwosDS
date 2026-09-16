// New code (not a port of anything in OpenSWOS) -- small shared helpers
// needed by multiple ported modules.
#pragma once

#include <stdint.h>

// Portable arithmetic right shift for a 32-bit signed value. C11 leaves the
// result of `>>` on a negative signed operand implementation-defined
// (6.5.7p5) -- GCC has always implemented it as an arithmetic shift in
// practice (both devkitPro's mingw64 and BlocksDS's arm-none-eabi-gcc are
// GCC, so this is not expected to ever actually differ), but the physics
// code being ported relies on this behavior extensively (e.g. the sin/cos
// PC-damping shifts in swos_sprite_update.c), and it must match C#'s
// `int >> int`, which the C# language spec mandates is always arithmetic.
// This removes the reliance on "probably fine in practice" entirely rather
// than adding a second toolchain just to spot-check it.
static inline int32_t swosAsr32(int32_t value, int shift) {
    if (shift <= 0) return value;          // matches C#/C no-op semantics for shift 0
    uint32_t shifted = (uint32_t)value >> shift;
    if (value < 0) {
        // shift is in [1,31] here, so 32-shift is in [1,31] -- safe operand
        // for the unsigned left shift below (never the full 32-bit width).
        shifted |= (uint32_t)0xFFFFFFFFu << (32 - shift);
    }
    return (int32_t)shifted;
}
