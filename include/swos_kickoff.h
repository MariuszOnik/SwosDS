// SOURCE: openswos game/scripts/Sim/Port/Kickoff.cs:37-48, 379-525
// (PrepareForInitialKick + ReseatTeamsForNewHalf + their private helpers +
// CenterX/CenterY ONLY -- see below for why only these).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
//
// MINIMAL SLICE, NOT THE WHOLE FILE (525 lines): GameLoop.cs's
// comment-filtered dependency scan found exactly two real calls into
// Kickoff.cs -- PrepareForInitialKick() (the no-arg overload; the
// `int ignoredLegacyKickingTeamIndex` overload is a "PORT-COMPAT shim --
// remove after Main.cs rewire" per its own comment, dead code, not called
// from anywhere ported) and ReseatTeamsForNewHalf(). Deliberately NOT
// ported here: StartingMatch/InitPlayersBeforeEnteringPitch/
// DetermineStartingTeamAndTeamPlayingUp (match-boot-only, called from
// Main.cs's InitSwosVmFromMatchSetup -- not part of the ported scope yet;
// note DetermineStartingTeamAndTeamPlayingUp duplicates
// swosGameTimeDetermineStartingTeamAndTeamPlayingUp, ported in step 10) and
// KTeamsStartingCoordinates/BottomStartingPositions/TopStartingPositions
// (the latter two already mechanically extracted into
// swos_update_players_tables.h in step 7B; the former is InitPlayersBeforeEnteringPitch-only).
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
