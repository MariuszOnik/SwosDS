// SOURCE: openswos game/scripts/Sim/Port/SkillScaling.cs (full file).
// FIDELITY: VERIFIED_PC -- direct mechanical port; all six literal tables
// extracted mechanically (tools/extract_all_arrays.py ->
// generated/swos_skill_scaling_data.h), not hand-retyped, element counts
// checked against the C# source (8/50/245/240/1225/35).
//
// PHASE 1 BOOTSTRAP-COMPLETENESS FOLLOW-UP (2026-09-16): a real dependency
// of TeamDataLoader.WritePlayerInfos discovered while porting it (see
// swos_team_data_loader.h) -- the original SWOS player-price + skill
// scaling pipeline (GetPlayerPrice/AdjustPlayerSkills), enabled by default
// (SkillScaling.Enabled = true in the C# source). Fully self-contained
// (Memory/Rng/SwosTeamRecord/the already-ported TacticsLoader builtin
// tables only), so ported whole rather than deferred or approximated.
#pragma once

#include <stdbool.h>

#include "swos_team_record.h"

// SkillScaling.cs:36 -- master toggle. true = faithful original pipeline
// (matches production); false = write raw mod-8 file skills.
extern bool g_swosSkillScalingEnabled;

// SkillScaling.cs:42 -- "all player teams equal" menu option. Default off
// (matches the C# default and OpenSWOS's own menu default).
extern bool g_swosSkillScalingAllPlayerTeamsEqual;

// SkillScaling.cs:51 -- average player price CODE across the selected-team
// pool; 0 = unknown (TeamAppPercent degrades to 100).
extern int g_swosSkillScalingLeagueAvgTeamValue;

// SkillScaling.cs:201-303 (ComputePlayerPrice). playerNo is the in-game
// slot 1..10.
int swosSkillScalingComputePlayerPrice(const SwosPlayerRecord *p, const SwosTeamRecord *team,
                                        int playerNo, bool humanControlled);

// SkillScaling.cs:341-360 (ComputePricePercent). Draws exactly one Rng2 byte.
int swosSkillScalingComputePricePercent(int computedPrice, int filePrice, bool humanControlled);

// SkillScaling.cs:374-393 (ScaleSkill). Draws exactly one Rng2 byte.
// Returns the UNCLAMPED scaled skill -- caller clamps to 7.
int swosSkillScalingScaleSkill(int rawSkill, int pricePercent, int teamAppPercent);

// SkillScaling.cs:402-410 (ComputeTeamValue / GetAveragePlayerPrice).
int swosSkillScalingComputeTeamValue(const SwosTeamRecord *t);

// SkillScaling.cs:425-431 (TeamAppPercent).
int swosSkillScalingTeamAppPercent(const SwosTeamRecord *t);
