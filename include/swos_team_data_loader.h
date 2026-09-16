// SOURCE: openswos game/scripts/Sim/Port/TeamDataLoader.cs:1-40 (offset
// constants + PlayerInfoSize ONLY).
// FIDELITY: VERIFIED_PC -- constants copied verbatim, no logic changes.
//
// FORWARD-PULLED DEPENDENCY, MINIMAL SLICE: PlayerActions.cs's
// GetPlayerInfoForSprite() resolves a sprite to its PlayerInfo record
// address (inGameTeamPtr + (ordinal-1) * PlayerInfoSize), and several
// PlayerActions.cs/PlayerUpdate.cs functions then read individual skill
// bytes out of that record (tackling, ballControl, passing, shooting,
// heading, finishing, speed, goalieSkill) via these offsets. That is ALL of
// TeamDataLoader.cs steps 5/5.5 need. TDL_OFF_GOALIE_SKILL was added in
// step 5.5 (PlayerUpdate.RunShotAtGoal reads it for the keeper's save
// chance) -- PlayerUpdate.cs inlines the (ordinal-1)*PlayerInfoSize math
// itself rather than calling GetPlayerInfoForSprite, so it needs the raw
// offset constant here too, same as PlayerActions.cs.
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

#define TDL_OFF_POSITION     4   // byte -- PlayerInfo.position (PlayerPosition enum, 0=goalkeeper)
#define TDL_OFF_PASSING      27  // byte -- PlayerInfo.passing skill (0..7)
#define TDL_OFF_SHOOTING     28  // byte -- PlayerInfo.shooting skill (0..7)
#define TDL_OFF_HEADING      29  // byte -- PlayerInfo.heading skill (0..7)
#define TDL_OFF_TACKLING     30  // byte -- PlayerInfo.tackling skill (0..7)
#define TDL_OFF_BALL_CONTROL 31  // byte -- PlayerInfo.ballControl skill (0..7)
#define TDL_OFF_SPEED        32  // byte -- PlayerInfo.speed skill (0..7)
#define TDL_OFF_FINISHING    33  // byte -- PlayerInfo.finishing skill (0..7)
#define TDL_OFF_GOALIE_SKILL 34  // byte -- PlayerInfo.goalieSkill (0..7)
