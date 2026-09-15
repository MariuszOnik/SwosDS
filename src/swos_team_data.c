// SOURCE: openswos game/scripts/SwosVm/TeamData.cs (full file)
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
#include "swos_team_data.h"
#include "swos_addr.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"

int swosTeamDataBase(bool top) {
    return top ? TEAMDATA_TOP_BASE : TEAMDATA_BOTTOM_BASE;
}

int32_t swosTeamDataOpponentsTeam(bool top) {
    return swosReadSignedDword(swosTeamDataBase(top) + TEAMDATA_OFF_OPPONENTS_TEAM);
}

int32_t swosTeamDataControlledPlayer(bool top) {
    return swosReadSignedDword(swosTeamDataBase(top) + TEAMDATA_OFF_CONTROLLED_PLAYER);
}
void swosTeamDataSetControlledPlayer(bool top, int32_t spritePtr) {
    swosWriteDword(swosTeamDataBase(top) + TEAMDATA_OFF_CONTROLLED_PLAYER, (uint32_t)spritePtr);
}

int32_t swosTeamDataControlledPlayerFromBase(int teamDataBase) {
    return swosReadSignedDword(teamDataBase + TEAMDATA_OFF_CONTROLLED_PLAYER);
}

int16_t swosTeamDataPlayerHasBall(bool top) {
    return swosReadSignedWord(swosTeamDataBase(top) + TEAMDATA_OFF_PLAYER_HAS_BALL);
}
void swosTeamDataSetPlayerHasBall(bool top, int16_t v) {
    swosWriteWord(swosTeamDataBase(top) + TEAMDATA_OFF_PLAYER_HAS_BALL, (uint16_t)v);
}

int16_t swosTeamDataCurrentAllowedDirection(bool top) {
    return swosReadSignedWord(swosTeamDataBase(top) + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION);
}
void swosTeamDataSetCurrentAllowedDirection(bool top, int16_t v) {
    swosWriteWord(swosTeamDataBase(top) + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, (uint16_t)v);
}

int16_t swosTeamDataControlledPlDirection(bool top) {
    return swosReadSignedWord(swosTeamDataBase(top) + TEAMDATA_OFF_CONTROLLED_PL_DIRECTION);
}
void swosTeamDataSetControlledPlDirection(bool top, int16_t v) {
    swosWriteWord(swosTeamDataBase(top) + TEAMDATA_OFF_CONTROLLED_PL_DIRECTION, (uint16_t)v);
}

int16_t swosTeamDataGoalkeeperSavedCommentTimer(bool top) {
    return swosReadSignedWord(swosTeamDataBase(top) + TEAMDATA_OFF_GOALKEEPER_SAVED_COMMENT_TIMER);
}
void swosTeamDataSetGoalkeeperSavedCommentTimer(bool top, int16_t v) {
    swosWriteWord(swosTeamDataBase(top) + TEAMDATA_OFF_GOALKEEPER_SAVED_COMMENT_TIMER, (uint16_t)v);
}

int16_t swosTeamDataGetSpinTimer(bool top) {
    return swosReadSignedWord(swosTeamDataBase(top) + TEAMDATA_OFF_SPIN_TIMER);
}
void swosTeamDataSetSpinTimer(bool top, int16_t v) {
    swosWriteWord(swosTeamDataBase(top) + TEAMDATA_OFF_SPIN_TIMER, (uint16_t)v);
}

int16_t swosTeamDataLeftSpin(bool top) {
    return swosReadSignedWord(swosTeamDataBase(top) + TEAMDATA_OFF_LEFT_SPIN);
}
void swosTeamDataSetLeftSpin(bool top, int16_t v) {
    swosWriteWord(swosTeamDataBase(top) + TEAMDATA_OFF_LEFT_SPIN, (uint16_t)v);
}

int16_t swosTeamDataRightSpin(bool top) {
    return swosReadSignedWord(swosTeamDataBase(top) + TEAMDATA_OFF_RIGHT_SPIN);
}
void swosTeamDataSetRightSpin(bool top, int16_t v) {
    swosWriteWord(swosTeamDataBase(top) + TEAMDATA_OFF_RIGHT_SPIN, (uint16_t)v);
}

int16_t swosTeamDataLongPass(bool top) {
    return swosReadSignedWord(swosTeamDataBase(top) + TEAMDATA_OFF_LONG_PASS);
}
void swosTeamDataSetLongPass(bool top, int16_t v) {
    swosWriteWord(swosTeamDataBase(top) + TEAMDATA_OFF_LONG_PASS, (uint16_t)v);
}

int16_t swosTeamDataLongSpinPass(bool top) {
    return swosReadSignedWord(swosTeamDataBase(top) + TEAMDATA_OFF_LONG_SPIN_PASS);
}
void swosTeamDataSetLongSpinPass(bool top, int16_t v) {
    swosWriteWord(swosTeamDataBase(top) + TEAMDATA_OFF_LONG_SPIN_PASS, (uint16_t)v);
}

int16_t swosTeamDataPassInProgress(bool top) {
    return swosReadSignedWord(swosTeamDataBase(top) + TEAMDATA_OFF_PASS_IN_PROGRESS);
}
void swosTeamDataSetPassInProgress(bool top, int16_t v) {
    swosWriteWord(swosTeamDataBase(top) + TEAMDATA_OFF_PASS_IN_PROGRESS, (uint16_t)v);
}

int16_t swosTeamDataGoalkeeperDivingRight(bool top) {
    return swosReadSignedWord(swosTeamDataBase(top) + TEAMDATA_OFF_GOALKEEPER_DIVING_RIGHT);
}
void swosTeamDataSetGoalkeeperDivingRight(bool top, int16_t v) {
    swosWriteWord(swosTeamDataBase(top) + TEAMDATA_OFF_GOALKEEPER_DIVING_RIGHT, (uint16_t)v);
}

int16_t swosTeamDataGoalkeeperDivingLeft(bool top) {
    return swosReadSignedWord(swosTeamDataBase(top) + TEAMDATA_OFF_GOALKEEPER_DIVING_LEFT);
}
void swosTeamDataSetGoalkeeperDivingLeft(bool top, int16_t v) {
    swosWriteWord(swosTeamDataBase(top) + TEAMDATA_OFF_GOALKEEPER_DIVING_LEFT, (uint16_t)v);
}

int16_t swosTeamDataBallOutOfPlayOrKeeper(bool top) {
    return swosReadSignedWord(swosTeamDataBase(top) + TEAMDATA_OFF_BALL_OUT_OF_PLAY_OR_KEEPER);
}
void swosTeamDataSetBallOutOfPlayOrKeeper(bool top, int16_t v) {
    swosWriteWord(swosTeamDataBase(top) + TEAMDATA_OFF_BALL_OUT_OF_PLAY_OR_KEEPER, (uint16_t)v);
}

int32_t swosTeamDataPlayersTable(bool top) {
    return swosReadSignedDword(swosTeamDataBase(top) + TEAMDATA_OFF_PLAYERS);
}
void swosTeamDataSetPlayersTable(bool top, int32_t v) {
    swosWriteDword(swosTeamDataBase(top) + TEAMDATA_OFF_PLAYERS, (uint32_t)v);
}

int32_t swosTeamDataGetTeamSpriteAddr(bool top, int slotInTeam) {
    int32_t tableAddr = swosTeamDataPlayersTable(top);
    return swosReadSignedDword(tableAddr + slotInTeam * 4);
}

void swosTeamDataInit(void) {
    swosWriteDword(TEAMDATA_TOP_BASE + TEAMDATA_OFF_OPPONENTS_TEAM, (uint32_t)TEAMDATA_BOTTOM_BASE);
    swosWriteDword(TEAMDATA_BOTTOM_BASE + TEAMDATA_OFF_OPPONENTS_TEAM, (uint32_t)TEAMDATA_TOP_BASE);

    // Players field -- points at per-team SpritesTable (populated by
    // swosPlayerSpriteInit()).
    swosWriteDword(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PLAYERS, (uint32_t)PLSPR_TEAM1_TABLE_BASE);
    swosWriteDword(TEAMDATA_BOTTOM_BASE + TEAMDATA_OFF_PLAYERS, (uint32_t)PLSPR_TEAM2_TABLE_BASE);

    // teamStatsPtr (+14) -- wires each team to its TeamStatsData backing.
    swosWriteDword(TEAMDATA_TOP_BASE + TEAMDATA_OFF_TEAM_STATS_PTR, (uint32_t)ADDR_topTeamStatsData);
    swosWriteDword(TEAMDATA_BOTTOM_BASE + TEAMDATA_OFF_TEAM_STATS_PTR, (uint32_t)ADDR_bottomTeamStatsData);

    // Reasonable defaults -- match values are typically refreshed before use.
    swosTeamDataSetSpinTimer(true, -1);
    swosTeamDataSetSpinTimer(false, -1);
    swosTeamDataSetCurrentAllowedDirection(true, -1);
    swosTeamDataSetCurrentAllowedDirection(false, -1);
    swosTeamDataSetGoalkeeperSavedCommentTimer(true, 0);
    swosTeamDataSetGoalkeeperSavedCommentTimer(false, 0);
    swosTeamDataSetGoalkeeperDivingRight(true, 0);
    swosTeamDataSetGoalkeeperDivingRight(false, 0);
    swosTeamDataSetGoalkeeperDivingLeft(true, 0);
    swosTeamDataSetGoalkeeperDivingLeft(false, 0);
}
