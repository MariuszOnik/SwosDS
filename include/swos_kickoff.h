// SOURCE: openswos game/scripts/Sim/Port/Kickoff.cs:37-48, 379-525
// (PrepareForInitialKick + ReseatTeamsForNewHalf + their private helpers +
// CenterX/CenterY ONLY -- see below for why only these).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
//
// GameLoop.cs's own comment-filtered dependency scan (step 11A) found only
// PrepareForInitialKick()/ReseatTeamsForNewHalf() as real calls, so those
// landed first. PHASE 1 BOOTSTRAP-COMPLETENESS FOLLOW-UP (2026-09-16, see
// README.md "Status: Phase 1"): StartingMatch()/InitPlayersBeforeEnteringPitch()
// (the real Main.cs match-boot path this whole follow-up exists to close
// the gap on) are now ported too, below. DetermineStartingTeamAndTeamPlayingUp
// is DELIBERATELY still not duplicated here -- it's identical source to
// swosGameTimeDetermineStartingTeamAndTeamPlayingUp (ported step 10),
// exactly as the C#'s own two copies are identical; callers use that one.
// KTeamsStartingCoordinates is mechanically extracted into
// generated/swos_kickoff_data.h (InitPlayersBeforeEnteringPitch-only;
// BottomStartingPositions/TopStartingPositions stay in
// swos_update_players_tables.h from step 7B, unrelated to this function).
#pragma once

// gameLoop.cpp:2115-2116. Playing-field centre (NOT the pitch image centre
// -- see the C#'s own note on the 449 vs 424 discrepancy).
#define KICKOFF_CENTER_X 336
#define KICKOFF_CENTER_Y 449

// gameLoop.cpp:2113-2157 -- prepareForInitialKick(). Ball to centre spot,
// gameState=0 so the walk-to-formation AI takes over, camera/turn flags
// pointed at the kicking team. Re-reads teamStarting/teamPlayingUp on every
// call (both flip between halves).
void swosKickoffPrepareForInitialKick(void);

// game.cpp:422-548 -- ReseatTeamsForNewHalf(). Half-time end-swap: xchg's
// the per-team identity fields (inGameTeamPtr/teamStatsPtr/playerNumber/
// playerCoachNumber/isPlCoach/players/teamNumber/tactics/shotChanceTable)
// between TopBase and BottomBase, then applies the per-team
// reset-to-constant block to both. See the C#'s own extensive note (task
// #173 BUG2 / #148) for why this must be a straight swap.
void swosKickoffReseatTeamsForNewHalf(void);

// game.cpp:551-705 -- InitPlayersBeforeEnteringPitch(). Stands all 22
// sprites at the pitch-side entry line (right of the playing field); the
// walk to formation happens later under gameState=0. The team playing up
// consumes the first 11 kKTeamsStartingCoordinates pairs, the other team
// the remaining 11. Draws one Rng byte per player (22 total).
void swosKickoffInitPlayersBeforeEnteringPitch(void);

// game.cpp:1450-1475 -- StartingMatch(). Match-start bookend: half 1, ball
// parked off-pitch, teams-data scalar reset, gameState=21
// (kStartingGame)/gameStatePl=101 with a 100-tick delay before the
// GameLoop ladder runs PrepareForInitialKick. Calls
// InitPlayersBeforeEnteringPitch() internally.
void swosKickoffStartingMatch(void);
