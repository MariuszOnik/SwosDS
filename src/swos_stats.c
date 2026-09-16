// SOURCE: openswos game/scripts/Sim/Port/Stats.cs (see swos_stats.h).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
#include "swos_stats.h"

#include <stdint.h>

#include "swos_addr.h"
#include "swos_ball_sprite.h"
#include "swos_memory.h"
#include "swos_result.h"
#include "swos_team_data.h"

#define K_GAME_STATE_IN_PROGRESS 100
#define K_PITCH_CENTER_Y 449
#define K_GOAL_ATTEMPT_LEFT  240
#define K_GOAL_ATTEMPT_RIGHT 431
#define K_GOAL_LEFT  303
#define K_GOAL_RIGHT 367

static void checkStatsTimer(void);

void swosStatsInitStats(void)
{
    swosWriteWord(ADDR_st_isGoalAttempt, 0);
    swosWriteWord(ADDR_st_showStats, 0);
}

void swosStatsToggleStats(void)
{
    if (swosReadSignedWord(ADDR_st_showingUserRequestedStats) != 0)
    {
        swosStatsHideStats();
    }
    else
    {
        swosWriteWord(ADDR_st_showStats, 1);
        swosWriteWord(ADDR_statsTimer, 1);
        swosResultHideResult();
    }
}

void swosStatsHideStats(void)
{
    swosWriteWord(ADDR_st_showStats, 0);
    swosWriteWord(ADDR_st_showingUserRequestedStats, 0);
    swosWriteWord(ADDR_statsTimer, 0);
}

bool swosStatsEnqueued(void) { return swosReadSignedWord(ADDR_st_showStats) != 0; }

bool swosStatsShowingUserRequestedStats(void)
{
    if (swosReadSignedWord(ADDR_st_showStats) != 0)
    {
        swosWriteWord(ADDR_st_showStats, 0);
        swosWriteWord(ADDR_st_showingUserRequestedStats, 1);
        swosWriteWord(ADDR_statsTimer, 1);
    }
    return swosReadSignedWord(ADDR_st_showingUserRequestedStats) != 0;
}

bool swosStatsShowingPostGameStats(void) { return swosReadSignedWord(ADDR_statsTimer) > 0; }

void swosStatsUpdateStatistics(void)
{
    bool showingUserStats = swosReadSignedWord(ADDR_st_showingUserRequestedStats) != 0;
    bool playingPenalties = swosReadWord(ADDR_playingPenalties) != 0;

    if (!showingUserStats && !playingPenalties)
    {
        int16_t gameStatePl = swosReadSignedWord(ADDR_gameStatePl);
        if (gameStatePl == K_GAME_STATE_IN_PROGRESS)
        {
            int32_t lastTeam = swosReadSignedDword(ADDR_lastTeamPlayed);
            if (lastTeam == TEAMDATA_TOP_BASE || lastTeam == TEAMDATA_BOTTOM_BASE)
            {
                int32_t statsPtr = swosReadSignedDword(lastTeam + TEAMDATA_OFF_TEAM_STATS_PTR);
                if (statsPtr != 0)
                {
                    int possession = swosReadWord(statsPtr + STATS_OFF_BALL_POSSESSION);
                    swosWriteWord(statsPtr + STATS_OFF_BALL_POSSESSION, (uint16_t)(possession + 1));
                }
            }

            int teamData = TEAMDATA_TOP_BASE;
            int32_t statsAddr = swosReadSignedDword(teamData + TEAMDATA_OFF_TEAM_STATS_PTR);
            int32_t ballDelta = swosReadSignedDword(BALLSPR_BASE + BALLSPR_OFF_DELTA_Y);

            int32_t ballYRaw = swosReadSignedDword(BALLSPR_BASE + BALLSPR_OFF_Y);
            const int32_t kPitchCenterYFp = K_PITCH_CENTER_Y << 16;

            bool isGoalAttempt = swosReadSignedWord(ADDR_st_isGoalAttempt) != 0;
            if (isGoalAttempt)
            {
                if (ballYRaw >= kPitchCenterYFp)
                {
                    teamData = TEAMDATA_BOTTOM_BASE;
                    ballDelta = -ballDelta;
                }
                if (teamData == lastTeam || ballDelta >= 0)
                    swosWriteWord(ADDR_st_isGoalAttempt, 0);
            }
            else if (swosReadSignedWord(ADDR_ballInGoalkeeperArea) != 0)
            {
                if (ballYRaw <= kPitchCenterYFp)
                {
                    teamData = TEAMDATA_BOTTOM_BASE;
                    ballDelta = -ballDelta;
                    statsAddr = swosReadSignedDword(teamData + TEAMDATA_OFF_TEAM_STATS_PTR);
                }

                bool playerHasBall = swosReadSignedWord(teamData + TEAMDATA_OFF_PLAYER_HAS_BALL) != 0;
                int16_t strikeDestX = swosReadSignedWord(ADDR_strikeDestX);

                if (ballDelta > 0 && teamData == lastTeam && !playerHasBall &&
                    strikeDestX >= K_GOAL_ATTEMPT_LEFT && strikeDestX <= K_GOAL_ATTEMPT_RIGHT)
                {
                    if (statsAddr != 0)
                    {
                        if (strikeDestX >= K_GOAL_LEFT && strikeDestX <= K_GOAL_RIGHT)
                        {
                            int onTarget = swosReadWord(statsAddr + STATS_OFF_ON_TARGET);
                            swosWriteWord(statsAddr + STATS_OFF_ON_TARGET, (uint16_t)(onTarget + 1));
                        }
                        int attempts = swosReadWord(statsAddr + STATS_OFF_GOAL_ATTEMPTS);
                        swosWriteWord(statsAddr + STATS_OFF_GOAL_ATTEMPTS, (uint16_t)(attempts + 1));
                    }
                    swosWriteWord(ADDR_st_isGoalAttempt, 1);
                }
            }
        }
        else
        {
            swosWriteWord(ADDR_st_isGoalAttempt, 0);
        }
    }

    checkStatsTimer();
}

static void checkStatsTimer(void)
{
    if (swosReadSignedWord(ADDR_statsTimer) < 0)
        swosStatsHideStats();
}

static SwosStatsTeamStats readTeamStats(int statsAddr)
{
    SwosStatsTeamStats s = {0};
    if (statsAddr == 0) return s;
    s.ballPossession = swosReadWord(statsAddr + STATS_OFF_BALL_POSSESSION);
    s.cornersWon     = swosReadWord(statsAddr + STATS_OFF_CORNERS_WON);
    s.foulsConceded  = swosReadWord(statsAddr + STATS_OFF_FOULS_CONCEDED);
    s.bookings       = swosReadWord(statsAddr + STATS_OFF_BOOKINGS);
    s.sendingsOff    = swosReadWord(statsAddr + STATS_OFF_SENDINGS_OFF);
    s.goalAttempts   = swosReadWord(statsAddr + STATS_OFF_GOAL_ATTEMPTS);
    s.onTarget       = swosReadWord(statsAddr + STATS_OFF_ON_TARGET);
    return s;
}

static void getTeamStatsPointers(int *leftStats, int *rightStats)
{
    int leftTeam  = TEAMDATA_TOP_BASE;
    int rightTeam = TEAMDATA_BOTTOM_BASE;

    int32_t topInGameTeam = swosReadSignedDword(TEAMDATA_TOP_BASE + TEAMDATA_OFF_IN_GAME_TEAM_PTR);
    int32_t swosTopInGame = swosReadSignedDword(ADDR_topTeamInGame);
    if (topInGameTeam != swosTopInGame)
    {
        int tmp = leftTeam; leftTeam = rightTeam; rightTeam = tmp;
    }

    *leftStats  = swosReadSignedDword(leftTeam  + TEAMDATA_OFF_TEAM_STATS_PTR);
    *rightStats = swosReadSignedDword(rightTeam + TEAMDATA_OFF_TEAM_STATS_PTR);
}

void swosStatsGetStats(SwosStatsGameStats *out)
{
    int leftPtr, rightPtr;
    getTeamStatsPointers(&leftPtr, &rightPtr);
    out->team1 = readTeamStats(leftPtr);
    out->team2 = readTeamStats(rightPtr);
}

void swosStatsDrawStatsIfNeeded(void)
{
    // stats.cpp:134-138 -- pure UI, already a no-op in the C# source.
}
