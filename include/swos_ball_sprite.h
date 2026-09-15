// SOURCE OF IMPLEMENTATION/BEHAVIOUR: openswos game/scripts/SwosVm/BallSprite.cs
// (full file) -- every offset and semantic below is OpenSWOS's, unchanged.
// FIDELITY: VERIFIED_PC.
//
// swos-port used AUXILIARY-ONLY, to understand *why* these offsets look the
// way they do -- never to override them. Its packed `struct Sprite`
// (../swos-port/src/sprites/Sprite.h, 110 bytes, #pragma pack(1)) matches
// every offset here exactly, with one instructive case: OpenSWOS's
// OffFrameIndicesTable widens swos-port's 2-byte `frameIndicesTable` +
// adjoining `tag02` padding field into one 4-byte pointer (their own
// AnimationTablesData system stores real Memory addresses, not swos-port's
// narrower in-table index). That's OpenSWOS's own deliberate design choice,
// kept as-is -- not "corrected" to swos-port's narrower field. If a future
// offset here had turned out to actually DISAGREE with swos-port, the rule
// is: keep OpenSWOS's value, port it as written, and flag the disagreement
// for a separate decision -- never silently prefer swos-port. See
// swos_team_data.h for the one confirmed case in this step where OpenSWOS
// itself is missing a field swos-port has (wonTheBallTimer) -- left out
// there too, matching OpenSWOS, not added.
//
// Ball sprite view over the Memory layer. Mirrors swos-port's
// `swos.ballSprite` struct fields by name, but each accessor reads/writes
// through Memory at fixed byte offsets -- so asm-translated code
// (`readMemory(esi+44, 2)`) and hand-written code both port to the SAME
// backing.
//
// FixedPoint values are SWOS-native Q16.16 (32-bit).
#pragma once

#include <stdint.h>

// Base address inside Memory for the ball sprite. Picked in the sprite
// memory pool region (SWOS_MEM_SIZE = 0x60000, pool starts at 0x4F800).
#define BALLSPR_BASE 0x4F800

// ---- Field offsets within the Sprite struct -----------------------------
#define BALLSPR_OFF_FRAME_INDICES_TABLE 18  // dword -- pointer to frame-index lookup table (see note above)
#define BALLSPR_OFF_FRAME_INDEX         22  // int16 -- current index INTO that table
#define BALLSPR_OFF_FRAME_DELAY         24  // int16 -- animation pacing delay
#define BALLSPR_OFF_CYCLE_FRAMES_TIMER  26  // int16 -- countdown to next frame switch
#define BALLSPR_OFF_X                   30  // FixedPoint Q16.16
#define BALLSPR_OFF_Y                   34  // FixedPoint Q16.16
#define BALLSPR_OFF_Z                   38  // FixedPoint Q16.16
#define BALLSPR_OFF_DIRECTION           42  // int16
#define BALLSPR_OFF_SPEED               44  // int16, Q8.8
#define BALLSPR_OFF_DELTA_X             46  // FixedPoint Q16.16
#define BALLSPR_OFF_DELTA_Y             50  // FixedPoint Q16.16
#define BALLSPR_OFF_DELTA_Z             54  // FixedPoint Q16.16
#define BALLSPR_OFF_DEST_X              58  // int16
#define BALLSPR_OFF_DEST_Y              60  // int16
#define BALLSPR_OFF_IMAGE_INDEX         70  // int16 -- current sprite image (-1 = hidden)
#define BALLSPR_OFF_FULL_DIRECTION      82  // int16 -- 0..255 angle (vs +42 which is 0..7 quantised)

// ---- Q16.16 fixed-point accessors ----------------------------------------
int32_t swosBallSpriteX(void);
void swosBallSpriteSetX(int32_t v);
int32_t swosBallSpriteY(void);
void swosBallSpriteSetY(int32_t v);
int32_t swosBallSpriteZ(void);
void swosBallSpriteSetZ(int32_t v);
int32_t swosBallSpriteDeltaX(void);
void swosBallSpriteSetDeltaX(int32_t v);
int32_t swosBallSpriteDeltaY(void);
void swosBallSpriteSetDeltaY(int32_t v);
int32_t swosBallSpriteDeltaZ(void);
void swosBallSpriteSetDeltaZ(int32_t v);

// Whole-pixel accessors -- high word of Q16.16. SWOS reads these via
// `word ptr [esi + Sprite.x + 2]` (offset +2 inside the 4-byte field).
int16_t swosBallSpriteXPixels(void);
void swosBallSpriteSetXPixels(int16_t v);
int16_t swosBallSpriteYPixels(void);
void swosBallSpriteSetYPixels(int16_t v);
int16_t swosBallSpriteZPixels(void);
void swosBallSpriteSetZPixels(int16_t v);

int16_t swosBallSpriteDirection(void);
void swosBallSpriteSetDirection(int16_t v);
int16_t swosBallSpriteSpeed(void);
void swosBallSpriteSetSpeed(int16_t v);
int16_t swosBallSpriteDestX(void);
void swosBallSpriteSetDestX(int16_t v);
int16_t swosBallSpriteDestY(void);
void swosBallSpriteSetDestY(int16_t v);

// Animation fields -- used by updateBall section 1 (frame index switching).
int32_t swosBallSpriteFrameIndicesTable(void);
void swosBallSpriteSetFrameIndicesTable(int32_t v);
int16_t swosBallSpriteFrameIndex(void);
void swosBallSpriteSetFrameIndex(int16_t v);
int16_t swosBallSpriteFrameDelay(void);
void swosBallSpriteSetFrameDelay(int16_t v);
int16_t swosBallSpriteCycleFramesTimer(void);
void swosBallSpriteSetCycleFramesTimer(int16_t v);
int16_t swosBallSpriteImageIndex(void);
void swosBallSpriteSetImageIndex(int16_t v);
int16_t swosBallSpriteFullDirection(void);
void swosBallSpriteSetFullDirection(int16_t v);
