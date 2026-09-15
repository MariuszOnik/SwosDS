// SOURCE: openswos game/scripts/SwosVm/Flags.cs (full file)
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
//
// 68k/x86 flag-register emulation. swos-port's disassembly sets these flags
// after each compare/arithmetic op so the next branch can read them; we mirror
// swos-port's SwosVM::flags as a global struct (SWOS's VM is single-threaded).
#pragma once

#include <stdbool.h>

typedef struct {
    bool zero;
    bool carry;
    bool sign;
    bool overflow;
} SwosFlags;

extern SwosFlags g_swosFlags;

void swosFlagsClear(void);

// tst.w -- sets zero+sign from a value, clears carry+overflow.
void swosFlagsSetFromTest16(int val);

// sub.w / cmp.w -- flags after `dst - src` at 16-bit width. Returns the
// truncated 16-bit result so the caller can write it back to dst (or discard
// for cmp).
short swosFlagsSetFromSub16(int dst, int src);

// add.w -- flags after `dst + src` at 16-bit width.
short swosFlagsSetFromAdd16(int dst, int src);

// and.w / or.w -- zero+sign from result, carry+overflow cleared.
short swosFlagsSetFromLogic16(int result);
