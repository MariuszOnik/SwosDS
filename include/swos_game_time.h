// SOURCE: openswos game/scripts/Sim/Port/GameTime.cs (full file, step 10 of
// the porting order). Match clock + half-time / full-time / extra-time /
// penalties transitions, mechanically ported from
// external/swos-port/src/game/gameTime.cpp (+ game.cpp for the
// initMatch()-adjacent helpers at the tail of the C# file).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
//
// AmigaModeActive() was already pulled forward in step 9 (AiBrain.cs's one
// real dependency on this file) -- kept here, now joined by the rest.
//
// FORWARD-PULLED DEPENDENCY, MINIMAL SLICE: NextPenalty calls
// GameLoop.PlayersLeavingPitch() (swos_game_loop.h, forward-pulled whole --
// see that header) when a shootout is decided.
//
// PORT-ONLY GLOBAL, NOT IN Memory: BallSim.CurrentPitchType
// (BallState.cs:127, default 4 = Normal) is a plain C#-side static field,
// not a Memory-backed slot -- InitPitchBallFactors is its only reader
// anywhere in the ported scope (comment-filtered-grep verified; Pitch.cs,
// the writer, is a different not-yet-ported layer). Ported as a plain C
// global (g_swosBallSimCurrentPitchType) here, matching the
// PlayerEnergy.EffectEnabled / TimeDeltaOverride precedent for port-only
// C#-side statics that live outside the emulated Memory buffer.
//
// OMITTED (documented, not stubbed, zero Memory/control-flow effect --
// confirmed by reading each one, same standard as every prior step):
//   - MatchAudio.PlayEndGameWhistle (StubPlayEndGameWhistleSample) -- pure
//     audio playback.
//   - HalftimeCeremonyStage / SetHalftimeCeremonyStage / s_halftimeCeremonyStage
//     / kHalftimeStageDwellTicks -- the C#'s own comment claims these are
//     "kept for Main.cs compile compatibility", but a whole-tree grep shows
//     ZERO references anywhere outside GameTime.cs itself (not even in
//     Main.cs) -- genuinely dead code, same category as InputControls.cs's
//     DebugForceP1Direction/DebugForceP1Fire (step 8).
//   - DrawGameTime / DrawGameTime(digit1,digit2,digit3) / DrawGameTimeImpl /
//     GetGameTimeSprites / GetSpriteWidth / StubDrawMenuSprite -- the whole
//     menu-digit rendering chain. StubDrawMenuSprite is ALREADY a no-op in
//     the C# source (`/* TODO */`, no MENUSPR.DAT sprite-descriptor loader
//     wired), so this entire call chain has zero Memory effect end to end
//     (confirmed by reading every function in it) -- there is nothing left
//     to port beyond a no-op the source already documents as one.
#pragma once

#include <stdbool.h>
#include <stdint.h>

// gameTime.cpp:770 (step 9). PC-locked to false until Amiga support lands.
bool swosGameTimeAmigaModeActive(void);

// BallState.cs:127 (BallSim.CurrentPitchType) -- port-only C#-side static,
// NOT a Memory slot. Default 4 = Normal (swos.ini default). See file header.
extern int g_swosBallSimCurrentPitchType;

// GameTime.cs:343 (TimeDeltaOverride) -- port-only C#-side static, NOT a
// Memory slot. >0 overrides the faithful kGameLenSecondsTable lookup so a
// menu-selected real-time half length can be honoured. 0 = use the table.
extern int g_swosGameTimeTimeDeltaOverride;

// gameTime.cpp:42-53 -- resetGameTime. Zeroes the clock digits/seconds/
// minutes/accumulator + the port-only stoppage/prolong statics, then
// re-derives timeDelta from gameLengthInGame (or g_swosGameTimeTimeDeltaOverride).
void swosGameTimeResetGameTime(void);

// gameTime.cpp:55-58.
bool swosGameTimeShowing(void);

// Port-only presentation aid (see .c for the full rationale): game-seconds
// elapsed in the current last-minute-prolong window. 0 when not in prolong.
int swosGameTimeStoppageGameSeconds(void);

// True while ProlongLastMinute is keeping the clock pinned at a period
// boundary (gt_gameSeconds < 0 AND a period-end handler exists for the
// current minute).
bool swosGameTimeInProlong(void);

// Resets the stoppage tick accumulator. Called by ResetGameTime + period
// transitions.
void swosGameTimeResetStoppage(void);

// gameTime.cpp:60-98 -- updateGameTime. The per-tick clock advance.
void swosGameTimeUpdateGameTime(void);

// gameTime.cpp:112-115.
uint32_t swosGameTimeInMinutes(void);

// gameTime.cpp:117-120 -- gameTimeAsBcd. Out-params replace the C# tuple
// return (digit1 = hundreds, digit2 = tens, digit3 = ones).
void swosGameTimeAsBcd(int *digit1, int *digit2, int *digit3);

// gameTime.cpp:122-125.
bool swosGameTimeAtZeroMinute(void);

// updatePlayers.cpp:8706-8804 (mechanical port hosted in GameTime.cs -- see
// that file's own call-site note). Per-player end-of-match happy/sad pose
// sweep; called by EndSecondHalf/EndSecondExtraTime right after EndOfGame.
void swosGameTimeMarkPlayersHappyOrSad(void);

// game.cpp:828-885 -- nextPenalty(). Internal in the C# (only SetPieces.
// AdvancePenaltiesTimer calls it, from the SAME assembly in C#'s case but a
// DIFFERENT translation unit here) -- exposed non-static so
// swos_set_pieces.c can call it, mirroring the C#'s own `internal` access.
void swosGameTimeNextPenalty(void);

// game.cpp:1361-1379 -- initPlayerCardChance().
void swosGameTimeInitPlayerCardChance(void);

// game.cpp:1381-1385 -- determineStartingTeamAndTeamPlayingUp().
void swosGameTimeDetermineStartingTeamAndTeamPlayingUp(void);

// game.cpp:1387-1401 -- initPitchBallFactors().
void swosGameTimeInitPitchBallFactors(void);

// game.cpp:1237-1241 -- saveTeams().
void swosGameTimeSaveTeams(void);

// game.cpp:1243-1247 -- restoreTeams().
void swosGameTimeRestoreTeams(void);

// game.cpp:1403-1448 (+ game.cpp:75) -- initGameVariables(), including the
// gameRandValue RNG draw the C# bundles into the same per-match-reset call.
void swosGameTimeInitGameVariables(void);
