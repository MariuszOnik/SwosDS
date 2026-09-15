// SOURCE: openswos game/scripts/SwosVm/Rng.cs (full file)
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
#include "swos_rng.h"
#include "swos_tables.h"

#include <stdint.h>

static uint8_t s_seed, s_xorKey, s_xorIndex;
static uint8_t s_seed2, s_xorKey2, s_xorIndex2;

void swosRngReseed(int seed) {
    s_seed = (uint8_t)(seed & 0xFF);
    s_xorIndex = (uint8_t)((seed >> 8) & 0xFF);
    s_xorKey = swos_rngTable[s_xorIndex];

    // Seed stream 2 from a different rotation of the same input so the two
    // streams diverge on tick 0 -- see Rng.cs:91-100 for why.
    s_seed2 = (uint8_t)((seed >> 16) & 0xFF);
    s_xorIndex2 = (uint8_t)((seed >> 24) & 0xFF);
    s_xorKey2 = swos_rngTable[s_xorIndex2];
}

int swosRngNextByte(void) {
    if (s_seed == 0) {
        s_xorIndex = (uint8_t)(s_xorIndex + 1);
        s_xorKey = swos_rngTable[s_xorIndex];
    }
    int result = swos_rngTable[s_seed] ^ s_xorKey;
    s_seed = (uint8_t)(s_seed + 1);
    return result & 0xFF;
}

int swosRngNextWord(void) {
    int lo = swosRngNextByte();
    int hi = swosRngNextByte();
    return lo | (hi << 8);
}

int swosRngNext(void) {
    int lo = swosRngNextWord();
    int hi = swosRngNextWord();
    return lo | (hi << 16);
}

int swosRngNextRange(int max) {
    if (max <= 1) return 0;
    return swosRngNextByte() % max;
}

int swosRngNextByte2(void) {
    if (s_seed2 == 0) {
        s_xorIndex2 = (uint8_t)(s_xorIndex2 + 1);
        s_xorKey2 = swos_rngTable[s_xorIndex2];
    }
    int result = swos_rngTable[s_seed2] ^ s_xorKey2;
    s_seed2 = (uint8_t)(s_seed2 + 1);
    return result & 0xFF;
}

int swosRngNextWord2(void) {
    int lo = swosRngNextByte2();
    int hi = swosRngNextByte2();
    return lo | (hi << 8);
}

int swosRngNextRange2(int max) {
    if (max <= 1) return 0;
    return swosRngNextByte2() % max;
}
