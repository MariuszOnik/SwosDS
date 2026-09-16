// DS-adapter-only: sprite-frame selection for rendering. NOT part of the
// mechanical C#-port (there is no OpenSWOS source for "which atlas pixel to
// draw" -- OpenSWOS/swos-port target a different renderer entirely). The
// running/standing frame TABLES themselves are copied verbatim from
// ../../swos-ds/source/player.c's kRunningFrames/kStandingFrames, which are
// real data: local atlas indices (local = global - 341) derived from
// swos-port's own animationTables.in (playerRunning<Dir>Team1Frames /
// team1PlayerStandingFacing<Dir>Frames). Not fabricated placeholder frames.
//
// direction: 0..7, same convention as swos-vm-c's IC_FACING_* (see
// swos_input_controls.h) / PLSPR_OFF_DIRECTION -- 0=top, clockwise.
#pragma once

#include <stdbool.h>

int playerAnimGetFrame(int direction, int animTick, bool isMoving);
