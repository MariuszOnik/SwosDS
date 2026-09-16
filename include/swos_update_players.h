// SOURCE: openswos game/scripts/Sim/Port/UpdatePlayers.cs (full file, step
// 7B of the porting order).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
//
// This is the per-team-per-tick orchestrator: swosUpdatePlayersUpdate(top)
// is called once per team per tick (see UpdatePlayers.Update in the C#).
// Everything else in this file is file-local (static) -- the C# class's
// other members are all `private static`.
//
// Two hook boundaries, reusing the g_swosAiSetControlsDirectionHook /
// g_swosAiKickHook globals from swos_player_controlled.h (established step
// 6A) rather than new ones:
//   - AiBrain.SetControlsDirection / AiHelpers.AI_Kick -- wired to the real
//     implementations as of step 9 (see swos_ai_brain.h/swos_ai_helpers.h);
//     no longer an assert-backed stub.
//   - SetPieces.SetThrowInPlayerDestinationCoordinates / SetPieces.TickThrowIn
//     -- still step 10. New hooks, declared in swos_set_pieces.h, mirroring
//     the same assert-then-call convention (this one still assert-backed).
//
// Design decision (documented, not silently dropped): the C# source wraps
// three calls to PlayerHeader.SetPlayerWithNoBallDestination in
// `try { } catch (System.Exception) { /* tactics table not yet populated */ }`.
// C has no exceptions. swosSetPlayerWithNoBallDestination's own bounds are
// driven by small, realistic tacticsIdx/ordinal values that stay in-bounds
// in every reachable game state (SeedTeamData always populates tactics
// before Update() ever runs), so this port calls it directly, unguarded --
// matching the C# catch block's own comment that the fallback path is a
// no-op (the caller's fall-through behaviour after a caught exception, which
// this direct call already produces by simply continuing).
#pragma once

#include <stdbool.h>

// UpdatePlayers.cpp:47 (C# UpdatePlayers.Update). teamIndex: 0=top, 1=bottom.
void swosUpdatePlayersUpdate(int teamIndex);

// Resets the two PORT-ONLY-but-real per-team debounce statics (the
// "re-establish controlledPlayer" zero-tick counters and the carrier-stall
// counters -- see the .c file's header for why these are real gameplay
// state, not telemetry). Mirrors UpdatePlayers.ResetFallbackCounters() for
// the subset of C# statics this port actually keeps (the rest were C#-side
// dev-diagnostic counters with zero Memory effect, omitted -- see the .c
// file). Call between independent differential-test scenarios in the same
// process, exactly like every other step's *ResetTelemetry()/*ResetState().
void swosUpdatePlayersResetState(void);
