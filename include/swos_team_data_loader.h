// SOURCE: openswos game/scripts/Sim/Port/TeamDataLoader.cs:1-40 (offset
// constants + PlayerInfoSize ONLY).
// FIDELITY: VERIFIED_PC -- constants copied verbatim, no logic changes.
//
// FORWARD-PULLED DEPENDENCY, MINIMAL SLICE: PlayerActions.cs's
// GetPlayerInfoForSprite() resolves a sprite to its PlayerInfo record
// address (inGameTeamPtr + (ordinal-1) * PlayerInfoSize), and several
// PlayerActions.cs functions then read individual skill bytes out of that
// record (tackling, ballControl, passing, shooting, heading, finishing,
// speed) via these offsets. That is ALL of TeamDataLoader.cs this step
// needs.
//
// Deliberately NOT ported here: TeamDataLoader.WritePlayerInfos and
// WireTeamFields -- the functions that actually POPULATE PlayerInfo
// records from a loaded team file at match setup. Those pull in a whole
// separate subsystem (OpenSwos.Assets.TeamRecord/PlayerRecord -- team-file
// parsing, SkillScaling.cs, TeamPort.cs, PlayerEnergy.SeedSlot, and a
// Godot.GD.Print debug call) that is about LOADING TEAMS, not about
// per-tick MATCH SIMULATION -- a different layer from what PlayerActions.cs
// (and this port, so far) is doing. GetPlayerInfoForSprite works correctly
// against a PlayerInfo block populated by any means (in tests: poked
// directly into Memory at the expected offsets), so this slice does not
// depend on WritePlayerInfos/WireTeamFields ever running.
#pragma once

#define TDL_PLAYER_INFO_SIZE 61

#define TDL_OFF_PASSING      27  // byte -- PlayerInfo.passing skill (0..7)
#define TDL_OFF_SHOOTING     28  // byte -- PlayerInfo.shooting skill (0..7)
#define TDL_OFF_HEADING      29  // byte -- PlayerInfo.heading skill (0..7)
#define TDL_OFF_TACKLING     30  // byte -- PlayerInfo.tackling skill (0..7)
#define TDL_OFF_BALL_CONTROL 31  // byte -- PlayerInfo.ballControl skill (0..7)
#define TDL_OFF_SPEED        32  // byte -- PlayerInfo.speed skill (0..7)
#define TDL_OFF_FINISHING    33  // byte -- PlayerInfo.finishing skill (0..7)
