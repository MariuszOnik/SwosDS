// SOURCE: openswos game/scripts/Sim/Port/UpdateGoals.cs (full file)
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
//
// swosRegisterScorerHook: the PORT_PENDING boundary from step 4 is closed
// as of step 10 -- Result.cs is now fully ported (swos_result.h/.c) and
// GameTime.GameTimeAsBcd() (its one remaining real dependency) exists, so
// the hook is statically wired to the real implementation below.
#include "swos_update_goals.h"
#include "swos_addr.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"
#include "swos_result.h"
#include "swos_team_data.h"

#include <stdint.h>

#define GT_REGULAR 0
#define GT_PENALTY 1
#define GT_OWN_GOAL 2

#define kMaxGoals 99

SwosRegisterScorerFn swosRegisterScorerHook = swosResultRegisterScorer;

bool swosUpdateGoalsBumpTeamGoals(int teamNum) {
    bool isSecondTeam = teamNum == 2;
    bool playingPenalties = swosReadWord(ADDR_playingPenalties) != 0;

    // updateGoals.cpp:14-15 -- cap check uses totalGoals[isSecondTeam].
    int totalAddr = isSecondTeam ? ADDR_team2TotalGoals : ADDR_team1TotalGoals;
    if (swosReadWord(totalAddr) == kMaxGoals)
        return false;

    // updateGoals.cpp:17 -- ++*goalVars[playingPenalties][isSecondTeam].
    int bumpAddr;
    if (playingPenalties)
        bumpAddr = isSecondTeam ? ADDR_team2PenaltyGoals : ADDR_team1PenaltyGoals;
    else
        bumpAddr = isSecondTeam ? ADDR_team2TotalGoals : ADDR_team1TotalGoals;

    swosWriteWord(bumpAddr, (uint16_t)(swosReadWord(bumpAddr) + 1));

    // updateGoals.cpp:19-22 -- {digit1, digit2, statsGoals} for the scoring team.
    int digit1Addr = isSecondTeam ? ADDR_team2GoalsDigit1 : ADDR_team1GoalsDigit1;
    int digit2Addr = isSecondTeam ? ADDR_team2GoalsDigit2 : ADDR_team1GoalsDigit2;
    int statsAddr = isSecondTeam ? ADDR_statsTeam2Goals : ADDR_statsTeam1Goals;

    // updateGoals.cpp:26-29 -- ++digit2; if hits 10, carry to digit1 + clear.
    int d2 = swosReadWord(digit2Addr) + 1;
    if (d2 == 10) {
        swosWriteWord(digit1Addr, (uint16_t)(swosReadWord(digit1Addr) + 1));
        d2 = 0;
    }
    swosWriteWord(digit2Addr, (uint16_t)d2);

    // updateGoals.cpp:30 -- ++statsGoals.
    swosWriteWord(statsAddr, (uint16_t)(swosReadWord(statsAddr) + 1));

    return true;
}

void swosUpdateGoalsGoalScored(int teamNum, int scorerSlot) {
    // updateGoals.cpp:39-43 -- per-frame flag block.
    swosWriteWord(ADDR_goalScored, 1);
    swosWriteWord(ADDR_runSlower, 1);
    swosWriteWord(ADDR_lastTeamScoredNumber, (uint16_t)teamNum);
    swosWriteDword(ADDR_lastPlayerScored, (uint32_t)swosPlayerSpriteBase(scorerSlot));
    swosWriteDword(ADDR_currentScorer, (uint32_t)swosPlayerSpriteBase(scorerSlot));

    // updateGoals.cpp:45-46 -- match scorer's team sprite to top/bottom teamData.
    int16_t scorerTeamNum = swosPlayerSpriteTeamNumber(scorerSlot);
    int32_t scorerTeamGame = scorerTeamNum == 1
        ? swosReadSignedDword(ADDR_topTeamInGame)
        : swosReadSignedDword(ADDR_bottomTeamInGame);

    int32_t topInGamePtr = swosReadSignedDword(TEAMDATA_TOP_BASE + TEAMDATA_OFF_IN_GAME_TEAM_PTR);
    int teamPtr = topInGamePtr == scorerTeamGame ? TEAMDATA_TOP_BASE : TEAMDATA_BOTTOM_BASE;

    // updateGoals.cpp:48.
    swosWriteDword(ADDR_lastTeamScored, (uint32_t)teamPtr);

    // updateGoals.cpp:50 -- early-out on goal cap OR penalty-shootout goal.
    bool playingPenalties = swosReadWord(ADDR_playingPenalties) != 0;
    if (!swosUpdateGoalsBumpTeamGoals(teamNum) || playingPenalties)
        return;

    // updateGoals.cpp:53-61 -- pick goal type.
    int goalType = GT_REGULAR;
    swosWriteDword(ADDR_goalTypeScored, GT_REGULAR);

    if (scorerTeamNum != teamNum) {
        goalType = GT_OWN_GOAL;
        swosWriteDword(ADDR_goalTypeScored, GT_OWN_GOAL);
    } else if (swosReadWord(ADDR_penalty) != 0) {
        goalType = GT_PENALTY;
    }

    // updateGoals.cpp:63 -- registerScorer. See swos_update_goals.h's
    // PORT_PENDING note: not called with approximated behavior, simply
    // skipped when unset, exactly like the omitted MatchAudio triggers.
    if (swosRegisterScorerHook)
        swosRegisterScorerHook(swosPlayerSpriteBase(scorerSlot), teamNum, goalType);
}
