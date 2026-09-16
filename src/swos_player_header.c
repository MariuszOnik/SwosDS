// SOURCE: OpenSWOS PlayerHeader.cs, exact functions required by steps 6A/7A.
#include "swos_player_header.h"

#include <stdbool.h>
#include <stdint.h>

#include "swos_addr.h"
#include "swos_memory.h"
#include "swos_player_actions.h"
#include "swos_player_sprite.h"
#include "swos_team_data.h"
#include "swos_util.h"

// updatePlayers.cpp:15403-15431 game-state constants.
#define PH_ST_GOAL_OUT_LEFT     1
#define PH_ST_GOAL_OUT_RIGHT    2
#define PH_ST_KEEPER_HOLDS_BALL 3

// Tactics struct field offsets (swos.h:415-421, TeamTactics is 370 bytes).
#define PH_TACTICS_OFF_PLAYER_POS      9
#define PH_TACTICS_OFF_BALL_OUT_OF_PLAY 369

static int slotFromAddr(int addr) {
    return (addr - PLSPR_SPRITE_POOL_BASE) / PLSPR_SLOT_STRIDE;
}

static bool isTopTeam(int teamBase) {
    return teamBase == TEAMDATA_TOP_BASE;
}

static int16_t clamp16(int value, int low, int high) {
    if (value < low) value = low;
    if (value > high) value = high;
    return (int16_t)value;
}

void swosPlayerAttemptingJumpHeader(int spriteAddr, int direction) {
    swosWriteWord(spriteAddr + 98, 0); // Sprite.heading
    swosWriteWord(spriteAddr + PLSPR_OFF_DIRECTION, (uint16_t)direction);
    swosSetPlayerAnimationTable(spriteAddr, ADDR_kJumpHeaderAttemptAnimTableAddr);
    swosWriteByte(spriteAddr + PLSPR_OFF_PLAYER_DOWN_TIMER,
                  (uint8_t)swosReadSignedWord(ADDR_m_playerDownHeadingInterval));
    swosWriteByte(spriteAddr + PLSPR_OFF_PLAYER_STATE, 2); // PL_JUMP_HEADING

    int dst = ADDR_kDefaultDestinations + (direction << 2);
    int slot = slotFromAddr(spriteAddr);
    int x = swosPlayerSpriteXPixels(slot) + swosReadSignedWord(dst);
    int y = swosPlayerSpriteYPixels(slot) + swosReadSignedWord(dst + 2);
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_X, (uint16_t)clamp16(x, 81, 590));
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_Y, (uint16_t)clamp16(y, 129, 769));
    swosWriteWord(spriteAddr + PLSPR_OFF_SPEED,
                  swosReadWord(ADDR_kJumpHeaderSpeed));
}

void swosAttemptStaticHeader(int spriteAddr, int direction) {
    swosWriteWord(spriteAddr + 98, 0); // Sprite.heading
    swosWriteWord(spriteAddr + PLSPR_OFF_DIRECTION, (uint16_t)direction);
    int dst = ADDR_kDefaultDestinations + (direction << 2);
    int slot = slotFromAddr(spriteAddr);
    int x = swosPlayerSpriteXPixels(slot) + swosReadSignedWord(dst);
    int y = swosPlayerSpriteYPixels(slot) + swosReadSignedWord(dst + 2);
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_X, (uint16_t)clamp16(x, 81, 590));
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_Y, (uint16_t)clamp16(y, 129, 769));
    swosWriteWord(spriteAddr + PLSPR_OFF_SPEED,
                  swosReadWord(ADDR_kStaticHeaderPlayerSpeed));
    swosSetPlayerAnimationTable(spriteAddr, ADDR_kStaticHeaderAttemptAnimTableAddr);
    swosWriteByte(spriteAddr + PLSPR_OFF_PLAYER_STATE, 8); // PL_STATIC_HEADING
    swosWriteByte(spriteAddr + PLSPR_OFF_PLAYER_DOWN_TIMER, 20);
}

// updatePlayers.cpp:15873.
void swosSetStaticHeaderDirection(int spriteAddr, int teamBase) {
    int16_t curAllowedDir = swosReadSignedWord(teamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION);
    if (curAllowedDir < 0) return;

    // downTimer holds [0..55] so unsigned/signed `ja`/> agree in this range.
    int downTimer = swosReadByte(spriteAddr + PLSPR_OFF_PLAYER_DOWN_TIMER) & 0xFF;
    if (downTimer > 18) return;

    int32_t animTable = swosReadSignedDword(spriteAddr + PLSPR_OFF_ANIM_TABLE_PTR);
    if (animTable != ADDR_kStaticHeaderAttemptAnimTableAddr) return;

    int16_t pDir = swosReadSignedWord(spriteAddr + PLSPR_OFF_DIRECTION);
    int16_t diff = (int16_t)(curAllowedDir - pDir);
    if (diff == 0) return;

    diff &= 7;
    if (diff == 4) return; // can't pick a side for 180 degrees.

    if (diff < 4) {
        pDir = (int16_t)(pDir + 1);
    } else {
        pDir = (int16_t)(pDir - 1);
    }

    pDir = (int16_t)(pDir & 7);
    swosWriteWord(spriteAddr + PLSPR_OFF_DIRECTION, (uint16_t)pDir);
}

// updatePlayers.cpp:15676 -- goalkeeper branch of SetPlayerWithNoBallDestination.
// Projects the ball's x/y onto the goal-mouth band using the original's
// 16x16->32 signed multiply followed by an UNSIGNED 32-bit divide (the
// asm `imul`+`div` idiom -- preserved exactly, not "fixed" to a plain
// signed divide).
static void handleGoalkeeperBranch(int spriteAddr, bool topTeam,
                                    int16_t d6Ball, int16_t d7Ball) {
    // --- X axis ---
    int16_t d2 = 285;
    int16_t d3 = 387;
    int32_t d0 = d6Ball - 81;
    int32_t d1 = (d3 - d2) + 1;
    int32_t product = (int16_t)d0 * (int16_t)d1;
    uint16_t axMul = (uint16_t)product;
    uint16_t dxMul = (uint16_t)swosAsr32(product, 16);
    uint32_t dividend = ((uint32_t)dxMul << 16) | axMul;
    uint16_t quot = (uint16_t)(dividend / 510u);
    int16_t d0Result = (int16_t)quot;
    int16_t destX = (int16_t)(d0Result + d2);
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_X, (uint16_t)destX);

    // --- Y axis --- default top-team values (135,161); bottom-team (737,763).
    d2 = 135;
    d3 = 161;
    if (!topTeam) {
        d2 = 737;
        d3 = 763;
    }
    d0 = d7Ball - 129;
    d1 = (d3 - d2) + 1;
    product = (int16_t)d0 * (int16_t)d1;
    axMul = (uint16_t)product;
    dxMul = (uint16_t)swosAsr32(product, 16);
    dividend = ((uint32_t)dxMul << 16) | axMul;
    quot = (uint16_t)(dividend / 641u);
    d0Result = (int16_t)quot;
    int16_t destY = (int16_t)(d0Result + d2);
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_Y, (uint16_t)destY);
}

// updatePlayers.cpp:15380.
void swosSetPlayerWithNoBallDestination(int spriteAddr, int teamBase,
                                         int16_t ballXPx, int16_t ballYPx) {
    bool topTeam = isTopTeam(teamBase);

    int tacticsIdx = swosReadSignedWord(teamBase + TEAMDATA_OFF_TACTICS);
    int tableEntry = ADDR_g_tacticsTable + (tacticsIdx << 2);
    int32_t tacticsPtr = swosReadSignedDword(tableEntry);

    int16_t gameState = swosReadSignedWord(ADDR_gameState);
    bool ballOutOfPlay = (gameState == PH_ST_KEEPER_HOLDS_BALL)
                       || (gameState == PH_ST_GOAL_OUT_LEFT)
                       || (gameState == PH_ST_GOAL_OUT_RIGHT);

    if (ballOutOfPlay) {
        // Sign-extend byte -> int (cbw), then index g_tacticsTable << 2.
        int8_t oop = (int8_t)swosReadByte(tacticsPtr + PH_TACTICS_OFF_BALL_OUT_OF_PLAY);
        int oopEntry = ADDR_g_tacticsTable + (oop << 2);
        tacticsPtr = swosReadSignedDword(oopEntry);
    }

    int playerPosBase = tacticsPtr + PH_TACTICS_OFF_PLAYER_POS;

    int playerOrdinal = swosReadSignedWord(spriteAddr + PLSPR_OFF_PLAYER_ORDINAL);
    int outfieldIdx = playerOrdinal - 2;

    if (outfieldIdx < 0) {
        handleGoalkeeperBranch(spriteAddr, topTeam, ballXPx, ballYPx);
        return;
    }

    int posRowOff = outfieldIdx * 35;

    int16_t d3 = swosReadSignedWord(ADDR_playerXQuadrantOffset);
    int16_t d4 = swosReadSignedWord(ADDR_playerYQuadrantOffset);

    int16_t ballQuad = swosReadSignedWord(ADDR_ballQuadrantIndex);

    int posIndex;
    int d1Byte;
    if (topTeam) {
        posIndex = posRowOff + ballQuad;
        d1Byte = swosReadByte(playerPosBase + posIndex);
    } else {
        int mirror = 34 - ballQuad;
        posIndex = posRowOff + mirror;
        int posByte = swosReadByte(playerPosBase + posIndex);
        d1Byte = (uint8_t)(239 - posByte); // 0xEF
    }

    int d2i = d1Byte & 0x0F;
    int d1i = (d1Byte >> 4) & 0x0F;

    int16_t xCoord = swosReadSignedWord(ADDR_playerXQuadrantsCoordinates + (d1i << 1));
    d3 = (int16_t)(d3 + xCoord);

    int16_t yCoord = swosReadSignedWord(ADDR_playerYQuadrantCoordinates + (d2i << 1));
    d4 = (int16_t)(d4 + yCoord);
    d3 = (int16_t)(d3 - 4);

    if (!topTeam) d3 = (int16_t)(d3 + 8);

    if (d3 < 81) d3 = 81;
    if (d3 > 590) d3 = 590;
    if (d4 < 129) d4 = 129;
    if (d4 > 769) d4 = 769;

    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_X, (uint16_t)d3);
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_Y, (uint16_t)d4);
}
