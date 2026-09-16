// SOURCE: openswos game/scripts/Sim/Port/BallOutOfPlay.cs (full file)
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
//
// Mechanical port of `checkIfBallOutOfPlay` and helpers from swos-port's
// src/game/ball/ball.cpp:3007-4020. Called by BallUpdate's Section4 after
// the per-tick physics step. Three outcomes: goal scored, ball out of play
// (corner/throw-in/goal-out/near-miss), or ball still in play (no-op).
//
// Calls swosUpdateGoalsGoalScored() (swos_update_goals.h) on a goal -- a
// forward-pulled whole-file dependency (175 lines), fully ported per the
// project's standing rule against faking gameplay logic.
//
// MatchAudio.* calls in the original are OMITTED, not stubbed -- see
// swos_ball_update.h's header comment for the confirmed-no-state-effect
// rationale (identical situation here: goal/corner/throw-in/whistle/
// near-miss commentary, all `Instance?.DoX()`).
#pragma once

// ball.cpp:3007-4020. Checks whether the ball has left the pitch or scored,
// and transitions gameState/gameStatePl/foulXY/camera/lastTeamPlayedBeforeBreak
// accordingly. No-ops (beyond the whistle-flag housekeeping) if the ball is
// still in play.
void swosCheckIfBallOutOfPlay(void);
