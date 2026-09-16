// SOURCE: openswos game/scripts/Sim/Port/SetPieces.cs (full file, step 10
// of the porting order).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
//
// Step-7B/9 boundary this file closes: UpdatePlayers.cs (step 7B) has two
// genuinely executed call sites into SetPieces.cs --
// SetThrowInPlayerDestinationCoordinates (the l_player_taking_throw_in
// setup tail) and TickThrowIn (the PortPlayerState.kThrowIn dispatch arm).
// Both got assert-backed hooks at 7B, matching the AiBrain/AiHelpers
// convention from step 6A/9. The hook globals stay in place (per the
// user's explicit step-9 instruction: keep them as named, stable call
// points) and are now statically initialised to the real implementations
// below -- see swos_set_pieces.c.
//
// One new real dependency found by the comment-filtered scan:
// AdvancePenaltiesTimer calls GameTime.NextPenalty() (swos_game_time.h,
// this same step) when the inter-pen pause timer expires.
#pragma once

#include <stdbool.h>

typedef void (*SwosSetThrowInPlayerDestHook)(int spriteAddr);
typedef void (*SwosTickThrowInHook)(int throwerSpriteAddr, int ballSpriteAddr,
                                     int teamBase);

extern SwosSetThrowInPlayerDestHook g_swosSetThrowInPlayerDestHook;
extern SwosTickThrowInHook g_swosTickThrowInHook;

// Named, stable call points (mirrors swosRunControlledBranch's AiBrain/
// AiHelpers hook calls). The hook globals above are statically initialised
// to the real implementations as of this step, so the assert is now a
// plain defensive null-check -- it can only fire from a deliberate test
// override, not from an unported step.
void swosSetPiecesSetThrowInPlayerDestinationCoordinates(int spriteAddr);
void swosSetPiecesTickThrowIn(int throwerSpriteAddr, int ballSpriteAddr,
                               int teamBase);

// ===================================================================
// DispatchByGameState -- set-piece state machine entry point
// ===================================================================
// Routes a per-tick set-piece spawn/update based on `gameState`. See
// SetPieces.cs for the full original comment (not currently called from
// anywhere ported -- kept for parity/future GameLoop wiring, step 11).
void swosSetPiecesDispatchByGameState(int a1PlayerAddr, int a2BallAddr,
                                       int a5PlayerAddr, int a6TeamBase);

// ===================================================================
// TickSetPieces -- per-tick corner / goal-kick / throw-in / penalty /
// free-kick auto-resolver. See SetPieces.cs for the full rationale (a
// pragmatic ball+kicker snap-and-park in place of the not-yet-ported
// break-camera FSM). Not currently called from anywhere ported -- GameLoop
// (step 11) is its real caller; kept for parity/future wiring.
// ===================================================================
void swosSetPiecesTickSetPieces(void);

// updatePlayers.cpp:16320-16344 -- per-tick corner/goal-kick/free-kick/
// penalty AI turn-direction handlers. Not currently called from anywhere
// ported (same GameLoop-step-11 caller as TickSetPieces); kept for parity.
void swosSetPiecesTickCorner(int a6TeamBase);
void swosSetPiecesTickGoalKick(int a6TeamBase);
void swosSetPiecesTickFreeKick(int a6TeamBase);
void swosSetPiecesTickPenalty(int a5PlayerAddr, int a6TeamBase);

// gameLoop.cpp:430-470 -- AdvancePenaltiesTimer. Inter-pen pause +
// NextPenalty trigger. Not currently called from anywhere ported (GameLoop
// step 11); kept for parity/future wiring.
void swosSetPiecesAdvancePenaltiesTimer(void);
