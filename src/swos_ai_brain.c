// SOURCE: openswos game/scripts/Sim/Port/AiBrain.cs (full file, step 9).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes. See
// include/swos_ai_brain.h for the dependency scan, telemetry-omission
// rationale, and the note on the C#'s stale "port scope" header comment.
//
// The C# source disables CS0164 ("unreferenced label") because several asm
// labels are kept purely as fall-through documentation (asm line-number
// markers), never targeted by a goto. The same is true here -- mirror that
// suppression instead of deleting the labels, which would lose the asm
// line-number correlation the C#'s own comment says is the point of
// keeping them.
#pragma GCC diagnostic ignored "-Wunused-label"

#include "swos_ai_brain.h"

#include <stdbool.h>
#include <stdint.h>

#include "swos_addr.h"
#include "swos_ai_helpers.h"
#include "swos_ball_sprite.h"
#include "swos_game_time.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"
#include "swos_rng.h"
#include "swos_sprite_update.h"
#include "swos_team_data.h"

// ===========================================================================
// Amiga-mode direction-flip hooks -- ported from
// external/swos-port/src/game/amigaMode.cpp:64-81.
// ===========================================================================
//
// Mirrors m_preventDirectionFlip as a file-static because these two helpers
// are its only producer/consumer pair in retail. The m_enabled equivalent
// is swosGameTimeAmigaModeActive() (PC-locked to false).
//
// PC mode (AmigaModeActive == false): both helpers are no-ops by
// construction -- the latch never sets, so the post-hook never writes. Kept
// live (not hardcoded away) so swapping to Amiga mode is one flag flip, per
// the C#'s own documented intent.
static bool s_amigaPreventDirectionFlip;

// amigaMode.cpp:64-72 -- checkForAmigaModeDirectionFlipBan. Returns true
// when the ban is NOT active (asm zero-flag semantics -- negation of
// m_preventDirectionFlip). No current caller branches on the return value;
// kept for parity and future Amiga-mode-only callers.
static bool checkForAmigaModeDirectionFlipBan(int a5SpriteAddr)
{
    s_amigaPreventDirectionFlip = false;
    if (swosGameTimeAmigaModeActive() && a5SpriteAddr != 0)
    {
        int16_t xWhole = swosReadSignedWord(a5SpriteAddr + PLSPR_OFF_X + 2);
        int16_t yWhole = swosReadSignedWord(a5SpriteAddr + PLSPR_OFF_Y + 2);
        s_amigaPreventDirectionFlip =
            xWhole >= 273 && xWhole <= 398 &&
            (yWhole <= 158 || yWhole >= 740);
    }
    return !s_amigaPreventDirectionFlip;
}

// amigaMode.cpp:74-81 -- writeAmigaModeDirectionFlip. Returns true when a
// write happened.
static bool writeAmigaModeDirectionFlip(int a6TeamBase)
{
    if (s_amigaPreventDirectionFlip)
    {
        swosWriteWord(a6TeamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, (uint16_t)(int16_t)-1);
        return true;
    }
    return false;
}

// ===========================================================================
// AI_SetControlsDirection -- updatePlayers.cpp:15980
// ===========================================================================
void swosAiBrainSetControlsDirection(int a6TeamBase)
{
    // --------------------------------------------------------------
    // ENTRY block (L15982-16060)
    // --------------------------------------------------------------
    int16_t resetControls = swosReadSignedWord(a6TeamBase + TEAMDATA_OFF_RESET_CONTROLS);
    if (resetControls != 0)
        return;

    int16_t aiCounter = swosReadSignedWord(ADDR_AI_counter);
    if (aiCounter != 0)
        swosWriteWord(ADDR_AI_counter, (uint16_t)(aiCounter - 1));

    int16_t aiResumeTimer = swosReadSignedWord(ADDR_AI_resumePlayTimer);
    if (aiResumeTimer != 0)
        swosWriteWord(ADDR_AI_resumePlayTimer, (uint16_t)(aiResumeTimer - 1));

    // 16030-16033 -- AI_rand = Rand(): a fresh byte from the shared SWOS
    // rand stream every per-team entry.
    int aiRand = swosRngNextByte();
    swosWriteWord(ADDR_AI_rand, (uint16_t)aiRand);

    uint16_t gameTick = swosReadWord(ADDR_currentGameTick);

    int16_t aiTimer = swosReadSignedWord(a6TeamBase + TEAMDATA_OFF_AI_TIMER);
    swosWriteWord(a6TeamBase + TEAMDATA_OFF_AI_TIMER, (uint16_t)(aiTimer + 1));

    swosWriteWord(a6TeamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, (uint16_t)(int16_t)-1);
    swosWriteByte(a6TeamBase + TEAMDATA_OFF_FIRE_PRESSED, 0);
    swosWriteByte(a6TeamBase + TEAMDATA_OFF_FIRE_THIS_FRAME, 0);
    swosWriteByte(a6TeamBase + TEAMDATA_OFF_QUICK_FIRE, 0);
    swosWriteByte(a6TeamBase + TEAMDATA_OFF_NORMAL_FIRE, 0);

    int16_t inSubsMenu = swosReadSignedWord(ADDR_g_inSubstitutesMenu);
    if (inSubsMenu != 0)
        return;

    int d7 = -1;
    int d2Byte = 0;

    int a5 = swosReadSignedDword(a6TeamBase + TEAMDATA_OFF_CONTROLLED_PLAYER);
    int a4 = swosReadSignedDword(a6TeamBase + TEAMDATA_OFF_PASS_TO_PLAYER_PTR);

    if (a5 != 0)
    {
        d7 = swosReadSignedWord(a5 + PLSPR_OFF_DIRECTION);
    }

    // 16079-16095 -- goal-line Y constant, team-dependent.
    int d2Word;
    if (a6TeamBase == TEAMDATA_BOTTOM_BASE)
    {
        d2Word = 129;
    }
    else
    {
        d2Word = 769;
    }
    // FIDELITY FIX (task #242, preserved from the C#): in the asm D2 is ONE
    // register -- the 769/129 write above also seeds the low byte the spin-
    // direction sign test reads later (cseg_850F9 / l_activate_normal_fire)
    // whenever no intervening block rewrote D2.
    d2Byte = d2Word & 0xFF;

    int d1Word = 336;

    int16_t ballXPixels = swosBallSpriteXPixels();
    int d3Word = (int16_t)(ballXPixels - d1Word);

    int16_t ballYPixels = swosBallSpriteYPixels();
    int d4Word = (int16_t)(ballYPixels - d2Word);

    int d3 = (int)(int16_t)d3Word * (int)(int16_t)d3Word;
    int d4 = (int)(int16_t)d4Word * (int)(int16_t)d4Word;

    int d3Sum = d3 + d4;
    int d6 = d3Sum;

    SwosDeltasAndAngle calcResult = swosCalculateDeltaXAndY(
        256,
        (int16_t)ballXPixels,
        (int16_t)ballYPixels,
        (int16_t)d1Word,
        (int16_t)d2Word);
    int calcD0 = calcResult.direction;

    int d0 = (calcD0 < 0) ? 0 : calcD0;
    int d5 = d0 & 0xFFFF;

    // --------------------------------------------------------------
    // GAME_STATE_PL = 100 fast path (L16165-16176)
    // --------------------------------------------------------------
    int16_t gameStatePl = swosReadSignedWord(ADDR_gameStatePl);
    if (gameStatePl == 100)
        goto l_game_in_progress;

    // --------------------------------------------------------------
    // Not in progress: gate on last-team-played-before-break
    // --------------------------------------------------------------
    {
        int lastTeamBb = swosReadSignedDword(ADDR_lastTeamPlayedBeforeBreak);
        if (a6TeamBase != lastTeamBb)
            return;
    }

    {
        int16_t gameState = swosReadSignedWord(ADDR_gameState);

        if ((uint16_t)gameState < (uint16_t)21)
            goto l_game_not_over;

        if ((uint16_t)gameState > (uint16_t)30)
            goto l_game_not_over;

        {
            int16_t t1Comp = swosReadSignedWord(ADDR_team1Computer);
            if (t1Comp == 0)
                goto l_game_not_over;
        }
        {
            int16_t t2Comp = swosReadSignedWord(ADDR_team2Computer);
            if (t2Comp == 0)
                goto l_game_not_over;
        }

        if (gameState == 25)
            goto l_showing_result_on_halftime;

        if (gameState != 26)
            goto l_not_showing_final_result;

        {
            int16_t stoppageTotal = swosReadSignedWord(ADDR_stoppageTimerTotal);
            int16_t clearInterval = swosReadSignedWord(ADDR_m_clearResultInterval);
            if ((uint16_t)stoppageTotal < (uint16_t)clearInterval)
                return;
            goto l_interval_expired_fire;
        }

    l_showing_result_on_halftime:;
        {
            int16_t stoppageTotal = swosReadSignedWord(ADDR_stoppageTimerTotal);
            int16_t clearInterval = swosReadSignedWord(ADDR_m_clearResultInterval);
            if ((uint16_t)stoppageTotal < (uint16_t)clearInterval)
                return;
            goto l_interval_expired_fire;
        }

    l_not_showing_final_result:;
        {
            int16_t stoppageTotal = swosReadSignedWord(ADDR_stoppageTimerTotal);
            int16_t clearInterval = swosReadSignedWord(ADDR_m_clearResultHalftimeInterval);
            if ((uint16_t)stoppageTotal < (uint16_t)clearInterval)
                return;
        }

    l_interval_expired_fire:;
        swosWriteByte(a6TeamBase + TEAMDATA_OFF_FIRE_PRESSED, 1);
        return;
    }

l_game_not_over:;
    {
        // --------------------------------------------------------------
        // GAME_NOT_OVER
        // --------------------------------------------------------------
        {
            int d0Tick = (gameTick & 0x3F) + 100;
            int16_t stoppageActive = swosReadSignedWord(ADDR_stoppageTimerActive);
            if (d0Tick > stoppageActive)
                goto l_set_direction;
        }

        {
            int16_t pp = swosReadSignedWord(ADDR_playingPenalties);
            if (pp != 0)
                goto l_doing_penalties;
        }

        int16_t gameState = swosReadSignedWord(ADDR_gameState);

        if (gameState == 14)
            goto l_doing_penalties;

        if (gameState == 3)
            goto l_keepers_ball;

        if (gameState == 1)
            goto l_keepers_ball;

        if (gameState == 2)
            goto l_keepers_ball;

        if (gameState == 0)
            goto l_goal_scored;

        if ((uint16_t)gameState < (uint16_t)6)
            goto l_test_throw_in;

        if ((uint16_t)gameState <= (uint16_t)12)
            goto l_free_kick;

    l_test_throw_in:;
        if ((uint16_t)gameState < (uint16_t)15)
            goto l_test_foul;

        if ((uint16_t)gameState > (uint16_t)20)
            goto l_test_foul;

        {
            int idx = gameState - 15;
            uint8_t tableByte = swosReadByte(ADDR_AI_throwInDirections + idx);
            uint8_t d1Byte = tableByte;
            if (a6TeamBase == TEAMDATA_BOTTOM_BASE)
                goto cseg_84671_byD1;
            d1Byte = (uint8_t)(((d1Byte >> 4) & 0x0F) | ((d1Byte & 0x0F) << 4));

        cseg_84671_byD1:;
            {
                int d0Mask = aiRand & 0xF;
                if (d0Mask != 0)
                    goto cseg_84775;
                int mask = (d7 >= 0 && d7 < 32) ? (1 << (d7 & 0x1F)) : 0;
                int test = d1Byte & mask;
                if (test != 0)
                    goto l_apply_after_touch;
                goto cseg_84775;
            }
        }

    l_test_foul:;
        if (gameState == 13)
            goto l_free_kick;

        goto l_apply_after_touch;

    l_keepers_ball:;
        if ((aiRand & 1) == 0)
            goto cseg_84775;

        {
            int16_t camDir = swosReadSignedWord(ADDR_cameraDirection);
            if (d7 == camDir)
                goto l_apply_after_touch;
        }

        {
            int16_t ballXNow = swosBallSpriteXPixels();
            if ((uint16_t)ballXNow > (uint16_t)336)
                goto cseg_845CC;
        }

        if (d7 == 1) goto l_apply_after_touch;
        if (d7 == 3) goto l_apply_after_touch;

        goto cseg_84775;

    cseg_845CC:;
        if (d7 == 5) goto l_apply_after_touch;
        if (d7 == 7) goto l_apply_after_touch;
        goto cseg_84775;

    l_goal_scored:;
        {
            int16_t stoppageActive = swosReadSignedWord(ADDR_stoppageTimerActive);
            if ((uint16_t)stoppageActive < (uint16_t)150)
                goto cseg_84775;
        }
        {
            int16_t camDir = swosReadSignedWord(ADDR_cameraDirection);
            if (d7 == camDir)
                goto l_apply_after_touch;
        }
        goto cseg_84775;

    l_free_kick:;
        if ((aiRand & 0xF) == 0)
            goto cseg_84775;
        goto cseg_8470F;

    l_doing_penalties:;
        {
            int d0p = aiRand & 7;
            int mask = 1 << d0p;
            uint8_t turnFlags = swosReadByte(ADDR_playerTurnFlags);
            if ((turnFlags & mask) != 0)
            {
                swosWriteWord(a6TeamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, (uint16_t)d0p);
                return;
            }
        }
        if (a5 == 0) return; // defensive -- shouldn't reach here with a5==0
        {
            int16_t a5Dir = swosReadSignedWord(a5 + PLSPR_OFF_DIRECTION);
            if (a5Dir == 0) return;
            if (a5Dir == 4) return;
            goto l_apply_after_touch;
        }
    }

cseg_8470F:;
    {
        int d0w = (d5 + 16) & 0xFF;
        d0w = d0w >> 5;
        if (d0w == (d7 & 0xFFFF))
            goto l_apply_after_touch;
        goto cseg_84815;
    }

l_apply_after_touch:;
    if (d7 < 0) return;
    {
        int d2Apply = (d7 << 5) & 0xFFFF;
        int d2LoSubByte = ((d2Apply & 0xFF) - (d5 & 0xFF)) & 0xFF;
        d2Byte = d2LoSubByte;
        goto l_update_max_stoppage_time;
    }

cseg_84775:;
    {
        int d0FullDir = (d7 << 5) & 0xFFFF;
        int outDist;
        int closestPlayer = swosAiHelpersFindClosestPlayerToBallFacing(d0FullDir, a6TeamBase, &outDist);
        if (closestPlayer == -1)
            goto l_update_turn_direction;
        if (a5 == 0)
            goto l_update_turn_direction;
        int16_t a5Team = swosReadSignedWord(a5 + PLSPR_OFF_TEAM_NUMBER);
        int16_t a0Team = swosReadSignedWord(closestPlayer + PLSPR_OFF_TEAM_NUMBER);
        if (a5Team != a0Team)
            goto l_update_turn_direction;
        goto l_our_player_closest;
    }

cseg_84815:;
    {
        int d0w = ((d5 + 16) & 0xFF) >> 5;
        swosWriteWord(a6TeamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, (uint16_t)d0w);
        return;
    }

l_update_turn_direction:;
    {
        int d0Tick = gameTick & 0x0E;
        if (d0Tick != 0)
            return;

        int16_t turnDir = swosReadSignedWord(ADDR_AI_turnDirection);
        int d1Direction;

        if (turnDir < 0)
        {
            if (turnDir == -1)
            {
                goto cseg_84890;
            }
            swosWriteWord(ADDR_AI_turnDirection, (uint16_t)(turnDir + 1));
            {
                int16_t tdAfter = swosReadSignedWord(ADDR_AI_turnDirection);
                if (tdAfter != -1)
                    return;
            }
        cseg_84890:;
            d1Direction = 1;
            goto l_apply_turn_direction;
        }
        if (turnDir == 1)
            goto cseg_848BB;
        swosWriteWord(ADDR_AI_turnDirection, (uint16_t)(turnDir - 1));
        {
            int16_t tdAfter = swosReadSignedWord(ADDR_AI_turnDirection);
            if (tdAfter != 1)
                return;
        }
    cseg_848BB:;
        d1Direction = -1;

    l_apply_turn_direction:;
        {
            int16_t curTurnDir = swosReadSignedWord(ADDR_AI_turnDirection);
            int d0New = ((d7 + curTurnDir) & 7);
            uint8_t tf = swosReadByte(ADDR_playerTurnFlags);
            int mask = 1 << d0New;
            if ((tf & mask) == 0)
            {
                swosWriteWord(ADDR_AI_turnDirection, (uint16_t)d1Direction);
                return;
            }
            swosWriteWord(a6TeamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, (uint16_t)d0New);
            return;
        }
    }

l_set_direction:;
l_check_if_result_shown:;
    {
        int16_t aiRes = swosReadSignedWord(ADDR_AI_resultTimer);
        if (aiRes != 0)
            return;
    }

    {
        int16_t gs = swosReadSignedWord(ADDR_gameState);
        if (gs == 1) goto l_update_turn_direction;
        if (gs == 2) goto l_update_turn_direction;
        if (gs == 3) goto l_update_turn_direction;
        if ((uint16_t)gs >= (uint16_t)15)
        {
            if ((uint16_t)gs <= (uint16_t)20)
                goto l_update_turn_direction;
        }
        if (gs == 13) goto l_foul_or_free_kick;
        if ((uint16_t)gs < (uint16_t)6) return;
        if ((uint16_t)gs > (uint16_t)12) return;
    }

l_foul_or_free_kick:;
    {
        int d0w = ((d5 + 16) & 0xFF) >> 5;
        swosWriteWord(a6TeamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, (uint16_t)d0w);
        return;
    }

l_game_in_progress:;
    {
        int16_t pp = swosReadSignedWord(ADDR_playingPenalties);
        int16_t pen = swosReadSignedWord(ADDR_penalty);
        if (pp == 0 && pen == 0)
            goto l_no_penalty;
    }
    {
        int16_t spinTimer = swosReadSignedWord(a6TeamBase + TEAMDATA_OFF_SPIN_TIMER);
        if (spinTimer >= 0)
            goto l_ball_after_touch_allowed;
    }

l_no_penalty:;
    {
        int16_t aic = swosReadSignedWord(ADDR_AI_counter);
        if (aic != 0)
            swosAiHelpersSetDirectionTowardOpponentsGoal(a6TeamBase);
    }
    if (a5 == 0) return;
    {
        int16_t spinTimer = swosReadSignedWord(a6TeamBase + TEAMDATA_OFF_SPIN_TIMER);
        if (spinTimer >= 0)
            goto l_ball_after_touch_allowed;
    }

    {
        uint8_t plVeryClose = swosReadByte(a6TeamBase + TEAMDATA_OFF_PL_VERY_CLOSE_TO_BALL);
        if (plVeryClose != 0)
            goto l_theres_a_player_near;
        uint8_t plClose = swosReadByte(a6TeamBase + TEAMDATA_OFF_PL_CLOSE_TO_BALL);
        if (plClose != 0)
            goto l_theres_a_player_near;
        goto l_noone_near;
    }

l_theres_a_player_near:;
    {
        bool fired = swosAiHelpersDecideWhetherToTriggerFire(d7, a5, a6TeamBase);
        if (fired) return;
        // 17338-17369 -- dead branch protected by deadVarAlways0 (always 0).
        // Skipped semantically (always falls through to cseg_84A0D).
    }
    // cseg_84A0D:
    {
        int16_t a5Dir = swosReadSignedWord(a5 + PLSPR_OFF_DIRECTION);
        bool facingLeftOrRight = (a5Dir == 2 || a5Dir == 6);

        bool jumpTo84AEB = false;
        if (facingLeftOrRight)
        {
            if (a6TeamBase == TEAMDATA_TOP_BASE)
            {
                int16_t by = swosBallSpriteYPixels();
                if (by >= 740) jumpTo84AEB = true;
            }
            else
            {
                int16_t by = swosBallSpriteYPixels();
                if (by <= 158) jumpTo84AEB = true;
            }
        }

        if (!jumpTo84AEB)
        {
            if (d6 > 28800)
            {
                jumpTo84AEB = true;
            }
            else
            {
                if (d6 >= 12800)
                {
                    int randMask = aiRand & 3;
                    if (randMask != 0) jumpTo84AEB = true;
                }
            }
        }

        if (jumpTo84AEB)
            goto cseg_84AEB;

        // cseg_84A85: fine-grain byte-wise angle window.
        {
            int d1Byte = 15;
            if (!(d6 > 3200))
            {
                d1Byte = 50;
            }
            // cseg_84A9F:
            int16_t a5DirCs = swosReadSignedWord(a5 + PLSPR_OFF_DIRECTION);
            if (a5DirCs < 0)
                goto cseg_84AEB;
            int d2WordCs = a5DirCs & 0xFFFF;
            d2WordCs = (d2WordCs << 5) & 0xFFFF;
            int d2Lo = ((d2WordCs & 0xFF) - (d5 & 0xFF)) & 0xFF;
            d2WordCs = (d2WordCs & ~0xFF) | d2Lo;
            d2Byte = d2Lo;
            int8_t d2Signed = (int8_t)d2Lo;
            int8_t d1Signed = (int8_t)d1Byte;
            if (d2Signed > d1Signed)
                goto cseg_84AEB;
            d1Signed = (int8_t)(-d1Signed);
            if (d2Signed > d1Signed)
            {
                goto cseg_850F9;
            }
            goto cseg_84AEB;
        }
    }

cseg_84AEB:;
    {
        int16_t field84 = swosReadSignedWord(a6TeamBase + TEAMDATA_OFF_AI_FIELD_84);
        if (field84 != 0)
        {
            swosWriteWord(a6TeamBase + TEAMDATA_OFF_AI_FIELD_84, (uint16_t)(field84 - 1));
            int d0FullDir = (d7 << 5) & 0xFFFF;
            int outDist;
            int closest = swosAiHelpersFindClosestPlayerToBallFacing(d0FullDir, a6TeamBase, &outDist);
            if (closest == -1)
                goto cseg_84D57;
            if (a5 == 0)
                goto cseg_84D57;
            int16_t a5Team = swosReadSignedWord(a5 + PLSPR_OFF_TEAM_NUMBER);
            int16_t cTeam = swosReadSignedWord(closest + PLSPR_OFF_TEAM_NUMBER);
            if (a5Team == cTeam)
                goto l_our_player_closest;
            goto cseg_84D57;
        }
    }

cseg_84B5B:;
    {
        if ((uint32_t)d6 < (uint32_t)9800)
            goto cseg_84DD3;

        if (a5 == 0)
            goto cseg_84DD3;

        int a0Cs = swosReadSignedDword(a6TeamBase + TEAMDATA_OFF_OPPONENTS_TEAM);
        int a2Cs = swosReadSignedDword(a0Cs + TEAMDATA_OFF_CONTROLLED_PLAYER);

        if (a2Cs == 0)
            goto cseg_84BBE;

        {
            int a2BallDist = swosReadSignedDword(a2Cs + PLSPR_OFF_BALL_DISTANCE);
            if ((uint32_t)a2BallDist < (uint32_t)800)
                goto cseg_84C00;

            if ((uint32_t)a2BallDist < (uint32_t)5000)
                goto cseg_84C93;
        }

    cseg_84BBE:;
        a2Cs = swosReadSignedDword(a0Cs + TEAMDATA_OFF_PASS_TO_PLAYER_PTR);

        if (a2Cs == 0)
            goto cseg_84DD3;

        {
            int a2BallDistB = swosReadSignedDword(a2Cs + PLSPR_OFF_BALL_DISTANCE);
            if ((uint32_t)a2BallDistB < (uint32_t)800)
                goto cseg_84C00;

            if ((uint32_t)a2BallDistB < (uint32_t)5000)
                goto cseg_84C93;
        }

        goto cseg_84DD3;
    }

cseg_84C00:;
    {
        if (d6 > 180000)
            goto cseg_84F4B;

        int d0Full = (d7 << 5) & 0xFFFF;
        int outDist;
        int a0Cf = swosAiHelpersFindClosestPlayerToBallFacing(d0Full, a6TeamBase, &outDist);

        if (a0Cf == -1)
            goto cseg_84D10;

        int16_t a5Tn = swosReadSignedWord(a5 + PLSPR_OFF_TEAM_NUMBER);
        int16_t a0Tn = swosReadSignedWord(a0Cf + PLSPR_OFF_TEAM_NUMBER);
        if (a5Tn == a0Tn)
            goto l_our_player_closest;

        goto cseg_84D10;
        // 17772-17798 -- dead branch (cmp A0, -1; jnz ...) preserved for
        // parity in the C# comment; not reachable, nothing to port.
    }

cseg_84C93:;
    {
        if ((uint16_t)aiRand > (uint16_t)8)
            goto cseg_84CAD;

        if (d6 > 48400)
            goto cseg_84F4B;

    cseg_84CAD:;
        if ((gameTick & 12) != 0)
            goto cseg_84DD3;

        int d0Full = (d7 << 5) & 0xFFFF;
        int outDist;
        int a0Cf = swosAiHelpersFindClosestPlayerToBallFacing(d0Full, a6TeamBase, &outDist);

        if (a0Cf == -1)
            goto cseg_84D10;

        int16_t a5Tn = swosReadSignedWord(a5 + PLSPR_OFF_TEAM_NUMBER);
        int16_t a0Tn = swosReadSignedWord(a0Cf + PLSPR_OFF_TEAM_NUMBER);
        if (a5Tn == a0Tn)
            goto l_our_player_closest;
        // fall through into cseg_84D10.
    }

cseg_84D10:;
    {
        int16_t f84 = swosReadSignedWord(a6TeamBase + TEAMDATA_OFF_AI_FIELD_84);
        if (f84 != 0)
            goto cseg_84DD3;

        uint16_t frameCnt = swosReadWord(ADDR_frameCount);
        int d0Fc = frameCnt & 0x7F;
        if ((uint16_t)d0Fc >= (uint16_t)32)
            goto cseg_84DD3;

        swosWriteWord(a6TeamBase + TEAMDATA_OFF_AI_FIELD_84, 4);
        goto cseg_84D57;
    }

cseg_84DD3:;
    {
        uint8_t plClose = swosReadByte(a6TeamBase + TEAMDATA_OFF_PL_VERY_CLOSE_TO_BALL);
        if (plClose == 0)
            goto cseg_84E16;
        goto cseg_84DE0;
    }

cseg_84D57:;
    {
        uint8_t plClose = swosReadByte(a6TeamBase + TEAMDATA_OFF_PL_VERY_CLOSE_TO_BALL);
        if (plClose == 0)
            goto cseg_84E16;

        bool turnRight = (gameTick & 0x80) != 0;
        int16_t a5Dir = swosReadSignedWord(a5 + PLSPR_OFF_DIRECTION);
        int d0New = ((a5Dir + (turnRight ? 1 : -1)) & 7);
        swosWriteWord(a6TeamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, (uint16_t)d0New);
        return;
    }

cseg_84DE0:;
    {
        int d0w = ((d5 + 16) & 0xFF) >> 5;
        swosWriteWord(a6TeamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, (uint16_t)d0w);
        return;
    }

cseg_84E16:;
    {
        int16_t a5Dir = swosReadSignedWord(a5 + PLSPR_OFF_DIRECTION);
        swosWriteWord(a6TeamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, (uint16_t)a5Dir);
        return;
    }

l_decide_if_flipping_direction:;
    {
        int16_t fullDir = swosReadSignedWord(a5 + PLSPR_OFF_FULL_DIRECTION);
        int d0Calc = (fullDir & 0xFFFF);
        int d0LoByte = ((d0Calc & 0xFF) + 0x80) & 0xFF;
        d0Calc = (d0Calc & ~0xFF) | d0LoByte;
        d0Calc = (d0Calc + 16) & 0xFFFF;
        d0Calc &= 0xFF;
        d0Calc >>= 5;

        int16_t a5Dir = swosReadSignedWord(a5 + PLSPR_OFF_DIRECTION);
        if (d0Calc == a5Dir)
            goto l_set_opposite_direction;

        int ballDist = swosReadSignedDword(a5 + PLSPR_OFF_BALL_DISTANCE);
        if ((uint32_t)ballDist < (uint32_t)800)
            goto l_set_opposite_direction;

        int d1Tick = gameTick & 0x0E;
        if (d1Tick == 0)
            goto l_set_opposite_direction;

        swosWriteWord(a6TeamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, (uint16_t)a5Dir);
        return;

    l_set_opposite_direction:;
        swosWriteWord(a6TeamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, (uint16_t)d0Calc);
        return;
    }

l_our_player_closest:;
    swosWriteWord(a6TeamBase + TEAMDATA_OFF_AI_FIELD_84, 0);
    {
        int16_t rpt = swosReadSignedWord(ADDR_AI_resumePlayTimer);
        if (rpt != 0) return;
        int outOpp;
        bool carry = swosAiHelpersResumeGameDelay(a6TeamBase, &outOpp);
        if (!carry) return;
        swosWriteWord(ADDR_AI_resumePlayTimer, 15);
        swosWriteWord(a6TeamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, (uint16_t)d7);
        swosWriteByte(a6TeamBase + TEAMDATA_OFF_QUICK_FIRE, 1);
        int16_t aiMax = swosReadSignedWord(ADDR_AI_maxStoppageTime);
        int16_t stoppageActive = swosReadSignedWord(ADDR_stoppageTimerActive);
        if (aiMax <= stoppageActive)
            swosWriteWord(ADDR_AI_maxStoppageTime, (uint16_t)stoppageActive);
        return;
    }

l_update_max_stoppage_time:;
    {
        int16_t aiMax = swosReadSignedWord(ADDR_AI_maxStoppageTime);
        int16_t stoppageActive = swosReadSignedWord(ADDR_stoppageTimerActive);
        if (aiMax <= stoppageActive)
            swosWriteWord(ADDR_AI_maxStoppageTime, (uint16_t)stoppageActive);
    }
l_decide_after_touch_strength:;
    {
        swosWriteWord(ADDR_AI_resumePlayTimer, 15);

        int16_t gsAt = swosReadSignedWord(ADDR_gameState);
        if (gsAt == 14)
            goto l_weak_after_touch;

        if (gsAt == 31)
            goto l_weak_after_touch;

        if ((uint16_t)gsAt < (uint16_t)6)
            goto l_test_corner;

        if ((uint16_t)gsAt <= (uint16_t)12)
            goto l_check_distance_from_the_goal;

    l_test_corner:;
        if (gsAt == 4)
            goto l_medium_after_touch;
        if (gsAt == 5)
            goto l_medium_after_touch;

        {
            int d0Rand = aiRand & 0x18;
            if (d0Rand != 0)
                goto l_check_distance_from_the_goal;
            if (d0Rand == 16)
                goto l_weak_after_touch;
            if (d0Rand == 8)
                goto l_medium_after_touch;
            goto l_strong_after_touch;
        }

    l_check_distance_from_the_goal:;
        if ((uint32_t)d6 < (uint32_t)28800)
            goto l_weak_after_touch;
        if ((uint32_t)d6 < (uint32_t)57800)
            goto l_medium_after_touch;

    l_strong_after_touch:;
        swosWriteWord(a6TeamBase + TEAMDATA_OFF_AI_AFTER_TOUCH_STRENGTH, 2);
        goto l_activate_normal_fire;

    l_medium_after_touch:;
        swosWriteWord(a6TeamBase + TEAMDATA_OFF_AI_AFTER_TOUCH_STRENGTH, 1);
        goto l_activate_normal_fire;

    l_weak_after_touch:;
        swosWriteWord(a6TeamBase + TEAMDATA_OFF_AI_AFTER_TOUCH_STRENGTH, 0);
        // fall through to l_activate_normal_fire.
    }

l_activate_normal_fire:;
    {
        swosWriteWord(a6TeamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, (uint16_t)d7);
        swosWriteByte(a6TeamBase + TEAMDATA_OFF_NORMAL_FIRE, 1);

        int16_t gsAct = swosReadSignedWord(ADDR_gameState);
        if (gsAct == 14)
            goto l_no_after_touch;
        if (gsAct == 31)
            goto l_no_after_touch;

        if (gsAct == 4)
            goto l_corner;
        if (gsAct == 5)
            goto l_corner;

        swosWriteWord(ADDR_AI_resumePlayTimer, 15);
        swosWriteWord(a6TeamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, (uint16_t)d7);
        swosWriteByte(a6TeamBase + TEAMDATA_OFF_NORMAL_FIRE, 1);

        int d0Spin = 0;

        if (gsAct != 13)
            goto cseg_8536B;

        if (a6TeamBase != TEAMDATA_TOP_BASE)
            goto cseg_8535D;

        {
            int a2Foul = swosReadSignedDword(a6TeamBase + TEAMDATA_OFF_PASS_TO_PLAYER_PTR);
            if (a2Foul != 0)
            {
                int16_t a2Y = swosReadSignedWord(a2Foul + PLSPR_OFF_Y + 2);
                if (a2Y >= 682)
                    goto cseg_85384;
            }
            goto cseg_8536B;
        }

    cseg_8535D:;
        {
            int a2Foul = swosReadSignedDword(a6TeamBase + TEAMDATA_OFF_PASS_TO_PLAYER_PTR);
            if (a2Foul != 0)
            {
                int16_t a2Y = swosReadSignedWord(a2Foul + PLSPR_OFF_Y + 2);
                if (a2Y <= 216)
                    goto cseg_85384;
            }
            // fall through to cseg_8536B.
        }

    cseg_8536B:;
        d0Spin = -1;
        {
            int8_t d2Sign = (int8_t)d2Byte;
            if (d2Sign >= 0)
                goto cseg_85384;
        }
        d0Spin = -(-1); // = 1
        // fall through

    cseg_85384:;
        swosWriteWord(a6TeamBase + TEAMDATA_OFF_AI_BALL_SPIN_DIRECTION, (uint16_t)d0Spin);
        return;

    l_corner:;
        {
            int d0Corner = aiRand & 7;
            if (d0Corner < 3)
                goto cseg_853FA;
            if (d0Corner < 6)
                goto cseg_853DB;
            // else fall through to l_no_after_touch
        }

    l_no_after_touch:;
        swosWriteWord(a6TeamBase + TEAMDATA_OFF_AI_BALL_SPIN_DIRECTION, 0);
        return;

    cseg_853DB:;
        {
            int d1SpinL = -1;
            int d0SideL = ((d7 & 0xFFFF) - 1) & 7;
            int maskTfL = 1 << d0SideL;
            uint8_t tfL = swosReadByte(ADDR_playerTurnFlags);
            if ((tfL & maskTfL) == 0)
                goto l_no_after_touch;
            swosWriteWord(a6TeamBase + TEAMDATA_OFF_AI_BALL_SPIN_DIRECTION, (uint16_t)d1SpinL);
            return;
        }

    cseg_853FA:;
        {
            int d1SpinR = 1;
            int d0SideR = ((d7 & 0xFFFF) + 1) & 7;
            int maskTfR = 1 << d0SideR;
            uint8_t tfR = swosReadByte(ADDR_playerTurnFlags);
            if ((tfR & maskTfR) == 0)
                goto l_no_after_touch;
            swosWriteWord(a6TeamBase + TEAMDATA_OFF_AI_BALL_SPIN_DIRECTION, (uint16_t)d1SpinR);
            return;
        }
    }

l_noone_near:;
    {
        bool fired = swosAiHelpersDecideWhetherToTriggerFire(d7, a5, a6TeamBase);
        if (fired) return;

        if (a4 == 0)
            goto l_pass_to_player_too_far_or_null;

        {
            int a4Dist = swosReadSignedDword(a4 + PLSPR_OFF_BALL_DISTANCE);
            int a5Dist = swosReadSignedDword(a5 + PLSPR_OFF_BALL_DISTANCE);
            if ((a4Dist - a5Dist) < 50)
                goto l_pass_to_player_too_far_or_null;
        }

        {
            int passPtr = swosReadSignedDword(a6TeamBase + TEAMDATA_OFF_PASS_TO_PLAYER_PTR);
            int ctlPtr = swosReadSignedDword(a6TeamBase + TEAMDATA_OFF_CONTROLLED_PLAYER);
            swosWriteDword(a6TeamBase + TEAMDATA_OFF_PASS_TO_PLAYER_PTR, (uint32_t)ctlPtr);
            swosWriteDword(a6TeamBase + TEAMDATA_OFF_CONTROLLED_PLAYER, (uint32_t)passPtr);
            return;
        }

    l_pass_to_player_too_far_or_null:;
        if (a4 == 0)
            goto l_decide_if_flipping_direction;

        {
            // 18981-18987 -- deadVarAlways0 gate. Always 0 -> skip to next check.
            int16_t dv = swosReadSignedWord(ADDR_deadVarAlways0);
            (void)dv;
        }

        {
            int16_t topPl = swosReadSignedWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PLAYER_NUMBER);
            if (topPl != 0)
                goto l_decide_if_flipping_direction;
            int16_t botPl = swosReadSignedWord(TEAMDATA_BOTTOM_BASE + TEAMDATA_OFF_PLAYER_NUMBER);
            if (botPl != 0)
                goto l_decide_if_flipping_direction;
            goto l_randomly_flip_or_continue_direction;
        }
    }

l_randomly_flip_or_continue_direction:;
    {
        int d0Tick = gameTick & 0x18;
        if (d0Tick != 0)
            goto l_use_current_player_direction;

        checkForAmigaModeDirectionFlipBan(a5);

        int idx = aiRand & 2;
        int16_t rotateAmount = swosReadSignedWord(ADDR_AI_randomRotateTable + idx);
        int16_t fullDir = swosReadSignedWord(a5 + PLSPR_OFF_FULL_DIRECTION);
        int d0Calc = (rotateAmount + fullDir) & 0xFFFF;
        int d0LoByte = ((d0Calc & 0xFF) + 0x80) & 0xFF;
        d0Calc = (d0Calc & ~0xFF) | d0LoByte;
        d0Calc = (d0Calc + 16) & 0xFFFF;
        d0Calc &= 0xFF;
        d0Calc >>= 5;
        swosWriteWord(a6TeamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, (uint16_t)d0Calc);
        return;
    }

l_use_current_player_direction:;
    swosWriteWord(a6TeamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, (uint16_t)d7);
    writeAmigaModeDirectionFlip(a6TeamBase);
    return;

cseg_850F9:;
    {
        // The asm contains THREE near-identical blocks (one each for
        // after-touch strength 0, 1, 2) but only the FIRST is wired by
        // goto -- the other two are dead code in the compiled binary. Only
        // the live block is ported here, matching the C#.
        int16_t rptCs = swosReadSignedWord(ADDR_AI_resumePlayTimer);
        if (rptCs != 0) return;

        int outOpp;
        bool carryRgd = swosAiHelpersResumeGameDelay(a6TeamBase, &outOpp);
        if (!carryRgd) return;

        swosWriteWord(a6TeamBase + TEAMDATA_OFF_AI_AFTER_TOUCH_STRENGTH, 0);
        // cseg_85178:
        swosWriteWord(ADDR_AI_resumePlayTimer, 15);
        swosWriteWord(a6TeamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, (uint16_t)d7);
        swosWriteByte(a6TeamBase + TEAMDATA_OFF_NORMAL_FIRE, 1);

        int d0Cs = -1;
        int8_t d2SignCs = (int8_t)d2Byte;
        if (d2SignCs < 0)
        {
            d0Cs = -d0Cs;
        }
        // cseg_851B4:
        swosWriteWord(a6TeamBase + TEAMDATA_OFF_AI_BALL_SPIN_DIRECTION, (uint16_t)d0Cs);
        return;
    }

cseg_84F4B:;
    {
        int d0F4B = ((int16_t)(d5 + 16)) & 0xFFFF;
        d0F4B &= 0xFF;
        d0F4B = (d0F4B & 0xFFFF) >> 5;
        d0F4B = (int16_t)(d0F4B - d7);
        d0F4B = d0F4B & 7;
        if (d0F4B == 0)
            goto cseg_84FA0;
        if (d0F4B == 1)
            goto cseg_84FA0;
        // After `& 7`, "-1" lands as 7 -- this compare is dead code in the
        // compiled binary, preserved mechanically for parity.
        if (d0F4B == -1)
            goto cseg_84FA0;
        goto cseg_84D10;
    }

cseg_84FA0:;
    {
        swosWriteWord(a6TeamBase + TEAMDATA_OFF_AI_AFTER_TOUCH_STRENGTH, 2);
        if (!(d6 > 115600))
            swosWriteWord(a6TeamBase + TEAMDATA_OFF_AI_AFTER_TOUCH_STRENGTH, 1);
        // fall through into cseg_84FCA.
    }

cseg_84FCA:;
    {
        swosWriteWord(a6TeamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, (uint16_t)d7);
        swosWriteByte(a6TeamBase + TEAMDATA_OFF_NORMAL_FIRE, 1);
        int d2 = (d7 << 5) & 0xFFFF;
        int d2Lo = ((d2 & 0xFF) - (d5 & 0xFF)) & 0xFF;
        d2 = (d2 & 0xFF00) | d2Lo;
        swosWriteWord(ADDR_AI_resumePlayTimer, 15);
        int d0Spin = -1;
        int8_t d2Sign = (int8_t)d2Lo;
        if (d2Sign >= 0)
            goto cseg_85025;
        d0Spin = -d0Spin;

    cseg_85025:;
        {
            int ballDeltaY = swosBallSpriteDeltaY();
            int16_t ballY2 = swosBallSpriteYPixels();
            if (ballDeltaY < 0)
                goto cseg_8503F;
            if (ballY2 > 555)
                goto cseg_850E1;
            goto cseg_8504E;

        cseg_8503F:;
            if ((uint16_t)ballY2 < (uint16_t)342)
                goto cseg_850E1;
            // fall through into cseg_8504E.
        }

    cseg_8504E:;
        {
            int d1 = (gameTick & 0x1C) >> 2;
            int16_t ballX2 = swosBallSpriteXPixels();
            if ((uint16_t)ballX2 < (uint16_t)193)
                goto cseg_85080;
            if ((uint16_t)ballX2 < (uint16_t)478)
                goto cseg_85097;
            // fall through into cseg_85080.

        cseg_85080:;
            if ((uint16_t)ballX2 < (uint16_t)118)
                goto cseg_850C5;
            if (ballX2 > 553)
                goto cseg_850C5;
            goto cseg_850AE;

        cseg_85097:;
            if (d1 == 0)
                goto cseg_850CF;
            if ((uint16_t)d1 < (uint16_t)5)
                goto cseg_850E1;
            goto cseg_850DA;

        cseg_850AE:;
            if (d1 == 0)
                goto cseg_850DA;
            if ((uint16_t)d1 < (uint16_t)4)
                goto cseg_850CF;
            goto cseg_850E1;

        cseg_850C5:;
            if ((uint16_t)d1 < (uint16_t)4)
                goto cseg_850E1;
            // fall through into cseg_850CF.

        cseg_850CF:;
            d0Spin = 0;
            goto cseg_850E1;

        cseg_850DA:;
            d0Spin = (int16_t)(-d0Spin);
            // fall through into cseg_850E1.

        cseg_850E1:;
            swosWriteWord(a6TeamBase + TEAMDATA_OFF_AI_BALL_SPIN_DIRECTION, (uint16_t)(int16_t)d0Spin);
            return;
        }
    }

l_ball_after_touch_allowed:;
    {
        int16_t d1Raw = swosReadSignedWord(a6TeamBase + TEAMDATA_OFF_CONTROLLED_PL_DIRECTION);
        int d1 = d1Raw & 0xFFFF;

        int16_t pp = swosReadSignedWord(ADDR_playingPenalties);
        int16_t pen = swosReadSignedWord(ADDR_penalty);
        if (pp != 0 || pen != 0)
            goto l_no_ball_after_touch_local;

        if ((aiRand & 1) == 0)
            goto l_no_ball_after_touch_local;

        {
            int16_t spinDir = swosReadSignedWord(a6TeamBase + TEAMDATA_OFF_AI_BALL_SPIN_DIRECTION);
            if (spinDir < 0)
                goto l_apply_left_spin_local;
            if (spinDir > 0)
                goto l_apply_right_spin_local;
        }

    l_no_ball_after_touch_local:;
        {
            int16_t ats = swosReadSignedWord(a6TeamBase + TEAMDATA_OFF_AI_AFTER_TOUCH_STRENGTH);
            if (ats != 1)
                goto l_do_long_kick;
            swosWriteWord(a6TeamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, (uint16_t)(int16_t)-1);
            return;
        }

    l_do_long_kick:;
        {
            int a0Table = ADDR_AI_longKickTable;
            int16_t ats2 = swosReadSignedWord(a6TeamBase + TEAMDATA_OFF_AI_AFTER_TOUCH_STRENGTH);
            int byteOff = ats2 << 1;
            int16_t delta = swosReadSignedWord(a0Table + byteOff);
            int newD1 = (d1 + delta) & 7;
            swosWriteWord(a6TeamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, (uint16_t)newD1);
            return;
        }

    l_apply_left_spin_local:;
        {
            // Retail computes (D1-1)&7 but DISCARDS it, then adds the table
            // delta to the UNMODIFIED D1 -- dead-but-documented parity note
            // (dead in the asm too), preserved from the C#.
            int a0Table = ADDR_AI_leftSpinTable;
            int16_t ats2 = swosReadSignedWord(a6TeamBase + TEAMDATA_OFF_AI_AFTER_TOUCH_STRENGTH);
            int byteOff = ats2 << 1;
            int16_t delta = swosReadSignedWord(a0Table + byteOff);
            int newD1 = (d1 + delta) & 7;
            swosWriteWord(a6TeamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, (uint16_t)newD1);
            return;
        }

    l_apply_right_spin_local:;
        {
            // Same discarded-adjustment parity note as the left-spin arm.
            int a0Table = ADDR_AI_rotateRightTable;
            int16_t ats2 = swosReadSignedWord(a6TeamBase + TEAMDATA_OFF_AI_AFTER_TOUCH_STRENGTH);
            int byteOff = ats2 << 1;
            int16_t delta = swosReadSignedWord(a0Table + byteOff);
            int newD1 = (d1 + delta) & 7;
            swosWriteWord(a6TeamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, (uint16_t)newD1);
            return;
        }
    }
}
