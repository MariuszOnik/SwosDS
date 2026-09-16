// SOURCE: openswos game/scripts/Sim/Port/UpdateGoals.cs (full file)
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
//
// Goal scoring logic ported from swos-port's src/game/updateGoals.cpp.
// Triggered when the ball crosses a goal line -- bumps the team's goal
// counter, updates scoreline digits + stats, and registers the scorer.
//
// PORT_PENDING(Result.RegisterScorer): the original's GoalScored ends by
// calling Result.RegisterScorer, which appends the scorer's name/minute to
// a results-screen scorer list and bumps a per-player goalsScored stat --
// UI/stats bookkeeping with NO reader anywhere else in the ported scope
// (confirmed 2026-09-16: nothing branches on its result, and it doesn't
// touch Memory -- Result.cs keeps the scorer list in plain C# arrays
// outside the emulated memory buffer entirely). It also needs
// GameTime.GameTimeAsBcd() (GameTime.cs, a later porting step) for the
// goal's match-minute. Deferred via swosRegisterScorerHook (defaults to
// NULL -- see below): this keeps the exact integration point Result.cs
// will need to fill in later, without inventing scorer data or a fake
// clock now. It is NOT called with approximated behavior; if the hook is
// unset, it is simply not called, same as the audio omissions in
// swos_ball_update.h.
#pragma once

#include <stdbool.h>

// Call signature matches OpenSwos.Sim.Port.Result.RegisterScorer(int
// scorerSpriteAddr, int teamNum, int goalType). Set by whichever later step
// ports Result.cs; NULL (the default) means "not wired up yet" -- GoalScored
// then simply skips the call, exactly like the omitted MatchAudio triggers.
typedef void (*SwosRegisterScorerFn)(int scorerSpriteAddr, int teamNum, int goalType);
extern SwosRegisterScorerFn swosRegisterScorerHook;

// updateGoals.cpp:3-33. Bumps team total + penalty + scoreline digits +
// stats counter for the scoring team. Returns false if the team already hit
// the 99-goal cap (caller then skips scorer registration).
bool swosUpdateGoalsBumpTeamGoals(int teamNum);

// updateGoals.cpp:35-64. teamNum = 1 or 2 (whose net was hit). scorerSlot =
// the player sprite slot that last touched the ball (may differ from
// teamNum for an own goal). Sets the per-frame goal event flags, bumps the
// goal counters, determines goal type (regular/own-goal/penalty), and calls
// swosRegisterScorerHook if set (see PORT_PENDING note above).
void swosUpdateGoalsGoalScored(int teamNum, int scorerSlot);
