// SOURCE: openswos game/scripts/SwosVm/Memory.cs:1477-2271 (`Memory.Init`)
// FIDELITY: VERIFIED_PC for the pcMode branch (this is what OpenSWOS's boot
// path always runs -- AmigaModeActive() is hard-locked to false, per the
// GameTime.cs note quoted in ../audyt-openswos-sim.md Part 0 "Krok 0"). The
// pcMode=false (Amiga) branch is ported too, since Memory.Init itself
// branches on the pcMode parameter and both sides are equally mechanical --
// but nothing in this repo calls swosMemoryInit(false) yet, so that path is
// UNEXERCISED beyond the golden-dump test added alongside this file.
//
// Direct mechanical port, no logic changes, exact same call order as the
// source (including the PlayerSprite.Init()/AnimationTablesData.Init()/
// TeamData.Init() calls partway through -- see the comments at their call
// sites in Memory.cs for why that specific order matters).
#include "swos_memory.h"
#include "swos_addr.h"
#include "swos_anim_tables.h"
#include "swos_player_sprite.h"
#include "swos_rng.h"
#include "swos_team_data.h"

#include <stdint.h>

#define LEN(a) ((int)(sizeof(a) / sizeof((a)[0])))

void swosMemoryInit(bool pcMode) {
    swosMemoryClear();

    // Ball kick speeds -- same in both modes (swos.asm data section).
    swosWriteWord(ADDR_kBallKickingSpeed, 2208);
    swosWriteWord(ADDR_kHighKickBallSpeed, 2688);
    swosWriteWord(ADDR_kNormalKickBallSpeed, 2560);
    swosWriteWord(ADDR_kPlayerTacklingSpeed, 1792);
    swosWriteWord(ADDR_kJumpHeaderSpeed, 2048);

    // Tackling downtime tables (16 bytes each = 8 entries of WORD, indexed
    // by player tackling skill 0..7).
    for (int i = 0; i < 8; i++) {
        swosWriteWord(ADDR_kPlayerTacklingDownTime + i * 2, (uint16_t)(30 - 3 * i));
        swosWriteWord(ADDR_kComputerTacklingDownTime + i * 2, 3);
    }

    // Goalkeeper speeds -- same in both modes.
    swosWriteWord(ADDR_kGoalkeeperCatchSpeed, 768);
    swosWriteWord(ADDR_kGoalkeeperMoveToBallSpeed, 1024);
    swosWriteWord(ADDR_kGoalkeeperSpeedWhenGameStopped, 1024);
    swosWriteWord(ADDR_kSubstitutedPlayerSpeed, 1536);
    swosWriteWord(ADDR_kRefereeSpeed, 1024);

    // Vertical deltas Q16.16 -- same in both modes.
    swosWriteDword(ADDR_kBallKickingDeltaZ, 0x14000);
    swosWriteDword(ADDR_kBallJumpHeaderDeltaZ, 0xA000);

    // amigaMode.cpp:35-37 (Amiga) / 53-55 (PC).
    if (pcMode) {
        swosWriteWord(ADDR_kBallGroundConstant, 13);
        swosWriteWord(ADDR_kBallAirConstant, 4);
        swosWriteDword(ADDR_kGravityConstant, 3291);
        swosWriteWord(ADDR_kKeeperSaveDistance, 16);
    } else {
        swosWriteWord(ADDR_kBallGroundConstant, 16);
        swosWriteWord(ADDR_kBallAirConstant, 10);
        swosWriteDword(ADDR_kGravityConstant, 4608);
        swosWriteWord(ADDR_kKeeperSaveDistance, 24);
    }

    // Pitch-dependent values -- Normal pitch (type 4) defaults.
    swosWriteWord(ADDR_ballSpeedBounceFactor, 64);
    swosWriteWord(ADDR_ballBounceFactor, 96);
    swosWriteWord(ADDR_pitchBallSpeedFactor, 0);

    // Pre-match menu selections.
    swosWriteWord(ADDR_gamePitchType, (uint16_t)-1);
    swosWriteWord(ADDR_gamePitchTypeOrSeason, 0);
    swosWriteWord(ADDR_gameSeason, 0);
    swosWriteWord(ADDR_plg_D0_param, 0);

    // Dive deltas (PC mode by default, amigaMode.cpp:12-13).
    {
        static const uint32_t divePc[8] = { 0x28000, 0x30000, 0x38000, 0x40000, 0x48000, 0x50000, 0x58000, 0x60000 };
        static const uint32_t diveAmiga[8] = { 0x30000, 0x38000, 0x40000, 0x48000, 0x50000, 0x58000, 0x60000, 0x68000 };
        const uint32_t *dive = pcMode ? divePc : diveAmiga;
        for (int i = 0; i < 8; i++)
            swosWriteDword(ADDR_kGoalkeeperDiveDeltasBase + i * 4, dive[i]);
    }

    // kBallPlOffsets (swos.asm:245818).
    {
        static const int16_t plOffs[16] = { 0, -1, 1, -1, 1, 0, 1, 1, 0, 1, -1, 1, -1, 0, -1, -1 };
        for (int i = 0; i < LEN(plOffs); i++)
            swosWriteWord(ADDR_kBallPlOffsetsBase + i * 2, (uint16_t)plOffs[i]);
    }

    // kPlayerWithBallOffsets (swos.asm:245869).
    {
        static const int16_t plWithBallOffs[16] = { 0, 1, -1, 1, -1, 0, -1, -1, 0, -1, 1, -1, 1, 0, 1, 1 };
        for (int i = 0; i < LEN(plWithBallOffs); i++)
            swosWriteWord(ADDR_kPlayerWithBallOffsets + i * 2, (uint16_t)plWithBallOffs[i]);
    }

    // Spin / kick-after-touch constants (swos.asm:203954-203989).
    swosWriteDword(ADDR_kHighKickDeltaZ, 0x20000);
    swosWriteDword(ADDR_kNormalKickDeltaZ, 0x16000);

    // kSpinMultiplierFactor[10].
    {
        static const int16_t spinMult[10] = { 5, 4, 3, 2, 2, 2, 2, 1, 1, 1 };
        for (int i = 0; i < LEN(spinMult); i++)
            swosWriteWord(ADDR_kSpinMultiplierFactor + i * 2, (uint16_t)spinMult[i]);
    }

    // kKickSpinFactor -- swos.asm:203967-203969. 8 directions x 4 words each.
    {
        static const int16_t kickSpin[32] = {
            -32,   0,  32,   0,  // dir 0 (N)
              0, -23,  23,   0,  // dir 1 (NE)
              0, -32,   0,  32,  // dir 2 (E)
             23,   0,   0,  23,  // dir 3 (SE)
             32,   0, -32,   0,  // dir 4 (S)
              0,  23, -23,   0,  // dir 5 (SW)
              0,  32,   0, -32,  // dir 6 (W)
            -23,   0,   0, -23,  // dir 7 (NW)
        };
        for (int i = 0; i < LEN(kickSpin); i++)
            swosWriteWord(ADDR_kKickSpinFactor + i * 2, (uint16_t)kickSpin[i]);
    }

    // kPassingSpinFactor -- swos.asm:203975-203977. Same layout, smaller values.
    {
        static const int16_t passSpin[32] = {
            -16,   0,  16,   0,  // dir 0
              0, -11,  11,   0,  // dir 1
              0, -16,   0,  16,  // dir 2
             11,   0,   0,  11,  // dir 3
             16,   0, -16,   0,  // dir 4
              0,  11, -11,   0,  // dir 5
              0,  16,   0, -16,  // dir 6
            -11,   0,   0, -11,  // dir 7
        };
        for (int i = 0; i < LEN(passSpin); i++)
            swosWriteWord(ADDR_kPassingSpinFactor + i * 2, (uint16_t)passSpin[i]);
    }

    // AI after-touch delta tables -- swos.asm:246182-246188.
    {
        static const int16_t aiRandomRotate[2] = { -32, 32 };
        static const int16_t aiLeftSpin[3] = { -1, -2, -3 };
        static const int16_t aiRotateRight[3] = { 1, 2, 3 };
        static const int16_t aiLongKick[3] = { 0, -999, 4 };
        for (int i = 0; i < LEN(aiRandomRotate); i++)
            swosWriteWord(ADDR_AI_randomRotateTable + i * 2, (uint16_t)aiRandomRotate[i]);
        for (int i = 0; i < LEN(aiLeftSpin); i++)
            swosWriteWord(ADDR_AI_leftSpinTable + i * 2, (uint16_t)aiLeftSpin[i]);
        for (int i = 0; i < LEN(aiRotateRight); i++)
            swosWriteWord(ADDR_AI_rotateRightTable + i * 2, (uint16_t)aiRotateRight[i]);
        for (int i = 0; i < LEN(aiLongKick); i++)
            swosWriteWord(ADDR_AI_longKickTable + i * 2, (uint16_t)aiLongKick[i]);
    }

    // Global game state -- sane defaults.
    swosWriteWord(ADDR_gameStatePl, 100);  // ST_GAME_IN_PROGRESS
    swosWriteWord(ADDR_gameState, 0);
    swosWriteWord(ADDR_hideBall, 0);
    swosWriteWord(ADDR_ballShadowImageIndex, 1183);
    swosWriteWord(ADDR_ballNextX, 0);
    swosWriteWord(ADDR_ballNextY, 0);

    // Ball frame-index tables (swos.asm:219721, 219724).
    {
        static const int16_t movingFrames[5] = { 1182, 1181, 1180, 1179, -1 };
        static const int16_t staticFrames[3] = { 1182, 1182, -1 };
        for (int i = 0; i < LEN(movingFrames); i++)
            swosWriteWord(ADDR_ballMovingFrameIndices_Table + i * 2, (uint16_t)movingFrames[i]);
        for (int i = 0; i < LEN(staticFrames); i++)
            swosWriteWord(ADDR_ballStaticFrameIndices_Table + i * 2, (uint16_t)staticFrames[i]);
        swosWriteDword(ADDR_ballMovingFrameIndices_Addr, (uint32_t)ADDR_ballMovingFrameIndices_Table);
        swosWriteDword(ADDR_ballStaticFrameIndices_Addr, (uint32_t)ADDR_ballStaticFrameIndices_Table);
    }

    // Substitution + last-team defaults.
    swosWriteWord(ADDR_g_substituteInProgress, 0);
    swosWriteDword(ADDR_teamThatSubstitutes, 0);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, 0);

    // ballNextGroundX/Y default to -1 (matches updatePlayers.cpp:916).
    swosWriteWord(ADDR_ballNextGroundX, (uint16_t)-1);
    swosWriteWord(ADDR_ballNextGroundY, (uint16_t)-1);

    // Match score + scorer state -- start zeroed (documents the contract).
    swosWriteWord(ADDR_team1TotalGoals, 0);
    swosWriteWord(ADDR_team2TotalGoals, 0);
    swosWriteWord(ADDR_team1PenaltyGoals, 0);
    swosWriteWord(ADDR_team2PenaltyGoals, 0);
    swosWriteWord(ADDR_team1GoalsDigit1, 0);
    swosWriteWord(ADDR_team1GoalsDigit2, 0);
    swosWriteWord(ADDR_statsTeam1Goals, 0);
    swosWriteWord(ADDR_team2GoalsDigit1, 0);
    swosWriteWord(ADDR_team2GoalsDigit2, 0);
    swosWriteWord(ADDR_statsTeam2Goals, 0);
    swosWriteWord(ADDR_goalScored, 0);
    swosWriteWord(ADDR_runSlower, 0);
    swosWriteWord(ADDR_penalty, 0);
    swosWriteWord(ADDR_playingPenalties, 0);

    // Referee -- start off-screen with no card pending.
    swosWriteWord(ADDR_refState, 0);     // kRefOffScreen
    swosWriteWord(ADDR_whichCard, 0);    // kNoCard
    swosWriteWord(ADDR_refTimer, 0);
    swosWriteWord(ADDR_lastFrameTicks, 1);

    // Game-length -- default to medium (1 = 18 sec/min-of-game).
    swosWriteWord(ADDR_gameLengthInGame, 1);

    // Game-time module state -- pristine.
    swosWriteDword(ADDR_gt_gameTimeInMinutes, 0);
    swosWriteDword(ADDR_gt_endGameCounter, 0);
    swosWriteDword(ADDR_gt_timeDelta, 18);  // matches kGameLenSecondsTable[1]
    swosWriteDword(ADDR_gt_gameSeconds, 0);
    swosWriteDword(ADDR_gt_secondsSwitchAccumulator, 0);
    swosWriteWord(ADDR_gt_showTime, 0);

    // Extra-time / penalty defaults -- disabled.
    swosWriteWord(ADDR_extraTimeState, 0);
    swosWriteWord(ADDR_penaltiesState, 0);
    swosWriteWord(ADDR_secondLeg, 0);
    swosWriteWord(ADDR_playing2ndGame, 0);
    swosWriteWord(ADDR_stateGoal, 0);

    // Match-half / kickoff direction.
    swosWriteWord(ADDR_halfNumber, 1);
    swosWriteWord(ADDR_teamPlayingUp, 1);
    swosWriteWord(ADDR_teamStarting, 1);

    // Penalty-shootout bookkeeping defaults -- zeroed.
    swosWriteWord(ADDR_savedTeam1Goals, 0);
    swosWriteWord(ADDR_savedTeam2Goals, 0);
    swosWriteWord(ADDR_team1PenaltyShooterIndex, 0);
    swosWriteWord(ADDR_team2PenaltyShooterIndex, 0);
    swosWriteWord(ADDR_team1PenaltyAttempts, 0);
    swosWriteWord(ADDR_team2PenaltyAttempts, 0);

    // ball.cpp:checkIfBallOutOfPlay defaults -- pristine values.
    swosWriteWord(ADDR_playRefereeWhistle, 0);
    swosWriteDword(ADDR_lastPlayerPlayed, 0);
    swosWriteDword(ADDR_lastKeeperPlayed, 0);
    swosWriteWord(ADDR_statsTeam1GoalsCopy2, 0);
    swosWriteWord(ADDR_statsTeam2GoalsCopy2, 0);
    swosWriteWord(ADDR_goalCameraMode, 0);
    swosWriteWord(ADDR_teamNumThatScored, 0);
    swosWriteDword(ADDR_teamScoredDataPtr, 0);
    swosWriteDword(ADDR_teamScoredGamePtr, 0);
    swosWriteWord(ADDR_goalCounter, 0);
    swosWriteWord(ADDR_patternsGoalCounter, 0);
    swosWriteWord(ADDR_breakCameraMode, 0);
    swosWriteWord(ADDR_goalOut, 0);
    swosWriteWord(ADDR_forceLeftTeam, 0);
    swosWriteWord(ADDR_cameraXVelocity, 0);
    swosWriteWord(ADDR_cameraYVelocity, 0);
    swosWriteWord(ADDR_stoppageTimerTotal, 0);
    swosWriteWord(ADDR_stoppageTimerActive, 0);
    swosWriteWord(ADDR_cameraDirection, 0);
    swosWriteByte(ADDR_playerTurnFlags, 0);
    swosWriteWord(ADDR_gameNotInProgressCounterWriteOnly, 0);
    swosWriteWord(ADDR_lastPlayerTurnFlags, 0);

    // spinningLogo.cpp:12-15 -- defaults.
    swosWriteWord(ADDR_sl_enabled, 0);
    swosWriteWord(ADDR_sl_frameIndex, 0);
    swosWriteWord(ADDR_sl_pictureIndex, 0);

    // Corner flag + curPlayerNum sprite arrays -- zeroed.
    for (int i = 0; i < 24; i++) swosWriteByte(ADDR_cornerFlags + i, 0);
    for (int i = 0; i < 16; i++) swosWriteByte(ADDR_curPlayerNumSprites + i, 0);

    // playerNameDisplay.cpp:11-12 -- pnd_* defaults (hidden state).
    swosWriteWord(ADDR_pnd_topTeam, 0);
    swosWriteWord(ADDR_pnd_playerOrdinal, (uint16_t)-1);
    swosWriteWord(ADDR_pnd_visible, 0);
    swosWriteWord(ADDR_pnd_nobodysBallLastFrame, 0);

    // Section 4 of updateBall -- quadrant tables + indexed outputs.
    {
        static const int16_t xQuadLimits[5] = { 81, 183, 285, 387, 489 };
        static const int16_t yQuadLimits[7] = { 129, 220, 312, 403, 495, 586, 678 };
        for (int i = 0; i < LEN(xQuadLimits); i++)
            swosWriteWord(ADDR_ballXQuadrantLimits + i * 2, (uint16_t)xQuadLimits[i]);
        for (int i = 0; i < LEN(yQuadLimits); i++)
            swosWriteWord(ADDR_ballYQuadrantLimits + i * 2, (uint16_t)yQuadLimits[i]);
    }
    swosWriteWord(ADDR_ballQuadrantIndex, 0);
    swosWriteWord(ADDR_ballXQuadrantDead, 0);
    swosWriteWord(ADDR_ballYQuadrantDead, 0);
    swosWriteWord(ADDR_playerXQuadrantOffset, 0);
    swosWriteWord(ADDR_playerYQuadrantOffset, 0);
    swosWriteWord(ADDR_currentGameTick, 0);

    // updatePlayers.cpp -- entry-point bookkeeping defaults.
    swosWriteDword(ADDR_prevLastPlayer, 0);
    swosWriteDword(ADDR_prevLastTeamPlayed, 0);
    swosWriteWord(ADDR_ballInUpperPenaltyArea, 0);
    swosWriteWord(ADDR_ballInLowerPenaltyArea, 0);
    swosWriteWord(ADDR_ballInGoalkeeperArea, 0);
    swosWriteDword(ADDR_lastPlayerBeforeGoalkeeper, 0);
    swosWriteWord(ADDR_nobodysBallTimer, 0);

    // gameLoop.cpp orchestrator state -- pristine.
    swosWriteWord(ADDR_playGame, 1);  // matches gameLoop.cpp:86 (top of gameLoop()).
    swosWriteDword(ADDR_frameCounter, 0);
    swosWriteDword(ADDR_teamSwitchCounter, 0);
    swosWriteWord(ADDR_fireBlocked, 0);
    swosWriteWord(ADDR_spaceReplayTimer, 0);
    swosWriteWord(ADDR_penaltiesTimer, 0);
    swosWriteWord(ADDR_inGameCounter, 0);
    swosWriteWord(ADDR_stoppageEventTimer, 0);
    swosWriteWord(ADDR_gameNotInProgressCounter, 0);
    swosWriteWord(ADDR_lastGameTick, 0);
    swosWriteWord(ADDR_loadCrowdChantSampleFlag, 0);
    swosWriteWord(ADDR_m_initalKickInterval, 825);
    swosWriteWord(ADDR_initialKickWriteOnlyTicks, 0);
    swosWriteWord(ADDR_m_penaltiesInterval, 110);
    swosWriteWord(ADDR_m_playingMatch, 0);

    // gameLoop.cpp break-camera FSM private state.
    swosWriteWord(ADDR_breakState, 0);
    swosWriteWord(ADDR_m_goalCameraInterval, 55);
    swosWriteWord(ADDR_m_allowPlayerControlCameraInterval, 550);
    swosWriteWord(ADDR_cameraCoordinatesValid, 1);
    swosWriteWord(ADDR_ballOutOfGameTimer, 0);
    swosWriteWord(ADDR_writeOnlyVar03, 0);
    swosWriteWord(ADDR_g_autoSaveHighlights, 0);
    swosWriteWord(ADDR_g_autoReplays, 0);
    swosWriteWord(ADDR_saveHighlightScene, 0);
    swosWriteWord(ADDR_instantReplayFlag, 0);
    swosWriteWord(ADDR_userRequestedReplay, 0);

    // Goalkeeper jump / deflect speeds -- swos.asm:202381-202392, 245841.
    swosWriteWord(ADDR_kGoalkeeperNearJumpSpeed, 1024);
    swosWriteWord(ADDR_kGoalkeeperFarJumpSpeed, 2048);
    swosWriteWord(ADDR_kGoalkeeperFarJumpSlowerSpeed, 1280);
    swosWriteWord(ADDR_kGoalkeeperStrongDeflectBallSpeed, 1536);
    swosWriteWord(ADDR_kGoalkeeperMediumDeflectBallSpeed, 1024);
    swosWriteWord(ADDR_kGoalkeeperWeakDeflectBallSpeed, 512);
    swosWriteDword(ADDR_kGoalkeeperDeflectDeltaZ, 49152);

    // Penalty save distances -- swos.asm:203733/203736.
    swosWriteWord(ADDR_kKeeperPenaltySaveDistanceFar, 20);
    swosWriteWord(ADDR_kKeeperPenaltySaveDistanceNear, 12);

    // updatePlayers.cpp:2124 -- kShotAtGoalMinumumSpeed. swos.asm:202413 `dw 512`.
    swosWriteWord(ADDR_kShotAtGoalMinumumSpeed, 512);

    // updatePlayers.cpp:2334 -- goalScoredChances table (15 bytes).
    for (int i = 0; i < 15; i++)
        swosWriteByte(ADDR_goalScoredChances + i, (uint8_t)(i + 1));

    // updatePlayers.cpp:2547 -- dseg_1105EF. swos.asm:202375 `dw 512`.
    swosWriteWord(ADDR_dseg_1105EF, 512);

    // updatePlayers.cpp:4190 -- dseg_110BDB. swos.asm:203728.
    {
        static const int16_t diveParryShifts[8] = { 1, 4, 3, 3, 3, 2, 2, 2 };
        for (int i = 0; i < LEN(diveParryShifts); i++)
            swosWriteWord(ADDR_dseg_110BDB + i * 2, (uint16_t)diveParryShifts[i]);
    }

    // updatePlayers.cpp:3877 -- dseg_17EECC. swos.asm:246164-246167 (60 bytes).
    {
        // The run of 7s must be SIXTEEN pairs (32 bytes) -- see Memory.cs's
        // comment on this exact array for the historical off-by-one bug this
        // guards against. Verified vs original-amiga-swos.asm:35574-35604
        // (sField_18) + swos.asm:246164.
        static const uint8_t penaltyDiveRow[60] = {
            1, 0, 0, 4, 176, 0, 0, 4, 0, 1,
            7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
            7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,   // 16x 7
            2, 0, 0, 0, 14, 0, 13, 0, 3, 0, 10, 0, 5, 0, 1, 0, 8, 0,
        };
        for (int i = 0; i < LEN(penaltyDiveRow); i++)
            swosWriteByte(ADDR_dseg_17EECC + i, penaltyDiveRow[i]);
    }

    // ---- player.cpp skill-indexed tables (swos.asm:245768-245813) ---------
    {
        static const uint8_t kPlAvgTacklingBallControlDiffChance_values[8] = { 16, 17, 18, 19, 20, 21, 22, 23 };
        for (int i = 0; i < LEN(kPlAvgTacklingBallControlDiffChance_values); i++)
            swosWriteByte(ADDR_kPlAvgTacklingBallControlDiffChance + i, kPlAvgTacklingBallControlDiffChance_values[i]);
    }
    {
        static const int16_t kBallSpeedDeltaWhenControlled_values[8] = { 130, 116, 102, 88, 74, 60, 46, 32 };
        for (int i = 0; i < LEN(kBallSpeedDeltaWhenControlled_values); i++)
            swosWriteWord(ADDR_kBallSpeedDeltaWhenControlled + i * 2, (uint16_t)kBallSpeedDeltaWhenControlled_values[i]);
    }
    {
        static const int16_t dseg_17E276_values[8] = { 4, 5, 6, 8, 11, 14, 17, 21 };
        for (int i = 0; i < LEN(dseg_17E276_values); i++)
            swosWriteWord(ADDR_dseg_17E276 + i * 2, (uint16_t)dseg_17E276_values[i]);
    }
    {
        static const int16_t kBallSpeedFinishing_values[8] = { -288, -160, -32, 96, 224, 352, 480, 608 };
        for (int i = 0; i < LEN(kBallSpeedFinishing_values); i++)
            swosWriteWord(ADDR_kBallSpeedFinishing + i * 2, (uint16_t)kBallSpeedFinishing_values[i]);
    }
    {
        static const int16_t kBallSpeedKicking_values[8] = { -384, -270, -162, -54, 54, 162, 270, 384 };
        for (int i = 0; i < LEN(kBallSpeedKicking_values); i++)
            swosWriteWord(ADDR_kBallSpeedKicking + i * 2, (uint16_t)kBallSpeedKicking_values[i]);
    }
    swosWriteWord(ADDR_kStaticHeaderBallSpeed, 1792);
    {
        static const int16_t kPlayerHeaderSpeedIncrease_values[8] = { -336, -288, -240, -192, -144, -96, -48, 0 };
        for (int i = 0; i < LEN(kPlayerHeaderSpeedIncrease_values); i++)
            swosWriteWord(ADDR_kPlayerHeaderSpeedIncrease + i * 2, (uint16_t)kPlayerHeaderSpeedIncrease_values[i]);
    }
    swosWriteDword(ADDR_kHeaderLowJumpHeight, 0x20000);
    swosWriteDword(ADDR_kHeaderHighJumpHeight, 0x24000);
    {
        static const int16_t kAIFailedPassChance_values[8] = { 6, 4, 3, 2, 1, 0, 0, 0 };
        for (int i = 0; i < LEN(kAIFailedPassChance_values); i++)
            swosWriteWord(ADDR_kAIFailedPassChance + i * 2, (uint16_t)kAIFailedPassChance_values[i]);
    }

    // kPassingSpeed* -- swos.asm:245779-245794.
    swosWriteWord(ADDR_kPassingSpeedCloserThan2500, 1536);
    swosWriteWord(ADDR_kPassingSpeed_2500_10000, 1664);
    swosWriteWord(ADDR_kPassingSpeed_10000_22500, 1792);
    swosWriteWord(ADDR_kPassingSpeed_22500_40000, 1877);
    swosWriteWord(ADDR_kPassingSpeed_40000_62500, 1962);
    swosWriteWord(ADDR_kPassingSpeed_62500_90000, 2048);
    swosWriteWord(ADDR_kPassingSpeed_90000_122500, 2133);
    swosWriteWord(ADDR_kPassingSpeedFurtherThan122500, 2218);

    {
        static const int16_t kBallSpeedPassingIncrease_values[8] = { 0, 48, 96, 144, 192, 256, 320, 384 };
        for (int i = 0; i < LEN(kBallSpeedPassingIncrease_values); i++)
            swosWriteWord(ADDR_kBallSpeedPassingIncrease + i * 2, (uint16_t)kBallSpeedPassingIncrease_values[i]);
    }
    swosWriteWord(ADDR_kFreePassReleasingBallSpeed, 1792);

    // ---- Ball-destination delta tables (swos.asm:245580-245598) -----------
    {
        static const int16_t kLeftThrowInBallDestDelta_values[16] = {
              250, -1000,  1000, -1000,  1000,    0,  1000, 1000,
              250,  1000, -1000,  1000, -1000,    0, -1000, -1000,
        };
        for (int i = 0; i < LEN(kLeftThrowInBallDestDelta_values); i++)
            swosWriteWord(ADDR_kLeftThrowInBallDestDelta + i * 2, (uint16_t)kLeftThrowInBallDestDelta_values[i]);
    }
    {
        static const int16_t kRightThrowInBallDestDelta_values[16] = {
             -250, -1000,  1000, -1000,  1000,    0,  1000, 1000,
             -250,  1000, -1000,  1000, -1000,    0, -1000, -1000,
        };
        for (int i = 0; i < LEN(kRightThrowInBallDestDelta_values); i++)
            swosWriteWord(ADDR_kRightThrowInBallDestDelta + i * 2, (uint16_t)kRightThrowInBallDestDelta_values[i]);
    }
    {
        static const int16_t kPenaltyBallDestDelta_values[16] = {
                0, -1000,   500, -1000,  1000,    0,   500, 1000,
                0,  1000,  -500,  1000, -1000,    0,  -500, -1000,
        };
        for (int i = 0; i < LEN(kPenaltyBallDestDelta_values); i++)
            swosWriteWord(ADDR_kPenaltyBallDestDelta + i * 2, (uint16_t)kPenaltyBallDestDelta_values[i]);
    }
    {
        static const int16_t kUpperLeftCornerBallDestDelta_values[16] = {
                0, -1000,  1000, -1000,  1000,  150,  1000,  300,
              250,  1000, -1000,  1000, -1000,    0, -1000, -1000,
        };
        for (int i = 0; i < LEN(kUpperLeftCornerBallDestDelta_values); i++)
            swosWriteWord(ADDR_kUpperLeftCornerBallDestDelta + i * 2, (uint16_t)kUpperLeftCornerBallDestDelta_values[i]);
    }
    {
        static const int16_t kUpperRightCornerBallDestDelta_values[16] = {
                0, -1000,  1000, -1000,  1000,    0,  1000, 1000,
             -250,  1000, -1000,   350, -1000,  150, -1000, -1000,
        };
        for (int i = 0; i < LEN(kUpperRightCornerBallDestDelta_values); i++)
            swosWriteWord(ADDR_kUpperRightCornerBallDestDelta + i * 2, (uint16_t)kUpperRightCornerBallDestDelta_values[i]);
    }
    {
        static const int16_t kLowerLeftCornerBallDestDelta_values[16] = {
              250, -1000,  1000,  -350,  1000, -150,  1000, 1000,
                0,  1000, -1000,  1000, -1000,    0, -1000, -1000,
        };
        for (int i = 0; i < LEN(kLowerLeftCornerBallDestDelta_values); i++)
            swosWriteWord(ADDR_kLowerLeftCornerBallDestDelta + i * 2, (uint16_t)kLowerLeftCornerBallDestDelta_values[i]);
    }
    {
        // kLowerRightCornerBallDestDelta -- swos.asm:245598. Declared in the
        // asm as `db` byte-pairs; decoded as signed LE-words here (matches
        // Memory.cs's note verbatim).
        static const int16_t kLowerRightCornerBallDestDelta_values[16] = {
             -250, -1000,  1000, -1000,  1000,    0,  1000, 1000,
                0,  1000, -1000,  1000, -1000, -150, -1000, -350,
        };
        for (int i = 0; i < LEN(kLowerRightCornerBallDestDelta_values); i++)
            swosWriteWord(ADDR_kLowerRightCornerBallDestDelta + i * 2, (uint16_t)kLowerRightCornerBallDestDelta_values[i]);
    }
    {
        // kDefaultDestinations -- swos.asm:245575. dir 0=N,1=NE,2=E,3=SE,4=S,5=SW,6=W,7=NW.
        static const int16_t defaultDests[16] = {
                0, -1000,    // dir 0 (N)
             1000, -1000,    // dir 1 (NE)
             1000,     0,    // dir 2 (E)
             1000,  1000,    // dir 3 (SE)
                0,  1000,    // dir 4 (S)
            -1000,  1000,    // dir 5 (SW)
            -1000,     0,    // dir 6 (W)
            -1000, -1000,    // dir 7 (NW)
        };
        for (int i = 0; i < LEN(defaultDests); i++)
            swosWriteWord(ADDR_kDefaultDestinations + i * 2, (uint16_t)defaultDests[i]);
    }

    // updatePlayers.cpp:10658 / 10912 -- pristine defaults.
    swosWriteWord(ADDR_ballDefensiveX, 0);
    swosWriteWord(ADDR_ballNotHighZ, 0);
    swosWriteWord(ADDR_goalkeeperDiveDeadVar, 0);

    // Fouls / cards -- defaults match game.cpp:170 / gameLoop.cpp:416.
    swosWriteWord(ADDR_cardsDisallowed, 0);
    swosWriteWord(ADDR_playerCardChance, 0);
    swosWriteWord(ADDR_plg_D3_param, 0);
    swosWriteWord(ADDR_team1NumAllowedInjuries, 4);
    swosWriteWord(ADDR_team2NumAllowedInjuries, 4);
    swosWriteDword(ADDR_lastTackleNearestTeammate, 0);

    // inGameTeamPlayerOffsets -- index * 61 (sizeof(PlayerGame) = 0x3D = 61).
    for (int i = 0; i < 11; i++)
        swosWriteWord(ADDR_inGameTeamPlayerOffsets + i * 2, (uint16_t)(int16_t)(i * 61));

    // dseg_17E3EE / dseg_17E3F3 -- 5-byte card-progression tables, 0-init
    // (values not published in any header reachable from OpenSWOS -- see
    // Memory.cs's comment on this exact block).
    for (int i = 0; i < 5; i++) swosWriteByte(ADDR_dseg_17E3EE + i, 0);
    for (int i = 0; i < 5; i++) swosWriteByte(ADDR_dseg_17E3F3 + i, 0);

    // playerTackled injury tables -- swos.asm:245826-245837.
    {
        static const uint8_t kInjuryLevels[7] = { 42, 7, 5, 4, 3, 2, 1 };
        static const uint8_t kInjuryLevelAlreadyInjured[7] = { 14, 15, 12, 9, 7, 5, 2 };
        static const int16_t dseg_17E2EC[8] = { 0, 60, 70, 80, 90, 100, 110, 130 };
        static const uint8_t kTackleInjuryProbability[4] = { 48, 28, 20, 14 };
        static const uint8_t kTackleInjuryProbabilityAlreadyInjured[4] = { 96, 57, 41, 28 };
        for (int i = 0; i < LEN(kInjuryLevels); i++)
            swosWriteByte(ADDR_kInjuryLevels + i, kInjuryLevels[i]);
        for (int i = 0; i < LEN(kInjuryLevelAlreadyInjured); i++)
            swosWriteByte(ADDR_kInjuryLevelAlreadyInjured + i, kInjuryLevelAlreadyInjured[i]);
        for (int i = 0; i < LEN(dseg_17E2EC); i++)
            swosWriteWord(ADDR_dseg_17E2EC + i * 2, (uint16_t)dseg_17E2EC[i]);
        for (int i = 0; i < LEN(kTackleInjuryProbability); i++)
            swosWriteByte(ADDR_kTackleInjuryProbability + i, kTackleInjuryProbability[i]);
        for (int i = 0; i < LEN(kTackleInjuryProbabilityAlreadyInjured); i++)
            swosWriteByte(ADDR_kTackleInjuryProbabilityAlreadyInjured + i, kTackleInjuryProbabilityAlreadyInjured[i]);
    }

    // AI helper globals -- all pristine 0.
    swosWriteWord(ADDR_AI_counter, 0);
    swosWriteWord(ADDR_AI_attackHalf, 0);
    swosWriteWord(ADDR_AI_counterWriteOnly, 0);
    swosWriteWord(ADDR_deadVarAlways0, 0);
    swosWriteDword(ADDR_dseg_1309C1, 0);

    // Player sprite pool + per-team SpritesTable initialization. Must run
    // BEFORE swosTeamDataInit (which reads PLSPR_TEAM1/2_TABLE_BASE
    // addresses to set the `players` field).
    swosPlayerSpriteInit();

    // Animation tables (swos.asm:218368-219641) + frame-indices arrays. Must
    // run before SetPlayerAnimationTable can be called. Wires up 17
    // PlayerAnimationTable structs at ADDR_k*AnimTableAddr.
    swosAnimTablesInit();

    // TeamData cross-pointers (opponentsTeam) + per-team players pointers + safe defaults.
    swosTeamDataInit();

    // gameControls.cpp:33-46 -- resetGameControls() defaults (documents the
    // contract; the clear above already zeroed everything).
    swosWriteDword(ADDR_ic_pl1FireCounter, 0);
    swosWriteDword(ADDR_ic_pl2FireCounter, 0);
    swosWriteByte(ADDR_ic_pl1LastFired, 0);
    swosWriteByte(ADDR_ic_pl2LastFired, 0);
    swosWriteDword(ADDR_ic_oldPl1Events, 0);   // kNoGameEvents
    swosWriteDword(ADDR_ic_oldPl2Events, 0);
    swosWriteDword(ADDR_ic_pl1LastVertical, 0);
    swosWriteDword(ADDR_ic_pl1LastHorizontal, 0);
    swosWriteDword(ADDR_ic_pl2LastVertical, 0);
    swosWriteDword(ADDR_ic_pl2LastHorizontal, 0);
    swosWriteDword(ADDR_ic_pl1Events, 0);
    swosWriteDword(ADDR_ic_pl2Events, 0);
    swosWriteWord(ADDR_ic_pl1Fire, 0);
    swosWriteWord(ADDR_ic_pl2Fire, 0);

    // PlayerHeader port -- initialise constants used by
    // playerAttemptingJumpHeader / attemptStaticHeader /
    // setPlayerWithNoBallDestination.
    swosWriteWord(ADDR_kStaticHeaderPlayerSpeed, 256);
    swosWriteWord(ADDR_m_playerDownHeadingInterval, 55);
    swosWriteWord(ADDR_m_playerDownTacklingInterval, 55);
    // updatePlayers.cpp:9-10 -- PC defaults for the post-result intervals.
    // amigaMode.cpp:32-33,50-51 overrides via PortTuning setters (Amiga
    // 600/350, PC 660/385); OpenSWOS hard-locks PC.
    swosWriteWord(ADDR_m_clearResultInterval, 660);
    swosWriteWord(ADDR_m_clearResultHalftimeInterval, 385);

    // playerXQuadrantsCoordinates (swos.asm:245750) -- 15 words.
    {
        static const int16_t xQuadCoords[15] = {
            98, 132, 166, 200, 234, 268, 302, 336,
            370, 404, 438, 472, 506, 540, 574,
        };
        for (int i = 0; i < LEN(xQuadCoords); i++)
            swosWriteWord(ADDR_playerXQuadrantsCoordinates + i * 2, (uint16_t)xQuadCoords[i]);
    }
    // playerYQuadrantCoordinates (swos.asm:245753) -- 16 words.
    {
        static const int16_t yQuadCoords[16] = {
            149, 189, 229, 269, 309, 349, 389, 429,
            469, 509, 549, 589, 629, 669, 709, 749,
        };
        for (int i = 0; i < LEN(yQuadCoords); i++)
            swosWriteWord(ADDR_playerYQuadrantCoordinates + i * 2, (uint16_t)yQuadCoords[i]);
    }

    // g_tacticsTable (swos.asm:209342) -- 19 dword pointers into teamTacticsPool.
    for (int i = 0; i < 19; i++)
        swosWriteDword(ADDR_g_tacticsTable + i * 4, (uint32_t)(ADDR_teamTacticsPool + i * 370));

    // dseg_17DEF4 (swos.asm:245601) -- 28 x word ball-Z height table.
    {
        static const int16_t dseg_17DEF4_values[28] = {
            7, 13, 11, 8, 5, 2, 20, 2, 5, 8, 11, 13, 7, 20,
            5, 11, 9, 6, 3, 0, 20, 0, 3, 6, 9, 11, 5, 20,
        };
        for (int i = 0; i < LEN(dseg_17DEF4_values); i++)
            swosWriteWord(ADDR_dseg_17DEF4 + i * 2, (uint16_t)dseg_17DEF4_values[i]);
    }

    // kGoalKeeperClaimingBallHeight (swos.asm:245604) -- 7 x word.
    {
        static const int16_t kGoalKeeperClaimingBallHeight_values[7] = { 17, 17, 17, 10, 8, 5, 255 };
        for (int i = 0; i < LEN(kGoalKeeperClaimingBallHeight_values); i++)
            swosWriteWord(ADDR_kGoalKeeperClaimingBallHeight + i * 2, (uint16_t)kGoalKeeperClaimingBallHeight_values[i]);
    }

    // Goal-sprite image-index slots start at -1 (no image).
    swosWriteWord(ADDR_goal1TopSprite_ImageIndex, (uint16_t)-1);
    swosWriteWord(ADDR_goal2BottomSprite_ImageIndex, (uint16_t)-1);

    // playerMarkSprite -- initial imageIndex -1 (hidden).
    swosWriteWord(ADDR_playerMarkSprite_ImageIndex, (uint16_t)-1);
    swosWriteWord(ADDR_playerMarkSprite_XWhole, 0);
    swosWriteWord(ADDR_playerMarkSprite_YWhole, 0);
    swosWriteWord(ADDR_playerMarkSprite_ZWhole, 0);

    // Display-sprite dirty flag starts clean.
    swosWriteWord(ADDR_displaySpritesDirtyFlag, 0);

    // Deterministic RNG seed. Tied to currentGameTick so each match restart
    // picks a reproducible byte stream (zero at boot).
    swosRngReseed(swosReadWord(ADDR_currentGameTick));

    swosMemoryMarkInitialised();
}
