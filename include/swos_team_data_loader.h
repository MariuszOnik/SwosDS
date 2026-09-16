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
// Step 11 (Bench.cs full port) extends this with OffSubstituted/OffCards/
// OffFace -- the substitution eligibility checks (PlayerWasSubstituted,
// IsPlayerOkToSelect/Substitute, FindInitialPlayerToBeSubstituted) and the
// post-swap sprite-frame re-derivation (InitializePlayerSpriteFrameIndices)
// read these three fields (comment-filtered-grep verified against the
// whole file).
//
// PHASE 1 BOOTSTRAP-COMPLETENESS FOLLOW-UP (2026-09-16): WritePlayerInfos/
// WireTeamFields ARE now ported below -- see README.md "Status: Phase 1"
// for the audit that motivated it. Real team-FILE parsing is still NOT
// ported (no ADF/TEAM.* reader exists in this repo) -- callers supply a
// SwosTeamRecord (swos_team_record.h), a plain data-transport struct that
// stands in for a loaded TeamRecord, same as before. SkillScaling.cs (the
// price/skill-scaling pipeline, enabled by default) and PlayerEnergy.SeedSlot
// are both real dependencies and are ported too (swos_skill_scaling.h,
// swos_player_energy.h). The debug Godot.GD.Print call at the end of
// WritePlayerInfos (a one-line skill-sum diagnostic, zero Memory effect) is
// omitted, same pattern as every other GD.Print in this port.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "swos_team_record.h"

#define TDL_PLAYER_INFO_SIZE 61

#define TDL_OFF_SUBSTITUTED  0   // byte -- PlayerInfo.substituted (0/1)
#define TDL_OFF_INDEX        1   // byte -- PlayerInfo.index (roster index in the team FILE)
#define TDL_OFF_GOALS_SCORED 2   // byte -- PlayerInfo.goalsScored
#define TDL_OFF_SHIRT_NUMBER 3   // byte -- PlayerInfo.shirtNumber
#define TDL_OFF_CARDS        10  // byte -- PlayerInfo.cards (0/1/2; >=2 = sent off)
#define TDL_OFF_FACE         5   // byte -- PlayerInfo.face (portrait/skin index)
#define TDL_OFF_POSITION     4   // byte -- PlayerInfo.position (PlayerPosition enum, 0=goalkeeper)
#define TDL_OFF_SHORT_NAME   12  // 15 bytes -- PlayerInfo.shortName
#define TDL_OFF_PASSING      27  // byte -- PlayerInfo.passing skill (0..7)
#define TDL_OFF_SHOOTING     28  // byte -- PlayerInfo.shooting skill (0..7)
#define TDL_OFF_HEADING      29  // byte -- PlayerInfo.heading skill (0..7)
#define TDL_OFF_TACKLING     30  // byte -- PlayerInfo.tackling skill (0..7)
#define TDL_OFF_BALL_CONTROL 31  // byte -- PlayerInfo.ballControl skill (0..7)
#define TDL_OFF_SPEED        32  // byte -- PlayerInfo.speed skill (0..7)
#define TDL_OFF_FINISHING    33  // byte -- PlayerInfo.finishing skill (0..7)
#define TDL_OFF_GOALIE_SKILL 34  // byte -- PlayerInfo.goalieSkill (0..7)
#define TDL_OFF_INJURIES_BITS 35 // byte -- PlayerInfo.injuriesBitfield
#define TDL_OFF_FULL_NAME    38  // 23 bytes -- PlayerInfo.fullName

// TeamDataLoader.cs:85-263 (WritePlayerInfos). Writes 16 PlayerInfo records
// starting at playersBase (matches team1InGameTeamPlayers/
// team2InGameTeamPlayers). `team` may be NULL (no-op, matches the C#'s
// `if (team is null) return`).
void swosTeamDataLoaderWritePlayerInfos(int playersBase, const SwosTeamRecord *team,
                                         bool isHumanControlled);

// TeamDataLoader.cs:268-322 (WireTeamFields). Wires TeamData's
// inGameTeamPtr/teamNumber/tactics/playerNumber/shotChanceTable, and the
// display name used by the result screen.
void swosTeamDataLoaderWireTeamFields(bool top, const SwosTeamRecord *team,
                                       int playersBaseAddr, int shotChanceTableAddr,
                                       int nameStorageAddr, bool isHumanControlled,
                                       int defaultTacticsIndex);

// TeamDataLoader.cs:346-357 (GoalieSkillFromPrice).
uint8_t swosTeamDataLoaderGoalieSkillFromPrice(int priceCode, bool top);
