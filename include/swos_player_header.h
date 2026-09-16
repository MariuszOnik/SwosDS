// Minimal real slice of PlayerHeader.cs required by PlayerControlled.cs
// (step 6A) and UpdatePlayers.cs (step 7A).
#pragma once

#include <stdbool.h>
#include <stdint.h>

void swosPlayerAttemptingJumpHeader(int spriteAddr, int direction);
void swosAttemptStaticHeader(int spriteAddr, int direction);

// updatePlayers.cpp:15873. Nudges a static-heading player's facing by one
// step toward team.currentAllowedDirection.
void swosSetStaticHeaderDirection(int spriteAddr, int teamBase);

// updatePlayers.cpp:15380. Sets a no-ball player's destX/destY from tactics
// positioning (outfielders) or goal-mouth ball projection (goalkeepers).
// ballXPx/ballYPx: ball.x/y.whole, or foulX/YCoordinate during set-pieces
// (caller's choice -- see the .c file).
void swosSetPlayerWithNoBallDestination(int spriteAddr, int teamBase,
                                         int16_t ballXPx, int16_t ballYPx);

