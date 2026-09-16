// SOURCE: openswos game/scripts/Sim/Port/Result.cs (see swos_result.h for
// the exact slice ported / omitted).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
#include "swos_result.h"

#include <string.h>

#include "swos_addr.h"
#include "swos_game_time.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"

// GameState enum (swos.h:568-595) -- values referenced here.
#define K_ST_RESULT_ON_HALFTIME   25
#define K_ST_RESULT_AFTER_THE_GAME 26

static SwosResultScorerInfo s_team1Scorers[SWOS_RESULT_MAX_SCORERS];
static SwosResultScorerInfo s_team2Scorers[SWOS_RESULT_MAX_SCORERS];

static char s_team1NameCache[SWOS_RESULT_NAME_BUF];
static char s_team2NameCache[SWOS_RESULT_NAME_BUF];

// PlayerInfo struct field offsets -- swos.h:162-208 (61-byte struct).
#define K_PLAYERINFO_OFF_GOALS_SCORED 2
#define K_PLAYERINFO_OFF_SHIRT_NUMBER 3
#define K_PLAYERINFO_SIZE             61

static int readPlayerShirtNumber(int playerInfoAddr)
{
    return playerInfoAddr == 0 ? 1 : swosReadByte(playerInfoAddr + K_PLAYERINFO_OFF_SHIRT_NUMBER);
}

// result.cpp:180 -- concedingTeam->numOwnGoals++. PORT-DIVERGENCE: see
// Result.cs's own BumpNumOwnGoals comment -- `teamPtr` here is a
// topTeamInGame/bottomTeamInGame base (players[0]), not the full TeamGame
// struct the original's numOwnGoals lives in; writing +40 would clobber
// player 0 (the keeper)'s fullName. numOwnGoals has no reader anywhere in
// the ported scope, so the write is skipped rather than corrupting the
// roster -- same divergence, same reasoning as the C# source.
static void bumpNumOwnGoals(int teamPtr) { (void)teamPtr; }

static void bumpGoalsScored(int playerInfoAddr)
{
    if (playerInfoAddr == 0) return;
    unsigned char g = swosReadByte(playerInfoAddr + K_PLAYERINFO_OFF_GOALS_SCORED);
    swosWriteByte(playerInfoAddr + K_PLAYERINFO_OFF_GOALS_SCORED, (unsigned char)(g + 1));
}

// result.cpp:363-429 -- updateScorersText, partial (see header). Signals a
// scorer-list change to a future host renderer via a dirty flag.
static void markScorersTextDirty(int scorerSpriteAddr, int teamPtr, int teamNum, int slot, int line)
{
    (void)scorerSpriteAddr; (void)teamPtr; (void)teamNum; (void)slot; (void)line;
    swosWriteWord(ADDR_res_scorersDirtyFlag, 1);
}

// result.cpp:104-118 -- resetResult.
void swosResultReset(const char *team1Name, const char *team2Name)
{
    swosWriteWord(ADDR_res_showResult, 0);

    for (int i = 0; i < SWOS_RESULT_MAX_SCORERS; i++)
    {
        s_team1Scorers[i].shirtNum = 0;
        s_team1Scorers[i].numGoals = 0;
        s_team1Scorers[i].numLines = 0;
        s_team2Scorers[i].shirtNum = 0;
        s_team2Scorers[i].numGoals = 0;
        s_team2Scorers[i].numLines = 0;
    }

    // result.cpp:115-117 -- team1NameLength (stub: character count, matches
    // the C#'s own "renderer overrides anyway" comment).
    int len = team1Name == NULL ? 0 : (int)strlen(team1Name);
    swosWriteWord(ADDR_res_team1NameLength, (unsigned short)len);

    s_team1NameCache[0] = '\0';
    s_team2NameCache[0] = '\0';
    if (team1Name != NULL) { strncpy(s_team1NameCache, team1Name, SWOS_RESULT_NAME_BUF - 1); s_team1NameCache[SWOS_RESULT_NAME_BUF - 1] = '\0'; }
    if (team2Name != NULL) { strncpy(s_team2NameCache, team2Name, SWOS_RESULT_NAME_BUF - 1); s_team2NameCache[SWOS_RESULT_NAME_BUF - 1] = '\0'; }
}

const char *swosResultGetTeam1Name(void) { return s_team1NameCache; }
const char *swosResultGetTeam2Name(void) { return s_team2NameCache; }

// result.cpp:120-137 -- updateResult.
void swosResultUpdateResult(void)
{
    int32_t resultTimer = swosReadSignedDword(ADDR_resultTimer);

    if (resultTimer < 0)
    {
        swosResultHideResult();
        return;
    }

    if (resultTimer > 0)
    {
        if (resultTimer == SWOS_RESULT_END_OF_HALF ||
            resultTimer == SWOS_RESULT_GAME_BREAK ||
            resultTimer == SWOS_RESULT_MAX_TICKS)
        {
            // result.cpp:212-227 -- resetResultTimer.
            int32_t newT = resultTimer;
            switch (resultTimer)
            {
                case SWOS_RESULT_END_OF_HALF: newT = SWOS_RESULT_AT_HALFTIME_LEN; break;
                case SWOS_RESULT_GAME_BREAK:  newT = SWOS_RESULT_AT_GAME_BREAK_LEN; break;
                case SWOS_RESULT_MAX_TICKS:   newT = SWOS_RESULT_TICK_CLAMPED; break;
                default: break;
            }
            swosWriteDword(ADDR_resultTimer, (uint32_t)newT);
            swosWriteWord(ADDR_res_showResult, 1);
            resultTimer = swosReadSignedDword(ADDR_resultTimer);
        }

        int16_t lastFrameTicks = swosReadSignedWord(ADDR_lastFrameTicks);
        resultTimer -= lastFrameTicks;
        swosWriteDword(ADDR_resultTimer, (uint32_t)resultTimer);

        if (resultTimer <= 0)
        {
            int16_t gameState = swosReadSignedWord(ADDR_gameState);
            if (gameState == K_ST_RESULT_ON_HALFTIME || gameState == K_ST_RESULT_AFTER_THE_GAME)
                swosWriteWord(ADDR_statsTimer, SWOS_RESULT_MAX_TICKS);
            swosResultHideResult();
        }
    }
}

// result.cpp:139-143 -- hideResult.
void swosResultHideResult(void)
{
    swosWriteDword(ADDR_resultTimer, 0);
    swosWriteWord(ADDR_res_showResult, 0);
}

// result.cpp:145-156 -- drawResult (pure UI gate).
bool swosResultShouldDrawResult(void) { return swosReadSignedWord(ADDR_res_showResult) != 0; }

// result.cpp:158-210 -- registerScorer.
void swosResultRegisterScorer(int scorerSpriteAddr, int teamNum, int goalType)
{
    int32_t topTeamPtr    = swosReadSignedDword(ADDR_topTeamInGame);
    int32_t bottomTeamPtr = swosReadSignedDword(ADDR_bottomTeamInGame);
    if (topTeamPtr == 0 || bottomTeamPtr == 0) return;

    SwosResultScorerInfo *scorers;
    int32_t scoringTeamPtr, concedingTeamPtr;

    if (teamNum == 1)
    {
        scorers          = s_team1Scorers;
        scoringTeamPtr   = topTeamPtr;
        concedingTeamPtr = bottomTeamPtr;
    }
    else
    {
        scorers          = s_team2Scorers;
        scoringTeamPtr   = bottomTeamPtr;
        concedingTeamPtr = topTeamPtr;
    }

    // result.cpp:175-176 -- player = scoringTeam->players[scorer.playerOrdinal-1].
    int16_t playerOrdinal = scorerSpriteAddr == 0
        ? 1
        : swosReadSignedWord(scorerSpriteAddr + PLSPR_OFF_PLAYER_ORDINAL);
    int playerInfoAddr = scoringTeamPtr + (playerOrdinal - 1) * K_PLAYERINFO_SIZE;
    int shirtNum = readPlayerShirtNumber(playerInfoAddr);

    if (goalType == SWOS_GOAL_OWN_GOAL)
    {
        int32_t tmp = scoringTeamPtr;
        scoringTeamPtr = concedingTeamPtr;
        concedingTeamPtr = tmp;
        bumpNumOwnGoals(concedingTeamPtr);
        shirtNum += 1000;
    }
    else
    {
        bumpGoalsScored(playerInfoAddr);
    }

    // result.cpp:187-209 -- find slot + append goal.
    int currentSlot = 0;
    int currentLine = 0;
    while (currentSlot < SWOS_RESULT_MAX_SCORERS && currentLine < SWOS_RESULT_MAX_SCORERS)
    {
        SwosResultScorerInfo *info = &scorers[currentSlot];
        if (info->shirtNum == 0 || info->shirtNum == shirtNum)
        {
            if (info->shirtNum == 0)
            {
                info->numGoals = 0;
                info->numLines = 1;
            }
            info->shirtNum = shirtNum;
            if (info->numGoals != SWOS_RESULT_MAX_GOALS)
            {
                int d1, d2, d3;
                swosGameTimeAsBcd(&d1, &d2, &d3);
                SwosResultGoalInfo *goal = &info->goals[info->numGoals];
                goal->type = goalType;
                goal->timeDigit1 = (unsigned char)d1;
                goal->timeDigit2 = (unsigned char)d2;
                goal->timeDigit3 = (unsigned char)d3;
                info->numGoals++;
                markScorersTextDirty(scorerSpriteAddr, scoringTeamPtr, teamNum, currentSlot, currentLine);
            }
            break;
        }
        else
        {
            currentSlot++;
            currentLine += info->numLines;
        }
    }
}

const SwosResultScorerInfo *swosResultGetTeam1Scorers(void) { return s_team1Scorers; }
const SwosResultScorerInfo *swosResultGetTeam2Scorers(void) { return s_team2Scorers; }
