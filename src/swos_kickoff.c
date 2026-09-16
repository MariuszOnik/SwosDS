// SOURCE: openswos game/scripts/Sim/Port/Kickoff.cs (see swos_kickoff.h for
// the exact slice ported).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
#include "swos_kickoff.h"

#include <stdint.h>

#include "swos_addr.h"
#include "swos_ball_update.h"
#include "swos_memory.h"
#include "swos_player_actions.h"
#include "swos_player_sprite.h"
#include "swos_referee.h"
#include "swos_rng.h"
#include "swos_team_data.h"
#include "swos_team_port.h"

#include "generated/swos_kickoff_data.h"

static void swapWord(int fieldOffset);
static void swapDword(int fieldOffset);
static void resetPerTeamFieldsForNewHalf(int teamBase);
static void kickoffInitTeamsData(void);

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

// PHASE 1 BOOTSTRAP-COMPLETENESS FOLLOW-UP (2026-09-16, see README.md
// "Status: Phase 1"). game.cpp:551-705 -- InitPlayersBeforeEnteringPitch.
void swosKickoffInitPlayersBeforeEnteringPitch(void)
{
    swosRefereeRemoveReferee();

    int16_t teamPlayingUp = swosReadSignedWord(ADDR_teamPlayingUp);
    int tableBase = teamPlayingUp == 1
        ? PLSPR_TEAM1_TABLE_BASE
        : PLSPR_TEAM2_TABLE_BASE;

    int coordIndex = 0;

    for (int teamPass = 0; teamPass < 2; teamPass++)
    {
        for (int i = 0; i < PLSPR_TEAM_SIZE; i++)
        {
            int spriteAddr = swosReadSignedDword(tableBase + i * 4);

            int16_t coordX = kKTeamsStartingCoordinates[coordIndex * 2];
            int16_t coordY = kKTeamsStartingCoordinates[coordIndex * 2 + 1];
            coordIndex++;

            if (spriteAddr == 0)
                continue;

            int16_t xPix = (int16_t)(coordX + 591);
            swosWriteWord(spriteAddr + PLSPR_OFF_X + 2, (uint16_t)xPix);

            int16_t yPix = (int16_t)(coordY + 449);
            swosWriteWord(spriteAddr + PLSPR_OFF_Y + 2, (uint16_t)yPix);

            xPix = (int16_t)(xPix + (swosRngNextByte() & 7));
            swosWriteWord(spriteAddr + PLSPR_OFF_X + 2, (uint16_t)xPix);

            swosWriteWord(spriteAddr + PLSPR_OFF_DEST_X, (uint16_t)xPix);
            swosWriteWord(spriteAddr + PLSPR_OFF_DEST_Y, (uint16_t)yPix);

            swosWriteWord(spriteAddr + PLSPR_OFF_Z + 2, 0);
            swosWriteWord(spriteAddr + PLSPR_OFF_SPEED, 0);
            swosWriteByte(spriteAddr + PLSPR_OFF_PLAYER_STATE, 0);
            swosWriteByte(spriteAddr + PLSPR_OFF_PLAYER_DOWN_TIMER, 0);
            swosWriteWord(spriteAddr + PLSPR_OFF_FRAME_INDEX, (uint16_t)-1);
            swosWriteWord(spriteAddr + PLSPR_OFF_CYCLE_FRAMES_TIMER, 1);
            swosWriteWord(spriteAddr + PLSPR_OFF_IMAGE_INDEX, (uint16_t)-1);
            swosWriteWord(spriteAddr + PLSPR_OFF_DIRECTION, 0);
            swosWriteWord(spriteAddr + PLSPR_OFF_ON_SCREEN, 1);

            if (swosReadSignedWord(ADDR_gameState) == 21)
            {
                swosWriteWord(spriteAddr + PLSPR_OFF_SENT_AWAY, 0);
                swosWriteWord(spriteAddr + PLSPR_OFF_CARDS, 0);
                swosWriteWord(spriteAddr + PLSPR_OFF_INJURY_LEVEL, 0);
            }

            swosSetPlayerAnimationTable(spriteAddr, ADDR_playerNormalStandingAnimTable);
        }

        tableBase = teamPlayingUp == 2
            ? PLSPR_TEAM1_TABLE_BASE
            : PLSPR_TEAM2_TABLE_BASE;
    }
}

// game.cpp:396-421 -- the InitTeamsData scalar-reset block, StartingMatch's
// own private copy (same source block as swos_game_time.c's
// initTeamsDataForExtraTime -- see that function's header comment; the C#
// keeps two independent copies too, so this mirrors that duplication
// rather than sharing one C helper across files).
static void kickoffInitTeamsData(void)
{
    swosWriteDword(ADDR_currentScorer, 0);
    swosWriteDword(ADDR_lastPlayerBeforeGoalkeeper, 0);
    swosWriteWord(ADDR_goalScored, 0);
    swosWriteWord(ADDR_runSlower, 0);
    swosWriteWord(ADDR_whichCard, 0);
    swosWriteDword(ADDR_bookedPlayer, 0);
    swosWriteWord(ADDR_playerHadBall, 0);
    swosWriteDword(ADDR_lastKeeperPlayed, 0);
    swosWriteDword(ADDR_lastTeamPlayed, 0);
    swosWriteDword(ADDR_lastPlayerPlayed, 0);
    swosWriteWord(ADDR_penalty, 0);
    swosWriteWord(ADDR_goalCameraMode, 0);
    swosWriteWord(ADDR_goalOut, 0);
    swosWriteWord(ADDR_gameNotInProgressCounterWriteOnly, 0);
    swosWriteWord(ADDR_fireBlocked, 0);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, 0);
    swosWriteWord(ADDR_stoppageTimerTotal, 0);
    swosWriteWord(ADDR_stoppageTimerActive, 0);
    swosWriteWord(ADDR_stoppageEventTimer, 0);
    swosWriteWord(ADDR_inGameCounter, 0);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosWriteWord(ADDR_gameState, 100);
    swosWriteWord(ADDR_breakState, 0);
    swosWriteWord(ADDR_breakCameraMode, (uint16_t)-1);
}

// PHASE 1 BOOTSTRAP-COMPLETENESS FOLLOW-UP (2026-09-16). game.cpp:1450-1475
// -- StartingMatch.
void swosKickoffStartingMatch(void)
{
    const int kStartingBallX = 1672;
    const int kStartingBallY = 449;
    const int kInitialDelayBeforeKickOff = 100;

    swosWriteWord(ADDR_halfNumber, 1);
    swosWriteWord(ADDR_hideBall, 0);
    swosSetBallPosition(kStartingBallX, kStartingBallY);

    kickoffInitTeamsData();

    swosWriteWord(ADDR_stoppageEventTimer, kInitialDelayBeforeKickOff);
    swosWriteWord(ADDR_gameState, 21);
    swosWriteWord(ADDR_gameStatePl, 101);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, TEAMDATA_TOP_BASE);
    swosWriteWord(ADDR_stoppageTimerTotal, 0);
    swosWriteWord(ADDR_stoppageTimerActive, 0);

    swosWriteWord(ADDR_breakCameraMode, (uint16_t)-1);
    swosWriteWord(ADDR_cameraDirection, (uint16_t)-1);
    swosWriteWord(ADDR_cameraXVelocity, 0);
    swosWriteWord(ADDR_cameraYVelocity, 0);

    swosTeamPortStopAllPlayers();
    swosKickoffInitPlayersBeforeEnteringPitch();

    swosWriteWord(ADDR_showFansCounter, 100);
}
