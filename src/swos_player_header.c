// SOURCE: OpenSWOS PlayerHeader.cs, exact functions required by step 6A.
#include "swos_player_header.h"

#include <stdint.h>

#include "swos_addr.h"
#include "swos_memory.h"
#include "swos_player_actions.h"
#include "swos_player_sprite.h"

static int slotFromAddr(int addr) {
    return (addr - PLSPR_SPRITE_POOL_BASE) / PLSPR_SLOT_STRIDE;
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
