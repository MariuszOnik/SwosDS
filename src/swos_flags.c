// SOURCE: openswos game/scripts/SwosVm/Flags.cs (full file)
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
#include "swos_flags.h"

#include <stdint.h>

SwosFlags g_swosFlags;

void swosFlagsClear(void) {
    g_swosFlags.zero = false;
    g_swosFlags.carry = false;
    g_swosFlags.sign = false;
    g_swosFlags.overflow = false;
}

void swosFlagsSetFromTest16(int val) {
    int16_t s = (int16_t)val;
    g_swosFlags.zero = s == 0;
    g_swosFlags.sign = s < 0;
    g_swosFlags.carry = false;
    g_swosFlags.overflow = false;
}

short swosFlagsSetFromSub16(int dst, int src) {
    int16_t dstSigned = (int16_t)dst;
    int16_t srcSigned = (int16_t)src;
    int32_t res32 = (int32_t)dstSigned - (int32_t)srcSigned;
    int16_t res = (int16_t)res32;
    g_swosFlags.zero = res == 0;
    g_swosFlags.sign = res < 0;
    // Overflow when dst and src have different signs AND result sign != dst sign.
    g_swosFlags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ res)) < 0;
    // Carry (= borrow) when unsigned dst < unsigned src.
    g_swosFlags.carry = (uint16_t)dstSigned < (uint16_t)srcSigned;
    return res;
}

short swosFlagsSetFromAdd16(int dst, int src) {
    int16_t dstSigned = (int16_t)dst;
    int16_t srcSigned = (int16_t)src;
    int32_t res32 = (int32_t)dstSigned + (int32_t)srcSigned;
    int16_t res = (int16_t)res32;
    g_swosFlags.zero = res == 0;
    g_swosFlags.sign = res < 0;
    // Overflow when dst and src have same sign AND result sign differs.
    g_swosFlags.overflow = ((~(dstSigned ^ srcSigned)) & (dstSigned ^ res)) < 0;
    // Unsigned carry-out.
    g_swosFlags.carry = ((uint32_t)(uint16_t)dstSigned + (uint32_t)(uint16_t)srcSigned) > 0xFFFFu;
    return res;
}

short swosFlagsSetFromLogic16(int result) {
    int16_t res = (int16_t)result;
    g_swosFlags.zero = res == 0;
    g_swosFlags.sign = res < 0;
    g_swosFlags.carry = false;
    g_swosFlags.overflow = false;
    return res;
}
