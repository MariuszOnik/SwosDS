// SOURCE: openswos game/scripts/SwosVm/Memory.cs:2279-2318 (Read*/Write* helpers)
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
// See swos_memory.h for what's NOT yet ported (Memory.Init).
#include "swos_memory.h"

#include <string.h>

static uint8_t s_mem[SWOS_MEM_SIZE];
static bool s_initialised = false;

uint8_t swosReadByte(int addr) {
    return s_mem[addr];
}

uint16_t swosReadWord(int addr) {
    return (uint16_t)(s_mem[addr] | (s_mem[addr + 1] << 8));
}

int16_t swosReadSignedWord(int addr) {
    return (int16_t)swosReadWord(addr);
}

uint32_t swosReadDword(int addr) {
    return (uint32_t)(s_mem[addr]
                     | (s_mem[addr + 1] << 8)
                     | (s_mem[addr + 2] << 16)
                     | (s_mem[addr + 3] << 24));
}

int32_t swosReadSignedDword(int addr) {
    return (int32_t)swosReadDword(addr);
}

void swosWriteByte(int addr, int value) {
    s_mem[addr] = (uint8_t)value;
}

void swosWriteWord(int addr, int value) {
    s_mem[addr] = (uint8_t)(value & 0xFF);
    s_mem[addr + 1] = (uint8_t)((value >> 8) & 0xFF);
}

void swosWriteDword(int addr, int value) {
    s_mem[addr] = (uint8_t)(value & 0xFF);
    s_mem[addr + 1] = (uint8_t)((value >> 8) & 0xFF);
    s_mem[addr + 2] = (uint8_t)((value >> 16) & 0xFF);
    s_mem[addr + 3] = (uint8_t)((value >> 24) & 0xFF);
}

void swosMemoryInitStub(void) {
    memset(s_mem, 0, sizeof(s_mem));
    s_initialised = true;
}

bool swosMemoryIsInitialised(void) {
    return s_initialised;
}

const uint8_t *swosMemoryView(int addr, int length) {
    (void)length;
    return &s_mem[addr];
}
