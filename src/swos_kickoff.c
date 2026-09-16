// SOURCE: openswos game/scripts/Sim/Port/Kickoff.cs (see swos_kickoff.h for
// the exact slice ported).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
#include "swos_kickoff.h"

#include <stdint.h>

#include "swos_addr.h"
#include "swos_ball_update.h"
#include "swos_memory.h"
#include "swos_team_data.h"
#include "swos_team_port.h"

static void swapWord(int fieldOffset);
static void swapDword(int fieldOffset);
static void resetPerTeamFieldsForNewHalf(int teamBase);

// gameLoop.cpp:2113-2157 -- prepareForInitialKick.
void swosKickoffPrepareForInitialKick(void)
{
    swosSetBallPosition(KICKOFF_CENTER_X, KICKOFF_CENTER_Y);

    int lastTeamPtr = TEAMDATA_TOP_BASE;
    int16_t cameraDir = 4;
    uint8_t turnFlags = 0x7C;

    int16_t teamStarting = swosReadSignedWord(ADDR_teamStarting);
    int16_t teamPlayingUp = swosReadSignedWord(ADDR_teamPlayingUp);
    if (teamStarting != teamPlayingUp)
    {
        lastTeamPtr = TEAMDATA_BOTTOM_BASE;
        cameraDir = 0;
        turnFlags = 0xC7;
    }

    swosWriteWord(ADDR_gameState, 0);
    swosWriteWord(ADDR_breakCameraMode, (uint16_t)-1);
    swosWriteWord(ADDR_gameStatePl, 101);
    swosWriteWord(ADDR_gameNotInProgressCounterWriteOnly, 0);
    swosWriteWord(ADDR_foulXCoordinate, 336);
    swosWriteWord(ADDR_foulYCoordinate, 449);
    swosWriteWord(ADDR_cameraDirection, (uint16_t)cameraDir);
    swosWriteByte(ADDR_playerTurnFlags, turnFlags);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)lastTeamPtr);
    swosWriteWord(ADDR_stoppageTimerTotal, 0);
    swosWriteWord(ADDR_stoppageTimerActive, 0);

    swosTeamPortStopAllPlayers();

    swosWriteWord(ADDR_cameraXVelocity, 0);
    swosWriteWord(ADDR_cameraYVelocity, 0);
}

// game.cpp:422-548 -- ReseatTeamsForNewHalf.
void swosKickoffReseatTeamsForNewHalf(void)
{
    swapDword(TEAMDATA_OFF_IN_GAME_TEAM_PTR);
    swapDword(TEAMDATA_OFF_TEAM_STATS_PTR);
    swapWord(TEAMDATA_OFF_PLAYER_NUMBER);
    swapWord(TEAMDATA_OFF_PLAYER_COACH_NUMBER);
    swapWord(TEAMDATA_OFF_IS_PL_COACH);
    swapDword(TEAMDATA_OFF_PLAYERS);
    swapWord(TEAMDATA_OFF_TEAM_NUMBER);
    swapWord(TEAMDATA_OFF_TACTICS);
    swapDword(TEAMDATA_OFF_SHOT_CHANCE_TABLE);

    resetPerTeamFieldsForNewHalf(TEAMDATA_TOP_BASE);
    resetPerTeamFieldsForNewHalf(TEAMDATA_BOTTOM_BASE);
}

// game.cpp:479-508 -- the per-team reset-to-constant block.
static void resetPerTeamFieldsForNewHalf(int teamBase)
{
    swosWriteWord(teamBase + TEAMDATA_OFF_GOALKEEPER_PLAYING, 0);
    swosWriteWord(teamBase + TEAMDATA_OFF_RESET_CONTROLS, 0);
    swosWriteWord(teamBase + TEAMDATA_OFF_UPDATE_PLAYER_INDEX, 10);
    swosWriteDword(teamBase + TEAMDATA_OFF_CONTROLLED_PLAYER, 0);
    swosWriteDword(teamBase + TEAMDATA_OFF_PASS_TO_PLAYER_PTR, 0);
    swosWriteDword(teamBase + TEAMDATA_OFF_PASSING_KICKING_PLAYER, 0);
    swosWriteWord(teamBase + TEAMDATA_OFF_PLAYER_HAS_BALL, 0);
    swosWriteWord(teamBase + TEAMDATA_OFF_GOALKEEPER_DIVING_RIGHT, 0);
    swosWriteWord(teamBase + TEAMDATA_OFF_GOALKEEPER_DIVING_LEFT, 0);
    swosWriteWord(teamBase + TEAMDATA_OFF_BALL_OUT_OF_PLAY_OR_KEEPER, 0);
    swosWriteWord(teamBase + TEAMDATA_OFF_GOALIE_PLAYING_OR_OUT, 0);
    swosWriteWord(teamBase + TEAMDATA_OFF_PASSING_BALL, 0);
    swosWriteWord(teamBase + TEAMDATA_OFF_PASSING_TO_PLAYER, 0);
    swosWriteWord(teamBase + TEAMDATA_OFF_PLAYER_SWITCH_TIMER, 0);
    swosWriteWord(teamBase + TEAMDATA_OFF_BALL_IN_PLAY, 0);
    swosWriteWord(teamBase + TEAMDATA_OFF_BALL_OUT_OF_PLAY, 0);
    swosWriteWord(teamBase + TEAMDATA_OFF_PASS_KICK_TIMER, 0);
    swosWriteWord(teamBase + TEAMDATA_OFF_BALL_CAN_BE_CONTROLLED, 0);
    swosWriteWord(teamBase + TEAMDATA_OFF_BALL_CONTROLLING_PLAYER_DIRECTION, (uint16_t)-1);
    swosWriteWord(teamBase + 138, 0); // wonTheBallTimer (+138) -- see swos_team_data.h header note
    swosWriteWord(teamBase + TEAMDATA_OFF_SPIN_TIMER, (uint16_t)-1);
    swosWriteWord(teamBase + TEAMDATA_OFF_GOALKEEPER_SAVED_COMMENT_TIMER, 0);
    swosWriteDword(teamBase + TEAMDATA_OFF_LAST_HEADING_PLAYER, 0);
    swosWriteWord(teamBase + TEAMDATA_OFF_OFS78, 0);
    swosWriteWord(teamBase + TEAMDATA_OFF_HEADER_OR_TACKLE, 0);
    swosWriteWord(teamBase + TEAMDATA_OFF_SHOOTING, 0);
    swosWriteWord(teamBase + TEAMDATA_OFF_PASS_IN_PROGRESS, 0);
}

static void swapWord(int fieldOffset)
{
    int16_t a = swosReadSignedWord(TEAMDATA_TOP_BASE + fieldOffset);
    int16_t b = swosReadSignedWord(TEAMDATA_BOTTOM_BASE + fieldOffset);
    swosWriteWord(TEAMDATA_TOP_BASE + fieldOffset, (uint16_t)b);
    swosWriteWord(TEAMDATA_BOTTOM_BASE + fieldOffset, (uint16_t)a);
}

static void swapDword(int fieldOffset)
{
    int32_t a = swosReadSignedDword(TEAMDATA_TOP_BASE + fieldOffset);
    int32_t b = swosReadSignedDword(TEAMDATA_BOTTOM_BASE + fieldOffset);
    swosWriteDword(TEAMDATA_TOP_BASE + fieldOffset, (uint32_t)b);
    swosWriteDword(TEAMDATA_BOTTOM_BASE + fieldOffset, (uint32_t)a);
}
