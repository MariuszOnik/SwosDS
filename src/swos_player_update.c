// SOURCE: openswos game/scripts/Sim/Port/PlayerUpdate.cs:58-92
// (UpdateBallWithControllingGoalkeeper ONLY -- see header for why).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
#include "swos_player_update.h"
#include "swos_addr.h"
#include "swos_ball_sprite.h"
#include "swos_ball_update.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"
#include "swos_util.h"

#include <stdint.h>

void swosUpdateBallWithControllingGoalkeeper(int controllingPlayerAddr) {
    // player.cpp:202-208 -- dir = sprite.direction; byteOffset = dir << 2.
    int dir = swosReadSignedWord(controllingPlayerAddr + PLSPR_OFF_DIRECTION);
    int byteOffset = dir << 2; // each entry is 4 bytes (dx word + dy word)

    // player.cpp:210-220 -- newX = player.x.whole + kBallPlOffsets[byteOffset].
    int playerX = swosReadSignedWord(controllingPlayerAddr + PLSPR_OFF_X + 2);
    int playerY = swosReadSignedWord(controllingPlayerAddr + PLSPR_OFF_Y + 2);

    int offsX = swosReadSignedWord(ADDR_kBallPlOffsetsBase + byteOffset);
    int offsY = swosReadSignedWord(ADDR_kBallPlOffsetsBase + byteOffset + 2);

    int16_t newX = (int16_t)(playerX + offsX);
    int16_t newY = (int16_t)(playerY + offsY);

    // player.cpp:232-242 -- write ball position + destination + clear speed.
    swosBallSpriteSetSpeed(0);
    swosBallSpriteSetXPixels(newX);
    swosBallSpriteSetYPixels(newY);
    swosBallSpriteSetDestX(newX);
    swosBallSpriteSetDestY(newY);

    // player.cpp:243-263 -- sar deltaZ by 1 (preserves sign), then negate
    // ONLY if the result was positive. Net effect: dz <= 0 always (falling).
    int32_t dz = swosBallSpriteDeltaZ();
    int32_t dzHalf = swosAsr32(dz, 1);
    if (dzHalf > 0) dzHalf = -dzHalf;
    swosBallSpriteSetDeltaZ(dzHalf);

    // player.cpp:264 -- call resetBothTeamSpinTimers.
    swosResetBothTeamSpinTimers();
}
