// SOURCE: openswos game/scripts/Sim/Port/SpriteUpdate.cs (full file)
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
//
// Ports from swos-port's src/sprites/updateSprite.cpp -- shared sprite
// helpers used by ball, player, keeper, and referee update paths.
//
// Calls into swosSetPlayerAnimationTable() (swos_player_actions.h), a
// forward-pulled dependency from PlayerActions.cs -- see that header for
// why only one function exists there ahead of PlayerActions.cs's own step.
#pragma once

#include <stdint.h>

// Mirrors updateSprite.cpp:91-95.
typedef struct {
    int32_t deltaX;    // FixedPoint Q16.16
    int32_t deltaY;    // FixedPoint Q16.16
    int32_t direction; // -1 = no movement, else 0..255
} SwosDeltasAndAngle;

// updateSprite.cpp:231-336. Computes motion vector toward destination.
// speed: Q8.8 fixed point. x/y/destX/destY: whole pixels.
SwosDeltasAndAngle swosCalculateDeltaXAndY(int speed, int x, int y, int destX, int destY);

// Convenience overload that takes a Sprite address inside Memory and writes
// the result back to the sprite's deltaX, deltaY, direction, fullDirection.
// Mirrors updateSpriteDirectionAndDeltas (updateSprite.cpp:101-112).
void swosUpdateSpriteDirectionAndDeltas(int spriteBase);

// movePlayers -- updateSprite.cpp:145-155. Per-tick sprite delta -> position
// integration for the 22 player slots.
void swosMoveAllPlayers(void);

// SetNextPlayerFrame -- swos.asm:102834-102971. Per-tick animation tick +
// goal-cheer overlay for one player sprite.
void swosSetNextPlayerFrame(int spriteBase);

// updateSpriteAnimation -- updateSprite.cpp:114-143. Per-tick animation
// advance (no frameOffset add, no goal-cheer -- see SetNextPlayerFrame for
// the full version players use).
void swosUpdateSpriteAnimation(int spriteBase);

// moveSprite -- updateSprite.cpp:157-186. Integrates the sprite's Q16.16
// position by its Q16.16 delta, clamping to destination on overshoot.
void swosMoveSprite(int spriteBase);
