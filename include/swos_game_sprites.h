// SOURCE: openswos game/scripts/Sim/Port/GameSprites.cs (full file, step 11
// of the porting order -- real local dependency of GameLoop.cs).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
//
// EXCLUDED (matches the C#'s own exclusion): drawSprites, sortDisplaySprites,
// shouldZoomSprite, verifySprites, initDisplaySprites, initGameSprites,
// initializePlayerSpriteFrameIndices -- all pure render / renderer-owned
// sprite-bind setup.
//
// The out-of-range-shirt-number diagnostic (Godot.GD.PrintErr in
// UpdateControlledPlayerNumbers) is a debounced debug print with zero
// Memory effect -- omitted, matching every other debug-print omission in
// this port (e.g. GameLoop's own "[PORT-SAFETY]" print). The guarded branch
// itself (hide the digit instead of drawing a wrong number) IS ported.
#pragma once

#include <stdbool.h>

// sprites.h:62-65.
#define GS_CORNER_FLAG_SPRITE_START 1184
#define GS_SMALL_DIGIT1             1188

// gameSprites.cpp:254-257.
#define GS_LEFT_CORNER_FLAG_X   81
#define GS_RIGHT_CORNER_FLAG_X  590
#define GS_TOP_CORNER_FLAG_Y    129
#define GS_BOTTOM_CORNER_FLAG_Y 769

// Per-flag stride within ADDR_cornerFlags. +0 word imageIndex, +2 word x,
// +4 word y.
#define GS_CORNER_FLAG_STRIDE           6
#define GS_CORNER_FLAG_OFF_IMAGE_INDEX  0
#define GS_CORNER_FLAG_OFF_X            2
#define GS_CORNER_FLAG_OFF_Y            4

// gameSprites.cpp:283 (sic, "kPlayerNumberOfset" in the source).
#define GS_PLAYER_NUMBER_OFFSET 20

// Per-sprite stride within ADDR_curPlayerNumSprites. +0 imageIndex, +2 x,
// +4 y, +6 z.
#define GS_CUR_PLAYER_NUM_STRIDE          8
#define GS_CUR_PLAYER_NUM_OFF_IMAGE_INDEX 0
#define GS_CUR_PLAYER_NUM_OFF_X           2
#define GS_CUR_PLAYER_NUM_OFF_Y           4
#define GS_CUR_PLAYER_NUM_OFF_Z           6

// gameSprites.cpp:252-279 -- per-tick refresh of all four corner-flag sprites.
void swosGameSpritesUpdateCornerFlags(void);

// gameSprites.cpp:281-312 -- per-tick controlled-player shirt-number badge.
void swosGameSpritesUpdateControlledPlayerNumbers(void);

// gameSprites.cpp:231-250 -- pure lookup helpers.
int swosGameSpritesGetPlayerSpriteOffsetFromFace(int face);
int swosGameSpritesGetGoalkeeperSpriteOffset(bool topTeam, int face);

// Renderer accessors.
void swosGameSpritesGetCornerFlag(int index, int *x, int *y, int *imageIndex);
void swosGameSpritesGetCurPlayerNumSprite(int teamIndex, int *x, int *y, int *z, int *imageIndex);
