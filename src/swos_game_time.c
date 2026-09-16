// SOURCE: openswos game/scripts/Sim/Port/GameTime.cs (see swos_game_time.h
// for the exact slice ported / omitted).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
#include "swos_game_time.h"

#include <string.h>

#include "swos_addr.h"
#include "swos_audio_events.h"
#include "swos_ball_sprite.h"
#include "swos_game_loop.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"
#include "swos_rng.h"
#include "swos_team_data.h"
#include "swos_team_port.h"

// ---- GameState constants (mirror swos.h, matching the C#'s own local copies) --
#define K_ST_GAME_IN_PROGRESS      100
#define K_ST_GAME_STATE_PL_STOPPED 101
#define K_ST_FIRST_HALF_ENDED      29
#define K_ST_GAME_ENDED            30
#define K_ST_FIRST_EXTRA_STARTING  27
#define K_ST_FIRST_EXTRA_ENDED     28

// gameTime.cpp:183-184.
#define K_UPPER_PENALTY_AREA_LOWER_LINE 216
#define K_LOWER_PENALTY_AREA_UPPER_LINE 682
// pitchConstants.h:3-4.
#define K_PITCH_CENTER_Y 449

// PORT-ONLY. Same rationale/value as the C# (GameTime.cs:50-61) -- caps how
// long the last-minute prolong may keep resetting the whistle countdown
// before it's allowed to drain, working around updatePlayers.cpp's keeper/
// AI not always clearing a camped ball out of the penalty box (task #153).
#define K_PROLONG_BACKSTOP_TICKS 60

// gameTime.cpp:129 -- kGameLenSecondsTable[gameLengthInGame].
static const int kGameLenSecondsTable[4] = { 30, 18, 12, 9 };

int g_swosBallSimCurrentPitchType = 4;
int g_swosGameTimeTimeDeltaOverride = 0;

// gameTime.cpp:17 -- m_gameTime[1..3] backed onto Memory (see header).
static int32_t readGameTimeDigit(int slot) { return swosReadSignedDword(ADDR_gt_gameTime + slot * 4); }
static void writeGameTimeDigit(int slot, int32_t v) { swosWriteDword(ADDR_gt_gameTime + slot * 4, (uint32_t)v); }

// Port-only statics -- see swosGameTimeStoppageGameSeconds/InProlong.
static int s_stoppageRealTicks = 0;

// ---- Forward declarations of static helpers (defined further below) -------
static void initTimeDelta(void);
static bool isGameAtMinute(uint32_t minute);
static void endFirstHalf(void);
static void endSecondHalf(void);
static bool prolongLastMinute(void);
static void endSecondExtraTime(void);
static bool hasPeriodEndHandler(void);
static void callPeriodEndHandlerIfAny(void);
static bool isNextMinuteLastInPeriod(void);
static void bumpGameTime(void);
static void bumpPlayersLastPlayedHalfAtHalfStart(void);
static void bumpPlayersLastPlayedHalfAtHalfEnd(void);
static void setupLastMinuteSwitchNextFrame(void);
static void endFirstHalfImpl(void);
static void endOfGame(void);
static void resetBothTeamsPlayerPassingKicking(void);
static void resetPlayerPassingKicking(int teamBase);
static void startFirstExtraTime(void);
static void endFirstExtraTime(void);
static void startPenalties(void);
static void initTeamsDataForExtraTime(void);
static void resetPenaltyShooters(void);
static int readPlayerInfoCards(int playerInfoAddr);
static int readPlayerInfoHalfPlayed(int playerInfoAddr);
static void writePlayerInfoHalfPlayed(int playerInfoAddr, int v);

// gameTime.cpp:770 (step 9, unchanged).
bool swosGameTimeAmigaModeActive(void) { return false; }

// gameTime.cpp:42-53 -- resetGameTime.
void swosGameTimeResetGameTime(void)
{
    swosWriteWord(ADDR_gt_showTime, 0);
    swosWriteDword(ADDR_gt_gameSeconds, 0);
    writeGameTimeDigit(0, 0);
    writeGameTimeDigit(1, 0);
    writeGameTimeDigit(2, 0);
    writeGameTimeDigit(3, 0);
    swosWriteDword(ADDR_gt_gameTimeInMinutes, 0);
    swosWriteDword(ADDR_gt_secondsSwitchAccumulator, 0);
    swosWriteDword(ADDR_gt_endGameCounter, 0);
    s_stoppageRealTicks = 0;

    initTimeDelta();
}

// gameTime.cpp:55-58.
bool swosGameTimeShowing(void) { return swosReadWord(ADDR_gt_showTime) != 0; }

// Mirrors the per-second mapping used in UpdateGameTime -- see header.
int swosGameTimeStoppageGameSeconds(void)
{
    int32_t timeDelta = swosReadSignedDword(ADDR_gt_timeDelta);
    if (timeDelta <= 0) return 0;
    int ticksPerSecond = swosGameTimeAmigaModeActive() ? 49 : 54;
    return s_stoppageRealTicks * timeDelta / ticksPerSecond;
}

bool swosGameTimeInProlong(void)
{
    return swosReadSignedDword(ADDR_gt_gameSeconds) < 0 && hasPeriodEndHandler();
}

void swosGameTimeResetStoppage(void) { s_stoppageRealTicks = 0; }

// gameTime.cpp:60-98 -- updateGameTime.
void swosGameTimeUpdateGameTime(void)
{
    swosWriteWord(ADDR_gt_showTime, 1);

    int32_t gameSeconds = swosReadSignedDword(ADDR_gt_gameSeconds);
    bool lastMinuteSwitchAboutToHappen = gameSeconds < 0;

    if (lastMinuteSwitchAboutToHappen)
    {
        // gameTime.cpp:66-77.
        int32_t endGameCounter = swosReadSignedDword(ADDR_gt_endGameCounter);
        int16_t lastFrameTicks = swosReadSignedWord(ADDR_lastFrameTicks);
        endGameCounter -= lastFrameTicks;
        swosWriteDword(ADDR_gt_endGameCounter, (uint32_t)endGameCounter);

        s_stoppageRealTicks += lastFrameTicks;

        if (endGameCounter < 0)
        {
            if (hasPeriodEndHandler())
            {
                // swos.asm:112106-112139 -- defer while goal celebration drains.
                int16_t goalCounter = swosReadSignedWord(ADDR_goalCounter);
                if (goalCounter != 0)
                    return;

                swosWriteDword(ADDR_gt_gameSeconds, 0);
                swosWriteWord(ADDR_stateGoal, 0);
                // StubPlayEndGameWhistleSample now wired to a real sound
                // (see swos_audio_events.h).
                swosAudioFireEvent(SWOS_AUDIO_EVENT_END_GAME_WHISTLE);
                s_stoppageRealTicks = 0;
                callPeriodEndHandlerIfAny();
            }
        }
        else if (prolongLastMinute() && s_stoppageRealTicks < K_PROLONG_BACKSTOP_TICKS)
        {
            setupLastMinuteSwitchNextFrame();
        }
    }
    else
    {
        // gameTime.cpp:78-97.
        int16_t gameStatePl = swosReadSignedWord(ADDR_gameStatePl);
        bool playingPenalties = swosReadWord(ADDR_playingPenalties) != 0;
        if (gameStatePl != K_ST_GAME_IN_PROGRESS || playingPenalties) return;

        int32_t timeDelta = swosReadSignedDword(ADDR_gt_timeDelta);
        int32_t accumulator = swosReadSignedDword(ADDR_gt_secondsSwitchAccumulator);
        accumulator -= timeDelta;

        if (accumulator < 0)
        {
            int ticksPerSecond = swosGameTimeAmigaModeActive() ? 49 : 54;
            accumulator += ticksPerSecond;

            int16_t lastFrameTicks = swosReadSignedWord(ADDR_lastFrameTicks);
            gameSeconds += lastFrameTicks;

            if (gameSeconds >= 60)
            {
                gameSeconds = 0;
                bumpGameTime();

                if (isGameAtMinute(1) || isGameAtMinute(46))
                    bumpPlayersLastPlayedHalfAtHalfStart();

                if (isNextMinuteLastInPeriod())
                {
                    // Resync gameSeconds from the -1 sentinel SetupLastMinuteSwitchNextFrame
                    // just wrote, so the write below doesn't clobber it back to 0.
                    setupLastMinuteSwitchNextFrame();
                    gameSeconds = swosReadSignedDword(ADDR_gt_gameSeconds);
                }
            }

            swosWriteDword(ADDR_gt_gameSeconds, (uint32_t)gameSeconds);
        }

        swosWriteDword(ADDR_gt_secondsSwitchAccumulator, (uint32_t)accumulator);
    }
}

// gameTime.cpp:112-115.
uint32_t swosGameTimeInMinutes(void) { return swosReadDword(ADDR_gt_gameTimeInMinutes); }

// gameTime.cpp:117-120.
void swosGameTimeAsBcd(int *digit1, int *digit2, int *digit3)
{
    *digit1 = readGameTimeDigit(1);
    *digit2 = readGameTimeDigit(2);
    *digit3 = readGameTimeDigit(3);
}

// gameTime.cpp:122-125.
bool swosGameTimeAtZeroMinute(void) { return swosReadDword(ADDR_gt_gameTimeInMinutes) == 0; }

static void initTimeDelta(void)
{
    int16_t gameLengthInGame = swosReadSignedWord(ADDR_gameLengthInGame);
    if (gameLengthInGame < 0) gameLengthInGame = 0;
    if (gameLengthInGame > 3) gameLengthInGame = 3;
    int td = g_swosGameTimeTimeDeltaOverride > 0
        ? g_swosGameTimeTimeDeltaOverride
        : kGameLenSecondsTable[gameLengthInGame];
    swosWriteDword(ADDR_gt_timeDelta, (uint32_t)td);
}

static bool isGameAtMinute(uint32_t minute) { return minute == swosReadDword(ADDR_gt_gameTimeInMinutes); }

// gameTime.cpp:139-143 -- endFirstHalf.
static void endFirstHalf(void)
{
    endFirstHalfImpl();
    bumpPlayersLastPlayedHalfAtHalfEnd();
}

// gameTime.cpp:145-176 -- endSecondHalf.
static void endSecondHalf(void)
{
    bumpPlayersLastPlayedHalfAtHalfEnd();

    int32_t statsT1 = swosReadWord(ADDR_statsTeam1Goals);
    int32_t statsT2 = swosReadWord(ADDR_statsTeam2Goals);
    swosWriteWord(ADDR_statsTeam1GoalsCopy, (uint16_t)statsT1);
    swosWriteWord(ADDR_statsTeam2GoalsCopy, (uint16_t)statsT2);

    int32_t totalT1 = swosReadWord(ADDR_team1TotalGoals);
    int32_t totalT2 = swosReadWord(ADDR_team2TotalGoals);

    if (totalT1 == totalT2)
    {
        totalT1 = statsT1 + 2 * swosReadSignedWord(ADDR_team1GoalsFirstLeg);
        totalT2 = statsT2 + 2 * swosReadSignedWord(ADDR_team2GoalsFirstLeg);

        bool secondLeg = swosReadWord(ADDR_secondLeg) != 0;
        int16_t playing2ndGame = swosReadSignedWord(ADDR_playing2ndGame);
        bool gameTied = !secondLeg || playing2ndGame != 1 || totalT1 == totalT2;

        if (gameTied)
        {
            int16_t extraTimeState = swosReadSignedWord(ADDR_extraTimeState);
            int16_t penaltiesState = swosReadSignedWord(ADDR_penaltiesState);
            if (extraTimeState != 0)
            {
                swosWriteWord(ADDR_extraTimeState, (uint16_t)-1);
                startFirstExtraTime();
            }
            else if (penaltiesState != 0)
            {
                swosWriteWord(ADDR_penaltiesState, (uint16_t)-1);
                startPenalties();
            }
            else
            {
                swosWriteDword(ADDR_winningTeamPtr, 0);
                endOfGame();
                swosGameTimeMarkPlayersHappyOrSad();
            }
            return;
        }
    }

    int32_t winningTeamGame = totalT1 > totalT2
        ? swosReadSignedDword(ADDR_topTeamInGame)
        : swosReadSignedDword(ADDR_bottomTeamInGame);
    swosWriteDword(ADDR_winningTeamPtr, (uint32_t)winningTeamGame);
    endOfGame();
    swosGameTimeMarkPlayersHappyOrSad();
}

// updatePlayers.cpp:8706-8804 -- see header.
void swosGameTimeMarkPlayersHappyOrSad(void)
{
    if (swosReadSignedWord(ADDR_gameState) != K_ST_GAME_ENDED) return;

    int32_t winningTeamPtr = swosReadSignedDword(ADDR_winningTeamPtr);
    if (winningTeamPtr == 0) return;

    int32_t topInGame = swosReadSignedDword(ADDR_topTeamInGame);
    bool topIsWinner = winningTeamPtr == topInGame;
    int16_t winningTeamNumber = topIsWinner ? 1 : 2;

    const uint8_t PL_NORMAL = 0;
    const uint8_t PL_SAD = 14;
    const uint8_t PL_HAPPY = 15;
    for (int slot = 0; slot < PLSPR_TOTAL_SLOTS; slot++)
    {
        int spriteAddr = swosPlayerSpriteBase(slot);

        uint8_t curState = swosReadByte(spriteAddr + PLSPR_OFF_PLAYER_STATE);
        if (curState != PL_NORMAL) continue;

        if (swosReadSignedWord(spriteAddr + PLSPR_OFF_PLAYER_ORDINAL) == 1)
            continue;

        int32_t dx = swosReadSignedDword(spriteAddr + PLSPR_OFF_DELTA_X);
        int32_t dy = swosReadSignedDword(spriteAddr + PLSPR_OFF_DELTA_Y);
        if ((dx | dy) != 0) continue;

        if (swosRngNextByte() > 64) continue;

        int16_t spriteTeamNum = swosPlayerSpriteTeamNumber(slot);
        uint8_t newState = (spriteTeamNum == winningTeamNumber) ? PL_HAPPY : PL_SAD;
        swosWriteByte(spriteAddr + PLSPR_OFF_PLAYER_STATE, newState);
    }
}

// gameTime.cpp:178-196 -- prolongLastMinute.
static bool prolongLastMinute(void)
{
    int16_t gameStatePl = swosReadSignedWord(ADDR_gameStatePl);
    if (gameStatePl != K_ST_GAME_IN_PROGRESS) return true;

    int16_t ballY = swosReadSignedWord(BALLSPR_BASE + BALLSPR_OFF_Y + 2);

    bool ballInsidePenaltyArea =
        ballY <= K_UPPER_PENALTY_AREA_LOWER_LINE || ballY > K_LOWER_PENALTY_AREA_UPPER_LINE;

    int attackingTeam = ballY > K_PITCH_CENTER_Y ? TEAMDATA_TOP_BASE : TEAMDATA_BOTTOM_BASE;
    int32_t lastTeamPlayed = swosReadSignedDword(ADDR_lastTeamPlayed);
    bool attackInProgress = lastTeamPlayed == attackingTeam;

    return ballInsidePenaltyArea || attackInProgress;
}

// gameTime.cpp:198-222 -- endSecondExtraTime.
static void endSecondExtraTime(void)
{
    int32_t totalT1 = swosReadWord(ADDR_team1TotalGoals);
    int32_t totalT2 = swosReadWord(ADDR_team2TotalGoals);

    if (totalT1 == totalT2)
    {
        totalT1 = swosReadWord(ADDR_statsTeam1Goals) + 2 * swosReadSignedWord(ADDR_team1GoalsFirstLeg);
        totalT2 = swosReadWord(ADDR_statsTeam2Goals) + 2 * swosReadSignedWord(ADDR_team2GoalsFirstLeg);

        bool secondLeg = swosReadWord(ADDR_secondLeg) != 0;
        bool playing2ndGame = swosReadWord(ADDR_playing2ndGame) != 0;
        bool gameTied = !secondLeg || !playing2ndGame || totalT1 == totalT2;

        if (gameTied)
        {
            int16_t penaltiesState = swosReadSignedWord(ADDR_penaltiesState);
            if (penaltiesState != 0)
            {
                swosWriteWord(ADDR_penaltiesState, (uint16_t)-1);
                startPenalties();
            }
            else
            {
                swosWriteDword(ADDR_winningTeamPtr, 0);
                endOfGame();
                swosGameTimeMarkPlayersHappyOrSad();
            }
            return;
        }
    }

    int32_t winningTeamGame = totalT1 > totalT2
        ? swosReadSignedDword(ADDR_topTeamInGame)
        : swosReadSignedDword(ADDR_bottomTeamInGame);
    swosWriteDword(ADDR_winningTeamPtr, (uint32_t)winningTeamGame);
    endOfGame();
    swosGameTimeMarkPlayersHappyOrSad();
}

// gameTime.cpp:242-256 -- getPeriodEndHandler / isNextMinuteLastInPeriod,
// restructured from "return a delegate" into "query" + "call if any" (same
// switch, same behaviour -- see swos_game_time.h).
static bool hasPeriodEndHandler(void)
{
    uint32_t mins = swosReadDword(ADDR_gt_gameTimeInMinutes);
    return mins == 45 || mins == 90 || mins == 105 || mins == 120;
}
static void callPeriodEndHandlerIfAny(void)
{
    uint32_t mins = swosReadDword(ADDR_gt_gameTimeInMinutes);
    switch (mins)
    {
        case 45:  endFirstHalf(); break;
        case 90:  endSecondHalf(); break;
        case 105: endFirstExtraTime(); break;
        case 120: endSecondExtraTime(); break;
        default: break;
    }
}
static bool isNextMinuteLastInPeriod(void) { return hasPeriodEndHandler(); }

// gameTime.cpp:273-284 -- bumpGameTime.
static void bumpGameTime(void)
{
    int32_t d3 = readGameTimeDigit(3) + 1;
    if (d3 >= 10)
    {
        d3 = 0;
        int32_t d2 = readGameTimeDigit(2) + 1;
        if (d2 >= 10)
        {
            d2 = 0;
            int32_t d1 = readGameTimeDigit(1);
            if (d1 < 9)
                writeGameTimeDigit(1, d1 + 1);
        }
        writeGameTimeDigit(2, d2);
    }
    writeGameTimeDigit(3, d3);

    int32_t mins = swosReadSignedDword(ADDR_gt_gameTimeInMinutes) + 1;
    swosWriteDword(ADDR_gt_gameTimeInMinutes, (uint32_t)mins);
}

// gameTime.cpp:286-310 -- bumpPlayersLastPlayedHalfAt{Start,End} +
// forEachPlayer. The C# shares one forEachPlayer(Action<int>) helper across
// both callers; ported here as two direct loops (C has no lightweight
// closures) -- same two-team/11-player walk, same per-player condition.
static void bumpPlayersLastPlayedHalfAtHalfStart(void)
{
    int32_t bases[2] = { swosReadSignedDword(ADDR_topTeamInGame), swosReadSignedDword(ADDR_bottomTeamInGame) };
    for (int team = 0; team < 2; team++)
    {
        if (bases[team] == 0) continue;
        for (int i = 0; i < 11; i++)
        {
            int playerInfoAddr = bases[team] + i * 61;
            int cards = readPlayerInfoCards(playerInfoAddr);
            int halfPlayed = readPlayerInfoHalfPlayed(playerInfoAddr);
            if (cards < 2 && halfPlayed != 2)
                writePlayerInfoHalfPlayed(playerInfoAddr, 1);
        }
    }
}
static void bumpPlayersLastPlayedHalfAtHalfEnd(void)
{
    int32_t bases[2] = { swosReadSignedDword(ADDR_topTeamInGame), swosReadSignedDword(ADDR_bottomTeamInGame) };
    for (int team = 0; team < 2; team++)
    {
        if (bases[team] == 0) continue;
        for (int i = 0; i < 11; i++)
        {
            int playerInfoAddr = bases[team] + i * 61;
            int cards = readPlayerInfoCards(playerInfoAddr);
            int halfPlayed = readPlayerInfoHalfPlayed(playerInfoAddr);
            if (cards < 2 && halfPlayed == 1)
                writePlayerInfoHalfPlayed(playerInfoAddr, 2);
        }
    }
}

// gameTime.cpp:312-316 -- setupLastMinuteSwitchNextFrame.
static void setupLastMinuteSwitchNextFrame(void)
{
    swosWriteDword(ADDR_gt_gameSeconds, (uint32_t)-1);
    int endGame = swosGameTimeAmigaModeActive() ? 50 : 55;
    swosWriteDword(ADDR_gt_endGameCounter, (uint32_t)endGame);
}

// swos.asm:112089-112113 -- EndFirstHalf (asm footprint, distinct from the
// C#'s own endFirstHalf() wrapper above).
static void endFirstHalfImpl(void)
{
    swosWriteWord(ADDR_hideBall, 0);
    swosWriteWord(ADDR_stoppageEventTimer, 100);
    swosWriteWord(ADDR_gameState, K_ST_FIRST_HALF_ENDED);
    swosWriteWord(ADDR_breakCameraMode, (uint16_t)-1);
    swosWriteWord(ADDR_gameStatePl, K_ST_GAME_STATE_PL_STOPPED);
    swosWriteWord(ADDR_gameNotInProgressCounterWriteOnly, 0);
    swosWriteWord(ADDR_cameraDirection, (uint16_t)-1);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)TEAMDATA_TOP_BASE);
    swosWriteWord(ADDR_stoppageTimerTotal, 0);
    swosWriteWord(ADDR_stoppageTimerActive, 0);
    resetBothTeamsPlayerPassingKicking();
    swosWriteWord(ADDR_cameraXVelocity, 0);
    swosWriteWord(ADDR_cameraYVelocity, 0);
    // swos.asm:112103-112109 -- writeOnlyAtEndOfHalf gate; write-only field,
    // no readers anywhere in the ported scope (see C# comment). Preserve
    // the read side-effects, drop the dead write.
    (void)swosReadSignedWord(ADDR_playGame);
    (void)swosReadSignedWord(ADDR_goalCounter);
}

// swos.asm:112119-112143 -- EndOfGame.
static void endOfGame(void)
{
    swosWriteWord(ADDR_hideBall, 0);
    swosWriteWord(ADDR_stoppageEventTimer, 150);
    swosWriteWord(ADDR_gameState, K_ST_GAME_ENDED);
    swosWriteWord(ADDR_breakCameraMode, (uint16_t)-1);
    swosWriteWord(ADDR_gameStatePl, K_ST_GAME_STATE_PL_STOPPED);
    swosWriteWord(ADDR_gameNotInProgressCounterWriteOnly, 0);
    swosWriteWord(ADDR_cameraDirection, (uint16_t)-1);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)TEAMDATA_TOP_BASE);
    swosWriteWord(ADDR_stoppageTimerTotal, 0);
    swosWriteWord(ADDR_stoppageTimerActive, 0);
    resetBothTeamsPlayerPassingKicking();
    swosWriteWord(ADDR_cameraXVelocity, 0);
    swosWriteWord(ADDR_cameraYVelocity, 0);
    (void)swosReadSignedWord(ADDR_playGame);
    (void)swosReadSignedWord(ADDR_goalCounter);
}

// swos.asm:112149-112166 -- ResetBothTeamsPlayerPassingKicking.
static void resetBothTeamsPlayerPassingKicking(void)
{
    uint16_t ptf = swosReadByte(ADDR_playerTurnFlags);
    swosWriteWord(ADDR_lastPlayerTurnFlags, ptf);

    resetPlayerPassingKicking(TEAMDATA_TOP_BASE);
    resetPlayerPassingKicking(TEAMDATA_BOTTOM_BASE);

    // swos.asm:112161-112162 -- bug-for-bug: only the bottom team's
    // goalkeeperPlaying is cleared here (esi still points at bottomTeamData
    // from the last resetPlayerPassingKicking call in the original).
    swosWriteWord(TEAMDATA_BOTTOM_BASE + TEAMDATA_OFF_GOALKEEPER_PLAYING, 0);
}

// swos.asm:112174-112186 -- ResetPlayerPassingKicking.
static void resetPlayerPassingKicking(int teamBase)
{
    swosWriteWord(teamBase + TEAMDATA_OFF_BALL_IN_PLAY, 0);
    swosWriteWord(teamBase + TEAMDATA_OFF_BALL_OUT_OF_PLAY, 0);
    swosWriteDword(teamBase + TEAMDATA_OFF_CONTROLLED_PLAYER, 0);
    swosWriteDword(teamBase + TEAMDATA_OFF_PASS_TO_PLAYER_PTR, 0);
    swosWriteWord(teamBase + TEAMDATA_OFF_PASSING_BALL, 0);
    swosWriteWord(teamBase + TEAMDATA_OFF_PASSING_TO_PLAYER, 0);
    swosWriteWord(teamBase + TEAMDATA_OFF_PLAYER_SWITCH_TIMER, 0);
    swosWriteDword(teamBase + TEAMDATA_OFF_PASSING_KICKING_PLAYER, 0);
}

// swos.asm:103925-103962 -- StartFirstExtraTime.
static void startFirstExtraTime(void)
{
    swosWriteWord(ADDR_halfNumber, 1);
    swosWriteWord(ADDR_teamPlayingUp, (uint16_t)((swosRngNextByte() & 1) + 1));
    swosWriteWord(ADDR_teamStarting, (uint16_t)((swosRngNextByte() & 1) + 1));
    swosWriteWord(ADDR_hideBall, 0);

    initTeamsDataForExtraTime();

    swosWriteWord(ADDR_stoppageEventTimer, 110);
    swosWriteWord(ADDR_gameState, K_ST_FIRST_EXTRA_STARTING);
    swosWriteWord(ADDR_breakCameraMode, (uint16_t)-1);
    swosWriteWord(ADDR_gameStatePl, K_ST_GAME_STATE_PL_STOPPED);
    swosWriteWord(ADDR_gameNotInProgressCounterWriteOnly, 0);
    swosWriteWord(ADDR_cameraDirection, (uint16_t)-1);

    int16_t teamStarting = swosReadSignedWord(ADDR_teamStarting);
    int16_t teamPlayingUp = swosReadSignedWord(ADDR_teamPlayingUp);
    int lastTeam = (teamStarting == teamPlayingUp) ? TEAMDATA_TOP_BASE : TEAMDATA_BOTTOM_BASE;
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)lastTeam);

    swosWriteWord(ADDR_stoppageTimerTotal, 0);
    swosWriteWord(ADDR_stoppageTimerActive, 0);

    swosTeamPortStopAllPlayers();

    swosWriteWord(ADDR_cameraXVelocity, 0);
    swosWriteWord(ADDR_cameraYVelocity, 0);
}

// swos.asm:103968-103998 -- EndFirstExtraTime.
static void endFirstExtraTime(void)
{
    swosWriteWord(ADDR_halfNumber, 2);

    int16_t tpu = swosReadSignedWord(ADDR_teamPlayingUp);
    swosWriteWord(ADDR_teamPlayingUp, (uint16_t)(3 - tpu));
    int16_t ts = swosReadSignedWord(ADDR_teamStarting);
    swosWriteWord(ADDR_teamStarting, (uint16_t)(3 - ts));

    swosWriteWord(ADDR_hideBall, 0);

    initTeamsDataForExtraTime();

    swosWriteWord(ADDR_stoppageEventTimer, 110);
    swosWriteWord(ADDR_gameState, K_ST_FIRST_EXTRA_ENDED);
    swosWriteWord(ADDR_breakCameraMode, (uint16_t)-1);
    swosWriteWord(ADDR_gameStatePl, K_ST_GAME_STATE_PL_STOPPED);
    swosWriteWord(ADDR_gameNotInProgressCounterWriteOnly, 0);
    swosWriteWord(ADDR_cameraDirection, (uint16_t)-1);

    int16_t teamStarting = swosReadSignedWord(ADDR_teamStarting);
    int16_t teamPlayingUp = swosReadSignedWord(ADDR_teamPlayingUp);
    int lastTeam = (teamStarting == teamPlayingUp) ? TEAMDATA_TOP_BASE : TEAMDATA_BOTTOM_BASE;
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)lastTeam);

    swosWriteWord(ADDR_stoppageTimerTotal, 0);
    swosWriteWord(ADDR_stoppageTimerActive, 0);

    swosTeamPortStopAllPlayers();

    swosWriteWord(ADDR_cameraXVelocity, 0);
    swosWriteWord(ADDR_cameraYVelocity, 0);
}

// swos.asm:100500-100560 -- StartPenalties.
static void startPenalties(void)
{
    swosWriteWord(ADDR_penaltiesState, (uint16_t)-1);

    int16_t s1 = swosReadSignedWord(ADDR_statsTeam1Goals);
    int16_t s2 = swosReadSignedWord(ADDR_statsTeam2Goals);
    swosWriteWord(ADDR_savedTeam1Goals, s1);
    swosWriteWord(ADDR_savedTeam2Goals, s2);

    swosWriteWord(ADDR_statsTeam1Goals, 0);
    swosWriteWord(ADDR_team1GoalsDigit1, 0);
    swosWriteWord(ADDR_team1GoalsDigit2, 0);
    swosWriteWord(ADDR_statsTeam2Goals, 0);
    swosWriteWord(ADDR_team2GoalsDigit1, 0);
    swosWriteWord(ADDR_team2GoalsDigit2, 0);

    swosWriteWord(ADDR_team1PenaltyGoals, 0);
    swosWriteWord(ADDR_team2PenaltyGoals, 0);

    swosWriteWord(ADDR_teamPlayingUp, (uint16_t)((swosRngNextByte() & 1) + 1));
    swosWriteWord(ADDR_teamStarting, (uint16_t)((swosRngNextByte() & 1) + 1));

    swosWriteWord(ADDR_team1PenaltyShooterIndex, 11);
    swosWriteWord(ADDR_team2PenaltyShooterIndex, 11);
    swosWriteWord(ADDR_team1PenaltyAttempts, 0);
    swosWriteWord(ADDR_team2PenaltyAttempts, 0);

    swosWriteWord(ADDR_playingPenalties, 1);
    swosWriteWord(ADDR_dontShowScorers, 1);

    resetPenaltyShooters();

    swosGameTimeNextPenalty();
}

// game.cpp:396-421 -- InitTeamsDataForExtraTime (scalar reset block only --
// see the C#'s own comment on why the per-team field reseat is skipped).
static void initTeamsDataForExtraTime(void)
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
    // game.cpp:420 -- breakState. PHASE 1 BOOTSTRAP FOLLOW-UP (2026-09-16):
    // ADDR_breakState didn't exist yet when this function was first ported;
    // it landed with the step-11B break-camera-mode ladder. Filling the gap
    // now while porting Kickoff.cs's InitTeamsData (game.cpp:396-421, the
    // exact same source block, ported there with this write present from
    // the start -- see swos_kickoff.c) so the two duplicate copies stay
    // consistent, matching the C# source's own duplication.
    swosWriteWord(ADDR_breakState, 0);
    swosWriteWord(ADDR_breakCameraMode, (uint16_t)-1);
}

// swos.asm:100533-100557 -- ResetPenaltyShooters.
static void resetPenaltyShooters(void)
{
    for (int slot = 0; slot < PLSPR_TOTAL_SLOTS; slot++)
    {
        int spriteAddr = swosPlayerSpriteBase(slot);
        swosWriteWord(spriteAddr + PLSPR_OFF_CARDS, 0);
        swosWriteWord(spriteAddr + PLSPR_OFF_SENT_AWAY, 0);
    }
}

// game.cpp:828-885 -- NextPenalty.
void swosGameTimeNextPenalty(void)
{
    int16_t t1Attempts = swosReadSignedWord(ADDR_team1PenaltyAttempts);
    int16_t t2Attempts = swosReadSignedWord(ADDR_team2PenaltyAttempts);
    int16_t t1Goals    = swosReadSignedWord(ADDR_team1PenaltyGoals);
    int16_t t2Goals    = swosReadSignedWord(ADDR_team2PenaltyGoals);

    int total = t1Attempts + t2Attempts;
    bool finish = false;

    if (total >= 10)
    {
        if (t1Attempts == t2Attempts && t1Goals != t2Goals)
            finish = true;
    }
    else
    {
        int gap1 = (5 - t1Attempts) + t1Goals;
        if (gap1 < t2Goals)
            finish = true;
        else
        {
            int gap2 = (5 - t2Attempts) + t2Goals;
            if (gap2 < t1Goals)
                finish = true;
        }
    }

    if (finish)
    {
        swosWriteWord(ADDR_playingPenalties, 0);
        int16_t savedT1 = swosReadSignedWord(ADDR_savedTeam1Goals);
        swosWriteWord(ADDR_statsTeam1Goals, savedT1);
        int16_t savedT2 = swosReadSignedWord(ADDR_savedTeam2Goals);
        swosWriteWord(ADDR_statsTeam2Goals, savedT2);
        swosGameLoopPlayersLeavingPitch();
        return;
    }

    // game.cpp:942-1078 -- l_next_penalty: per-pen FSM init.
    swosWriteWord(BALLSPR_BASE + BALLSPR_OFF_Z + 2, 0);

    int16_t tpu = swosReadSignedWord(ADDR_teamPlayingUp);
    swosWriteWord(ADDR_teamPlayingUp, (uint16_t)(3 - tpu));
    int16_t ts = swosReadSignedWord(ADDR_teamStarting);
    swosWriteWord(ADDR_teamStarting, (uint16_t)(3 - ts));

    swosWriteWord(ADDR_hideBall, 0);

    initTeamsDataForExtraTime();

    swosWriteWord(ADDR_gameState, 31);
    swosWriteWord(ADDR_breakCameraMode, (uint16_t)-1);
    swosWriteWord(ADDR_cameraDirection, 0);
    swosWriteByte(ADDR_playerTurnFlags, 131);
    swosWriteWord(ADDR_foulXCoordinate, 336);
    swosWriteWord(ADDR_foulYCoordinate, 187);
    swosWriteWord(ADDR_gameStatePl, K_ST_GAME_STATE_PL_STOPPED);
    swosWriteWord(ADDR_gameNotInProgressCounterWriteOnly, 0);

    int a6 = TEAMDATA_BOTTOM_BASE;
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)a6);

    swosWriteWord(ADDR_stoppageTimerTotal, 0);
    swosWriteWord(ADDR_stoppageTimerActive, 0);

    swosTeamPortStopAllPlayers();

    swosWriteWord(ADDR_cameraXVelocity, 0);
    swosWriteWord(ADDR_cameraYVelocity, 0);

    swosWriteWord(ADDR_penaltiesTimer, 0);

    int16_t bottomTeamNumber = swosReadSignedWord(a6 + TEAMDATA_OFF_TEAM_NUMBER);
    int shooterIndexAddr = (bottomTeamNumber == 1) ? ADDR_team1PenaltyShooterIndex : ADDR_team2PenaltyShooterIndex;

    int16_t shooterIdx = swosReadSignedWord(shooterIndexAddr);
    shooterIdx = (int16_t)(shooterIdx - 1);
    if (shooterIdx == 0)
        shooterIdx = 10;
    swosWriteWord(shooterIndexAddr, (uint16_t)shooterIdx);

    int32_t spritesTable = swosReadSignedDword(a6 + TEAMDATA_OFF_PLAYERS);
    int32_t shooterSprite = swosReadSignedDword(spritesTable + shooterIdx * 4);
    swosWriteDword(ADDR_penaltyShooterSprite, (uint32_t)shooterSprite);

    if (bottomTeamNumber == 1)
    {
        int16_t t1Att = swosReadSignedWord(ADDR_team1PenaltyAttempts);
        swosWriteWord(ADDR_team1PenaltyAttempts, (uint16_t)(t1Att + 1));
    }
    else
    {
        int16_t t2Att = swosReadSignedWord(ADDR_team2PenaltyAttempts);
        swosWriteWord(ADDR_team2PenaltyAttempts, (uint16_t)(t2Att + 1));
    }
}

// PlayerInfo field accessors -- swos.h:162-188. cards@+10, halfPlayed@+36.
static int readPlayerInfoCards(int playerInfoAddr)
{
    return playerInfoAddr == 0 ? 0 : swosReadByte(playerInfoAddr + 10);
}
static int readPlayerInfoHalfPlayed(int playerInfoAddr)
{
    return playerInfoAddr == 0 ? 0 : swosReadByte(playerInfoAddr + 36);
}
static void writePlayerInfoHalfPlayed(int playerInfoAddr, int v)
{
    if (playerInfoAddr != 0) swosWriteByte(playerInfoAddr + 36, v);
}

// game.cpp:1361-1379 -- initPlayerCardChance().
void swosGameTimeInitPlayerCardChance(void)
{
    static const int kPlayerCardChancesPerGameLength[4][16] = {
        { 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 7, 7, 8, 9, 10 },
        { 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5,  6 },
        { 1, 1, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4,  4 },
        { 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3,  3 },
    };

    int16_t row = swosReadSignedWord(ADDR_gameLengthInGame);
    if (row < 0) row = 0;
    if (row > 3) row = 3;

    int col = (swosRngNextByte() & 0x1E) >> 1;

    swosWriteWord(ADDR_playerCardChance, (uint16_t)kPlayerCardChancesPerGameLength[row][col]);
}

// game.cpp:1381-1385 -- determineStartingTeamAndTeamPlayingUp().
void swosGameTimeDetermineStartingTeamAndTeamPlayingUp(void)
{
    swosWriteWord(ADDR_teamPlayingUp, (uint16_t)((swosRngNextByte() & 1) + 1));
    swosWriteWord(ADDR_teamStarting,  (uint16_t)((swosRngNextByte() & 1) + 1));
}

// game.cpp:1387-1401 -- initPitchBallFactors().
void swosGameTimeInitPitchBallFactors(void)
{
    static const int kPitchBallSpeedInfluence[7]    = { -3, 4, 1, 0, 0, -1, -1 };
    static const int kBallSpeedBounceFactorTable[7] = { 24, 80, 80, 72, 64, 40, 32 };
    static const int kBallBounceFactorTable[7]      = { 88, 112, 104, 104, 96, 88, 80 };

    int pitchType = g_swosBallSimCurrentPitchType;
    if (pitchType < 0) pitchType = 0;
    if (pitchType > 6) pitchType = 6;

    swosWriteWord(ADDR_pitchBallSpeedFactor,  (uint16_t)kPitchBallSpeedInfluence[pitchType]);
    swosWriteWord(ADDR_ballSpeedBounceFactor, (uint16_t)kBallSpeedBounceFactorTable[pitchType]);
    swosWriteWord(ADDR_ballBounceFactor,      (uint16_t)kBallBounceFactorTable[pitchType]);
}

// game.cpp:1237-1247 -- saveTeams()/restoreTeams(). 1024-byte snapshot per
// side (Memory.cs's per-team in-game-team slot size -- see header comment
// on the C# side for why 1024 safely covers the live struct content).
#define K_IN_GAME_TEAM_SLOT_BYTES 1024
static uint8_t s_topTeamSaved[K_IN_GAME_TEAM_SLOT_BYTES];
static uint8_t s_bottomTeamSaved[K_IN_GAME_TEAM_SLOT_BYTES];

void swosGameTimeSaveTeams(void)
{
    const uint8_t *src = swosMemoryView(ADDR_team1InGameTeamHeader, K_IN_GAME_TEAM_SLOT_BYTES);
    memcpy(s_topTeamSaved, src, K_IN_GAME_TEAM_SLOT_BYTES);
    src = swosMemoryView(ADDR_team2InGameTeamHeader, K_IN_GAME_TEAM_SLOT_BYTES);
    memcpy(s_bottomTeamSaved, src, K_IN_GAME_TEAM_SLOT_BYTES);
}

void swosGameTimeRestoreTeams(void)
{
    for (int i = 0; i < K_IN_GAME_TEAM_SLOT_BYTES; i++)
        swosWriteByte(ADDR_team1InGameTeamHeader + i, s_topTeamSaved[i]);
    for (int i = 0; i < K_IN_GAME_TEAM_SLOT_BYTES; i++)
        swosWriteByte(ADDR_team2InGameTeamHeader + i, s_bottomTeamSaved[i]);
}

// game.cpp:1403-1448 (+ game.cpp:75) -- initGameVariables().
void swosGameTimeInitGameVariables(void)
{
    swosWriteWord(ADDR_playingPenalties, 0);
    swosWriteWord(ADDR_dontShowScorers, 0);
    swosWriteWord(ADDR_statsTimer, 0);
    swosWriteWord(ADDR_g_waitForPlayerToGoInTimer, 0);
    swosWriteWord(ADDR_g_substituteInProgress, 0);

    swosWriteWord(ADDR_statsTeam1Goals, 0);
    swosWriteWord(ADDR_team1GoalsDigit1, 0);
    swosWriteWord(ADDR_team1GoalsDigit2, 0);
    swosWriteWord(ADDR_statsTeam2Goals, 0);
    swosWriteWord(ADDR_team2GoalsDigit1, 0);
    swosWriteWord(ADDR_team2GoalsDigit2, 0);

    swosWriteWord(ADDR_team1TotalGoals, 0);
    swosWriteWord(ADDR_team2TotalGoals, 0);
    if (swosReadSignedWord(ADDR_secondLeg) != 0)
    {
        swosWriteWord(ADDR_team1TotalGoals, (uint16_t)swosReadSignedWord(ADDR_team1GoalsFirstLeg));
        swosWriteWord(ADDR_team2TotalGoals, (uint16_t)swosReadSignedWord(ADDR_team2GoalsFirstLeg));
    }

    swosWriteWord(ADDR_team1NumSubs, 0);
    swosWriteWord(ADDR_team2NumSubs, 0);

    // game.cpp:1427-1428 -- memset(&team{1,2}StatsData, 0, sizeof(TeamStatsData)).
    // 14 semantic bytes; the Memory.cs slot reserves 32 -- zero only the 14
    // the C# zeroes (over-zeroing the padding isn't in the original).
    for (int i = 0; i < 14; i++)
    {
        swosWriteByte(ADDR_topTeamStatsData + i, 0);
        swosWriteByte(ADDR_bottomTeamStatsData + i, 0);
    }

    // game.cpp:1430-1431 -- memset((char*)&team{1,2}Data + 24, 0, sizeof(team) - 24).
    const int kTeamDataScrubFrom = 24;
    const int kTeamDataScrubTo = 145;
    for (int i = kTeamDataScrubFrom; i < kTeamDataScrubTo; i++)
    {
        swosWriteByte(TEAMDATA_TOP_BASE + i, 0);
        swosWriteByte(TEAMDATA_BOTTOM_BASE + i, 0);
    }

    swosWriteWord(ADDR_goalCounter, 0);
    swosWriteWord(ADDR_stateGoal, 0);

    swosWriteWord(ADDR_cameraXVelocity, 0);
    swosWriteWord(ADDR_cameraYVelocity, 0);

    swosWriteWord(ADDR_ic_pl1Fire, 0);
    swosWriteWord(ADDR_ic_pl2Fire, 0);

    swosWriteWord(ADDR_longFireFlag, 0);
    swosWriteWord(ADDR_longFireTime, 0);

    swosWriteWord(ADDR_currentGameTick, 0);
    swosWriteWord(ADDR_currentTick, 0);

    swosWriteWord(ADDR_AI_turnDirection, 1);

    // game.cpp:75 -- one Rng byte (SAME stream-1 source as pitch.cpp /
    // initPlayerCardChance), stashed for ApplyTeamTactics (not yet ported).
    swosWriteWord(ADDR_gameRandValue, (uint16_t)swosRngNextByte());
}
