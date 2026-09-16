// SOURCE: openswos game/scripts/Sim/Port/SpriteUpdate.cs (full file)
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
#include "swos_sprite_update.h"
#include "swos_addr.h"
#include "swos_memory.h"
#include "swos_player_actions.h"
#include "swos_player_sprite.h"
#include "swos_tables.h"
#include "swos_util.h"

#include <stdbool.h>
#include <stdint.h>

static void stopSpriteIfReachedDestination(int spriteBase, int destX, int destY);
static void updateAnimationTableAndDestinationReached(int spriteBase);

// updateSprite.cpp:231-336.
SwosDeltasAndAngle swosCalculateDeltaXAndY(int speed, int x, int y, int destX, int destY) {
    SwosDeltasAndAngle result = { 0, 0, -1 };

    // updateSprite.cpp:233-238 -- split deltaX into sign + magnitude.
    bool xNegative = false;
    int deltaX = destX - x;
    if (deltaX < 0) { xNegative = true; deltaX = -deltaX; }

    // updateSprite.cpp:240-245 -- split deltaY into sign + magnitude.
    bool yNegative = false;
    int deltaY = destY - y;
    if (deltaY < 0) { yNegative = true; deltaY = -deltaY; }

    // updateSprite.cpp:247-250 -- halve repeatedly until both fit in [0, 32),
    // preserving ratio so kAngleTangent table (32x32) can be indexed.
    while (deltaX >= 32 || deltaY >= 32) {
        deltaX /= 2;
        deltaY /= 2;
    }

    // updateSprite.cpp:252 -- table lookup. kAngleTangent[deltaY][deltaX]
    // returns angle 0..64 (1st quadrant). -1 if both deltas are 0.
    int angle = swos_angleTangent[deltaY][deltaX];

    if (angle < 0) return result;  // no movement (deltaY==0 && deltaX==0)

    // updateSprite.cpp:267-291 -- transform 1st-quadrant angle to full 0..255
    // circle based on sign of (deltaX, deltaY).
    if (xNegative) {
        if (yNegative)
            angle = 192 - angle;
        else
            angle += 192;
    } else {
        if (yNegative)
            angle += 64;
        else
            angle = 64 - angle;
    }
    angle &= 0xff;

    // updateSprite.cpp:304 -- convert to SWOS direction convention (0 = top, CW).
    result.direction = (256 - angle + 128) & 0xff;

    // updateSprite.cpp:307-308 -- fetch sin/cos. cos = sine[angle],
    // sin = sine[(angle + 64) & 0xFF].
    int cos = swos_sineCosineTable[angle];
    int sin = swos_sineCosineTable[(angle + 64) & 0xff];

    // updateSprite.cpp:319-320 -- scale by speed, shift down to Q16.16.
    // swosAsr32 (not plain >>): sin*speed / cos*speed can be negative, and
    // C11 leaves >> on a negative signed operand implementation-defined --
    // must match C#'s int>>int, which the C# spec guarantees is arithmetic.
    sin = swosAsr32(sin * speed, 8);
    cos = swosAsr32(cos * speed, 8);

    // updateSprite.cpp:323-329 -- PC mode damping (x 41/64 ~= 0.640625).
    // OpenSWOS is hard-locked to PC mode. Translates the 41/64 multiplier
    // into shifts: result = x - x/4 - x/16 - x/32 - x/64.
    sin = sin - swosAsr32(sin, 2) - swosAsr32(sin, 4) - swosAsr32(sin, 5) - swosAsr32(sin, 6);
    cos = cos - swosAsr32(cos, 2) - swosAsr32(cos, 4) - swosAsr32(cos, 5) - swosAsr32(cos, 6);

    result.deltaX = cos;
    result.deltaY = sin;
    return result;
}

void swosUpdateSpriteDirectionAndDeltas(int spriteBase) {
    int x = swosReadSignedWord(spriteBase + PLSPR_OFF_X + 2);      // x.whole()
    int y = swosReadSignedWord(spriteBase + PLSPR_OFF_Y + 2);      // y.whole()
    int destX = swosReadSignedWord(spriteBase + PLSPR_OFF_DEST_X);
    int destY = swosReadSignedWord(spriteBase + PLSPR_OFF_DEST_Y);
    int speed = swosReadSignedWord(spriteBase + PLSPR_OFF_SPEED);

    SwosDeltasAndAngle result = swosCalculateDeltaXAndY(speed, x, y, destX, destY);

    swosWriteDword(spriteBase + PLSPR_OFF_DELTA_X, (uint32_t)result.deltaX);
    swosWriteDword(spriteBase + PLSPR_OFF_DELTA_Y, (uint32_t)result.deltaY);
    swosWriteWord(spriteBase + PLSPR_OFF_FULL_DIRECTION, (uint16_t)result.direction);
    // updateSprite.cpp:111 -- direction = ((fullDirection + 16) & 0xff) >> 5
    // gives 8-direction quantisation (0..7) from full 0..255 angle. Applied
    // UNCONDITIONALLY: for direction == -1 (no movement) it yields
    // (15 & 0xff) >> 5 = 0, NOT -1 -- writing -1 here would poison every
    // dir*4 table index downstream. Match it exactly.
    int dir8 = ((result.direction + 16) & 0xff) >> 5;
    swosWriteWord(spriteBase + PLSPR_OFF_DIRECTION, (uint16_t)dir8);
}

// movePlayers -- updateSprite.cpp:145-155.
// SetNextPlayerFrame (animation index walk) IS ported here (unlike the
// C++ split which only does frame movement) -- see swosSetNextPlayerFrame.
void swosMoveAllPlayers(void) {
    for (int slot = 0; slot < PLSPR_TOTAL_SLOTS; slot++) {
        int spriteBase = swosPlayerSpriteBase(slot);
        swosSetNextPlayerFrame(spriteBase);
        swosMoveSprite(spriteBase);
        updateAnimationTableAndDestinationReached(spriteBase);
    }
}

// SetNextPlayerFrame -- swos.asm:102834-102971. See SpriteUpdate.cs's own
// header comment for the full rationale (frameOffset bake-in, goal-cheer,
// and the direction-change re-bind that is an OpenSWOS addition, not in
// the original asm, suppressed for goalie-save/injured states per bugs
// #178/#179 -- see the C# source comment for the full incident writeup).
void swosSetNextPlayerFrame(int spriteBase) {
    // 102836-102838 -- if (!sprite.onScreen) return.
    int16_t onScreen = swosReadSignedWord(spriteBase + PLSPR_OFF_ON_SCREEN);
    if (onScreen == 0) return;

    // --- Direction-change re-bind (OpenSWOS addition, not in asm) -------
    int16_t curDir = swosReadSignedWord(spriteBase + PLSPR_OFF_DIRECTION);
    int16_t startDir = swosReadSignedWord(spriteBase + PLSPR_OFF_STARTING_DIRECTION);
    uint8_t rebindState = swosReadByte(spriteBase + PLSPR_OFF_PLAYER_STATE);
    bool suppressRebind = rebindState == 4 || rebindState == 6
        || rebindState == 7 || rebindState == 11   // goalie saves (#178/#179)
        || rebindState == 13;                       // PL_ROLLING_INJURED writhe
    if (curDir != startDir && !suppressRebind) {
        int32_t animTable = swosReadSignedDword(spriteBase + PLSPR_OFF_ANIM_TABLE_PTR);
        if (animTable > 0 && animTable < 0x60000) {
            swosSetPlayerAnimationTable(spriteBase, animTable);
        }
    }

    // 102840-102841 -- if (--sprite.cycleFramesTimer != 0) return.
    int16_t timer = swosReadSignedWord(spriteBase + PLSPR_OFF_CYCLE_FRAMES_TIMER);
    timer = (int16_t)(timer - 1);
    swosWriteWord(spriteBase + PLSPR_OFF_CYCLE_FRAMES_TIMER, (uint16_t)timer);
    if (timer != 0) return;

    // 102842-102847 -- sprite.frameIndex++; cycleFramesTimer = frameDelay.
    int16_t frameIndex = swosReadSignedWord(spriteBase + PLSPR_OFF_FRAME_INDEX);
    frameIndex = (int16_t)(frameIndex + 1);
    int16_t frameDelay = swosReadSignedWord(spriteBase + PLSPR_OFF_FRAME_DELAY);
    swosWriteWord(spriteBase + PLSPR_OFF_CYCLE_FRAMES_TIMER, (uint16_t)frameDelay);

    // 102848-102850 -- A1 = sprite.frameIndicesTable.
    int32_t fitPtr = swosReadSignedDword(spriteBase + PLSPR_OFF_FRAME_INDICES_TABLE);
    if (fitPtr <= 0 || fitPtr > 0x60000) {
        swosWriteWord(spriteBase + PLSPR_OFF_FRAME_INDEX, (uint16_t)frameIndex);
        return;
    }

    // 102852-102893 -- @@next_frame_index loop.
    const int kLastFrameLoopMarker = -999;
    const int kLastFrameHoldMarker = -101;
    const int kFrameLoopbackMarker = -100;

    int d0;
    int iter = 0;
    bool exitToOut = false;
    while (true) {
        if (++iter > 16) break;

        // 102854-102861 -- D0 = *(word*)(A1 + frameIndex*2).
        d0 = swosReadSignedWord(fitPtr + (uint16_t)frameIndex * 2);

        if (d0 >= 0) break;

        if (d0 == kLastFrameLoopMarker) {
            frameIndex = 0;
            continue;
        }

        if (d0 == kLastFrameHoldMarker) {
            frameIndex = (int16_t)(frameIndex - 1);
            exitToOut = true;
            break;
        }

        if (d0 <= kFrameLoopbackMarker) {
            int offset = d0 - kFrameLoopbackMarker;
            frameIndex = (int16_t)(frameIndex + offset);
            continue;
        }

        // 102870-102879 -- variable-pause opcode (-1..-99).
        int newDelay = -d0;
        swosWriteWord(spriteBase + PLSPR_OFF_FRAME_DELAY, (uint16_t)newDelay);
        swosWriteWord(spriteBase + PLSPR_OFF_CYCLE_FRAMES_TIMER, (uint16_t)newDelay);
        frameIndex = (int16_t)(frameIndex + 1);
    }

    swosWriteWord(spriteBase + PLSPR_OFF_FRAME_INDEX, (uint16_t)frameIndex);
    if (exitToOut) return;

    // 102902-102907 -- @@index_positive: frameSwitchCounter += 1;
    // D0 += sprite.frameOffset (final picture index).
    int16_t fsCounter = swosReadSignedWord(spriteBase + PLSPR_OFF_FRAME_SWITCH_COUNTER);
    fsCounter = (int16_t)(fsCounter + 1);
    swosWriteWord(spriteBase + PLSPR_OFF_FRAME_SWITCH_COUNTER, (uint16_t)fsCounter);

    d0 = swosReadSignedWord(fitPtr + (uint16_t)frameIndex * 2);
    int16_t frameOffset = swosReadSignedWord(spriteBase + PLSPR_OFF_FRAME_OFFSET);
    d0 = (int16_t)(d0 + frameOffset);

    // 102908-102960 -- goal-cheer overlay.
    bool cheer = false;
    int16_t goal = swosReadSignedWord(ADDR_goalScored);
    if (goal != 0) {
        uint8_t playerState = swosReadByte(spriteBase + PLSPR_OFF_PLAYER_STATE);
        if (playerState == 0) { // PL_NORMAL
            int16_t dir = swosReadSignedWord(spriteBase + PLSPR_OFF_DIRECTION);
            if (dir == 0 || dir == 4) {
                int16_t lastTeamScored = swosReadSignedWord(ADDR_lastTeamScoredNumber);
                int16_t teamNum = swosReadSignedWord(spriteBase + PLSPR_OFF_TEAM_NUMBER);
                if (lastTeamScored == teamNum) {
                    int16_t ord = swosReadSignedWord(spriteBase + PLSPR_OFF_PLAYER_ORDINAL);
                    if (ord != 1) { // not a keeper
                        int32_t lastPlayerScored = swosReadSignedDword(ADDR_lastPlayerScored);
                        uint16_t tick = swosReadWord(ADDR_currentGameTick);
                        if (spriteBase == lastPlayerScored) {
                            // 102939-102943 -- cheering 78.90625% of time.
                            if ((tick & 0x7F) <= 100) cheer = true;
                        } else {
                            // 102948-102955 -- cheering 50% of time.
                            int d1 = ((ord & 0xFFFF) << 2) + tick;
                            if ((d1 & 0x3F) <= 31) cheer = true;
                        }
                    }
                }
            }
        }
    }
    if (cheer) {
        // 102959-102960 -- add 365, sub 341 -> +24.
        d0 = (int16_t)(d0 + 24);
    }

    // 102962-102966 -- @@set_picture_index: sprite.imageIndex = D0.
    swosWriteWord(spriteBase + PLSPR_OFF_IMAGE_INDEX, (uint16_t)(int16_t)d0);
}

// updateSpriteAnimation -- updateSprite.cpp:114-143.
void swosUpdateSpriteAnimation(int spriteBase) {
    const int kLastFrameLoopMarker = -999;
    const int kLastFrameHoldMarker = -101;
    const int kFrameLoopbackMarker = -100;

    // updateSprite.cpp:116 -- gate on onScreen && --cycleFramesTimer == 0.
    int16_t onScreen = swosReadSignedWord(spriteBase + PLSPR_OFF_ON_SCREEN);
    if (onScreen == 0) return;

    int16_t timer = swosReadSignedWord(spriteBase + PLSPR_OFF_CYCLE_FRAMES_TIMER);
    timer = (int16_t)(timer - 1);
    swosWriteWord(spriteBase + PLSPR_OFF_CYCLE_FRAMES_TIMER, (uint16_t)timer);
    if (timer != 0) return;

    // updateSprite.cpp:117-118 -- advance frameIndex; reload timer from frameDelay.
    int16_t frameIndex = swosReadSignedWord(spriteBase + PLSPR_OFF_FRAME_INDEX);
    frameIndex = (int16_t)(frameIndex + 1);
    int16_t frameDelay = swosReadSignedWord(spriteBase + PLSPR_OFF_FRAME_DELAY);
    swosWriteWord(spriteBase + PLSPR_OFF_CYCLE_FRAMES_TIMER, (uint16_t)frameDelay);

    int32_t fitPtr = swosReadSignedDword(spriteBase + PLSPR_OFF_FRAME_INDICES_TABLE);
    if (fitPtr <= 0 || fitPtr > 0x60000) {
        swosWriteWord(spriteBase + PLSPR_OFF_FRAME_INDEX, (uint16_t)frameIndex);
        return;
    }

    // updateSprite.cpp:122-141 -- do/while(frame < 0).
    int frame;
    int iter = 0;
    do {
        if (++iter > 16) break;

        frame = swosReadSignedWord(fitPtr + (uint16_t)frameIndex * 2);

        if (frame >= 0) {
            int16_t fsCounter = swosReadSignedWord(spriteBase + PLSPR_OFF_FRAME_SWITCH_COUNTER);
            fsCounter = (int16_t)(fsCounter + 1);
            swosWriteWord(spriteBase + PLSPR_OFF_FRAME_SWITCH_COUNTER, (uint16_t)fsCounter);
            swosWriteWord(spriteBase + PLSPR_OFF_IMAGE_INDEX, (uint16_t)(int16_t)frame);
        } else if (frame == kLastFrameLoopMarker) {
            frameIndex = 0;
        } else if (frame == kLastFrameHoldMarker) {
            frameIndex = (int16_t)(frameIndex - 1);
            break;
        } else if (frame <= kFrameLoopbackMarker) {
            int offset = frame - kFrameLoopbackMarker;
            frameIndex = (int16_t)(frameIndex + offset);
        } else {
            int newDelay = -frame;
            swosWriteWord(spriteBase + PLSPR_OFF_FRAME_DELAY, (uint16_t)newDelay);
            swosWriteWord(spriteBase + PLSPR_OFF_CYCLE_FRAMES_TIMER, (uint16_t)newDelay);
            frameIndex = (int16_t)(frameIndex + 1);
        }
    } while (frame < 0);

    swosWriteWord(spriteBase + PLSPR_OFF_FRAME_INDEX, (uint16_t)frameIndex);
}

// moveSprite -- updateSprite.cpp:157-186.
void swosMoveSprite(int spriteBase) {
    int destX = swosReadSignedWord(spriteBase + PLSPR_OFF_DEST_X);
    int destY = swosReadSignedWord(spriteBase + PLSPR_OFF_DEST_Y);
    int32_t destXq = (int32_t)destX << 16;
    int32_t destYq = (int32_t)destY << 16;

    // updateSprite.cpp:159-170 -- X integration.
    int32_t deltaX = swosReadSignedDword(spriteBase + PLSPR_OFF_DELTA_X);
    if (deltaX != 0) {
        int32_t xRaw = swosReadSignedDword(spriteBase + PLSPR_OFF_X);
        xRaw += deltaX;
        bool reachedX = deltaX > 0 ? destXq <= xRaw : destXq >= xRaw;
        if (reachedX) {
            xRaw = destXq;
            deltaX = 0;
            swosWriteDword(spriteBase + PLSPR_OFF_DELTA_X, 0);
        }
        swosWriteDword(spriteBase + PLSPR_OFF_X, (uint32_t)xRaw);
    }

    // updateSprite.cpp:172-183 -- Y integration.
    int32_t deltaY = swosReadSignedDword(spriteBase + PLSPR_OFF_DELTA_Y);
    if (deltaY != 0) {
        int32_t yRaw = swosReadSignedDword(spriteBase + PLSPR_OFF_Y);
        yRaw += deltaY;
        bool reachedY = deltaY > 0 ? destYq <= yRaw : destYq >= yRaw;
        if (reachedY) {
            yRaw = destYq;
            deltaY = 0;
            swosWriteDword(spriteBase + PLSPR_OFF_DELTA_Y, 0);
        }
        swosWriteDword(spriteBase + PLSPR_OFF_Y, (uint32_t)yRaw);
    }

    // updateSprite.cpp:185 -- stopSpriteIfReachedDestination (belt-and-braces).
    stopSpriteIfReachedDestination(spriteBase, destX, destY);
}

// stopSpriteIfReachedDestination -- updateSprite.cpp:188-213.
static void stopSpriteIfReachedDestination(int spriteBase, int destX, int destY) {
    int32_t deltaX = swosReadSignedDword(spriteBase + PLSPR_OFF_DELTA_X);
    if (deltaX != 0) {
        int32_t xRaw = swosReadSignedDword(spriteBase + PLSPR_OFF_X);
        int32_t destXq = (int32_t)destX << 16;
        bool stop = (deltaX > 0 && destXq <= xRaw) || (deltaX < 0 && destXq >= xRaw);
        if (stop) {
            swosWriteDword(spriteBase + PLSPR_OFF_X, (uint32_t)destXq);
            swosWriteDword(spriteBase + PLSPR_OFF_DELTA_X, 0);
        }
    }

    int32_t deltaY = swosReadSignedDword(spriteBase + PLSPR_OFF_DELTA_Y);
    if (deltaY != 0) {
        int32_t yRaw = swosReadSignedDword(spriteBase + PLSPR_OFF_Y);
        int32_t destYq = (int32_t)destY << 16;
        bool stop = (deltaY > 0 && destYq <= yRaw) || (deltaY < 0 && destYq >= yRaw);
        if (stop) {
            swosWriteDword(spriteBase + PLSPR_OFF_Y, (uint32_t)destYq);
            swosWriteDword(spriteBase + PLSPR_OFF_DELTA_Y, 0);
        }
    }
}

// updateAnimationTableAndDestinationReached -- updateSprite.cpp:215-229.
static void updateAnimationTableAndDestinationReached(int spriteBase) {
    // updateSprite.cpp:217 -- onScreen || gameStatePl != kInProgress.
    int16_t onScreen = swosReadSignedWord(spriteBase + PLSPR_OFF_ON_SCREEN);
    int16_t gameStatePl = swosReadSignedWord(ADDR_gameStatePl);
    const int16_t kStInProgress = 100;
    bool onScreenOrStopped = (onScreen != 0) || (gameStatePl != kStInProgress);

    // updateSprite.cpp:218 -- state == kNormal && stationary().
    uint8_t state = swosReadByte(spriteBase + PLSPR_OFF_PLAYER_STATE);
    int32_t dx = swosReadSignedDword(spriteBase + PLSPR_OFF_DELTA_X);
    int32_t dy = swosReadSignedDword(spriteBase + PLSPR_OFF_DELTA_Y);
    bool stationary = (dx == 0 && dy == 0);

    if (!(onScreenOrStopped && state == 0 && stationary)) return;

    // updateSprite.cpp:220-222 -- promote destReachedState 2 (kTraveling) ->
    // 3 (kReached) when game stopped AND breakCameraMode == 3.
    int16_t breakCameraMode = swosReadSignedWord(ADDR_breakCameraMode);
    if (gameStatePl != kStInProgress && breakCameraMode == 3) {
        int16_t reachedState = swosReadSignedWord(spriteBase + PLSPR_OFF_DEST_REACHED_STATE);
        if (reachedState == 2) // kTraveling
            swosWriteWord(spriteBase + PLSPR_OFF_DEST_REACHED_STATE, 3); // kReached
    }

    // updateSprite.cpp:223-227 -- switch to standing anim table if not already there.
    int32_t currentAnimTable = swosReadSignedDword(spriteBase + PLSPR_OFF_ANIM_TABLE_PTR);
    if (currentAnimTable != ADDR_kPlayerStandingAnimTableAddr) {
        swosSetPlayerAnimationTable(spriteBase, ADDR_kPlayerStandingAnimTableAddr);
    }
}
