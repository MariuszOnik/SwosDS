// See player_anim.h. Tables copied verbatim from
// ../../swos-ds/source/player.c (kRunningFrames/kStandingFrames/
// RUN_FRAME_HOLD_TICKS) -- real data extracted from the original game's
// animation tables, not fabricated. swos-ds is left unmodified; this is a
// standalone copy for this project's own renderer.
#include "player_anim.h"

#define DIR_COUNT 8
#define RUN_FRAME_HOLD_TICKS 6

static const unsigned char kRunningFrames[DIR_COUNT][4] = {
    /* top          */ { 2,  0,  1,  0},
    /* top_right    */ {23, 22, 21, 22},
    /* right        */ { 8,  6,  7,  6},
    /* bottom_right */ {17, 16, 15, 16},
    /* bottom       */ { 5,  3,  4,  3},
    /* bottom_left  */ {14, 13, 12, 13},
    /* left         */ {11,  9, 10,  9},
    /* top_left     */ {20, 19, 18, 19},
};

static const unsigned char kStandingFrames[DIR_COUNT] = {
    /* top          */  0,
    /* top_right    */ 22,
    /* right        */  6,
    /* bottom_right */ 16,
    /* bottom       */  3,
    /* bottom_left  */ 13,
    /* left         */  9,
    /* top_left     */ 19,
};

int playerAnimGetFrame(int direction, int animTick, bool isMoving)
{
    if (direction < 0 || direction >= DIR_COUNT)
        direction = 0;

    if (!isMoving)
        return kStandingFrames[direction];

    int step = (animTick / RUN_FRAME_HOLD_TICKS) % 4;
    return kRunningFrames[direction][step];
}
