// SOURCE: openswos game/scripts/Sim/Port/TacticsLoader.cs (LoadAllTactics
// only). See swos_tactics_loader.h for the full FIDELITY/scope note.
#include "swos_tactics_loader.h"

#include <stdint.h>

#include "swos_addr.h"
#include "swos_memory.h"

#define TACTICS_STRUCT_SIZE 370
#define NUM_TACTICS         19
#define NUM_BUILTIN_TACTICS 12

#include "generated/swos_tactics_data.h"

// g_tacticsTable order (swos.asm:209342-209361), same as TacticsLoader.cs's
// own comment -- slots 0-11 are the built-ins in this exact order.
static const uint8_t *const kBuiltinTactics[NUM_BUILTIN_TACTICS] = {
    Tact442, Tact541, Tact451, Tact532, Tact352, Tact433,
    Tact424, Tact343, TactSweep, Tact523, TactAttack, TactDefend,
};

void swosTacticsLoaderLoadAllTactics(void) {
    for (int slot = 0; slot < NUM_TACTICS; slot++) {
        int slotAddr = ADDR_teamTacticsPool + slot * TACTICS_STRUCT_SIZE;
        if (slot < NUM_BUILTIN_TACTICS) {
            const uint8_t *src = kBuiltinTactics[slot];
            for (int i = 0; i < TACTICS_STRUCT_SIZE; i++)
                swosWriteByte(slotAddr + i, src[i]);
        } else {
            // USER_A..F + editTacticsCurrentTactics -- zero-initialised,
            // matching swos.asm's InitUserTactics (memset(.., 0, 370)).
            for (int i = 0; i < TACTICS_STRUCT_SIZE; i++)
                swosWriteByte(slotAddr + i, 0);
        }
    }
}
