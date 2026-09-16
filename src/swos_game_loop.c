// SOURCE: openswos game/scripts/Sim/Port/GameLoop.cs (see swos_game_loop.h
// for the exact scope ported / omitted).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes. Labels
// and goto preserved exactly where the C# used them.
#include "swos_game_loop.h"

#include <stdint.h>

#include "swos_addr.h"
#include "swos_ball_sprite.h"
#include "swos_ball_update.h"
#include "swos_bench.h"
#include "swos_camera.h"
#include "swos_game_sprites.h"
#include "swos_game_time.h"
#include "swos_input_controls.h"
#include "swos_kickoff.h"
#include "swos_memory.h"
#include "swos_player_actions.h"
#include "swos_player_energy.h"
#include "swos_player_name_display.h"
#include "swos_player_sprite.h"
#include "swos_referee.h"
#include "swos_result.h"
#include "swos_set_pieces.h"
#include "swos_spinning_logo.h"
#include "swos_sprite_update.h"
#include "swos_stats.h"
#include "swos_team_data.h"
#include "swos_team_port.h"
#include "swos_update_players.h"

// game.cpp:711 -- GameState::kPlayersGoingToShower. game.cpp:713 -- kStopped.
#define K_ST_PLAYERS_GOING_TO_SHOWER 24
#define K_ST_STOPPED 101
#define K_ST_GAME_IN_PROGRESS 100
#define K_ST_WAITING_ON_PLAYER 102
#define K_ST_KEEPER_HOLDS_THE_BALL 3
#define K_ST_STARTING_GAME 21
#define K_ST_CAMERA_GOING_TO_SHOWERS 22
#define K_ST_GOING_TO_HALFTIME 23
#define K_ST_RESULT_ON_HALFTIME 25
#define K_ST_RESULT_AFTER_THE_GAME 26
#define K_ST_FIRST_HALF_ENDED 29
#define K_ST_GAME_ENDED 30
#define K_ST_FIRST_EXTRA_STARTING 27
#define K_ST_FIRST_EXTRA_ENDED 28

void swosGameLoopPlayersLeavingPitch(void)
{
    swosWriteWord(ADDR_hideBall, 0);
    swosWriteWord(ADDR_stoppageEventTimer, 275);
    swosWriteWord(ADDR_gameState, K_ST_PLAYERS_GOING_TO_SHOWER);
    swosWriteWord(ADDR_breakCameraMode, (uint16_t)-1);
    swosWriteWord(ADDR_gameStatePl, K_ST_STOPPED);
    swosWriteWord(ADDR_gameNotInProgressCounterWriteOnly, 0);
    swosWriteWord(ADDR_cameraDirection, (uint16_t)-1);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)TEAMDATA_TOP_BASE);
    swosWriteWord(ADDR_stoppageTimerTotal, 0);
    swosWriteWord(ADDR_stoppageTimerActive, 0);
    swosTeamPortStopAllPlayers();
    swosWriteWord(ADDR_cameraXVelocity, 0);
    swosWriteWord(ADDR_cameraYVelocity, 0);
    swosWriteWord(ADDR_stateGoal, 0);
}

// ---- Forward declarations of static helpers --------------------------------
static void stubLoadCrowdChantSampleIfNeeded(void);
static void readTimerDelta(void);
static void stubHandlePauseAndStats(void);
static void playEnqueuedSamples(void);
static void initGoalSprites(void);
static void doGoalkeeperSprites(void);
static void markPlayer(void);
static void drawAnimatedPatterns(void);
static void updatePlayersWrap(bool top);
static void negateResultPanelTimers(void);
static void dispatchStoppageEventTriggered(int16_t gameStatePl);
static void dispatchBreakCameraMode(void);
static void mode0(int16_t gameState);
static void mode1(int16_t gameState);
static void mode2(int16_t gameState);
static void mode3(int16_t gameState);
static void mode4(int16_t gameState);
static void mode5(int16_t gameState);
static void mode6(int16_t gameState);
static void mode7(int16_t gameState);
static void mode8(int16_t gameState);

// ====================================================================
// Tick -- gameLoop.cpp:95-130
// ====================================================================
void swosGameLoopTick(void)
{
    stubLoadCrowdChantSampleIfNeeded();
    swosGameLoopUpdateTimers();
    stubHandlePauseAndStats();
    // StubHandleKeys() omitted -- truly a no-op, see header.
    swosGameLoopCoreGameUpdate();
}

// ====================================================================
// updateTimers -- gameLoop.cpp:265-273
// ====================================================================
void swosGameLoopUpdateTimers(void)
{
    readTimerDelta();

    int32_t frame = swosReadSignedDword(ADDR_frameCounter) + 1;
    swosWriteDword(ADDR_frameCounter, (uint32_t)frame);

    int16_t lastFrameTicks = swosReadSignedWord(ADDR_lastFrameTicks);
    if (lastFrameTicks <= 0) lastFrameTicks = 1;
    uint16_t gameTick = swosReadWord(ADDR_currentGameTick);
    swosWriteWord(ADDR_currentGameTick, (uint16_t)(gameTick + (uint16_t)lastFrameTicks));

    int16_t spaceReplay = swosReadSignedWord(ADDR_spaceReplayTimer);
    if (spaceReplay != 0)
        swosWriteWord(ADDR_spaceReplayTimer, (uint16_t)(spaceReplay - 1));
}

// swos.asm:18874-18889 -- ReadTimerDelta.
static void readTimerDelta(void)
{
    uint16_t cur = swosReadWord(ADDR_currentGameTick);
    uint16_t last = swosReadWord(ADDR_lastGameTick);
    uint16_t delta = (uint16_t)(cur - last);
    if (delta == 0) delta = 1;
    swosWriteWord(ADDR_lastFrameTicks, delta);
    swosWriteWord(ADDR_lastGameTick, cur);
}

// gameLoop.cpp:275-281 -- handlePauseAndStats. Only the real side-effect
// (the one-shot statsEnqueued clear) is ported -- see header.
static void stubHandlePauseAndStats(void)
{
    int16_t statsEnqueued = swosReadSignedWord(ADDR_gl_statsEnqueued);
    if (statsEnqueued != 0)
        swosWriteWord(ADDR_gl_statsEnqueued, 0);
}

// gameLoop.cpp:1903-1909 -- loadCrowdChantSampleIfNeeded. Only the one-shot
// flag clear is ported -- see header.
static void stubLoadCrowdChantSampleIfNeeded(void)
{
    if (swosReadWord(ADDR_loadCrowdChantSampleFlag) != 0)
    {
        // MatchAudio.LoadCrowdChant() omitted -- audio, see header.
        swosWriteWord(ADDR_loadCrowdChantSampleFlag, 0);
    }
}

// comments.cpp:180-210 -- playEnqueuedSamples. Only the real side-effect
// (goalCounter-- while > 0) is ported -- see header.
static void playEnqueuedSamples(void)
{
    int16_t goalCounter = swosReadSignedWord(ADDR_goalCounter);
    if (goalCounter != 0)
        swosWriteWord(ADDR_goalCounter, (uint16_t)(goalCounter - 1));
    // MatchAudio.Tick() omitted -- audio, no-op headless, see header.
}

// gameLoop.cpp:1911-1915 -- initGoalSprites.
#define K_TOP_GOAL_SPRITE    1205
#define K_BOTTOM_GOAL_SPRITE 1206
static void initGoalSprites(void)
{
    swosWriteWord(ADDR_goal1TopSprite_ImageIndex, K_TOP_GOAL_SPRITE);
    swosWriteWord(ADDR_goal2BottomSprite_ImageIndex, K_BOTTOM_GOAL_SPRITE);
}

// ====================================================================
// coreGameUpdate -- gameLoop.cpp:289-317
// ====================================================================
void swosGameLoopCoreGameUpdate(void)
{
    swosCameraMoveCamera();
    playEnqueuedSamples();
    swosGameTimeUpdateGameTime();
    initGoalSprites();
    swosGameLoopUpdateGameTimersAndCameraBreakMode();

    if (!swosGameLoopUpdateFireBlocked())
    {
        bool top = swosGameLoopSelectTeamForUpdate();
        swosUpdateTeamControls(top);
        updatePlayersWrap(top);
        swosPostUpdateTeamControls(top);
    }

    swosBallUpdateTick();
    swosMoveAllPlayers();
    swosRefereeUpdateReferee();
    swosGameSpritesUpdateCornerFlags();
    swosSpinningLogoUpdateSpinningLogo();
    doGoalkeeperSprites();
    swosGameSpritesUpdateControlledPlayerNumbers();
    markPlayer();
    swosPlayerNameDisplayUpdateCurrentPlayerName();
    swosRefereeUpdateBookedPlayerNumberSprite();
    swosResultUpdateResult();
    drawAnimatedPatterns();
    swosBenchUpdateBench();
    swosStatsUpdateStatistics();
}

static void updatePlayersWrap(bool top)
{
    int teamIndex = top ? 0 : 1;
    swosUpdatePlayersUpdate(teamIndex);
}

// gameControls.cpp:48-56 -- updateFireBlocked.
bool swosGameLoopUpdateFireBlocked(void)
{
    uint16_t fireBlocked = swosReadWord(ADDR_fireBlocked);
    if (fireBlocked != 0)
    {
        if (!swosIsAnyPlayerFiring())
            swosWriteWord(ADDR_fireBlocked, 0);
        return true;
    }
    return false;
}

// gameControls.cpp:58-62 -- selectTeamForUpdate.
bool swosGameLoopSelectTeamForUpdate(void)
{
    int32_t counter = swosReadSignedDword(ADDR_teamSwitchCounter) + 1;
    swosWriteDword(ADDR_teamSwitchCounter, (uint32_t)counter);
    return (counter & 1) != 0; // true = top
}

// ====================================================================
// updateGameTimersAndCameraBreakMode -- gameLoop.cpp:426-1853
// ====================================================================
void swosGameLoopUpdateGameTimersAndCameraBreakMode(void)
{
    // PORT NOTE (interval seeds) -- see C# comment: correct two stale
    // Memory.Init seeds idempotently, leaving a deliberate override intact.
    if (swosReadSignedWord(ADDR_m_goalCameraInterval) == 50)
        swosWriteWord(ADDR_m_goalCameraInterval, 55);
    if (swosReadSignedWord(ADDR_m_allowPlayerControlCameraInterval) == 75)
        swosWriteWord(ADDR_m_allowPlayerControlCameraInterval, 550);

    // gameLoop.cpp:430-469 -- penalty shootout inter-pen pause.
    swosSetPiecesAdvancePenaltiesTimer();

    int16_t gameStatePl = swosReadSignedWord(ADDR_gameStatePl);
    if (gameStatePl == K_ST_GAME_IN_PROGRESS)
    {
        int16_t ticks = swosReadSignedWord(ADDR_lastFrameTicks);
        int16_t ig = swosReadSignedWord(ADDR_inGameCounter);
        swosWriteWord(ADDR_inGameCounter, (uint16_t)(ig + ticks));
        return;
    }

    // l_game_not_in_progress.
    int16_t notIn = swosReadSignedWord(ADDR_gameNotInProgressCounterWriteOnly);
    swosWriteWord(ADDR_gameNotInProgressCounterWriteOnly, (uint16_t)(notIn + 1));

    int16_t ticks2 = swosReadSignedWord(ADDR_lastFrameTicks);
    int16_t st = swosReadSignedWord(ADDR_stoppageTimerTotal);
    swosWriteWord(ADDR_stoppageTimerTotal, (uint16_t)(st + ticks2));

    if (gameStatePl == K_ST_WAITING_ON_PLAYER)
    {
        int16_t active = swosReadSignedWord(ADDR_stoppageTimerActive);
        active = (int16_t)(active + ticks2);
        swosWriteWord(ADDR_stoppageTimerActive, (uint16_t)active);

        int32_t teamPtr = swosReadSignedDword(ADDR_lastTeamPlayedBeforeBreak);
        if (teamPtr == 0)
            return;
        int16_t playerNumber = swosReadSignedWord(teamPtr + TEAMDATA_OFF_PLAYER_NUMBER);

        int16_t interval = swosReadSignedWord(ADDR_m_initalKickInterval);

        // === PORT-ONLY LAST-RESORT SAFETY NET === (see C# comment).
        if (playerNumber == 0 && (uint16_t)active >= (uint16_t)(2 * interval))
        {
            int32_t kicker = swosReadSignedDword(teamPtr + TEAMDATA_OFF_CONTROLLED_PLAYER);
            if (kicker == 0)
                kicker = swosReadSignedDword(teamPtr + TEAMDATA_OFF_PASSING_KICKING_PLAYER);
            // Godot.GD.Print(...) omitted -- debug log, zero Memory effect.
            if (kicker != 0)
                swosPlayerKickingBall(teamPtr, kicker);
            swosWriteWord(ADDR_stoppageTimerActive, 0);
            return;
        }
        // === end PORT-ONLY safety net ===

        if (playerNumber != 0)
            return;

        if ((uint16_t)active < (uint16_t)interval)
            return;

        swosKickoffPrepareForInitialKick();

        int16_t kickTicks = swosReadSignedWord(ADDR_initialKickWriteOnlyTicks);
        swosWriteWord(ADDR_initialKickWriteOnlyTicks, (uint16_t)(kickTicks + 1));

        return;
    }

    // l_not_waiting_on_player -- gameLoop.cpp:577-816.
    int16_t gameState = swosReadSignedWord(ADDR_gameState);

    if ((uint16_t)gameState >= 21 && (uint16_t)gameState <= 30)
    {
        bool firePressed = false;

        if (swosReadByte(TEAMDATA_TOP_BASE + TEAMDATA_OFF_FIRE_PRESSED) != 0)
            firePressed = true;
        else if (swosReadByte(TEAMDATA_BOTTOM_BASE + TEAMDATA_OFF_FIRE_PRESSED) != 0)
            firePressed = true;
        else
        {
            int16_t topCoach = swosReadSignedWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PLAYER_COACH_NUMBER);
            int16_t bottomCoach = swosReadSignedWord(TEAMDATA_BOTTOM_BASE + TEAMDATA_OFF_PLAYER_COACH_NUMBER);
            if (topCoach == 1 || bottomCoach == 1)
            {
                if (swosReadSignedWord(ADDR_ic_pl1Fire) != 0)
                    firePressed = true;
            }
            if (!firePressed && (topCoach == 2 || bottomCoach == 2))
            {
                if (swosReadSignedWord(ADDR_ic_pl2Fire) != 0)
                    firePressed = true;
            }
            if (!firePressed)
            {
                if (gameState == K_ST_RESULT_AFTER_THE_GAME &&
                    (swosReadSignedWord(ADDR_ic_pl1Fire) != 0 || swosReadSignedWord(ADDR_ic_pl2Fire) != 0))
                    firePressed = true;
            }
        }

        if (firePressed)
        {
            if (gameState == K_ST_RESULT_ON_HALFTIME)
            {
                negateResultPanelTimers();
                swosGameLoopSetCameraMovingToShowerState();
                swosKickoffPrepareForInitialKick();
                swosWriteWord(ADDR_stoppageEventTimer, 0);
                return;
            }
            if (gameState == K_ST_RESULT_AFTER_THE_GAME)
            {
                negateResultPanelTimers();
                swosWriteWord(ADDR_playGame, 0);
                swosWriteWord(ADDR_stoppageEventTimer, 0);
                return;
            }
            if (gameState == K_ST_STARTING_GAME)
            {
                swosWriteWord(ADDR_showFansCounter, 0);
                swosKickoffPrepareForInitialKick();
                swosWriteWord(ADDR_stoppageEventTimer, 0);
                return;
            }
            if (gameState == K_ST_CAMERA_GOING_TO_SHOWERS)
            {
                swosKickoffPrepareForInitialKick();
                swosWriteWord(ADDR_stoppageEventTimer, 0);
                return;
            }
            // Other states in 21..30 fall through to l_game_started --
            // matches the original (only 25/26/21/22 have skip handlers).
        }
    }

    // l_game_started -- gameLoop.cpp:818-843.
    int16_t evt = swosReadSignedWord(ADDR_stoppageEventTimer);
    if (evt != 0)
    {
        evt = (int16_t)(evt - ticks2);
        swosWriteWord(ADDR_stoppageEventTimer, (uint16_t)evt);
        if (evt > 0)
            return;
    }

    // l_stoppage_event_triggered.
    dispatchStoppageEventTriggered(gameStatePl);
}

// The recurring `neg resultTimer ; neg timeVar ; neg statsTimer` triple.
static void negateResultPanelTimers(void)
{
    int32_t rt = swosReadSignedDword(ADDR_resultTimer);
    swosWriteDword(ADDR_resultTimer, (uint32_t)(-rt));
    int16_t tv = swosReadSignedWord(ADDR_timeVar);
    swosWriteWord(ADDR_timeVar, (uint16_t)(-tv));
    int16_t stt = swosReadSignedWord(ADDR_statsTimer);
    swosWriteWord(ADDR_statsTimer, (uint16_t)(-stt));
}

// gameLoop.cpp:845-1108 -- l_stoppage_event_triggered dispatcher.
static void dispatchStoppageEventTriggered(int16_t gameStatePl)
{
    swosWriteWord(ADDR_stoppageEventTimer, 0);

    if (gameStatePl != K_ST_STOPPED)
    {
        dispatchBreakCameraMode();
        return;
    }

    int16_t gs = swosReadSignedWord(ADDR_gameState);

    if (gs == K_ST_RESULT_ON_HALFTIME)
    {
        negateResultPanelTimers();
        swosGameLoopSetCameraMovingToShowerState();
        return;
    }
    if (gs == K_ST_RESULT_AFTER_THE_GAME)
    {
        negateResultPanelTimers();
        swosWriteWord(ADDR_playGame, 0);
        return;
    }
    if (gs == K_ST_STARTING_GAME)
    {
        swosKickoffPrepareForInitialKick();
        return;
    }
    if (gs == K_ST_CAMERA_GOING_TO_SHOWERS)
    {
        swosKickoffPrepareForInitialKick();
        return;
    }
    if (gs == K_ST_FIRST_HALF_ENDED)
    {
        swosGameLoopFirstHalfJustEnded();
        return;
    }
    if (gs == K_ST_GOING_TO_HALFTIME)
    {
        negateResultPanelTimers();
        swosGameLoopGoToHalftime();
        return;
    }
    if (gs == K_ST_GAME_ENDED)
    {
        swosGameLoopPlayersLeavingPitch();
        return;
    }
    if (gs == K_ST_PLAYERS_GOING_TO_SHOWER)
    {
        swosGameLoopGameOver();
        return;
    }
    if (gs == K_ST_FIRST_EXTRA_STARTING)
    {
        swosKickoffPrepareForInitialKick();
        return;
    }
    if (gs == K_ST_FIRST_EXTRA_ENDED)
    {
        swosKickoffPrepareForInitialKick();
        return;
    }

    // l_first_extra_not_ended -- gameLoop.cpp:1075-1108. Plain stoppage.
    swosWriteWord(ADDR_gameStatePl, (uint16_t)gs);

    if (gs == K_ST_KEEPER_HOLDS_THE_BALL
        && swosReadSignedWord(ADDR_g_inSubstitutesMenu) == 0
        && swosReadSignedWord(ADDR_g_cameraLeavingSubsTimer) == 0)
    {
        swosWriteWord(ADDR_timeVar, 31000);
    }

    // cseg_73993.
    swosWriteWord(ADDR_breakCameraMode, 0);
}

// gameLoop.cpp:1110-1846 -- l_game_running. Branches on breakCameraMode.
static void dispatchBreakCameraMode(void)
{
    int16_t mode = swosReadSignedWord(ADDR_breakCameraMode);

    if (swosReadSignedWord(ADDR_g_inSubstitutesMenu) != 0) return;
    if (swosReadSignedWord(ADDR_g_waitForPlayerToGoInTimer) != 0) return;

    int16_t gameState = swosReadSignedWord(ADDR_gameState);

    if (mode == 0) { mode0(gameState); return; }
    if (mode == 1) { mode1(gameState); return; }
    if (mode == 2) { mode2(gameState); return; }
    if (mode == 4) { mode4(gameState); return; }
    if (mode == 3) { mode3(gameState); return; }
    if (mode == 5) { mode5(gameState); return; }
    if (mode == 6) { mode6(gameState); return; }
    if (mode == 7) { mode7(gameState); return; }
    if (mode == 8) { mode8(gameState); return; }

    // l_assert_failed -- unknown mode is a no-op (a -1 mode can transiently
    // be observed between a state setter and the next dispatcher pass).
}

// gameLoop.cpp:1127-1175 -- breakCameraMode == 0 branch.
static void mode0(int16_t gameState)
{
    int32_t dx = swosReadSignedDword(BALLSPR_BASE + BALLSPR_OFF_DELTA_X);
    if (dx != 0) return;
    int32_t dy = swosReadSignedDword(BALLSPR_BASE + BALLSPR_OFF_DELTA_Y);
    if (dy != 0) return;

    swosWriteWord(ADDR_breakCameraMode, 1);

    if (gameState == K_ST_KEEPER_HOLDS_THE_BALL) return;

    int16_t interval = swosReadSignedWord(ADDR_m_goalCameraInterval);
    swosWriteWord(ADDR_stoppageEventTimer, (uint16_t)interval);
    int16_t goalCameraMode = swosReadSignedWord(ADDR_goalCameraMode);
    if (goalCameraMode != 0)
        swosWriteWord(ADDR_stoppageEventTimer, 75);
}

// gameLoop.cpp:1177-1344 -- breakCameraMode == 1 branch.
static void mode1(int16_t gameState)
{
    // gameLoop.cpp:1189-1254 -- the asm's gameState comparisons before
    // l_game_stopped are dead flag updates (no conditional jump on the
    // final one) -- EVERY gameState reaches l_game_stopped. See C# comment.

    if (swosReadSignedWord(ADDR_playingPenalties) != 0)
        goto l_no_auto_replay;

    {
        int16_t goalCameraMode = swosReadSignedWord(ADDR_goalCameraMode);
        if (goalCameraMode == 0)
            goto l_no_auto_replay;

        if (swosReadSignedWord(ADDR_g_autoSaveHighlights) != 0)
            swosWriteWord(ADDR_saveHighlightScene, 1);

        swosWriteWord(ADDR_loadCrowdChantSampleFlag, 1);

        if (swosReadSignedWord(ADDR_g_autoReplays) == 0)
            goto l_no_auto_replay;

        swosWriteWord(ADDR_userRequestedReplay, 0);
        swosWriteWord(ADDR_instantReplayFlag, 1);
        swosWriteWord(ADDR_loadCrowdChantSampleFlag, 1);
    }

l_no_auto_replay:
    swosWriteDword(ADDR_currentScorer, 0);

    {
        int16_t playingPenalties = swosReadSignedWord(ADDR_playingPenalties);
        if (playingPenalties != 0)
        {
            if (gameState != 31) // ST_PENALTIES
                return;
        }
    }

    if (gameState != K_ST_KEEPER_HOLDS_THE_BALL)
    {
        int16_t foulX = swosReadSignedWord(ADDR_foulXCoordinate);
        int16_t foulY = swosReadSignedWord(ADDR_foulYCoordinate);
        swosSetBallPosition(foulX, foulY);
    }

    swosWriteWord(ADDR_breakCameraMode, 2);
}

// gameLoop.cpp:1346-1470 -- breakCameraMode == 2 branch (cseg_73B28).
static void mode2(int16_t gameState)
{
    if (swosReadSignedWord(ADDR_whichCard) != 0) return;
    if (swosReadSignedWord(ADDR_cameraCoordinatesValid) == 0) return;

    swosWriteWord(ADDR_goalScored, 0);

    if (gameState == K_ST_KEEPER_HOLDS_THE_BALL)
    {
        swosWriteWord(ADDR_gameStatePl, K_ST_WAITING_ON_PLAYER);
        swosWriteWord(ADDR_inGameCounter, 0);
        swosWriteWord(ADDR_breakState, (uint16_t)gameState);
    }

    // cseg_73B85.
    if (swosReadSignedWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_RESET_CONTROLS) == 0)
    {
        int32_t table = swosReadSignedDword(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PLAYERS);
        if (table != 0)
        {
            for (int i = 0; i <= 10; i++)
            {
                int32_t spriteAddr = swosReadSignedDword(table + i * 4);
                if (spriteAddr == 0) continue;
                swosWriteWord(spriteAddr + PLSPR_OFF_DEST_REACHED_STATE, 1);
            }
        }
    }

    // cseg_73BCE.
    if (swosReadSignedWord(TEAMDATA_BOTTOM_BASE + TEAMDATA_OFF_RESET_CONTROLS) == 0)
    {
        int32_t table = swosReadSignedDword(TEAMDATA_BOTTOM_BASE + TEAMDATA_OFF_PLAYERS);
        if (table != 0)
        {
            for (int i = 0; i <= 10; i++)
            {
                int32_t spriteAddr = swosReadSignedDword(table + i * 4);
                if (spriteAddr == 0) continue;
                swosWriteWord(spriteAddr + PLSPR_OFF_DEST_REACHED_STATE, 1);
            }
        }
    }

    // cseg_73C17.
    if (gameState == K_ST_KEEPER_HOLDS_THE_BALL)
    {
        swosWriteWord(swosPlayerSpriteBase(PLSPR_SLOT_GOALIE1) + PLSPR_OFF_DEST_REACHED_STATE, 3);
        swosWriteWord(swosPlayerSpriteBase(PLSPR_SLOT_GOALIE2) + PLSPR_OFF_DEST_REACHED_STATE, 3);
    }

    // cseg_73C33.
    swosWriteWord(ADDR_breakCameraMode, 4);
}

// gameLoop.cpp:1472-1492 -- breakCameraMode == 4 branch (cseg_73C3D).
static void mode4(int16_t gameState)
{
    (void)gameState;
    if (swosReadSignedWord(ADDR_g_inSubstitutesMenu) != 0) return;
    swosWriteWord(ADDR_breakCameraMode, 3);
}

// gameLoop.cpp:1494-1640 -- breakCameraMode == 3 branch (cseg_73C60).
static void mode3(int16_t gameState)
{
    if (swosReadSignedWord(ADDR_refState) != 0) return;
    if (swosReadSignedWord(ADDR_injuriesForever) != 0) return;

    for (int slot = 0; slot < PLSPR_TOTAL_SLOTS; slot++)
    {
        int spriteAddr = swosPlayerSpriteBase(slot);

        bool checkThisSprite = gameState == 0 || gameState == 14 || gameState == 31;
        if (!checkThisSprite)
        {
            if (swosReadSignedWord(spriteAddr + PLSPR_OFF_ON_SCREEN) == 0)
                continue;
        }

        if (swosReadSignedWord(spriteAddr + PLSPR_OFF_DEST_REACHED_STATE) != 3)
            return;
    }

    swosWriteWord(ADDR_runSlower, 0);

    // gameLoop.cpp:1599-1636 -- ResetAnimatedPatternsForBothTeams is
    // COMMENTED OUT in the swos-port source; no observable side-effect.

    // cseg_73DCB.
    swosWriteWord(ADDR_breakCameraMode, 5);
}

// gameLoop.cpp:1642-1657 -- breakCameraMode == 5 branch (cseg_73DD5).
static void mode5(int16_t gameState)
{
    (void)gameState;
    swosWriteWord(ADDR_whichCard, 0);
    swosWriteDword(ADDR_bookedPlayer, 0);
    swosWriteWord(ADDR_breakCameraMode, 6);
}

// gameLoop.cpp:1659-1688 -- breakCameraMode == 6 branch (cseg_73DFC).
static void mode6(int16_t gameState)
{
    swosWriteWord(ADDR_ballOutOfGameTimer, 0);

    if (gameState != K_ST_KEEPER_HOLDS_THE_BALL)
        swosWriteWord(ADDR_timeVar, 31000);

    swosWriteWord(ADDR_breakCameraMode, 7);
}

// gameLoop.cpp:1690-1809 -- breakCameraMode == 7 branch (cseg_73E2C).
static void mode7(int16_t gameState)
{
    int32_t a6 = swosReadSignedDword(ADDR_lastTeamPlayedBeforeBreak);
    if (a6 == 0) return;
    swosWriteWord(a6 + TEAMDATA_OFF_BALL_IN_PLAY, 1);

    int16_t boogTimer = swosReadSignedWord(ADDR_ballOutOfGameTimer);
    boogTimer = (int16_t)(boogTimer + 1);
    swosWriteWord(ADDR_ballOutOfGameTimer, (uint16_t)boogTimer);

    int16_t allowInterval = swosReadSignedWord(ADDR_m_allowPlayerControlCameraInterval);
    if ((uint16_t)boogTimer >= (uint16_t)allowInterval)
    {
        // gameLoop.cpp:1724-1751 -- four write-only debug vars elided (no
        // Memory.Addr slots, zero readers -- see swos_addr.h coverage).
        swosBenchCheckIfGoalkeeperClaimedTheBall();
        return;
    }

    // cseg_73E8B.
    int32_t controlled = swosReadSignedDword(a6 + TEAMDATA_OFF_CONTROLLED_PLAYER);
    if (controlled == 0)
        return;

    uint8_t playerState = swosReadByte(controlled + PLSPR_OFF_PLAYER_STATE);
    if (playerState == 5) // PL_THROW_IN
    {
        swosSetPlayerAnimationTable(controlled, ADDR_aboutToThrowInAnimTable);
    }
    else
    {
        swosSetPlayerAnimationTable(controlled, ADDR_playerNormalStandingAnimTable);
    }

    // cseg_73ED6.
    swosWriteWord(a6 + TEAMDATA_OFF_BALL_OUT_OF_PLAY, 1);

    swosWriteWord(ADDR_breakCameraMode, 8);

    if (gameState == K_ST_KEEPER_HOLDS_THE_BALL)
        return;

    if (swosReadSignedWord(ADDR_g_inSubstitutesMenu) != 0)
        return;

    swosWriteDword(ADDR_resultTimer, 31000);
}

// gameLoop.cpp:1811-1846 -- breakCameraMode == 8 branch (cseg_73F0A).
static void mode8(int16_t gameState)
{
    swosWriteWord(ADDR_writeOnlyVar03, 0);

    // StubPlayRefereeWhistleSample() omitted -- audio, see header.
    (void)gameState;

    // cseg_73F2C.
    swosWriteWord(ADDR_goalCameraMode, 0);
    swosWriteWord(ADDR_gameStatePl, K_ST_WAITING_ON_PLAYER);
    swosWriteWord(ADDR_inGameCounter, 0);
    swosWriteWord(ADDR_breakState, (uint16_t)gameState);
}

// ====================================================================
// DoGoalkeeperSprites -- swos.asm:110867-110991
// ====================================================================
static void doGoalkeeperSprites(void)
{
    bool topRight    = swosReadSignedWord(TEAMDATA_TOP_BASE    + TEAMDATA_OFF_GOALKEEPER_DIVING_RIGHT) != 0;
    bool topLeft     = swosReadSignedWord(TEAMDATA_TOP_BASE    + TEAMDATA_OFF_GOALKEEPER_DIVING_LEFT)  != 0;
    bool bottomRight = swosReadSignedWord(TEAMDATA_BOTTOM_BASE + TEAMDATA_OFF_GOALKEEPER_DIVING_RIGHT) != 0;
    bool bottomLeft  = swosReadSignedWord(TEAMDATA_BOTTOM_BASE + TEAMDATA_OFF_GOALKEEPER_DIVING_LEFT)  != 0;

    int a6TeamBase;
    int16_t d0Offset;
    bool diveRight;

    if (topRight)         { d0Offset = 1;  a6TeamBase = TEAMDATA_TOP_BASE;    diveRight = true;  }
    else if (topLeft)      { d0Offset = 1;  a6TeamBase = TEAMDATA_TOP_BASE;    diveRight = false; }
    else if (bottomRight)  { d0Offset = -1; a6TeamBase = TEAMDATA_BOTTOM_BASE; diveRight = true;  }
    else if (bottomLeft)   { d0Offset = -1; a6TeamBase = TEAMDATA_BOTTOM_BASE; diveRight = false; }
    else return;

    // USER-VISIBLE BUG FIX (task #186) -- see C# comment: the diving
    // keeper's sprite is selected by the diving team's teamNumber, not by
    // which physical end carries the dive flag.
    int16_t divingTeamNumber = swosReadSignedWord(a6TeamBase + TEAMDATA_OFF_TEAM_NUMBER);
    int a5KeeperBase = (divingTeamNumber == 1)
        ? swosPlayerSpriteBase(PLSPR_SLOT_GOALIE1)
        : swosPlayerSpriteBase(PLSPR_SLOT_GOALIE2);

    int16_t zPixels;

    if (diveRight)
    {
        int16_t imageIndex = swosReadSignedWord(a5KeeperBase + PLSPR_OFF_IMAGE_INDEX);

        int16_t d1Base;
        if (a5KeeperBase == swosPlayerSpriteBase(PLSPR_SLOT_GOALIE1))
        {
            d1Base = 971;
            if (imageIndex >= 1029 && imageIndex < 1057) d1Base = 1029;
        }
        else
        {
            d1Base = 1087;
            if (imageIndex >= 1145 && imageIndex < 1173) d1Base = 1145;
        }

        int diff = imageIndex - d1Base;
        int tableIndex = diff * 2;
        if (tableIndex < 0) tableIndex = 0;
        if (tableIndex <= 27 * 2)
        {
            zPixels = swosReadSignedWord(ADDR_dseg_17DEF4 + tableIndex);
        }
        else
        {
            int overIndex = tableIndex - 28 * 2;
            if (overIndex > 6 * 2) overIndex = 6 * 2;
            zPixels = swosReadSignedWord(ADDR_kGoalKeeperClaimingBallHeight + overIndex);
        }
    }
    else
    {
        int16_t frameSwitchCounter = swosReadSignedWord(a5KeeperBase + PLSPR_OFF_FRAME_SWITCH_COUNTER);
        int tableIndex = frameSwitchCounter * 2;
        if (tableIndex < 0)     tableIndex = 0;
        if (tableIndex > 6 * 2) tableIndex = 6 * 2;
        zPixels = swosReadSignedWord(ADDR_kGoalKeeperClaimingBallHeight + tableIndex);
    }

    // Common tail -- pin ball to keeper.
    int16_t kx = swosReadSignedWord(a5KeeperBase + PLSPR_OFF_X + 2);
    int16_t ky = swosReadSignedWord(a5KeeperBase + PLSPR_OFF_Y + 2);
    swosWriteWord(BALLSPR_BASE + BALLSPR_OFF_X + 2, (uint16_t)kx);
    swosWriteWord(BALLSPR_BASE + BALLSPR_OFF_Y + 2, (uint16_t)(ky + d0Offset));
    swosWriteWord(BALLSPR_BASE + BALLSPR_OFF_Z + 2, 0);
    swosWriteDword(BALLSPR_BASE + BALLSPR_OFF_DELTA_Z, 0);
    swosWriteWord(BALLSPR_BASE + BALLSPR_OFF_SPEED, 0);
    swosWriteWord(BALLSPR_BASE + BALLSPR_OFF_Z + 2, (uint16_t)zPixels);
}

// ====================================================================
// markPlayer -- gameLoop.cpp:1919-2010
// ====================================================================
#define K_PLAYER_MARK_SPRITE 1204
static void markPlayer(void)
{
    uint16_t tick = swosReadWord(ADDR_currentGameTick);
    int a1TeamBase = ((tick & 0x10) == 0) ? TEAMDATA_TOP_BASE : TEAMDATA_BOTTOM_BASE;

    swosWriteWord(ADDR_playerMarkSprite_ImageIndex, (uint16_t)-1);

    int32_t inGameTeamPtr = swosReadSignedDword(a1TeamBase + TEAMDATA_OFF_IN_GAME_TEAM_PTR);
    if (inGameTeamPtr == 0) return;
    const int kTeamGameOffMarkedPlayer = 20;
    int16_t markedPlayer = swosReadSignedWord(inGameTeamPtr + kTeamGameOffMarkedPlayer);
    if (markedPlayer < 0) return;

    if (markedPlayer >= 11) return;

    int32_t spritesTableAddr = swosReadSignedDword(a1TeamBase + TEAMDATA_OFF_PLAYERS);
    if (spritesTableAddr == 0) return;
    int32_t targetSpriteAddr = swosReadSignedDword(spritesTableAddr + markedPlayer * 4);
    if (targetSpriteAddr == 0) return;

    uint8_t playerState = swosReadByte(targetSpriteAddr + PLSPR_OFF_PLAYER_STATE);
    if (playerState != 0) return;

    swosWriteWord(ADDR_playerMarkSprite_ImageIndex, K_PLAYER_MARK_SPRITE);
    int16_t tx = swosReadSignedWord(targetSpriteAddr + PLSPR_OFF_X + 2);
    int16_t ty = swosReadSignedWord(targetSpriteAddr + PLSPR_OFF_Y + 2);
    int16_t tz = swosReadSignedWord(targetSpriteAddr + PLSPR_OFF_Z + 2);
    swosWriteWord(ADDR_playerMarkSprite_XWhole, (uint16_t)tx);
    swosWriteWord(ADDR_playerMarkSprite_YWhole, (uint16_t)ty);
    swosWriteWord(ADDR_playerMarkSprite_ZWhole, (uint16_t)(tz + 20));
}

// ====================================================================
// DrawAnimatedPatterns -- pitch.cpp:351-357. Only the real side-effect
// (showFansCounter--) is ported -- the rest is pure render.
// ====================================================================
static void drawAnimatedPatterns(void)
{
    // replayingNow() -- replay system not ported; defaults to false.
    if (swosReadSignedWord(ADDR_g_trainingGame) != 0) return;
    int16_t fansCounter = swosReadSignedWord(ADDR_showFansCounter);
    if (fansCounter != 0)
        swosWriteWord(ADDR_showFansCounter, (uint16_t)(fansCounter - 1));
}

// ====================================================================
// Half-end / ET-end / shower state transitions
// ====================================================================

// gameLoop.cpp:2012-2072 -- setCameraMovingToShowerState.
void swosGameLoopSetCameraMovingToShowerState(void)
{
    swosWriteWord(ADDR_halfNumber, 2);

    int16_t tpu = swosReadSignedWord(ADDR_teamPlayingUp);
    swosWriteWord(ADDR_teamPlayingUp, (uint16_t)(3 - tpu));
    int16_t ts = swosReadSignedWord(ADDR_teamStarting);
    swosWriteWord(ADDR_teamStarting, (uint16_t)(3 - ts));

    swosSetBallPosition(1672, 449);
    swosWriteWord(ADDR_hideBall, 0);

    // InitTeamsData: (a) scalar resets, (b) per-team pointer reseat.
    swosKickoffReseatTeamsForNewHalf();
    swosWriteDword(ADDR_currentScorer, 0);
    swosWriteDword(ADDR_lastPlayerBeforeGoalkeeper, 0);
    swosWriteWord(ADDR_goalScored, 0);
    swosWriteWord(ADDR_runSlower, 0);
    swosWriteWord(ADDR_penalty, 0);

    swosWriteWord(ADDR_stoppageEventTimer, 110);
    swosWriteWord(ADDR_gameState, K_ST_CAMERA_GOING_TO_SHOWERS);
    swosWriteWord(ADDR_breakCameraMode, (uint16_t)-1);
    swosWriteWord(ADDR_gameStatePl, K_ST_STOPPED);
    swosWriteWord(ADDR_gameNotInProgressCounterWriteOnly, 0);
    swosWriteWord(ADDR_cameraDirection, (uint16_t)-1);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)TEAMDATA_TOP_BASE);
    swosWriteWord(ADDR_stoppageTimerTotal, 0);
    swosWriteWord(ADDR_stoppageTimerActive, 0);
    swosTeamPortStopAllPlayers();
    swosWriteWord(ADDR_cameraXVelocity, 0);
    swosWriteWord(ADDR_cameraYVelocity, 0);

    // gameLoop.cpp:2061-2068 -- gated audio setup (nullSampleTimer, not a
    // Memory.Addr slot); the gate read itself is preserved for parity.
    (void)swosReadSignedWord(ADDR_playGame);

    // jmp initPlayersBeforeEnteringPitch() -- not yet ported (match-boot
    // layer); the state transition above is the gameplay-visible part.
}

// gameLoop.cpp:2074-2090 -- firstHalfJustEnded.
void swosGameLoopFirstHalfJustEnded(void)
{
    // OpenSWOS enhancement: half-time energy recovery.
    swosPlayerEnergyRecoverAtHalfTime();

    swosWriteWord(ADDR_hideBall, 0);
    swosWriteWord(ADDR_stoppageEventTimer, 275);
    swosWriteWord(ADDR_gameState, K_ST_GOING_TO_HALFTIME);
    swosWriteWord(ADDR_breakCameraMode, (uint16_t)-1);
    swosWriteWord(ADDR_gameStatePl, K_ST_STOPPED);
    swosWriteWord(ADDR_gameNotInProgressCounterWriteOnly, 0);
    swosWriteWord(ADDR_cameraDirection, (uint16_t)-1);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)TEAMDATA_TOP_BASE);
    swosWriteWord(ADDR_stoppageTimerTotal, 0);
    swosWriteWord(ADDR_stoppageTimerActive, 0);
    swosTeamPortStopAllPlayers();
    swosWriteWord(ADDR_cameraXVelocity, 0);
    swosWriteWord(ADDR_cameraYVelocity, 0);
    swosWriteWord(ADDR_stateGoal, 0);
}

// gameLoop.cpp:2092-2111 -- goToHalftime.
void swosGameLoopGoToHalftime(void)
{
    swosSetBallPosition(1672, 449);
    swosWriteDword(ADDR_resultTimer, 30000);
    swosWriteWord(ADDR_timeVar, 32000);
    swosWriteWord(ADDR_stoppageEventTimer, 770);
    swosWriteWord(ADDR_gameState, K_ST_RESULT_ON_HALFTIME);
    swosWriteWord(ADDR_breakCameraMode, (uint16_t)-1);
    swosWriteWord(ADDR_gameStatePl, K_ST_STOPPED);
    swosWriteWord(ADDR_gameNotInProgressCounterWriteOnly, 0);
    swosWriteWord(ADDR_cameraDirection, (uint16_t)-1);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)TEAMDATA_TOP_BASE);
    swosWriteWord(ADDR_stoppageTimerTotal, 0);
    swosWriteWord(ADDR_stoppageTimerActive, 0);
    swosTeamPortStopAllPlayers();
    swosWriteWord(ADDR_cameraXVelocity, 0);
    swosWriteWord(ADDR_cameraYVelocity, 0);
}

// gameLoop.cpp:361-395 -- gameOver.
#define K_GAME_END_CAMERA_X 176
#define K_GAME_END_CAMERA_Y 80
#define K_BALL_OFF_COURT_X 1672
#define K_PITCH_CENTER_Y 449
void swosGameLoopGameOver(void)
{
    // gameFadeOut()/drawPitchAtCurrentCamera() -- render-only, skipped.

    swosCameraSetX(K_GAME_END_CAMERA_X << 16);
    swosCameraSetY(K_GAME_END_CAMERA_Y << 16);

    swosSetBallPosition(K_BALL_OFF_COURT_X, K_PITCH_CENTER_Y);

    swosWriteDword(ADDR_resultTimer, 30000);
    swosWriteWord(ADDR_stoppageEventTimer, 1650);
    swosWriteWord(ADDR_gameState, K_ST_RESULT_AFTER_THE_GAME);
    swosWriteWord(ADDR_breakCameraMode, (uint16_t)-1);
    swosWriteWord(ADDR_gameStatePl, K_ST_STOPPED);
    swosWriteWord(ADDR_cameraDirection, (uint16_t)-1);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)TEAMDATA_TOP_BASE);
    swosWriteWord(ADDR_stoppageTimerTotal, 0);
    swosWriteWord(ADDR_stoppageTimerActive, 0);

    swosTeamPortStopAllPlayers();

    swosWriteWord(ADDR_cameraXVelocity, 0);
    swosWriteWord(ADDR_cameraYVelocity, 0);

    // MatchAudio.PlayEndGameCrowd() omitted -- audio, see header.
    // m_doFadeIn = true -- outer-loop fade tracking, host-owned, see header.
}

bool swosGameLoopIsMatchRunning(void) { return swosReadSignedWord(ADDR_m_playingMatch) != 0; }
void swosGameLoopSetMatchRunning(bool running) { swosWriteWord(ADDR_m_playingMatch, running ? 1 : 0); }

void swosGameLoopSetPenaltiesInterval(int interval) { swosWriteWord(ADDR_m_penaltiesInterval, (uint16_t)interval); }
void swosGameLoopSetInitalKickInterval(int interval) { swosWriteWord(ADDR_m_initalKickInterval, (uint16_t)interval); }
void swosGameLoopSetGoalCameraInterval(int interval) { swosWriteWord(ADDR_m_goalCameraInterval, (uint16_t)interval); }
void swosGameLoopSetAllowPlayerControlCameraInterval(int interval) { swosWriteWord(ADDR_m_allowPlayerControlCameraInterval, (uint16_t)interval); }
