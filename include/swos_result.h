// SOURCE: openswos game/scripts/Sim/Port/Result.cs (full file, step 10 of
// the porting order). Result tracking + goal registration, mechanically
// ported from external/swos-port/src/game/result.cpp.
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
//
// SCOPE (matches the C#'s own header comment): owns the per-match SCORERS
// list, the result-display TIMER state machine, and the goal-event pipeline
// (RegisterScorer). Does NOT own the draw side of the result screen -- a
// future host renderer reads this state and draws independently.
//
// Closes the step-4 PORT_PENDING boundary: GoalScored's call to
// Result.RegisterScorer (swos_update_goals.h's swosRegisterScorerHook,
// default NULL since step 4) needed GameTime.GameTimeAsBcd() -- this same
// step. Wired at swos_update_goals.c's hook definition site (see .c).
//
// PORT-ONLY STORAGE, NOT IN Memory: the scorers list (m_team{1,2}Scorers,
// GoalInfo/ScorerInfo) and the team-name string cache are plain C#-side
// arrays/strings in the original (result.cpp:81-86 -- "we don't add 8x10
// byte arrays to Memory.cs because they're nested 3-deep"). Ported as plain
// C static arrays/char buffers here, same treatment as GameTime's
// TimeDeltaOverride / BallSim.CurrentPitchType.
//
// OMITTED (documented, not stubbed, zero Memory effect -- confirmed by
// reading each one): the actual text composition inside
// updateScorersText/drawResult (pure UI string building -- Godot draws;
// the dirty-flag signal that matters for cross-layer state IS ported, see
// MarkScorersTextDirty in the .c).
#pragma once

#include <stdbool.h>

#define SWOS_RESULT_MAX_SCORERS      8   // kMaxScorersForDisplay
#define SWOS_RESULT_MAX_GOALS        10  // kMaxGoalsPerScorer
#define SWOS_RESULT_NAME_BUF         64  // port-only cache size for team names

// result.cpp:41-47.
#define SWOS_RESULT_AT_HALFTIME_LEN   275
#define SWOS_RESULT_AT_GAME_BREAK_LEN 165
#define SWOS_RESULT_END_OF_HALF       30000
#define SWOS_RESULT_GAME_BREAK        31000
#define SWOS_RESULT_MAX_TICKS         32000
#define SWOS_RESULT_TICK_CLAMPED      29000

// GoalType (UpdateGoals.GoalType mirror).
#define SWOS_GOAL_REGULAR  0
#define SWOS_GOAL_PENALTY  1
#define SWOS_GOAL_OWN_GOAL 2

// result.cpp:49-71 -- GoalInfo.
typedef struct {
    int type;             // GoalType
    unsigned char timeDigit1;
    unsigned char timeDigit2;
    unsigned char timeDigit3;
} SwosResultGoalInfo;

// result.cpp:73-79 -- ScorerInfo.
typedef struct {
    int shirtNum;          // 0 = empty slot
    int numGoals;
    int numLines;
    SwosResultGoalInfo goals[SWOS_RESULT_MAX_GOALS];
} SwosResultScorerInfo;

// result.cpp:104-118 -- resetResult. team1Name/team2Name may be NULL.
void swosResultReset(const char *team1Name, const char *team2Name);

const char *swosResultGetTeam1Name(void);
const char *swosResultGetTeam2Name(void);

// result.cpp:120-137 -- updateResult.
void swosResultUpdateResult(void);

// result.cpp:139-143 -- hideResult.
void swosResultHideResult(void);

// result.cpp:145-156 -- drawResult (pure UI gate; the actual draw is
// omitted, see header note).
bool swosResultShouldDrawResult(void);

// result.cpp:158-210 -- registerScorer. Call signature matches
// swosRegisterScorerHook (swos_update_goals.h).
void swosResultRegisterScorer(int scorerSpriteAddr, int teamNum, int goalType);

// ---- Accessors for a future host renderer ----------------------------------
const SwosResultScorerInfo *swosResultGetTeam1Scorers(void);
const SwosResultScorerInfo *swosResultGetTeam2Scorers(void);
