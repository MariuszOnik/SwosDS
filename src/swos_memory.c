// SOURCE: openswos game/scripts/SwosVm/Memory.cs:2279-2318 (Read*/Write* helpers)
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
// The full Memory.Init(bool pcMode) is ported separately in swos_memory_init.c.
#include "swos_memory.h"

#include <assert.h>
#include <string.h>

static uint8_t s_mem[SWOS_MEM_SIZE];
static bool s_initialised = false;

// Debug-build-only bounds check (assert() is a no-op under NDEBUG, so a
// release/DS build stays branch-free here). Catches an out-of-range address
// immediately instead of silently reading/writing adjacent state -- cheap
// insurance while ~30k more lines get ported on top of this layer.
static void checkBounds(int addr, int width) {
    assert(addr >= 0 && addr + width <= SWOS_MEM_SIZE);
}

uint8_t swosReadByte(int addr) {
    checkBounds(addr, 1);
    return s_mem[addr];
}

uint16_t swosReadWord(int addr) {
    checkBounds(addr, 2);
    // Cast each byte to uint32_t *before* shifting: shifting a plain
    // uint8_t (promoted to int) by 8 is in range here, but the equivalent
    // dword read below would invoke undefined behaviour at the <<24 step
    // for any byte >= 0x80 (the shifted value wouldn't fit in a signed
    // int). Casting first keeps the whole expression unsigned throughout,
    // so apply the same style to both for one obviously-correct pattern
    // rather than two "it happens to work" ones.
    return (uint16_t)((uint32_t)s_mem[addr] | ((uint32_t)s_mem[addr + 1] << 8));
}

int16_t swosReadSignedWord(int addr) {
    return (int16_t)swosReadWord(addr);
}

uint32_t swosReadDword(int addr) {
    checkBounds(addr, 4);
    return (uint32_t)s_mem[addr]
         | ((uint32_t)s_mem[addr + 1] << 8)
         | ((uint32_t)s_mem[addr + 2] << 16)
         | ((uint32_t)s_mem[addr + 3] << 24);
}

int32_t swosReadSignedDword(int addr) {
    return (int32_t)swosReadDword(addr);
}

void swosWriteByte(int addr, int value) {
    checkBounds(addr, 1);
    s_mem[addr] = (uint8_t)value;
}

void swosWriteWord(int addr, uint16_t value) {
    checkBounds(addr, 2);
    s_mem[addr] = (uint8_t)(value & 0xFF);
    s_mem[addr + 1] = (uint8_t)((value >> 8) & 0xFF);
}

void swosWriteDword(int addr, uint32_t value) {
    checkBounds(addr, 4);
    s_mem[addr] = (uint8_t)(value & 0xFF);
    s_mem[addr + 1] = (uint8_t)((value >> 8) & 0xFF);
    s_mem[addr + 2] = (uint8_t)((value >> 16) & 0xFF);
    s_mem[addr + 3] = (uint8_t)((value >> 24) & 0xFF);
}

void swosMemoryInitStub(void) {
    memset(s_mem, 0, sizeof(s_mem));
    s_initialised = true;
}

void swosMemoryClear(void) {
    memset(s_mem, 0, sizeof(s_mem));
}

void swosMemoryMarkInitialised(void) {
    s_initialised = true;
}

bool swosMemoryIsInitialised(void) {
    return s_initialised;
}

const uint8_t *swosMemoryView(int addr, int length) {
    checkBounds(addr, length);
    return &s_mem[addr];
}
