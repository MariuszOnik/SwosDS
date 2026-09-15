// SOURCE: openswos game/scripts/SwosVm/PlayerSprite.cs (full file)
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
#include "swos_player_sprite.h"
#include "swos_memory.h"

int swosPlayerSpriteBase(int slot) {
    return PLSPR_SPRITE_POOL_BASE + slot * PLSPR_SLOT_STRIDE;
}

int swosPlayerSpriteFirstSlotForTeam(bool top) {
    return top ? 0 : PLSPR_TEAM_SIZE;
}

int swosPlayerSpriteGoalieSlot(bool top) {
    return top ? PLSPR_SLOT_GOALIE1 : PLSPR_SLOT_GOALIE2;
}

bool swosPlayerSpriteIsGoalie(int slot) {
    return slot == PLSPR_SLOT_GOALIE1 || slot == PLSPR_SLOT_GOALIE2;
}

int32_t swosPlayerSpriteX(int slot) { return swosReadSignedDword(swosPlayerSpriteBase(slot) + PLSPR_OFF_X); }
void swosPlayerSpriteSetX(int slot, int32_t v) { swosWriteDword(swosPlayerSpriteBase(slot) + PLSPR_OFF_X, (uint32_t)v); }
int32_t swosPlayerSpriteY(int slot) { return swosReadSignedDword(swosPlayerSpriteBase(slot) + PLSPR_OFF_Y); }
void swosPlayerSpriteSetY(int slot, int32_t v) { swosWriteDword(swosPlayerSpriteBase(slot) + PLSPR_OFF_Y, (uint32_t)v); }
int32_t swosPlayerSpriteZ(int slot) { return swosReadSignedDword(swosPlayerSpriteBase(slot) + PLSPR_OFF_Z); }
void swosPlayerSpriteSetZ(int slot, int32_t v) { swosWriteDword(swosPlayerSpriteBase(slot) + PLSPR_OFF_Z, (uint32_t)v); }

int16_t swosPlayerSpriteXPixels(int slot) { return swosReadSignedWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_X + 2); }
void swosPlayerSpriteSetXPixels(int slot, int16_t v) { swosWriteWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_X + 2, (uint16_t)v); }
int16_t swosPlayerSpriteYPixels(int slot) { return swosReadSignedWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_Y + 2); }
void swosPlayerSpriteSetYPixels(int slot, int16_t v) { swosWriteWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_Y + 2, (uint16_t)v); }
int16_t swosPlayerSpriteZPixels(int slot) { return swosReadSignedWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_Z + 2); }
void swosPlayerSpriteSetZPixels(int slot, int16_t v) { swosWriteWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_Z + 2, (uint16_t)v); }

int32_t swosPlayerSpriteDeltaX(int slot) { return swosReadSignedDword(swosPlayerSpriteBase(slot) + PLSPR_OFF_DELTA_X); }
void swosPlayerSpriteSetDeltaX(int slot, int32_t v) { swosWriteDword(swosPlayerSpriteBase(slot) + PLSPR_OFF_DELTA_X, (uint32_t)v); }
int32_t swosPlayerSpriteDeltaY(int slot) { return swosReadSignedDword(swosPlayerSpriteBase(slot) + PLSPR_OFF_DELTA_Y); }
void swosPlayerSpriteSetDeltaY(int slot, int32_t v) { swosWriteDword(swosPlayerSpriteBase(slot) + PLSPR_OFF_DELTA_Y, (uint32_t)v); }
int32_t swosPlayerSpriteDeltaZ(int slot) { return swosReadSignedDword(swosPlayerSpriteBase(slot) + PLSPR_OFF_DELTA_Z); }
void swosPlayerSpriteSetDeltaZ(int slot, int32_t v) { swosWriteDword(swosPlayerSpriteBase(slot) + PLSPR_OFF_DELTA_Z, (uint32_t)v); }

int16_t swosPlayerSpriteDirection(int slot) { return swosReadSignedWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_DIRECTION); }
void swosPlayerSpriteSetDirection(int slot, int16_t v) { swosWriteWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_DIRECTION, (uint16_t)v); }
int16_t swosPlayerSpriteSpeed(int slot) { return swosReadSignedWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_SPEED); }
void swosPlayerSpriteSetSpeed(int slot, int16_t v) { swosWriteWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_SPEED, (uint16_t)v); }
int16_t swosPlayerSpriteDestX(int slot) { return swosReadSignedWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_DEST_X); }
void swosPlayerSpriteSetDestX(int slot, int16_t v) { swosWriteWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_DEST_X, (uint16_t)v); }
int16_t swosPlayerSpriteDestY(int slot) { return swosReadSignedWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_DEST_Y); }
void swosPlayerSpriteSetDestY(int slot, int16_t v) { swosWriteWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_DEST_Y, (uint16_t)v); }

int16_t swosPlayerSpriteTeamNumber(int slot) { return swosReadSignedWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_TEAM_NUMBER); }
void swosPlayerSpriteSetTeamNumber(int slot, int16_t v) { swosWriteWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_TEAM_NUMBER, (uint16_t)v); }
int16_t swosPlayerSpritePlayerOrdinal(int slot) { return swosReadSignedWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_PLAYER_ORDINAL); }
void swosPlayerSpriteSetPlayerOrdinal(int slot, int16_t v) { swosWriteWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_PLAYER_ORDINAL, (uint16_t)v); }

uint8_t swosPlayerSpritePlayerState(int slot) { return swosReadByte(swosPlayerSpriteBase(slot) + PLSPR_OFF_PLAYER_STATE); }
void swosPlayerSpriteSetPlayerState(int slot, uint8_t v) { swosWriteByte(swosPlayerSpriteBase(slot) + PLSPR_OFF_PLAYER_STATE, v); }
int8_t swosPlayerSpritePlayerDownTimer(int slot) { return (int8_t)swosReadByte(swosPlayerSpriteBase(slot) + PLSPR_OFF_PLAYER_DOWN_TIMER); }
void swosPlayerSpriteSetPlayerDownTimer(int slot, int8_t v) { swosWriteByte(swosPlayerSpriteBase(slot) + PLSPR_OFF_PLAYER_DOWN_TIMER, (uint8_t)v); }

int16_t swosPlayerSpriteImageIndex(int slot) { return swosReadSignedWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_IMAGE_INDEX); }
void swosPlayerSpriteSetImageIndex(int slot, int16_t v) { swosWriteWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_IMAGE_INDEX, (uint16_t)v); }

int32_t swosPlayerSpriteBallDistance(int slot) { return swosReadSignedDword(swosPlayerSpriteBase(slot) + PLSPR_OFF_BALL_DISTANCE); }
void swosPlayerSpriteSetBallDistance(int slot, int32_t v) { swosWriteDword(swosPlayerSpriteBase(slot) + PLSPR_OFF_BALL_DISTANCE, (uint32_t)v); }

int16_t swosPlayerSpriteFullDirection(int slot) { return swosReadSignedWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_FULL_DIRECTION); }
void swosPlayerSpriteSetFullDirection(int slot, int16_t v) { swosWriteWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_FULL_DIRECTION, (uint16_t)v); }

int16_t swosPlayerSpriteTackleState(int slot) { return swosReadSignedWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_TACKLE_STATE); }
void swosPlayerSpriteSetTackleState(int slot, int16_t v) { swosWriteWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_TACKLE_STATE, (uint16_t)v); }
int16_t swosPlayerSpriteTacklingTimer(int slot) { return swosReadSignedWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_TACKLING_TIMER); }
void swosPlayerSpriteSetTacklingTimer(int slot, int16_t v) { swosWriteWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_TACKLING_TIMER, (uint16_t)v); }

void swosPlayerSpriteInit(void) {
    // Clear all 22 slot regions (in case Memory's real Init() didn't already).
    for (int slot = 0; slot < PLSPR_TOTAL_SLOTS; slot++) {
        int base = swosPlayerSpriteBase(slot);
        for (int i = 0; i < PLSPR_SPRITE_SIZE; i++)
            swosWriteByte(base + i, 0);

        // Default PlayerDirection = -1 (matches swos-port init for non-players).
        swosWriteWord(base + PLSPR_OFF_PLAYER_DIRECTION, (uint16_t)-1);

        // Sprite::init() -- set onScreen=1 and a sane animation-state default
        // so the per-tick updateSpriteAnimation can advance frames. Without
        // onScreen the short-circuit `!--cycleFramesTimer` is never tested
        // and the sprite freezes. frameIndex=-1, frameDelay=5,
        // cycleFramesTimer=1, frameSwitchCounter=-1.
        swosWriteWord(base + PLSPR_OFF_ON_SCREEN, 1);
        swosWriteWord(base + PLSPR_OFF_FRAME_INDEX, (uint16_t)-1);
        swosWriteWord(base + PLSPR_OFF_FRAME_DELAY, 5);
        swosWriteWord(base + PLSPR_OFF_CYCLE_FRAMES_TIMER, 1);
        swosWriteWord(base + PLSPR_OFF_FRAME_SWITCH_COUNTER, (uint16_t)-1);
        swosWriteWord(base + PLSPR_OFF_IMAGE_INDEX, (uint16_t)-1);  // hasNoImage() -> true
    }

    // Populate per-team sprite tables: each entry holds the absolute Memory
    // address of its corresponding slot.
    for (int i = 0; i < PLSPR_TEAM_SIZE; i++) {
        swosWriteDword(PLSPR_TEAM1_TABLE_BASE + i * 4, (uint32_t)swosPlayerSpriteBase(i));
        swosWriteDword(PLSPR_TEAM2_TABLE_BASE + i * 4, (uint32_t)swosPlayerSpriteBase(PLSPR_TEAM_SIZE + i));
    }

    // Set keeper ordinal (1) and outfielder ordinals (2..11) for each team.
    // swos-port uses ordinal=1 to mark the goalkeeper.
    for (int t = 0; t < 2; t++) {
        int firstSlot = t == 0 ? 0 : PLSPR_TEAM_SIZE;
        swosWriteWord(swosPlayerSpriteBase(firstSlot) + PLSPR_OFF_PLAYER_ORDINAL, 1);  // keeper
        swosWriteWord(swosPlayerSpriteBase(firstSlot) + PLSPR_OFF_TEAM_NUMBER, (uint16_t)(t + 1));
        for (int o = 1; o < PLSPR_TEAM_SIZE; o++) {
            swosWriteWord(swosPlayerSpriteBase(firstSlot + o) + PLSPR_OFF_PLAYER_ORDINAL, (uint16_t)(o + 1));  // 2..11
            swosWriteWord(swosPlayerSpriteBase(firstSlot + o) + PLSPR_OFF_TEAM_NUMBER, (uint16_t)(t + 1));
        }
    }
}
