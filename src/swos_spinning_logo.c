// SOURCE: openswos game/scripts/Sim/Port/SpinningLogo.cs (see
// swos_spinning_logo.h).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
#include "swos_spinning_logo.h"

#include <stdint.h>

#include "swos_addr.h"
#include "swos_bench.h"
#include "swos_memory.h"
#include "swos_stats.h"

void swosSpinningLogoUpdateSpinningLogo(void)
{
    if (swosReadSignedWord(ADDR_sl_enabled) == 0)
        return;

    bool inBenchMenus = swosBenchInBenchMenus();
    bool postGameStats = swosStatsShowingPostGameStats();
    bool logoSpinning = !inBenchMenus && !postGameStats;

    if (logoSpinning)
    {
        uint16_t tick = swosReadWord(ADDR_currentGameTick);
        if ((tick & 2) != 0)
        {
            int16_t fi = swosReadSignedWord(ADDR_sl_frameIndex);
            fi = (int16_t)((fi + 1) & 0x3f);
            swosWriteWord(ADDR_sl_frameIndex, (uint16_t)fi);
        }
    }

    int16_t frameIndex = swosReadSignedWord(ADDR_sl_frameIndex);
    swosWriteWord(ADDR_sl_pictureIndex, (uint16_t)(SL_BIG_S_SPRITE_START + frameIndex / 2));
}

bool swosSpinningLogoEnabled(void) { return swosReadSignedWord(ADDR_sl_enabled) != 0; }

void swosSpinningLogoSetEnabled(bool enabled)
{
    swosWriteWord(ADDR_sl_enabled, enabled ? 1 : 0);
}

int swosSpinningLogoGetPictureIndex(void) { return swosReadSignedWord(ADDR_sl_pictureIndex); }
