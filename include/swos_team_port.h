// SOURCE: openswos game/scripts/Sim/Port/TeamPort.cs:26-107, 66-187
// (StopAllPlayers/StopPlayers, UpdatePlayerShotChanceTable + its two
// literal tables ONLY -- see below for why only these).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
//
// FORWARD-PULLED DEPENDENCY, MINIMAL SLICE: PlayerUpdate.GoalkeeperClaimedTheBall
// calls StopAllPlayers (step 5.5); UpdatePlayers.cs calls
// UpdatePlayerShotChanceTable (step 7A, grep-verified: the only OTHER
// TeamPort member UpdatePlayers.cs uses). Deliberately NOT ported here:
// InitPlayerShotChanceTables (a documented no-op even in the C# source --
// see its own comment), GetPlayerShotChanceTable/GetGoalieShotChanceTableIndex
// (SWOS_TEST-only helpers, not called from any port so far). Port them when
// their own callers are ported.
#pragma once

#include <stdbool.h>

// team.cpp:26-44. Stops all 22 non-sent-off, PL_NORMAL players at their
// current position (destX/Y = X/Y) and clears both teams' ball/passing/
// keeper bookkeeping, including an unconditional goalkeeperPlaying reset
// for both teams (the C# source's comment mentions an original SWOS_TEST-
// only guard that would skip this for the top team, but OpenSWOS has no
// SWOS_TEST flag wired, so the actual code path always resets -- ported as
// written, not as commented).
void swosTeamPortStopAllPlayers(void);

// team.cpp:66-75. Picks the goalie-skill row (by PlayerInfo.goalieSkill,
// clamped 0..7) or the outfielder row, by PlayerInfo.position, and copies
// it into this team's shotChanceTable buffer (TeamData.OffShotChanceTable).
// playerInfoAddr == 0 is the "not wired yet" fallback -- see the .c file.
void swosTeamPortUpdatePlayerShotChanceTable(bool top, int playerInfoAddr);
