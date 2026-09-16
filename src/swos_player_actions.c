// SOURCE: openswos game/scripts/Sim/Port/PlayerActions.cs:2054-2104
// (SetPlayerAnimationTable ONLY -- see header for why only this one
// function exists here ahead of PlayerActions.cs's own step).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
#include "swos_player_actions.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"

#include <stdint.h>

void swosSetPlayerAnimationTable(int playerAddr, int animTable) {
    // 104312-104314 -- sprite.animationTable = A0.
    swosWriteDword(playerAddr + PLSPR_OFF_ANIM_TABLE_PTR, (uint32_t)animTable);

    // 104316-104324 -- D0 = (teamNumber - 1); if ordinal == 1 -> D0 += 2.
    int16_t teamNum = swosReadSignedWord(playerAddr + PLSPR_OFF_TEAM_NUMBER);
    int16_t d0Index = (int16_t)(teamNum - 1);
    int16_t ord = swosReadSignedWord(playerAddr + PLSPR_OFF_PLAYER_ORDINAL);
    if (ord == 1) {
        d0Index = (int16_t)(d0Index + 2);
    }

    // 104326-104333 -- D0 = (D0 << 3) + direction, then D0 <<= 2 (byte
    // offset into the 32-entry pointer table).
    d0Index = (int16_t)(d0Index << 3);
    int16_t pDir = swosReadSignedWord(playerAddr + PLSPR_OFF_DIRECTION);
    d0Index = (int16_t)(d0Index + pDir);
    int d0Off = ((uint16_t)d0Index & 0xFFFFu) << 2;

    // 104334-104337 -- sprite.frameDelay = *(word*)A0.
    int16_t frameDelay = swosReadSignedWord(animTable);
    swosWriteWord(playerAddr + PLSPR_OFF_FRAME_DELAY, (uint16_t)frameDelay);

    // 104338-104342 -- sprite.frameIndicesTable = *(dword*)(A0 + 2 + D0).
    int32_t fitPtr = swosReadSignedDword(animTable + 2 + d0Off);
    swosWriteDword(playerAddr + PLSPR_OFF_FRAME_INDICES_TABLE, (uint32_t)fitPtr);

    // 104343-104344 -- fatal_error path: animation table has a null pointer
    // at the requested (team, ordinal, direction) slot. The asm `int 3`s
    // here; OpenSWOS's port treats it as a soft no-op -- leaves the sprite
    // in whatever animation it had before (animationTable was already
    // written above, matching the asm which also leaves it updated).
    if (fitPtr == 0) {
        return;
    }

    // 104345-104354 -- reset cycle bookkeeping + cache startingDirection.
    swosWriteWord(playerAddr + PLSPR_OFF_FRAME_SWITCH_COUNTER, (uint16_t)-1);
    swosWriteWord(playerAddr + PLSPR_OFF_FRAME_INDEX, (uint16_t)-1);
    swosWriteWord(playerAddr + PLSPR_OFF_CYCLE_FRAMES_TIMER, 1);
    swosWriteWord(playerAddr + PLSPR_OFF_STARTING_DIRECTION, (uint16_t)pDir);
}
