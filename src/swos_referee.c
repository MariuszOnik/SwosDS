// SOURCE: openswos game/scripts/Sim/Port/Referee.cs (see swos_referee.h
// for the exact slice ported).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
#include "swos_referee.h"
#include "swos_addr.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"
#include "swos_rng.h"
#include "swos_util.h"

#include <stdint.h>

// referee.cpp:24 RefereeState.
#define REF_STATE_INCOMING 1

// pitchConstants.h:3.
#define REF_PITCH_CENTER_X 336
#define REF_PITCH_CENTER_Y 449

#define REF_SPEED 1024

// camera.cpp:82, referee.cpp:60 -- Camera.GetCameraYWhole(). Minimal
// forward-pull (not the rest of Camera.cs, 573 lines, not called from
// anything ported so far): read cameraY (Q16.16) and take the whole-pixel
// part via an arithmetic shift (swosAsr32, not a plain >>, since cameraY
// is a signed dword and C11 leaves >> on a negative signed value
// implementation-defined -- see swos_util.h).
static int cameraGetYWhole(void) {
    int32_t cameraY = swosReadSignedDword(ADDR_cameraY);
    return swosAsr32(cameraY, 16);
}

// referee.cpp:277-286 -- initRefereeAnimationTable.
static void initRefereeAnimationTable(int animTableAddr) {
    int16_t delay = swosReadSignedWord(animTableAddr);
    int16_t direction = swosReadSignedWord(REFSPR_BASE + PLSPR_OFF_DIRECTION);

    int frameTablePtr = swosReadSignedDword(animTableAddr + 2 + direction * 4);

    swosWriteWord(REFSPR_BASE + PLSPR_OFF_FRAME_DELAY, (uint16_t)delay);
    swosWriteDword(REFSPR_BASE + PLSPR_OFF_FRAME_INDICES_TABLE, (uint32_t)frameTablePtr);

    swosWriteWord(REFSPR_BASE + PLSPR_OFF_FRAME_SWITCH_COUNTER, (uint16_t)-1);
    swosWriteWord(REFSPR_BASE + PLSPR_OFF_FRAME_INDEX, (uint16_t)-1);
    swosWriteWord(REFSPR_BASE + PLSPR_OFF_CYCLE_FRAMES_TIMER, 1);
}

// gameSprites.cpp:101-111 -- markDisplaySpritesDirty (see Referee.cs's own
// comment: tells the host renderer the sprite set changed).
static void markDisplaySpritesDirty(void) {
    swosWriteWord(ADDR_displaySpritesDirtyFlag, 1);
}

// referee.cpp:50-75 -- activateReferee.
void swosRefereeActivate(void) {
    // Telemetry omitted (DbgActivations/DbgYellowCards/DbgRedCards/
    // DbgSecondYellowCards -- zero Memory effect, see header).

    int16_t foulX = swosReadSignedWord(ADDR_foulXCoordinate);
    int16_t foulY = swosReadSignedWord(ADDR_foulYCoordinate);

    swosWriteWord(REFSPR_BASE + PLSPR_OFF_DEST_X, (uint16_t)(foulX + 28));
    swosWriteWord(REFSPR_BASE + PLSPR_OFF_DEST_Y, (uint16_t)(foulY + 5));

    // referee.cpp:55-58 -- random horizontal offset for starting position.
    int xOffset = swosRngNextByte() / 8;
    if (foulX >= REF_PITCH_CENTER_X)
        xOffset = -xOffset;

    // referee.cpp:60-64 -- starting Y depends on which half the foul was on.
    int cameraY = cameraGetYWhole();
    int refStartY = cameraY - 20;
    if (foulY <= REF_PITCH_CENTER_Y)
        refStartY = cameraY + 215;

    int destX = swosReadSignedWord(REFSPR_BASE + PLSPR_OFF_DEST_X);
    swosWriteWord(REFSPR_BASE + PLSPR_OFF_X + 2, (uint16_t)(destX + xOffset));
    swosWriteWord(REFSPR_BASE + PLSPR_OFF_Y + 2, (uint16_t)refStartY);

    swosWriteWord(REFSPR_BASE + PLSPR_OFF_SPEED, REF_SPEED);
    swosWriteWord(REFSPR_BASE + PLSPR_OFF_VISIBLE, 1);

    markDisplaySpritesDirty();
    initRefereeAnimationTable(ADDR_refComingAnimTable);

    swosWriteWord(ADDR_refState, REF_STATE_INCOMING);
    // Telemetry omitted (DbgEnteredIncoming -- zero Memory effect, see header).
}
