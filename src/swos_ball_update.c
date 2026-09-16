// SOURCE: openswos game/scripts/Sim/Port/BallUpdate.cs (full file)
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes. Every
// `goto`/label in Section4 is preserved verbatim (C supports goto/labels
// natively, same as C#) -- do not restructure into if/else, it would risk
// silently changing which branch a given state falls into.
//
// See swos_ball_update.h for the MatchAudio omission rationale and the
// PlayerUpdate.cs/BallOutOfPlay.cs forward-pulled dependency notes.
#include "swos_ball_update.h"
#include "swos_addr.h"
#include "swos_ball_out_of_play.h"
#include "swos_ball_sprite.h"
#include "swos_flags.h"
#include "swos_memory.h"
#include "swos_player_update.h"
#include "swos_rng.h"
#include "swos_sprite_update.h"
#include "swos_team_data.h"
#include "swos_util.h"

#include <stdint.h>

static void section1HideAndFrameIndex(void);
static void section2DirectionAndFriction(void);
static void section3ApplyDeltasAndBounce(void);
static void section4GoalDetectionAndShadow(void);
static void extractFrameLoop(void);
static void applyAfterPass(bool topTeam);
static void resetSpinTimer(bool topTeam);
static int determineKickSpinSide(bool topTeam);
static int determinePassSpinSide(bool topTeam);
static void applySpinOffsetToDestKick(bool topTeam, int spinSide, int spinFactorTableBase);
static void applySpinOffsetToDestPass(bool topTeam, int spinSide, int spinFactorTableBase);
static void applyTick4HighOrNormalKickBoost(bool topTeam);
static void applyHighKick(void);
static void applyNormalKick(void);
static void applySpeedAdjustment(bool topTeam);
static void applyLongPassBoostIfNeeded(bool topTeam);
static void applyLongPassSpeedBoost(bool topTeam, bool longPassFlag);
static void incrementSpinTimerOrReset(bool topTeam);

// D5/D6/D7 snapshot passed between Section3 and Section4 (in asm, saved
// registers; here a module static so the call signature stays clean).
static int32_t s_section4PreX;
static int32_t s_section4PreY;
static int32_t s_section4PreZ;

void swosBallUpdateTick(void) {
    section1HideAndFrameIndex();
    section2DirectionAndFriction();
    section3ApplyDeltasAndBounce();
    section4GoalDetectionAndShadow();
}

void swosBallUpdateTickPhysicsOnly(void) {
    section1HideAndFrameIndex();
    section3ApplyDeltasAndBounce();
}

// ball.cpp:299-735 -- combined: apply deltas, keeper-holds-ball Z path,
// gravity + ground bounce, pitch barrier bounce.
static void section3ApplyDeltasAndBounce(void) {
    // ball.cpp:182-186 -- save pre-motion position.
    int32_t preX = swosBallSpriteX();
    int32_t preY = swosBallSpriteY();
    int32_t preZ = swosBallSpriteZ();

    // ball.cpp:300-324 -- apply deltaX/deltaY to position.
    int32_t dx = swosBallSpriteDeltaX();
    int32_t dy = swosBallSpriteDeltaY();
    int32_t dz = swosBallSpriteDeltaZ();

    swosBallSpriteSetX(preX + dx);
    swosBallSpriteSetY(preY + dy);

    // ball.cpp:325-336 -- gameState check: ST_KEEPER_HOLDS_BALL = 3.
    int16_t gameState = swosReadSignedWord(ADDR_gameState);
    bool keeperHoldsBall = gameState == 3;

    bool gotoSetDeltaZTo0 = false;
    bool gotoApplyDeltaZ = false;

    if (keeperHoldsBall) {
        // ball.cpp:338-372 -- substitution carve-out.
        bool substituteInProgress = swosReadWord(ADDR_g_substituteInProgress) != 0;
        bool skipKeeperZ = false;

        if (substituteInProgress) {
            int32_t teamThatSubs = swosReadSignedDword(ADDR_teamThatSubstitutes);
            int32_t lastTeamPlayed = swosReadSignedDword(ADDR_lastTeamPlayedBeforeBreak);
            if (teamThatSubs == lastTeamPlayed && teamThatSubs != 0) {
                int32_t controlledPlayer = swosTeamDataControlledPlayerFromBase(teamThatSubs);
                if (controlledPlayer == 0) {
                    // ball.cpp:370-372 -- all three conditions met.
                    swosBallSpriteSetZPixels(0);
                    gotoSetDeltaZTo0 = true;
                    skipKeeperZ = true;
                }
            }
        }

        if (!skipKeeperZ) {
            // ball.cpp:374-431 -- keeper_is_controlled.
            if (swosBallSpriteSpeed() != 0) {
                int32_t lastTeamPlayed = swosReadSignedDword(ADDR_lastTeamPlayedBeforeBreak);
                if (lastTeamPlayed != 0) {
                    int32_t controlledPlayer = swosTeamDataControlledPlayerFromBase(lastTeamPlayed);
                    if (controlledPlayer != 0) {
                        swosUpdateBallWithControllingGoalkeeper(controlledPlayer);
                    }
                }
            }

            // ball.cpp:433-456 -- check_keeper_z.
            int16_t zWhole = swosBallSpriteZPixels();
            if (zWhole == 5) {
                gotoSetDeltaZTo0 = true;
            } else if (zWhole > 5) {
                dz = -65538;
                gotoApplyDeltaZ = true;
            } else {
                dz = 131076;
                gotoApplyDeltaZ = true;
            }
        }
    } else {
        // ball.cpp:458-474 -- keeper_doesnt_hold_the_ball. Apply gravity.
        if (dz != 0) {
            int32_t gravity = swosReadSignedDword(ADDR_kGravityConstant);
            dz -= gravity;
            dz |= 1; // ball.cpp:474 -- force odd bit.
            gotoApplyDeltaZ = true;
        }
    }

    if (gotoSetDeltaZTo0) {
        dz = 0;
    } else if (gotoApplyDeltaZ) {
        // ball.cpp:476-547 -- apply_delta_z.
        int32_t newZ = swosBallSpriteZ() + dz;
        swosBallSpriteSetZ(newZ);

        if (newZ < 0) {
            // ball.cpp:494-513 -- XY speed reduction (unsigned 16x16->32 multiply).
            uint32_t uSpeed = (uint16_t)swosBallSpriteSpeed();
            uint32_t uBounceXY = (uint16_t)swosReadWord(ADDR_ballSpeedBounceFactor);
            uint32_t uProd = uSpeed * uBounceXY;
            uint32_t uShrink = uProd >> 8;
            int16_t shrinkLow = (int16_t)(uint16_t)uShrink;
            swosBallSpriteSetSpeed((int16_t)(swosBallSpriteSpeed() - shrinkLow));

            // ball.cpp:514 -- z.whole = 0.
            swosBallSpriteSetZPixels(0);

            // ball.cpp:515-536 -- reflect deltaZ.
            int32_t negDz = -dz;
            int32_t shiftedDz = swosAsr32(negDz, 8);
            uint32_t uShiftedLow = (uint16_t)shiftedDz;
            uint32_t uBounceZ = (uint16_t)swosReadWord(ADDR_ballBounceFactor);
            uint32_t uProdZ = uShiftedLow * uBounceZ;
            dz = negDz - (int32_t)uProdZ;
            dz |= 1;

            // ball.cpp:537-547 -- bounce audio gate. MatchAudio.PlayBounce()
            // omitted (see swos_ball_update.h) -- the ONLY state effect of
            // this branch either way is dz, already computed above.
            if (dz <= 40960) {
                dz = 0;
            }
        }
    }

    // ball.cpp:568-571 -- assign_delta_z.
    swosBallSpriteSetDeltaZ(dz);

    // ball.cpp:572-583 -- barrier checks (only when NOT ST_GAME_IN_PROGRESS).
    int16_t gameStatePl = swosReadSignedWord(ADDR_gameStatePl);
    if (gameStatePl != 100) {
        // ball.cpp:585-609 -- X bounds [53, 618].
        int16_t xWhole = swosReadSignedWord(BALLSPR_BASE + 32);
        if (xWhole < 53 || xWhole > 618) {
            swosReverseDestXDirection();
            swosBallSpriteSetSpeed((int16_t)((uint16_t)swosBallSpriteSpeed() >> 1));
            swosBallSpriteSetX(preX);
            swosBallSpriteSetY(preY);
            swosBallSpriteSetZ(preZ);
        }

        // ball.cpp:646-672 -- Y bounds [100, 799].
        int16_t yWhole = swosReadSignedWord(BALLSPR_BASE + 36);
        if (yWhole < 100 || yWhole > 799) {
            swosReverseDestYDirection();
            swosBallSpriteSetSpeed((int16_t)((uint16_t)swosBallSpriteSpeed() >> 1));
            swosBallSpriteSetX(preX);
            swosBallSpriteSetY(preY);
            swosBallSpriteSetZ(preZ);
        }
    }

    // ball.cpp:709-735 -- stash snapshot for Section4.
    s_section4PreX = preX;
    s_section4PreY = preY;
    s_section4PreZ = preZ;
}

// ball.cpp:737-2247 -- goal detection, post/bar collision, out-of-play
// dispatch, ball-shadow update, ball quadrant calc.
static void section4GoalDetectionAndShadow(void) {
    int32_t D1 = (int16_t)swosBallSpriteXPixels();
    int32_t D2 = (int16_t)swosBallSpriteYPixels();
    int32_t D3 = (int16_t)swosBallSpriteZPixels();
    int32_t D5 = s_section4PreX;
    int32_t D6 = s_section4PreY;
    int32_t D7 = s_section4PreZ;
    int32_t D4 = 0;

    D1 = (int16_t)(D1 - 1);

    swosFlagsSetFromSub16(D2, 128);
    if (!g_swosFlags.zero && g_swosFlags.sign == g_swosFlags.overflow)
        goto l_not_in_upper_goal;

    swosFlagsSetFromSub16(D2, 112);
    if (!g_swosFlags.zero && g_swosFlags.sign == g_swosFlags.overflow)
        goto l_in_upper_goal_y;

l_not_in_upper_goal:
    swosFlagsSetFromSub16(D2, 770);
    if (g_swosFlags.sign != g_swosFlags.overflow)
        goto l_not_in_lower_goal;

    swosFlagsSetFromSub16(D2, 785);
    if (g_swosFlags.sign == g_swosFlags.overflow)
        goto l_not_in_lower_goal;

    swosFlagsSetFromSub16(D3, 19);
    if (!g_swosFlags.zero && g_swosFlags.sign == g_swosFlags.overflow)
        goto l_not_in_lower_goal;

    swosFlagsSetFromSub16(D1, 295);
    if (g_swosFlags.zero || g_swosFlags.sign != g_swosFlags.overflow)
        goto l_not_in_lower_goal;

    swosFlagsSetFromSub16(D1, 372);
    if (!g_swosFlags.zero && g_swosFlags.sign == g_swosFlags.overflow)
        goto l_not_in_lower_goal;

    swosFlagsSetFromSub16(D3, 15);
    if (!g_swosFlags.zero && g_swosFlags.sign == g_swosFlags.overflow)
        goto l_ball_in_top_of_lower_goal;

    swosFlagsSetFromSub16(D1, 302);
    if (g_swosFlags.sign != g_swosFlags.overflow)
        goto l_left_edge_of_lower_goal;

    swosFlagsSetFromSub16(D1, 366);
    if (!g_swosFlags.zero && g_swosFlags.sign == g_swosFlags.overflow)
        goto l_left_edge_of_lower_goal;

    swosFlagsSetFromSub16(D2, 778);
    if (!g_swosFlags.zero && g_swosFlags.sign == g_swosFlags.overflow)
        goto l_ball_in_net;

    goto l_not_in_lower_goal;

l_in_upper_goal_y:
    swosFlagsSetFromSub16(D3, 19);
    if (!g_swosFlags.zero && g_swosFlags.sign == g_swosFlags.overflow)
        goto l_not_in_lower_goal;

    swosFlagsSetFromSub16(D1, 295);
    if (g_swosFlags.zero || g_swosFlags.sign != g_swosFlags.overflow)
        goto l_not_in_lower_goal;

    swosFlagsSetFromSub16(D1, 372);
    if (!g_swosFlags.zero && g_swosFlags.sign == g_swosFlags.overflow)
        goto l_not_in_lower_goal;

    swosFlagsSetFromSub16(D2, 123);
    if (!g_swosFlags.zero && g_swosFlags.sign == g_swosFlags.overflow)
        goto l_ball_just_in_upper_goal;

    swosFlagsSetFromSub16(D3, 10);
    if (!g_swosFlags.zero && g_swosFlags.sign == g_swosFlags.overflow)
        goto l_top_of_upper_goal;

    goto l_ball_in_upper_net;

l_ball_just_in_upper_goal:
    swosFlagsSetFromSub16(D3, 15);
    if (!g_swosFlags.zero && g_swosFlags.sign == g_swosFlags.overflow)
        goto l_top_of_upper_goal;
    // fall through to l_ball_in_upper_net

l_ball_in_upper_net:
    swosFlagsSetFromSub16(D1, 302);
    if (g_swosFlags.sign != g_swosFlags.overflow)
        goto l_left_edge_of_lower_goal;

    swosFlagsSetFromSub16(D1, 366);
    if (!g_swosFlags.zero && g_swosFlags.sign == g_swosFlags.overflow)
        goto l_left_edge_of_lower_goal;

    swosFlagsSetFromSub16(D2, 119);
    if (g_swosFlags.sign != g_swosFlags.overflow)
        goto l_ball_in_net;

    goto l_not_in_lower_goal;

l_top_of_upper_goal:
    {
        int32_t preZpx = (int16_t)(D7 >> 16);
        swosFlagsSetFromSub16(preZpx, 15);
        if (!g_swosFlags.zero && g_swosFlags.sign == g_swosFlags.overflow)
            goto l_top_of_the_goal;
    }
    swosBallSpriteSetDeltaZ(1);
    swosBallSpriteSetSpeed(0);
    swosBallSpriteSetX(D5);
    swosBallSpriteSetY(D6);
    swosBallSpriteSetZ(D7);
    goto l_not_in_lower_goal;

l_top_of_the_goal:
    swosBallSpriteSetDeltaZ(1);
    {
        int32_t yPx = swosBallSpriteYPixels();
        int16_t destY = (int16_t)(yPx - 1000);
        swosBallSpriteSetDestY(destY);
    }
    swosBallSpriteSetSpeed(512);
    swosBallSpriteSetZ(D7);
    goto l_not_in_lower_goal;

l_ball_in_top_of_lower_goal:
    {
        int32_t preZpx = (int16_t)(D7 >> 16);
        swosFlagsSetFromSub16(preZpx, 15);
        if (!g_swosFlags.zero && g_swosFlags.sign == g_swosFlags.overflow)
            goto cseg_7C69B;
    }
    swosBallSpriteSetDeltaZ(1);
    swosBallSpriteSetSpeed(0);
    swosBallSpriteSetX(D5);
    swosBallSpriteSetY(D6);
    swosBallSpriteSetZ(D7);
    goto l_not_in_lower_goal;

cseg_7C69B:
    swosBallSpriteSetDeltaZ(1);
    {
        int32_t yPx = swosBallSpriteYPixels();
        int16_t destY = (int16_t)(yPx + 1000);
        swosBallSpriteSetDestY(destY);
    }
    swosBallSpriteSetSpeed(512);
    swosBallSpriteSetZ(D7);
    goto l_not_in_lower_goal;

l_ball_in_net:
    swosReverseDestYDirection();
    swosResetBothTeamSpinTimers();
    {
        int16_t speed = swosBallSpriteSpeed();
        swosBallSpriteSetSpeed((int16_t)((uint16_t)speed >> 3));
    }
    goto cseg_7C756;

l_left_edge_of_lower_goal:
    swosReverseDestXDirection();
    swosResetBothTeamSpinTimers();
    {
        int16_t speed = swosBallSpriteSpeed();
        swosBallSpriteSetSpeed((int16_t)((uint16_t)speed >> 2));
    }
    // fall through to cseg_7C756

cseg_7C756:
    swosBallSpriteSetX(D5);
    swosBallSpriteSetY(D6);

l_not_in_lower_goal:
    {
        int16_t gsPl = swosReadSignedWord(ADDR_gameStatePl);
        swosFlagsSetFromSub16(gsPl, 100);
        if (!g_swosFlags.zero)
            goto cseg_7CA2C;
    }

    D1 = (int16_t)swosBallSpriteXPixels();
    D2 = (int16_t)swosBallSpriteYPixels();
    D3 = (int16_t)swosBallSpriteZPixels();

    D1 = (int16_t)(D1 - 1);

    swosFlagsSetFromSub16(D2, 128);
    if (g_swosFlags.zero || g_swosFlags.sign != g_swosFlags.overflow)
        goto cseg_7CA2C;

    swosFlagsSetFromSub16(D2, 132);
    if (g_swosFlags.zero || g_swosFlags.sign != g_swosFlags.overflow)
        goto cseg_7C7F0;

    swosFlagsSetFromSub16(D2, 770);
    if (g_swosFlags.sign == g_swosFlags.overflow)
        goto cseg_7CA2C;

    swosFlagsSetFromSub16(D2, 766);
    if (g_swosFlags.sign != g_swosFlags.overflow)
        goto cseg_7CA2C;
    // fall through to cseg_7C7F0

cseg_7C7F0:
    swosFlagsSetFromSub16(D3, 19);
    if (!g_swosFlags.zero && g_swosFlags.sign == g_swosFlags.overflow)
        goto cseg_7CA2C;

    swosFlagsSetFromSub16(D1, 295);
    if (g_swosFlags.zero || g_swosFlags.sign != g_swosFlags.overflow)
        goto cseg_7CA2C;

    swosFlagsSetFromSub16(D1, 372);
    if (!g_swosFlags.zero && g_swosFlags.sign == g_swosFlags.overflow)
        goto cseg_7CA2C;

    swosFlagsSetFromSub16(D3, 15);
    if (!g_swosFlags.zero && g_swosFlags.sign == g_swosFlags.overflow)
        goto l_penalty_goal;

    swosFlagsSetFromSub16(D1, 302);
    if (g_swosFlags.sign != g_swosFlags.overflow)
        goto l_own_goal;

    swosFlagsSetFromSub16(D1, 366);
    if (!g_swosFlags.zero && g_swosFlags.sign == g_swosFlags.overflow)
        goto l_own_goal;

    goto cseg_7CA2C;

l_penalty_goal:
    swosWriteWord(ADDR_goalTypeScored, 1);
    D4 = swosReadSignedWord(ADDR_currentGameTick);
    D4 = (int16_t)(D4 & 31);
    D4 = (int16_t)(D4 << 4);
    D4 = (int16_t)(D4 - 256);
    {
        int32_t dy = swosBallSpriteDeltaY();
        if (dy < -20480) goto cseg_7C911;
        if (dy > 20480) goto cseg_7C911;
    }
    goto l_reverse_delta_z;

l_own_goal:
    swosWriteWord(ADDR_goalTypeScored, 2);
    D4 = swosReadSignedWord(ADDR_currentGameTick);
    D4 = (int16_t)(D4 & 31);
    D4 = (int16_t)(D4 << 4);
    D4 = (int16_t)(D4 - 256);
    {
        int32_t dy = swosBallSpriteDeltaY();
        if (dy < -20480) goto cseg_7C911;
        if (dy > 20480) goto cseg_7C911;
    }
    swosReverseDestYDirection();
    swosResetBothTeamSpinTimers();
    swosBallSpriteSetDestY((int16_t)(swosBallSpriteDestY() + D4));
    goto cseg_7C989;

cseg_7C911:
    swosFlagsSetFromSub16(D2, 449);
    if (!g_swosFlags.zero && g_swosFlags.sign == g_swosFlags.overflow)
        goto cseg_7C92F;

    {
        int32_t dy = swosBallSpriteDeltaY();
        g_swosFlags.carry = false;
        g_swosFlags.overflow = false;
        g_swosFlags.sign = dy < 0;
        g_swosFlags.zero = dy == 0;
        if (!g_swosFlags.sign)
            goto cseg_7CA2C;
    }
    goto cseg_7C940;

cseg_7C92F:
    {
        int32_t dy = swosBallSpriteDeltaY();
        g_swosFlags.carry = false;
        g_swosFlags.overflow = false;
        g_swosFlags.sign = dy < 0;
        g_swosFlags.zero = dy == 0;
        if (g_swosFlags.sign)
            goto cseg_7CA2C;
    }
    // fall through to cseg_7C940

cseg_7C940:
    swosReverseDestYDirection();
    swosResetBothTeamSpinTimers();
    swosBallSpriteSetDestX((int16_t)(swosBallSpriteDestX() + D4));
    goto cseg_7C989;

l_reverse_delta_z:
    swosBallSpriteSetDeltaZ(-swosBallSpriteDeltaZ());
    swosBallSpriteSetZ(D7);
    D6 = D6 + 65536;
    // fall through to cseg_7C989

cseg_7C989:
    // ball.cpp:1483-1526 -- audio comment selection (omitted, see header);
    // the goalTypeScored reset is real.
    (void)swosRngNextByte(); // from ball.cpp:1484 -- consumed for lockstep parity even though the branch it picked is audio-only
    swosWriteWord(ADDR_goalTypeScored, 0);
    {
        int16_t speed = swosBallSpriteSpeed();
        int16_t sub = (int16_t)((uint16_t)speed >> 2);
        swosBallSpriteSetSpeed((int16_t)(speed - sub));
    }
    swosBallSpriteSetX(D5);
    swosBallSpriteSetY(D6);
    // fall through to cseg_7CA2C

cseg_7CA2C:
    // ball.cpp:1548-1591 -- out-of-play check.
    {
        int16_t gsPl = swosReadSignedWord(ADDR_gameStatePl);
        swosFlagsSetFromSub16(gsPl, 100);
        if (!g_swosFlags.zero)
            goto l_update_ball_shadow;
    }

    {
        int16_t x = swosBallSpriteXPixels();
        swosFlagsSetFromSub16(x, 81);
        if (g_swosFlags.sign != g_swosFlags.overflow) {
            swosCheckIfBallOutOfPlay();
            goto l_update_ball_shadow;
        }
    }

    {
        int16_t x = swosBallSpriteXPixels();
        swosFlagsSetFromSub16(x, 590);
        if (!g_swosFlags.zero && g_swosFlags.sign == g_swosFlags.overflow) {
            swosCheckIfBallOutOfPlay();
            goto l_update_ball_shadow;
        }
    }

    {
        int16_t y = swosBallSpriteYPixels();
        swosFlagsSetFromSub16(y, 129);
        if (g_swosFlags.sign != g_swosFlags.overflow) {
            swosCheckIfBallOutOfPlay();
            goto l_update_ball_shadow;
        }
    }

    {
        int16_t y = swosBallSpriteYPixels();
        swosFlagsSetFromSub16(y, 769);
        if (g_swosFlags.zero || g_swosFlags.sign != g_swosFlags.overflow)
            goto l_update_ball_shadow;
    }

    swosCheckIfBallOutOfPlay();

l_update_ball_shadow:
    {
        int shadowBase = ADDR_ballShadowSpriteBase;
        int32_t z = (int16_t)swosBallSpriteZPixels();
        int32_t zHalf = (uint16_t)z >> 1;
        int32_t xBall = swosBallSpriteXPixels();

        int32_t shadowX = xBall + zHalf + 1;
        swosWriteWord(shadowBase + 32, (uint16_t)(int16_t)shadowX);

        int32_t zQuarter = (uint16_t)zHalf >> 1;
        int32_t yBall = swosBallSpriteYPixels();
        int32_t shadowY = yBall + zQuarter + 1 - 10;
        swosWriteWord(shadowBase + 36, (uint16_t)(int16_t)shadowY);

        swosWriteWord(shadowBase + 40, (uint16_t)-10);
    }

    swosCalculateNextBallPosition();

    {
        int16_t gsPl = swosReadSignedWord(ADDR_gameStatePl);
        swosFlagsSetFromSub16(gsPl, 100);
        if (g_swosFlags.zero)
            goto l_game_in_progress;
    }

    D1 = swosReadSignedWord(ADDR_foulXCoordinate);
    D2 = swosReadSignedWord(ADDR_foulYCoordinate);

    {
        int16_t gs = swosReadSignedWord(ADDR_gameState);
        swosFlagsSetFromSub16(gs, 3);
        if (g_swosFlags.zero)
            goto l_keepers_ball_or_goal_out;
    }

    {
        int16_t gs = swosReadSignedWord(ADDR_gameState);
        swosFlagsSetFromSub16(gs, 1);
        if (g_swosFlags.zero)
            goto l_keepers_ball_or_goal_out;
    }

    {
        int16_t gs = swosReadSignedWord(ADDR_gameState);
        swosFlagsSetFromSub16(gs, 2);
        if (!g_swosFlags.zero)
            goto l_calc_x_ball_quadrant;
    }
    // fall through to l_keepers_ball_or_goal_out

l_keepers_ball_or_goal_out:
    D1 = 336;
    D2 = 449;
    goto l_calc_x_ball_quadrant;

l_game_in_progress:
    D1 = swosReadSignedWord(ADDR_ballNextX);
    D2 = swosReadSignedWord(ADDR_ballNextY);
    // fall through

l_calc_x_ball_quadrant:
    {
        int32_t D3q = 0;
        int32_t D0 = 0;
        int32_t A1 = ADDR_ballXQuadrantLimits + 2;
        int i;
        bool hitCarry = false;
        for (i = 0; i < 4; i++) {
            int16_t ax = swosReadSignedWord(A1);
            A1 += 2;
            swosFlagsSetFromSub16(D1, ax);
            if (g_swosFlags.carry) { hitCarry = true; break; }
            D0 = (int16_t)(D0 + 1);
            D3q = (int16_t)(D3q + 1);
        }
        if (!hitCarry) A1 = A1 + 2;

        // l_calc_y_ball_quadrant:
        swosWriteWord(ADDR_ballXQuadrantDead, (uint16_t)D0);
        {
            int16_t ax = swosReadSignedWord(A1 - 4);
            D1 = (int16_t)(D1 - ax);
            D1 = (int16_t)(D1 - 51);
            int32_t prod = (int16_t)D1 * 5;
            int32_t quot = prod / 15;
            D1 = (int16_t)quot;
            swosWriteWord(ADDR_playerXQuadrantOffset, (uint16_t)D1);
        }

        D0 = 0;
        A1 = ADDR_ballYQuadrantLimits + 2;
        hitCarry = false;
        for (i = 0; i < 6; i++) {
            int16_t ax = swosReadSignedWord(A1);
            A1 += 2;
            swosFlagsSetFromSub16(D2, ax);
            if (g_swosFlags.carry) { hitCarry = true; break; }
            D0 = (int16_t)(D0 + 1);
            D3q = (int16_t)(D3q + 5);
        }
        if (!hitCarry) A1 = A1 + 2;

        // l_set_y_quadrant:
        swosWriteWord(ADDR_ballYQuadrantDead, (uint16_t)D0);
        {
            int16_t ax = swosReadSignedWord(A1 - 4);
            D2 = (int16_t)(D2 - ax);
            D2 = (int16_t)(D2 - 45);
            int32_t prod = (int16_t)D2 * 5;
            int32_t quot = prod / 15;
            D2 = (int16_t)quot;
            swosWriteWord(ADDR_playerYQuadrantOffset, (uint16_t)D2);
        }
        swosWriteWord(ADDR_ballQuadrantIndex, (uint16_t)D3q);
    }
    // (end of updateBall)
}

// ball.cpp:180-298 -- direction recompute + friction reduction.
static void section2DirectionAndFriction(void) {
    // ---- Step 1: direction + delta recompute (ball.cpp:180-239) ----------
    int x = swosReadSignedWord(BALLSPR_BASE + 32);
    int y = swosReadSignedWord(BALLSPR_BASE + 36);
    int destX = swosBallSpriteDestX();
    int destY = swosBallSpriteDestY();
    int speed = swosBallSpriteSpeed();

    SwosDeltasAndAngle result = swosCalculateDeltaXAndY(speed, x, y, destX, destY);

    swosBallSpriteSetDeltaX(result.deltaX);
    swosBallSpriteSetDeltaY(result.deltaY);

    if (result.direction >= 0) {
        swosBallSpriteSetFullDirection((int16_t)result.direction);
        int dir8 = ((result.direction + 16) & 0xff) >> 5;
        swosBallSpriteSetDirection((int16_t)dir8);
    } else {
        swosBallSpriteSetDirection(-1);
    }

    // ---- Step 2: friction (ball.cpp:240-297) -----------------------------
    int friction = swosReadSignedWord(ADDR_kBallGroundConstant);

    bool topHasBall = swosTeamDataPlayerHasBall(true) != 0;
    bool botHasBall = swosTeamDataPlayerHasBall(false) != 0;
    if (!topHasBall && !botHasBall) {
        int16_t pitchFactor = swosReadSignedWord(ADDR_pitchBallSpeedFactor);
        friction += pitchFactor;
    }

    int16_t zWhole = swosBallSpriteZPixels();
    if (zWhole != 0) {
        friction = swosReadSignedWord(ADDR_kBallAirConstant);
    }

    int newSpeed = speed - friction;
    if (newSpeed < 0) newSpeed = 0;
    swosBallSpriteSetSpeed((int16_t)newSpeed);
}

// ball.cpp:13-178 -- hideBall flag, frame table pick, animation pacing.
static void section1HideAndFrameIndex(void) {
    uint16_t hideBall = swosReadWord(ADDR_hideBall);
    if (hideBall != 0) {
        swosBallSpriteSetImageIndex(-1);
        swosWriteWord(ADDR_ballShadowImageIndex, (uint16_t)-1);
        return;
    }

    swosWriteWord(ADDR_ballShadowImageIndex, 1183);

    int32_t deltaX = swosBallSpriteDeltaX();
    int32_t deltaY = swosBallSpriteDeltaY();
    bool isMoving = deltaX != 0 || deltaY != 0;

    int32_t desiredTable = isMoving ? ADDR_ballMovingFrameIndices_Table : ADDR_ballStaticFrameIndices_Table;
    int32_t currentTable = swosBallSpriteFrameIndicesTable();

    if (currentTable != desiredTable) {
        swosBallSpriteSetFrameIndicesTable(desiredTable);
        swosBallSpriteSetFrameIndex(0);
    }

    int16_t speed = swosBallSpriteSpeed();
    bool advanceFrame = false;

    if (speed != 0) {
        // ball.cpp:94-107 -- tickContrib = speed/512 + 1.
        int tickContrib = ((speed >> 8) >> 1) + 1;
        int timer = swosBallSpriteCycleFramesTimer() - tickContrib;
        swosBallSpriteSetCycleFramesTimer((int16_t)timer);

        if (timer < 0) {
            swosBallSpriteSetFrameIndex((int16_t)(swosBallSpriteFrameIndex() + 1));
            swosBallSpriteSetCycleFramesTimer(swosBallSpriteFrameDelay());
            advanceFrame = true;
        }
    }

    if (!advanceFrame) {
        int16_t imageIndex = swosBallSpriteImageIndex();
        if (imageIndex >= 0) return;
    }

    extractFrameLoop();
}

// ball.cpp:150-178 -- frame table walker.
static void extractFrameLoop(void) {
    int32_t tableBase = swosBallSpriteFrameIndicesTable();
    while (true) {
        int16_t idx = swosBallSpriteFrameIndex();
        int16_t frame = swosReadSignedWord(tableBase + idx * 2);

        if (frame >= 0) {
            swosBallSpriteSetImageIndex(frame);
            return;
        }

        swosBallSpriteSetFrameIndex(0);
    }
}

// ball.cpp:4022-4026.
void swosResetBothTeamSpinTimers(void) {
    swosTeamDataSetSpinTimer(true, -1);
    swosTeamDataSetSpinTimer(false, -1);
}

// ball.cpp:4029-4042.
void swosSetBallPosition(int x, int y) {
    swosBallSpriteSetSpeed(0);
    swosBallSpriteSetXPixels((int16_t)x);
    swosBallSpriteSetYPixels((int16_t)y);
    swosBallSpriteSetZ(0);
    swosBallSpriteSetDestX((int16_t)x);
    swosBallSpriteSetDestY((int16_t)y);
    swosBallSpriteSetDeltaZ(0);
}

// ball.cpp:4509-4541 (X) / 4551-4583 (Y).
void swosReverseDestXDirection(void) {
    int ballX = swosBallSpriteXPixels();
    int destX = swosBallSpriteDestX();
    swosBallSpriteSetDestX((int16_t)(2 * ballX - destX));
}

void swosReverseDestYDirection(void) {
    int ballY = swosBallSpriteYPixels();
    int destY = swosBallSpriteDestY();
    swosBallSpriteSetDestY((int16_t)(2 * ballY - destY));
}

// ball.cpp:4206-4501.
void swosCalculateNextBallPosition(void) {
    int32_t x = swosBallSpriteX();
    int32_t y = swosBallSpriteY();
    int32_t z = swosBallSpriteZ();
    int speed = swosBallSpriteSpeed();

    if (speed != 0) {
        int32_t dx = swosBallSpriteDeltaX();
        int32_t dy = swosBallSpriteDeltaY();
        int32_t dz = swosBallSpriteDeltaZ();
        int32_t gravity = swosReadSignedWord(ADDR_kGravityConstant);

        int substepShift = 0;
        if (dz > 0) {
            substepShift = 3;
        } else {
            int16_t zWhole = swosBallSpriteZPixels();
            if (zWhole <= 20) substepShift = 0;
            else if (zWhole <= 30) substepShift = 1;
            else if (zWhole <= 35) substepShift = 2;
            else substepShift = 3;
        }

        if (substepShift > 0) {
            dx <<= substepShift;
            dy <<= substepShift;
            gravity <<= substepShift;
            z = swosAsr32(z, substepShift);
        }

        while (true) {
            x += dx;
            y += dy;
            dz -= gravity;
            z += dz;
            if (z < 0) break;
        }
    }

    int16_t nextX = (int16_t)(x >> 16);
    int16_t nextY = (int16_t)(y >> 16);
    swosWriteWord(ADDR_ballNextX, (uint16_t)nextX);
    swosWriteWord(ADDR_ballNextY, (uint16_t)nextY);
}

// ====================================================================
// applyBallAfterTouch -- ball.cpp:2248-3005
// ====================================================================
void swosApplyBallAfterTouch(bool topTeam) {
    // ball.cpp:2251 -- if pass is active, take the passing branch instead.
    if (swosTeamDataPassInProgress(topTeam) != 0) {
        applyAfterPass(topTeam);
        return;
    }

    // ball.cpp:2258-2266 -- opponent keeper recently saved -> kill spin tracking.
    bool opponent = !topTeam;
    if (swosTeamDataGoalkeeperSavedCommentTimer(opponent) < 0) {
        resetSpinTimer(topTeam);
        return;
    }

    // ball.cpp:2268-2290.
    int16_t gameStatePl = swosReadSignedWord(ADDR_gameStatePl);
    int16_t gameState = swosReadSignedWord(ADDR_gameState);
    const int16_t ST_GAME_IN_PROGRESS = 100;
    const int16_t ST_KEEPER_HOLDS_BALL = 3;
    if (gameStatePl != ST_GAME_IN_PROGRESS && gameState == ST_KEEPER_HOLDS_BALL) {
        resetSpinTimer(topTeam);
        return;
    }

    // ball.cpp:2294-2306.
    int16_t spinTimer = swosTeamDataGetSpinTimer(topTeam);
    if (spinTimer < 0) return;
    if (spinTimer == 0) {
        swosTeamDataSetLeftSpin(topTeam, 0);
        swosTeamDataSetRightSpin(topTeam, 0);
    }

    // ball.cpp:2308-2378.
    int spinSide = determineKickSpinSide(topTeam);

    // ball.cpp:2379-2440.
    if (spinSide >= 0) {
        applySpinOffsetToDestKick(topTeam, spinSide, ADDR_kKickSpinFactor);
    }

    // ball.cpp:2442-2614.
    if (spinTimer == 4) {
        applyTick4HighOrNormalKickBoost(topTeam);
    }

    // ball.cpp:2635-2659.
    incrementSpinTimerOrReset(topTeam);
}

// ball.cpp:2664-3004 -- passing-side branch.
static void applyAfterPass(bool topTeam) {
    bool opponent = !topTeam;
    if (swosTeamDataGoalkeeperSavedCommentTimer(opponent) < 0) {
        resetSpinTimer(topTeam);
        return;
    }

    int16_t gameStatePl = swosReadSignedWord(ADDR_gameStatePl);
    int16_t gameState = swosReadSignedWord(ADDR_gameState);
    if (gameStatePl != 100 && gameState == 3) {
        resetSpinTimer(topTeam);
        return;
    }

    int16_t spinTimer = swosTeamDataGetSpinTimer(topTeam);
    if (spinTimer < 0) return;
    if (spinTimer == 0) {
        swosTeamDataSetLeftSpin(topTeam, 0);
        swosTeamDataSetRightSpin(topTeam, 0);
        swosTeamDataSetLongPass(topTeam, 0);
        swosTeamDataSetLongSpinPass(topTeam, 0);
    }

    int spinSide = determinePassSpinSide(topTeam);
    if (spinSide >= 0) {
        applySpinOffsetToDestPass(topTeam, spinSide, ADDR_kPassingSpinFactor);
    }

    if (swosTeamDataLongPass(topTeam) == 0) {
        applyLongPassBoostIfNeeded(topTeam);
    }

    incrementSpinTimerOrReset(topTeam);
}

// ----- helpers ------------------------------------------------------------

static void resetSpinTimer(bool topTeam) {
    swosTeamDataSetSpinTimer(topTeam, -1);
}

// ball.cpp:2308-2378 (kick) / 2718-2787 (pass) share this logic; KICK uses
// currentAllowedDirection as the reference direction, same as PASS (both
// read Memory.Addr equivalent -- see C# source comment at DeterminePassSpinSide).
static int determineKickSpinSide(bool topTeam) {
    if (swosTeamDataLeftSpin(topTeam) != 0) return 0;
    if (swosTeamDataRightSpin(topTeam) != 0) return 4;

    int allowedDir = swosTeamDataCurrentAllowedDirection(topTeam);
    if (allowedDir < 0) return -1;

    int ctrlDir = swosTeamDataControlledPlDirection(topTeam);
    int delta = ctrlDir - allowedDir;
    if (delta == 0) return -1;

    int dWrapped = delta & 7;
    if (dWrapped == 4) return -1;

    if (dWrapped < 4) {
        swosTeamDataSetLeftSpin(topTeam, 1);
        return 0;
    } else {
        swosTeamDataSetRightSpin(topTeam, 1);
        return 4;
    }
}

static int determinePassSpinSide(bool topTeam) {
    if (swosTeamDataLeftSpin(topTeam) != 0) return 0;
    if (swosTeamDataRightSpin(topTeam) != 0) return 4;

    int allowedDir = swosTeamDataCurrentAllowedDirection(topTeam);
    if (allowedDir < 0) return -1;

    int ctrlDir = swosTeamDataControlledPlDirection(topTeam);
    int delta = ctrlDir - allowedDir;
    if (delta == 0) return -1;

    int dWrapped = delta & 7;
    if (dWrapped == 4) return -1;

    if (dWrapped < 4) {
        swosTeamDataSetLeftSpin(topTeam, 1);
        return 0;
    } else {
        swosTeamDataSetRightSpin(topTeam, 1);
        return 4;
    }
}

// ball.cpp:2379-2440 (kick), 2789-2850 (pass). Kick uses
// controlledPlDirection as the reference direction for the spin lookup;
// pass uses ballSprite.direction (see C# ApplySpinOffsetToDest's
// useBallDirection parameter) -- split into two thin C wrappers around one
// shared body since C has no default-argument overloads.
static void applySpinOffsetToDestCommon(bool topTeam, int spinSide, int spinFactorTableBase, int refDir) {
    int byteOffset = refDir * 8 + spinSide;

    int16_t spinTimer = swosTeamDataGetSpinTimer(topTeam);
    int16_t spinMultiplier = swosReadSignedWord(ADDR_kSpinMultiplierFactor + spinTimer * 2);

    int16_t spinDx = swosReadSignedWord(spinFactorTableBase + byteOffset);
    int16_t spinDy = swosReadSignedWord(spinFactorTableBase + byteOffset + 2);

    int16_t xMul = (int16_t)(spinDx * spinMultiplier);
    int16_t yMul = (int16_t)(spinDy * spinMultiplier);

    swosBallSpriteSetDestX((int16_t)(swosBallSpriteDestX() + xMul));
    swosBallSpriteSetDestY((int16_t)(swosBallSpriteDestY() + yMul));
}

static void applySpinOffsetToDestKick(bool topTeam, int spinSide, int spinFactorTableBase) {
    int refDir = swosTeamDataControlledPlDirection(topTeam);
    applySpinOffsetToDestCommon(topTeam, spinSide, spinFactorTableBase, refDir);
}

static void applySpinOffsetToDestPass(bool topTeam, int spinSide, int spinFactorTableBase) {
    int refDir = swosBallSpriteDirection();
    applySpinOffsetToDestCommon(topTeam, spinSide, spinFactorTableBase, refDir);
}

// ball.cpp:2442-2614.
static void applyTick4HighOrNormalKickBoost(bool topTeam) {
    int allowedDir = swosTeamDataCurrentAllowedDirection(topTeam);
    if (allowedDir < 0) {
        applyNormalKick();
        applySpeedAdjustment(topTeam);
        return;
    }

    int ctrlDir = swosTeamDataControlledPlDirection(topTeam);
    int delta = ctrlDir - allowedDir;
    if (delta == 0) return;

    int dWrapped = delta & 7;
    if (dWrapped == 2 || dWrapped == 6) {
        applyNormalKick();
        applySpeedAdjustment(topTeam);
        return;
    }

    if (dWrapped < 3) return;
    if (dWrapped > 5) return;

    applyHighKick();
    applySpeedAdjustment(topTeam);
}

static void applyHighKick(void) {
    swosBallSpriteSetDeltaZ(swosReadSignedDword(ADDR_kHighKickDeltaZ));
    swosBallSpriteSetSpeed((int16_t)swosReadWord(ADDR_kHighKickBallSpeed));
}

static void applyNormalKick(void) {
    swosBallSpriteSetDeltaZ(swosReadSignedDword(ADDR_kNormalKickDeltaZ));
    swosBallSpriteSetSpeed((int16_t)swosReadWord(ADDR_kNormalKickBallSpeed));
}

// ball.cpp:2545-2613.
static void applySpeedAdjustment(bool topTeam) {
    int ctrlDir = swosTeamDataControlledPlDirection(topTeam);
    int16_t speed = swosBallSpriteSpeed();

    if (ctrlDir == 0 || ctrlDir == 4) {
        int16_t reduced = (int16_t)(speed - (speed >> 2));
        swosBallSpriteSetSpeed(reduced);
    } else if ((ctrlDir & 1) != 0) {
        int16_t q3_4 = (int16_t)(speed - (speed >> 2));
        int16_t q1_8 = (int16_t)(speed >> 3);
        swosBallSpriteSetSpeed((int16_t)(q3_4 + q1_8));
    }
    // else (ctrlDir == 2 or 6): cardinal perpendicular -- no change.
}

// ball.cpp:2854-2975.
static void applyLongPassBoostIfNeeded(bool topTeam) {
    int allowedDir = swosTeamDataCurrentAllowedDirection(topTeam);

    if (allowedDir < 0) {
        applyLongPassSpeedBoost(topTeam, true);
        return;
    }

    int ctrlDir = swosTeamDataControlledPlDirection(topTeam);
    int delta = ctrlDir - allowedDir;
    if (delta == 0) return;

    int dWrapped = delta & 7;
    if (dWrapped == 2 || dWrapped == 6) {
        applyLongPassSpeedBoost(topTeam, true);
        return;
    }

    if (dWrapped < 3) return;
    if (dWrapped > 5) return;

    applyLongPassSpeedBoost(topTeam, false);
}

static void applyLongPassSpeedBoost(bool topTeam, bool longPassFlag) {
    if (longPassFlag)
        swosTeamDataSetLongPass(topTeam, 1);
    else
        swosTeamDataSetLongSpinPass(topTeam, 1);

    int16_t speed = swosBallSpriteSpeed();
    int16_t boost = (int16_t)(speed >> 3);
    swosBallSpriteSetSpeed((int16_t)(speed + boost));
}

// ball.cpp:2635-2660 (kick) / 2980-3004 (pass).
static void incrementSpinTimerOrReset(bool topTeam) {
    int16_t t = (int16_t)(swosTeamDataGetSpinTimer(topTeam) + 1);
    swosTeamDataSetSpinTimer(topTeam, t);
    if (t == 10) {
        swosTeamDataSetSpinTimer(topTeam, -1);
    }
}
