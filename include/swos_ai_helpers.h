// SOURCE: openswos game/scripts/Sim/Port/AiHelpers.cs (full file, step 9 of
// the porting order). The small AI helpers (~700 LOC of the original asm);
// AI_SetControlsDirection itself lives in AiBrain.cs / swos_ai_brain.h.
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
//
// Comment-filtered dependency scan: every real call in this file is to
// Memory/TeamData/PlayerSprite/BallSprite, all already ported. Zero new
// dependencies.
#pragma once

#include <stdbool.h>

// updatePlayers.cpp:19313. Fires a kick when the controlling player's
// facing direction is close enough to the opponent's controlled player's
// allowedDirections angle. a1PlayerAddr: the controlling player sprite;
// a6TeamBase: that player's team.
void swosAiHelpersAiKick(int a1PlayerAddr, int a6TeamBase);

// updatePlayers.cpp:19461. Sets team.currentAllowedDirection toward the
// opposing goal + ball-x lane, gated on AI_counter and AI_attackHalf.
void swosAiHelpersSetDirectionTowardOpponentsGoal(int a6TeamBase);

// updatePlayers.cpp:19577. Decides whether the AI should press fire this
// frame, given the controlled player's facing direction (d7Direction),
// distance to the ball, and the ball's Z-height band. Returns true =
// "firing" (matches the asm's zero-flag-set convention).
bool swosAiHelpersDecideWhetherToTriggerFire(int d7Direction, int a5PlayerAddr,
                                              int a6TeamBase);

// updatePlayers.cpp:19845. Reads team.opponentsTeam (via outOpponentsTeam)
// and tests team.passKickTimer < 13 (the asm carry-flag semantics --
// despite the function's name/comment saying "== 13", the actual compare
// is `<`, preserved as written). Returns the carry value.
bool swosAiHelpersResumeGameDelay(int a6TeamBase, int *outOpponentsTeam);

// updatePlayers.cpp:19870. Searches BOTH teams' 11-slot sprite tables for
// the player closest to the ball whose fullDirection is within +/-16 of
// d0FullDir (byte-wise), skipping team.controlledPlayer, sent-off players,
// and non-PL_NORMAL players. Returns the sprite address (-1 if none);
// outBallDistance receives that candidate's ballDistance (-1 if none found).
int swosAiHelpersFindClosestPlayerToBallFacing(int d0FullDir, int a6TeamBase,
                                                int *outBallDistance);
