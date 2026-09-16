// SOURCE: openswos game/scripts/Sim/Port/BallUpdate.cs (full file)
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
//
// Ported ball functions from swos-port's src/game/ball/ball.cpp. Mirrors
// idiomatic C++ (`swos.ballSprite.x = x;`) as BallSprite accessor calls --
// same semantics, same backing memory layout.
//
// swosBallUpdateTick() calls into swosCheckIfBallOutOfPlay()
// (swos_ball_out_of_play.h) and swosUpdateBallWithControllingGoalkeeper()
// (swos_player_update.h) -- both forward-pulled dependencies, fully ported
// (not stubbed), per the project's standing rule against faking gameplay
// logic. See swos_ball_out_of_play.h and swos_player_update.h for their own
// SOURCE/FIDELITY notes.
//
// MatchAudio.* calls in the original (PlayBounce, PostHitComment,
// BarHitComment, PlayMissGoalSample, etc.) are OMITTED here, not stubbed:
// their C# bodies are `Instance?.DoX()` calls into a Godot audio-player
// singleton with ZERO Memory/game-state writes -- confirmed by reading
// every call site (audyt 2026-09-16). They are external host events with
// no effect on the simulated VM, in the same category as the renderer
// itself; omitting them is not "faking" anything because there is no
// simulation logic in them to fake. See README.md for the full list.
#pragma once

#include <stdbool.h>

// Full per-tick pipeline: Section1 (hide/frame) + Section2 (direction/
// friction) + Section3 (apply deltas + bounce) + Section4 (goal detection +
// shadow + quadrant calc).
void swosBallUpdateTick(void);

// Bridge entry point: Section1 + Section3 only (skips Section2's direction
// recalc from destX/destY) -- used when an external velocity model owns
// ball motion and pre-populates deltaX/Y/Z itself.
void swosBallUpdateTickPhysicsOnly(void);

// ball.cpp:4022-4026. Resets per-team spin timers (-1 = no spin active).
void swosResetBothTeamSpinTimers(void);

// ball.cpp:4029-4042. Sets ball position, clears velocity, drops to ground.
void swosSetBallPosition(int x, int y);

// ball.cpp:4509-4541 / 4551-4583. Reflects destX/destY around the current
// ball position (post-bounce direction reversal).
void swosReverseDestXDirection(void);
void swosReverseDestYDirection(void);

// ball.cpp:4206-4501. Predicts where the ball will land (z reaches 0) and
// writes the whole-pixel result to Memory.Addr.ballNextX/Y.
void swosCalculateNextBallPosition(void);

// ball.cpp:2248-3005. Ball spin (curl) + post-kick speed/height adjustments
// -- called every tick while a kick/pass is "still in flight" (spinTimer >= 0).
void swosApplyBallAfterTouch(bool topTeam);
