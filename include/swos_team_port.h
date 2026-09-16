// SOURCE: openswos game/scripts/Sim/Port/TeamPort.cs:54-107 (StopAllPlayers
// + its private StopPlayers helper ONLY -- see below for why only these
// two).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
//
// FORWARD-PULLED DEPENDENCY, MINIMAL SLICE: PlayerUpdate.GoalkeeperClaimedTheBall
// calls TeamPort.StopAllPlayers() (grep-verified: the only TeamPort member
// PlayerUpdate.cs uses). Deliberately NOT ported here: kGoalieSkillTables/
// kPlayerShotChanceTable (literal tables) and InitPlayerShotChanceTables/
// UpdatePlayerShotChanceTable/GetPlayerShotChanceTable/
// GetGoalieShotChanceTableIndex -- none of those are called from
// PlayerUpdate.cs; they belong to team-file-loading/shot-chance-table setup,
// a different layer (see swos_team_data_loader.h for the parallel
// TeamDataLoader scoping decision). Port them when their own callers are
// ported.
#pragma once

// team.cpp:26-44. Stops all 22 non-sent-off, PL_NORMAL players at their
// current position (destX/Y = X/Y) and clears both teams' ball/passing/
// keeper bookkeeping, including an unconditional goalkeeperPlaying reset
// for both teams (the C# source's comment mentions an original SWOS_TEST-
// only guard that would skip this for the top team, but OpenSWOS has no
// SWOS_TEST flag wired, so the actual code path always resets -- ported as
// written, not as commented).
void swosTeamPortStopAllPlayers(void);
