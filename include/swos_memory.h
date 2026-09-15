// SOURCE: openswos game/scripts/SwosVm/Memory.cs
//   - Read*/Write* helpers (Memory.cs:2279-2313): FIDELITY VERIFIED_PC, direct port.
//   - swosMemoryInit (Memory.cs:1477-2271, `Memory.Init`): NOT YET PORTED.
//     Its body calls PlayerSprite.Init() / AnimationTablesData.Init() /
//     TeamData.Init(), none of which exist in this repo yet (see README.md
//     "Porting order" -- those are step 2, this Memory module is step 1).
//     Porting Init() now would either be dead code or require stubbing those
//     three calls, which would misrepresent it as complete. Call
//     swosMemoryInitStub() for now to zero the buffer and unblock testing the
//     Read/Write layer; replace with the real port once step 2 lands.
//
// Mirrors swos-port's g_memByte[] global byte array -- the runtime image of
// the original DOS .DATA segment + sprite memory pool. All translated
// functions read/write through these offsets (see swos_addr.h for the named
// constants). Endian is little-endian (x86 DOS -- swos-port preserves this
// even though the original Amiga was big-endian; the source being translated
// is IDA's disassembly of the PC binary).
#pragma once

#include <stdbool.h>
#include <stdint.h>

// 384 KB -- see Memory.cs:23-38 for the full layout breakdown (DOS .DATA
// segment 0x00000..0x4F7FF, sprite pool 0x4F800..0x5FFFF).
#define SWOS_MEM_SIZE 0x60000

// ---- Read helpers -----------------------------------------------------
uint8_t swosReadByte(int addr);
uint16_t swosReadWord(int addr);          // unsigned word
int16_t swosReadSignedWord(int addr);
uint32_t swosReadDword(int addr);
int32_t swosReadSignedDword(int addr);

// ---- Write helpers ----------------------------------------------------
void swosWriteByte(int addr, int value);
void swosWriteWord(int addr, int value);
void swosWriteDword(int addr, int value);

// ---- Lifecycle ----------------------------------------------------------
// Zeroes the backing buffer only. Placeholder for the real swosMemoryInit()
// (Memory.cs:1477 `Memory.Init`) -- see the file-level comment above.
void swosMemoryInitStub(void);

bool swosMemoryIsInitialised(void);

// Direct backing access (debugging/asserts only -- mirrors Memory.View).
const uint8_t *swosMemoryView(int addr, int length);
