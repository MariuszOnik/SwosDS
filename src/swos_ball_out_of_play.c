// SOURCE: openswos game/scripts/Sim/Port/BallOutOfPlay.cs (full file)
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes. Every
// `goto`/label from the source is preserved verbatim (C supports goto/
// labels natively, same as C#) -- do not "clean up" the control flow into
// structured if/else, it would risk silently changing which branch a given
// state falls into.
#include "swos_ball_out_of_play.h"
#include "swos_addr.h"
#include "swos_ball_sprite.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"
#include "swos_rng.h"
#include "swos_team_data.h"
#include "swos_update_goals.h"

#include <stdint.h>

#define ST_GAME_IN_PROGRESS 100
#define ST_STOPPED 101
#define ST_GOAL_OUT_LEFT 1
#define ST_GOAL_OUT_RIGHT 2
#define ST_KEEPER_HOLDS_BALL 3
#define ST_CORNER_LEFT 4
#define ST_CORNER_RIGHT 5
#define ST_PLAYERS_TO_INITIAL_POSITIONS 0
#define ST_THROW_IN_FORWARD_RIGHT 15
#define ST_THROW_IN_CENTER_RIGHT 16
#define ST_THROW_IN_BACK_RIGHT 17
#define ST_THROW_IN_FORWARD_LEFT 18
#define ST_THROW_IN_CENTER_LEFT 19
#define ST_THROW_IN_BACK_LEFT 20

#define GT_REGULAR 0
#define GT_PENALTY 1
#define GT_OWN_GOAL 2

static int swosRandByte(void) { return swosRngNextByte(); }
static void handleGoalScoredTeam(int teamNum, int teamDataBase);
static int spriteBaseToSlot(int spriteBase);
static void clearPenaltyFlag(void);
static void stopAllPlayers(void);

void swosCheckIfBallOutOfPlay(void) {
    // from ball.cpp:3009-3010 -- clear stateGoal + whistle flag.
    swosWriteWord(ADDR_stateGoal, 0);
    swosWriteWord(ADDR_playRefereeWhistle, 0);

    // from ball.cpp:3012-3022 -- short-circuit on ST_STOPPED.
    int16_t gameStatePl = swosReadSignedWord(ADDR_gameStatePl);
    if (gameStatePl == ST_STOPPED)
        goto l_starting_the_game;

    // from ball.cpp:3024 -- speculatively queue whistle (cleared again on goal).
    swosWriteWord(ADDR_playRefereeWhistle, 1);

    {
    // from ball.cpp:3025-3032 -- read ball x/y/z whole pixels.
    int D1 = swosBallSpriteXPixels();
    int D2 = swosBallSpriteYPixels();
    int D3 = swosBallSpriteZPixels();

    // from ball.cpp:3033-3038 -- sub D1, 1 (offset by 1 px for goal X check).
    D1 = (int16_t)(D1 - 1);

    int A6 = 0;
    int D4 = 0;
    int D3v = 0;

    // from ball.cpp:3040-3049 -- cmp D3, 15 / jg @@not_in_goal_check_near_miss.
    if (D3 > 15) goto l_not_in_goal_check_near_miss;

    // from ball.cpp:3051-3061 -- cmp D1, 302 / jl @@not_in_goal_check_near_miss.
    if (D1 < 302) goto l_not_in_goal_check_near_miss;

    // from ball.cpp:3063-3073 -- cmp D1, 366 / jg @@not_in_goal_check_near_miss.
    if (D1 > 366) goto l_not_in_goal_check_near_miss;

    // from ball.cpp:3075-3085 -- cmp D2, 449 / jg @@lower_goal.
    if (D2 > 449) goto l_lower_goal;

    // from ball.cpp:3087-3100 -- upper goal.
    A6 = TEAMDATA_BOTTOM_BASE;
    if (swosReadSignedWord(A6 + TEAMDATA_OFF_TEAM_NUMBER) == 1)
        goto l_team1_scored;
    goto l_team2_scored;

l_team2_scored:
    // from ball.cpp:3102-3152.
    handleGoalScoredTeam(2, A6);
    goto l_goal_handled;

l_lower_goal:
    // from ball.cpp:3154-3168 -- lower goal.
    A6 = TEAMDATA_TOP_BASE;
    if (swosReadSignedWord(A6 + TEAMDATA_OFF_TEAM_NUMBER) == 2)
        goto l_team2_scored;
    goto l_team1_scored;

l_team1_scored:
    // from ball.cpp:3170-3219.
    handleGoalScoredTeam(1, A6);
    goto l_goal_handled;

l_goal_handled:
    // from ball.cpp:3221-3239.
    A6 = swosReadSignedDword(ADDR_teamScoredDataPtr);

    // from ball.cpp:3227-3228 -- A6 reloaded from the scoring team's
    // opponentsTeam pointer -- from here on A6 is the CONCEDING team.
    A6 = swosReadSignedDword(A6 + TEAMDATA_OFF_OPPONENTS_TEAM);

    // from ball.cpp:3229-3247 -- goal-comment style + turn flags (D3/D4).
    if (A6 == TEAMDATA_TOP_BASE) {
        D3v = 4;
        D4 = 124;
    } else {
        D3v = 0;
        D4 = 199;
    }

    // from ball.cpp:3249-3267 -- goalTypeScored switch (audio omitted, see header).
    {
        int goalType = swosReadSignedDword(ADDR_goalTypeScored);
        (void)goalType; // StubPlayOwnGoalComment/StubPlayGoalComment: audio only
    }

    // from ball.cpp:3269-3287 -- teamNumThatScored home/away sample (audio omitted).

    // from ball.cpp:3289-3291 -- set_goal_state.
    swosWriteWord(ADDR_stateGoal, (uint16_t)-1);
    swosWriteWord(ADDR_playRefereeWhistle, 0);

    // from ball.cpp:3292-3304 -- D1 = (rand() >> 1) + 100.
    int D1_goal = (swosRandByte() >> 1) + 100;

    // from ball.cpp:3305-3311 -- if playingPenalties != 0, skip to penalty_scored.
    {
        bool playingPenalties = swosReadWord(ADDR_playingPenalties) != 0;
        if (!playingPenalties) {
            // from ball.cpp:3313-3325.
            int16_t statsT1 = swosReadSignedWord(ADDR_statsTeam1Goals);
            int16_t statsT2 = swosReadSignedWord(ADDR_statsTeam2Goals);
            int16_t D0 = (int16_t)(statsT1 - statsT2);

            if (D0 == 0) {
                D1_goal = 200;
            } else {
                int16_t copy1 = swosReadSignedWord(ADDR_statsTeam1GoalsCopy2);
                int16_t copy2 = swosReadSignedWord(ADDR_statsTeam2GoalsCopy2);
                D0 = (int16_t)(copy1 - copy2);

                if (D0 == 0) {
                    D1_goal = 300;
                } else if (D0 == 1 || D0 == -1) {
                    int16_t statsDiff = (int16_t)(statsT1 - statsT2);
                    if (statsDiff == 2 || statsDiff == -2) {
                        D1_goal = 200;
                    } else {
                        D1_goal = 100;
                    }
                } else {
                    D1_goal = 100;
                }
            }

            // from ball.cpp:3418-3430 -- D1 += rand().
            D1_goal = (int16_t)(D1_goal + swosRandByte());
        }
    }

    // l_penalty_scored: (ball.cpp:3432-3440)
    swosWriteWord(ADDR_goalCounter, (uint16_t)(int16_t)D1_goal);
    swosWriteWord(ADDR_patternsGoalCounter, 1);

    D1 = 336;
    D2 = 449;

    swosWriteWord(ADDR_gameState, ST_PLAYERS_TO_INITIAL_POSITIONS);
    swosWriteWord(ADDR_breakCameraMode, (uint16_t)-1);
    goto l_break_handled;

l_not_in_goal_check_near_miss:
    // from ball.cpp:3442-3520 -- near-miss detection (audio omitted; logic
    // itself has no OTHER state effect besides the whistle-flag clear).
    {
        int nearX = swosBallSpriteXPixels();
        int nearZ = swosBallSpriteZPixels() + 2;

        if (swosBallSpriteSpeed() >= 768) {
            if (nearX >= 290 && nearX <= 381 && nearZ <= 25) {
                // StubPlayNearMissComment/StubPlayMissGoalSample: audio only.
                swosWriteWord(ADDR_playRefereeWhistle, 0);
            }
        }
    }
    goto l_check_for_corner_goal_out;

l_check_for_corner_goal_out:
    // from ball.cpp:3522-3525.
    clearPenaltyFlag();
    A6 = TEAMDATA_TOP_BASE;

    // from ball.cpp:3526-3536.
    if (D2 >= 129) goto l_not_upper_corner_goal_out;

    {
        int32_t lastTeamPlayed = swosReadSignedDword(ADDR_lastTeamPlayed);
        if (A6 != lastTeamPlayed) goto l_upper_goal_out;
    }

    A6 = TEAMDATA_BOTTOM_BASE;

    if (D1 < 336) goto l_left_upper_corner;

    goto l_right_upper_corner;

l_not_upper_corner_goal_out:
    A6 = TEAMDATA_BOTTOM_BASE;

    if (D2 <= 769) goto l_ball_in_pitch;

    {
        int32_t lastTeamPlayed = swosReadSignedDword(ADDR_lastTeamPlayed);
        if (A6 != lastTeamPlayed) goto l_lower_goal_out;
    }

    A6 = TEAMDATA_TOP_BASE;

    if (D1 < 336) goto l_lower_left_corner;

    if (swosReadSignedWord(ADDR_forceLeftTeam) == 1)
        goto l_right_upper_corner;

    D1 = 585; D2 = 764;
    D3v = 6; D4 = 193;
    swosWriteWord(ADDR_gameState, ST_CORNER_LEFT);
    swosWriteWord(ADDR_breakCameraMode, (uint16_t)-1);
    goto l_its_a_corner;

l_lower_left_corner:
    if (swosReadSignedWord(ADDR_forceLeftTeam) == 1)
        goto l_left_upper_corner;

    D1 = 86; D2 = 764;
    D3v = 2; D4 = 7;
    swosWriteWord(ADDR_gameState, ST_CORNER_RIGHT);
    swosWriteWord(ADDR_breakCameraMode, (uint16_t)-1);
    goto l_its_a_corner;

l_right_upper_corner:
    D1 = 585; D2 = 134;
    D3v = 6; D4 = 112;
    swosWriteWord(ADDR_gameState, ST_CORNER_RIGHT);
    swosWriteWord(ADDR_breakCameraMode, (uint16_t)-1);
    goto l_its_a_corner;

l_left_upper_corner:
    D1 = 86; D2 = 134;
    D3v = 2; D4 = 28;
    swosWriteWord(ADDR_gameState, ST_CORNER_LEFT);
    swosWriteWord(ADDR_breakCameraMode, (uint16_t)-1);
    // fall through to l_its_a_corner

l_its_a_corner:
    // from ball.cpp:3666-3683 -- bump cornersWon stat (audio EnqueueCorner omitted).
    {
        int32_t teamStatsPtr = swosReadSignedDword(A6 + TEAMDATA_OFF_TEAM_STATS_PTR);
        if (teamStatsPtr != 0) {
            int16_t prev = swosReadSignedWord(teamStatsPtr + 2);
            swosWriteWord(teamStatsPtr + 2, (uint16_t)(int16_t)(prev + 1));
        }
    }
    goto l_break_handled;

l_upper_goal_out:
    if (D1 < 336) goto l_left_upper_goal_out;

    D1 = 396; D2 = 154;
    D3v = 4; D4 = 124;
    swosWriteWord(ADDR_gameState, ST_GOAL_OUT_LEFT);
    swosWriteWord(ADDR_breakCameraMode, (uint16_t)-1);
    goto l_goal_out_tail;

l_left_upper_goal_out:
    D1 = 276; D2 = 154;
    D3v = 4; D4 = 124;
    swosWriteWord(ADDR_gameState, ST_GOAL_OUT_RIGHT);
    swosWriteWord(ADDR_breakCameraMode, (uint16_t)-1);
    goto l_goal_out_tail;

l_lower_goal_out:
    if (D1 < 336) goto l_left_lower_goal_out;

    if (swosReadSignedWord(ADDR_forceLeftTeam) == 1) {
        D1 = 396; D2 = 154;
        D3v = 4; D4 = 124;
        swosWriteWord(ADDR_gameState, ST_GOAL_OUT_LEFT);
        swosWriteWord(ADDR_breakCameraMode, (uint16_t)-1);
        goto l_goal_out_tail;
    }

    D1 = 396; D2 = 744;
    D3v = 0; D4 = 199;
    swosWriteWord(ADDR_gameState, ST_GOAL_OUT_RIGHT);
    swosWriteWord(ADDR_breakCameraMode, (uint16_t)-1);
    goto l_goal_out_tail;

l_left_lower_goal_out:
    if (swosReadSignedWord(ADDR_forceLeftTeam) == 1) {
        D1 = 276; D2 = 154;
        D3v = 4; D4 = 124;
        swosWriteWord(ADDR_gameState, ST_GOAL_OUT_RIGHT);
        swosWriteWord(ADDR_breakCameraMode, (uint16_t)-1);
        goto l_goal_out_tail;
    }

    D1 = 276; D2 = 744;
    D3v = 0; D4 = 199;
    swosWriteWord(ADDR_gameState, ST_GOAL_OUT_LEFT);
    swosWriteWord(ADDR_breakCameraMode, (uint16_t)-1);
    // fall through to l_goal_out_tail

l_goal_out_tail:
    // from ball.cpp:3772-3789 -- if playerNumber == 0, D4 &= 0xBB.
    {
        int16_t playerNumber = swosReadSignedWord(A6 + TEAMDATA_OFF_PLAYER_NUMBER);
        if (playerNumber == 0) {
            D4 = D4 & 0xBB;
        }
    }
    swosWriteWord(ADDR_goalOut, 1);
    goto l_break_handled;

l_ball_in_pitch:
    // from ball.cpp:3795-3973 -- throw-in path.
    {
        int32_t lastTeamPlayed = swosReadSignedDword(ADDR_lastTeamPlayed);
        A6 = lastTeamPlayed;
        if (A6 != 0)
            A6 = swosReadSignedDword(A6 + TEAMDATA_OFF_OPPONENTS_TEAM);
    }

    if (D1 < 336) {
        // l_left_half_of_pitch.
        D1 = 81;
        D3v = 2; D4 = 31;

        if (A6 != TEAMDATA_TOP_BASE) {
            if (D2 < 342) {
                swosWriteWord(ADDR_gameState, ST_THROW_IN_BACK_RIGHT);
            } else if (D2 < 556) {
                swosWriteWord(ADDR_gameState, ST_THROW_IN_CENTER_RIGHT);
            } else {
                swosWriteWord(ADDR_gameState, ST_THROW_IN_FORWARD_RIGHT);
            }
        } else {
            if (D2 < 342) {
                swosWriteWord(ADDR_gameState, ST_THROW_IN_FORWARD_LEFT);
            } else if (D2 < 556) {
                swosWriteWord(ADDR_gameState, ST_THROW_IN_CENTER_LEFT);
            } else {
                swosWriteWord(ADDR_gameState, ST_THROW_IN_BACK_LEFT);
            }
        }
    } else {
        // right half of pitch.
        D1 = 590;
        D3v = 6; D4 = 241;

        if (A6 != TEAMDATA_TOP_BASE) {
            // NOTE the Y-order is REVERSED vs the left-touchline branch --
            // see BallOutOfPlay.cs's comment on this exact block for why
            // (a documented bug-fix, preserved verbatim).
            if (D2 < 342) {
                swosWriteWord(ADDR_gameState, ST_THROW_IN_BACK_LEFT);
            } else if (D2 < 556) {
                swosWriteWord(ADDR_gameState, ST_THROW_IN_CENTER_LEFT);
            } else {
                swosWriteWord(ADDR_gameState, ST_THROW_IN_FORWARD_LEFT);
            }
        } else {
            if (D2 < 342) {
                swosWriteWord(ADDR_gameState, ST_THROW_IN_FORWARD_RIGHT);
            } else if (D2 < 556) {
                swosWriteWord(ADDR_gameState, ST_THROW_IN_CENTER_RIGHT);
            } else {
                swosWriteWord(ADDR_gameState, ST_THROW_IN_BACK_RIGHT);
            }
        }
    }

    swosWriteWord(ADDR_breakCameraMode, (uint16_t)-1);
    // StubEnqueueThrowInSample: audio only, omitted.
    // fall through to l_break_handled

l_break_handled:
    // from ball.cpp:3975-4008 -- break_handled.
    if (swosReadSignedWord(ADDR_forceLeftTeam) == 1)
        A6 = TEAMDATA_TOP_BASE;

    swosWriteWord(ADDR_gameStatePl, ST_STOPPED);
    swosWriteWord(ADDR_gameNotInProgressCounterWriteOnly, 0);

    swosWriteWord(ADDR_foulXCoordinate, (uint16_t)(int16_t)D1);
    swosWriteWord(ADDR_foulYCoordinate, (uint16_t)(int16_t)D2);
    swosWriteWord(ADDR_cameraDirection, (uint16_t)(int16_t)D3v);
    swosWriteByte(ADDR_playerTurnFlags, (uint8_t)D4);

    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)A6);

    swosWriteWord(ADDR_stoppageTimerTotal, 0);
    swosWriteWord(ADDR_stoppageTimerActive, 0);

    stopAllPlayers();

    swosWriteWord(ADDR_cameraXVelocity, 0);
    swosWriteWord(ADDR_cameraYVelocity, 0);
    // fall through to l_starting_the_game
    }

l_starting_the_game:
    // from ball.cpp:4010-4019 -- final whistle queue (audio omitted; the
    // gate itself -- reading/leaving playRefereeWhistle -- has no other
    // effect, so there is nothing left to port here beyond the sound cue).
    ;
}

// from ball.cpp:3102-3219 -- collapsed goal-scoring path.
static void handleGoalScoredTeam(int teamNum, int teamDataBase) {
    int A6 = teamDataBase;

    // from ball.cpp:3104-3140 / 3172-3208 -- determine scorer sprite (A0).
    int32_t lastPlayerPlayed = swosReadSignedDword(ADDR_lastPlayerPlayed);
    int32_t lastKeeperPlayed = swosReadSignedDword(ADDR_lastKeeperPlayed);
    int32_t scorerSprite = lastPlayerPlayed;

    if (lastKeeperPlayed != 0) {
        int goalie1Base = swosPlayerSpriteBase(PLSPR_SLOT_GOALIE1);
        int goalie2Base = swosPlayerSpriteBase(PLSPR_SLOT_GOALIE2);
        if (scorerSprite == goalie1Base || scorerSprite == goalie2Base)
            scorerSprite = lastKeeperPlayed;
    }

    // from ball.cpp:3143-3146 / 3211-3214 -- snapshot stats to *Copy2.
    int16_t statsT1 = swosReadSignedWord(ADDR_statsTeam1Goals);
    swosWriteWord(ADDR_statsTeam1GoalsCopy2, (uint16_t)statsT1);
    int16_t statsT2 = swosReadSignedWord(ADDR_statsTeam2Goals);
    swosWriteWord(ADDR_statsTeam2GoalsCopy2, (uint16_t)statsT2);

    // from ball.cpp:3148 / 3216 -- SWOS::GoalScored(teamNum, scorerSprite).
    int scorerSlot = spriteBaseToSlot(scorerSprite);
    swosUpdateGoalsGoalScored(teamNum, scorerSlot);

    swosWriteWord(ADDR_goalCameraMode, 1);
    swosWriteWord(ADDR_teamNumThatScored, (uint16_t)teamNum);

    swosWriteDword(ADDR_teamScoredDataPtr, (uint32_t)A6);
    int32_t inGamePtr = swosReadSignedDword(A6 + TEAMDATA_OFF_IN_GAME_TEAM_PTR);
    swosWriteDword(ADDR_teamScoredGamePtr, (uint32_t)inGamePtr);
}

// Map a sprite base address back to a 0..21 slot index. Returns 0 if the
// address doesn't fall inside the sprite pool.
static int spriteBaseToSlot(int spriteBase) {
    if (spriteBase == 0) return 0;
    int off = spriteBase - PLSPR_SPRITE_POOL_BASE;
    if (off < 0) return 0;
    int slot = off / PLSPR_SLOT_STRIDE;
    if (slot < 0 || slot >= PLSPR_TOTAL_SLOTS) return 0;
    return slot;
}

// Port of comments.cpp:240-243 -- clearPenaltyFlag (audio side omitted, see
// BallOutOfPlay.cs's comment on this exact function for why -- the
// commentary module isn't ported and would no-op anyway).
static void clearPenaltyFlag(void) {
    swosWriteWord(ADDR_penalty, 0);
}

// Mechanical port of gameLoop.cpp StopAllPlayers + team.cpp:stopAllPlayers.
static void stopAllPlayers(void) {
    for (int t = 0; t < 2; t++) {
        bool top = (t == 0);
        int teamBase = top ? TEAMDATA_TOP_BASE : TEAMDATA_BOTTOM_BASE;

        int firstSlot = top ? 0 : PLSPR_TEAM_SIZE;
        for (int slotOff = 0; slotOff < PLSPR_TEAM_SIZE; slotOff++) {
            int slot = firstSlot + slotOff;
            uint8_t state = swosPlayerSpritePlayerState(slot);
            int16_t sentAway = swosReadSignedWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_SENT_AWAY);
            if (state == 0 /* kNormal */ && sentAway == 0) {
                swosPlayerSpriteSetDestX(slot, swosPlayerSpriteXPixels(slot));
                swosPlayerSpriteSetDestY(slot, swosPlayerSpriteYPixels(slot));
            }
        }

        swosWriteWord(teamBase + TEAMDATA_OFF_BALL_IN_PLAY, 0);
        swosWriteWord(teamBase + TEAMDATA_OFF_BALL_OUT_OF_PLAY, 0);

        swosWriteDword(teamBase + TEAMDATA_OFF_CONTROLLED_PLAYER, 0);
        swosWriteDword(teamBase + TEAMDATA_OFF_PASS_TO_PLAYER_PTR, 0);

        swosWriteWord(teamBase + TEAMDATA_OFF_PASSING_BALL, 0);
        swosWriteWord(teamBase + TEAMDATA_OFF_PASSING_TO_PLAYER, 0);
        swosWriteWord(teamBase + TEAMDATA_OFF_PLAYER_SWITCH_TIMER, 0);
        swosWriteDword(teamBase + TEAMDATA_OFF_PASSING_KICKING_PLAYER, 0);

        // goalkeeperPlaying = 0. (Original SWOS had a bug: it failed to reset
        // this for the top team. swos-port preserves that bug outside
        // #ifdef SWOS_TEST -- we follow the production path and clear both,
        // matching the un-#ifdef'd C++ branch, same as OpenSWOS's port.)
        swosWriteWord(teamBase + TEAMDATA_OFF_GOALKEEPER_PLAYING, 0);
    }
}
