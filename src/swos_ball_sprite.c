// SOURCE: openswos game/scripts/SwosVm/BallSprite.cs (full file)
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
#include "swos_ball_sprite.h"
#include "swos_memory.h"

int32_t swosBallSpriteX(void) { return swosReadSignedDword(BALLSPR_BASE + BALLSPR_OFF_X); }
void swosBallSpriteSetX(int32_t v) { swosWriteDword(BALLSPR_BASE + BALLSPR_OFF_X, (uint32_t)v); }
int32_t swosBallSpriteY(void) { return swosReadSignedDword(BALLSPR_BASE + BALLSPR_OFF_Y); }
void swosBallSpriteSetY(int32_t v) { swosWriteDword(BALLSPR_BASE + BALLSPR_OFF_Y, (uint32_t)v); }
int32_t swosBallSpriteZ(void) { return swosReadSignedDword(BALLSPR_BASE + BALLSPR_OFF_Z); }
void swosBallSpriteSetZ(int32_t v) { swosWriteDword(BALLSPR_BASE + BALLSPR_OFF_Z, (uint32_t)v); }
int32_t swosBallSpriteDeltaX(void) { return swosReadSignedDword(BALLSPR_BASE + BALLSPR_OFF_DELTA_X); }
void swosBallSpriteSetDeltaX(int32_t v) { swosWriteDword(BALLSPR_BASE + BALLSPR_OFF_DELTA_X, (uint32_t)v); }
int32_t swosBallSpriteDeltaY(void) { return swosReadSignedDword(BALLSPR_BASE + BALLSPR_OFF_DELTA_Y); }
void swosBallSpriteSetDeltaY(int32_t v) { swosWriteDword(BALLSPR_BASE + BALLSPR_OFF_DELTA_Y, (uint32_t)v); }
int32_t swosBallSpriteDeltaZ(void) { return swosReadSignedDword(BALLSPR_BASE + BALLSPR_OFF_DELTA_Z); }
void swosBallSpriteSetDeltaZ(int32_t v) { swosWriteDword(BALLSPR_BASE + BALLSPR_OFF_DELTA_Z, (uint32_t)v); }

int16_t swosBallSpriteXPixels(void) { return swosReadSignedWord(BALLSPR_BASE + BALLSPR_OFF_X + 2); }
void swosBallSpriteSetXPixels(int16_t v) { swosWriteWord(BALLSPR_BASE + BALLSPR_OFF_X + 2, (uint16_t)v); }
int16_t swosBallSpriteYPixels(void) { return swosReadSignedWord(BALLSPR_BASE + BALLSPR_OFF_Y + 2); }
void swosBallSpriteSetYPixels(int16_t v) { swosWriteWord(BALLSPR_BASE + BALLSPR_OFF_Y + 2, (uint16_t)v); }
int16_t swosBallSpriteZPixels(void) { return swosReadSignedWord(BALLSPR_BASE + BALLSPR_OFF_Z + 2); }
void swosBallSpriteSetZPixels(int16_t v) { swosWriteWord(BALLSPR_BASE + BALLSPR_OFF_Z + 2, (uint16_t)v); }

int16_t swosBallSpriteDirection(void) { return swosReadSignedWord(BALLSPR_BASE + BALLSPR_OFF_DIRECTION); }
void swosBallSpriteSetDirection(int16_t v) { swosWriteWord(BALLSPR_BASE + BALLSPR_OFF_DIRECTION, (uint16_t)v); }
int16_t swosBallSpriteSpeed(void) { return swosReadSignedWord(BALLSPR_BASE + BALLSPR_OFF_SPEED); }
void swosBallSpriteSetSpeed(int16_t v) { swosWriteWord(BALLSPR_BASE + BALLSPR_OFF_SPEED, (uint16_t)v); }
int16_t swosBallSpriteDestX(void) { return swosReadSignedWord(BALLSPR_BASE + BALLSPR_OFF_DEST_X); }
void swosBallSpriteSetDestX(int16_t v) { swosWriteWord(BALLSPR_BASE + BALLSPR_OFF_DEST_X, (uint16_t)v); }
int16_t swosBallSpriteDestY(void) { return swosReadSignedWord(BALLSPR_BASE + BALLSPR_OFF_DEST_Y); }
void swosBallSpriteSetDestY(int16_t v) { swosWriteWord(BALLSPR_BASE + BALLSPR_OFF_DEST_Y, (uint16_t)v); }

int32_t swosBallSpriteFrameIndicesTable(void) { return swosReadSignedDword(BALLSPR_BASE + BALLSPR_OFF_FRAME_INDICES_TABLE); }
void swosBallSpriteSetFrameIndicesTable(int32_t v) { swosWriteDword(BALLSPR_BASE + BALLSPR_OFF_FRAME_INDICES_TABLE, (uint32_t)v); }
int16_t swosBallSpriteFrameIndex(void) { return swosReadSignedWord(BALLSPR_BASE + BALLSPR_OFF_FRAME_INDEX); }
void swosBallSpriteSetFrameIndex(int16_t v) { swosWriteWord(BALLSPR_BASE + BALLSPR_OFF_FRAME_INDEX, (uint16_t)v); }
int16_t swosBallSpriteFrameDelay(void) { return swosReadSignedWord(BALLSPR_BASE + BALLSPR_OFF_FRAME_DELAY); }
void swosBallSpriteSetFrameDelay(int16_t v) { swosWriteWord(BALLSPR_BASE + BALLSPR_OFF_FRAME_DELAY, (uint16_t)v); }
int16_t swosBallSpriteCycleFramesTimer(void) { return swosReadSignedWord(BALLSPR_BASE + BALLSPR_OFF_CYCLE_FRAMES_TIMER); }
void swosBallSpriteSetCycleFramesTimer(int16_t v) { swosWriteWord(BALLSPR_BASE + BALLSPR_OFF_CYCLE_FRAMES_TIMER, (uint16_t)v); }
int16_t swosBallSpriteImageIndex(void) { return swosReadSignedWord(BALLSPR_BASE + BALLSPR_OFF_IMAGE_INDEX); }
void swosBallSpriteSetImageIndex(int16_t v) { swosWriteWord(BALLSPR_BASE + BALLSPR_OFF_IMAGE_INDEX, (uint16_t)v); }
int16_t swosBallSpriteFullDirection(void) { return swosReadSignedWord(BALLSPR_BASE + BALLSPR_OFF_FULL_DIRECTION); }
void swosBallSpriteSetFullDirection(int16_t v) { swosWriteWord(BALLSPR_BASE + BALLSPR_OFF_FULL_DIRECTION, (uint16_t)v); }
