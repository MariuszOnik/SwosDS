// SOURCE: openswos game/scripts/Sim/Port/Bench.cs (full file, step 11 of
// the porting order). Substitution / bench logic, mechanically ported from
// external/swos-port/src/game/bench/bench.cpp (144 LOC) +
// external/swos-port/src/game/bench/updateBench.cpp (980 LOC), plus the
// substituted-player walk-off/walk-in state machine the original hosts
// inside updatePlayers' per-player loop (updatePlayers.cpp:9051-9197) but
// which UpdatePlayers.cs carries as an explicit TODO -- ported here, at the
// same call slot the C# uses (UpdateBench, matching gameLoop.cpp's tick
// order: walk FSM steps before the menu FSM).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
//
// InBench()/InBenchMenus()/GetBenchState() were already forward-pulled as a
// minimal slice in steps 8/11A; this step supersedes that comment (the
// slice is now part of the full file) without changing their bodies.
//
// PORT-ONLY MODULE STATE (matches the C#'s own file-static fields, NOT
// Memory.Addr slots -- see Bench.cs's own header comment): the tap-counter
// pair, m_team/m_teamGame/m_teamNumber, m_goToBenchTimer,
// m_bench{1,2}Called, m_block{Directions,Fire}, m_fireTimer,
// m_lastDirection/m_movementDelayTimer, m_training/teamsSwapped/
// alternateTeamsTimer, the menu cursor fields, the 2x16 shirt-number table,
// and the four walk-FSM dest fields (no Memory.Addr slot upstream either).
// All reset by swosBenchInitBenchControls(), exactly like updateBench.cpp:
// 118-166 / InitBenchControls().
//
// Debug-only surface omitted (documented, not stubbed, zero Memory effect):
// DebugTapStateString/DebugLastPollGameStatePl/DebugLastPollBlocked/
// DebugLastPollUnavailable -- pure diagnostic captures for the original's
// --bench-test harness. The underlying calls they capture (BenchBlocked,
// BenchUnavailable) ARE still made in full (they tick down real timers).
//
// Audio omitted (MatchAudio.EnqueueSubstitute/EnqueueTactics -- pure
// playback, zero Memory effect, same pattern as every other MatchAudio
// omission in this port).
#pragma once

#include <stdbool.h>
#include <stdint.h>

// updateBench.h:3-10 -- BenchState enum.
#define BENCH_STATE_INITIAL             0
#define BENCH_STATE_ABOUT_TO_SUBSTITUTE 1
#define BENCH_STATE_FORMATION_MENU      2
#define BENCH_STATE_MARKING_PLAYERS     3
#define BENCH_STATE_OPPONENTS_BENCH     4

// bench.cpp:9-21 + updateBench.cpp:12-21 -- constants.
#define BENCH_X 27
#define BENCH_TOP_BENCH_Y 389
#define BENCH_BOTTOM_BENCH_Y 485
#define BENCH_TRAINING_PITCH_BENCH_Y 456
#define BENCH_PLAYER_GOING_IN_X 26
#define BENCH_PLAYER_GOING_IN_Y 449
#define BENCH_ENTER_BENCH_DELAY 15
#define BENCH_PLAYER_GOING_IN_DELAY 100
#define BENCH_NUM_TAPS_FOR_BENCH 2
#define BENCH_TAP_TIMEOUT_TICKS 15
#define BENCH_LEAVING_SUBS_DELAY 55
#define BENCH_SUBSTITUTE_FIRE_TICKS 8
#define BENCH_MAX_SUBSTITUTES 5
#define BENCH_NUM_FORMATION_ENTRIES 18
#define BENCH_SUBSTITUTED_PLAYER_X 39
#define BENCH_SUBSTITUTED_PLAYER_Y 449
#define BENCH_SUBSTITUTED_PLAYER_SPEED 1536

// Substitution allowance -- port-only C-side globals (not Memory-backed),
// same treatment as GameTime's g_swosBallSimCurrentPitchType/
// g_swosGameTimeTimeDeltaOverride. SWOS 96/97 friendly defaults.
extern int g_swosBenchGameMinSubstitutes;
extern int g_swosBenchGameMaxSubstitutes;

// ---- Public state queries (bench.cpp:42-65, updateBench.cpp:194-294) -----
bool swosBenchInBench(void);
int  swosBenchGetBenchState(void);
bool swosBenchInBenchMenus(void);
int  swosBenchGetBenchY(void);
int  swosBenchGetOpponentBenchY(void);
bool swosBenchTrainingTopTeam(void);
void swosBenchSetTrainingTopTeam(bool value);
void swosBenchRequestBench1(void);
void swosBenchRequestBench2(void);
int  swosBenchGetBenchPlayerIndex(void);
int  swosBenchGetBenchMenuSelectedPlayer(void);
int  swosBenchGetSelectedFormationEntry(void);
int  swosBenchPlayerToEnterGameIndex(void);
int  swosBenchPlayerToBeSubstitutedIndex(void);
int  swosBenchPlayerToBeSubstitutedPos(void);
int  swosBenchGetBenchPlayerShirtNumber(bool topTeam, int index);
bool swosBenchInBenchOrGoingTo(void);
bool swosBenchGoingToBenchDelay(void);
bool swosBenchSubstituteInProgress(void);
bool swosBenchNewPlayerAboutToGoIn(void);
void swosBenchSetSubstituteInProgress(void);
int  swosBenchGetBenchTeamBase(void);
int  swosBenchGetBenchTeamGameBase(void);
bool swosBenchTeamIsTop(void);
int  swosBenchGetBenchPlayerInfoAddr(int index);
int  swosBenchGetBenchPlayerPosition(int index);

// ---- Lifecycle -------------------------------------------------------------
void swosBenchInitBenchBeforeMatch(void);
void swosBenchInitBenchControls(void);
void swosBenchInvokeBench(void);
void swosBenchSetBenchOff(void);
void swosBenchSwapBenchWithOpponent(void);

// ---- Main per-tick entry (bench.cpp:36-40, gameLoop.cpp:315) --------------
void swosBenchUpdateBench(void);
bool swosBenchCheckControls(void);

// game.cpp:1218-1235. Called at bench-enter and bench-exit; also GameLoop's
// own real dependency (Mode7's timeout fallback).
void swosBenchCheckIfGoalkeeperClaimedTheBall(void);
