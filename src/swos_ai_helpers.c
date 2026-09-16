// SOURCE: openswos game/scripts/Sim/Port/AiHelpers.cs (full file, step 9).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes,
// including the two dead-var-gated early-out checks in AI_Kick and every
// register-emulation quirk (e.g. reading a TeamData offset off a *player*
// sprite address in AI_Kick's angle-gate block) -- preserved verbatim, not
// "corrected", per the project's mechanical-fidelity rule.
#include "swos_ai_helpers.h"

#include <stdint.h>

#include "swos_addr.h"
#include "swos_ball_sprite.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"
#include "swos_team_data.h"

// ===========================================================================
// AI_Kick -- updatePlayers.cpp:19313
// ===========================================================================
void swosAiHelpersAiKick(int a1PlayerAddr, int a6TeamBase)
{
    // 19315-19321 -- ax = deadVarAlways0; if zero goto cseg_85B62. Since
    // deadVarAlways0 is always 0, the conditional jump fires every time and
    // the next two checks are dead. Kept faithfully.
    int16_t ax = swosReadSignedWord(ADDR_deadVarAlways0);
    if (ax == 0)
        goto cseg_85B62;

    {
        int eax = swosReadSignedDword(ADDR_dseg_1309C1);
        if (a6TeamBase == eax)
            return;
    }

    {
        int a0 = swosReadSignedDword(a6TeamBase + TEAMDATA_OFF_OPPONENTS_TEAM);
        int eax = swosReadSignedDword(ADDR_dseg_1309C1);
        if (a0 == eax)
            return;
    }

cseg_85B62:;
    // 19352-19371 -- distance/phase gate. D0 = currentGameTick & 6 is
    // computed but only the AND's flags matter in the asm -- the value is
    // never read again. Preserved as a discarded local for documentation.
    {
        int16_t tick = swosReadSignedWord(ADDR_currentGameTick);
        int d0 = tick & 6;
        (void)d0;
    }

    {
        int ballDistance = swosReadSignedDword(a1PlayerAddr + PLSPR_OFF_BALL_DISTANCE);
        if (ballDistance > 200)
            return;
    }

    int16_t d7 = swosReadSignedWord(a6TeamBase + TEAMDATA_OFF_ALLOWED_DIRECTIONS);

    int a0 = swosReadSignedDword(a6TeamBase + TEAMDATA_OFF_OPPONENTS_TEAM);
    int16_t oppPlayerHasBall = swosReadSignedWord(a0 + TEAMDATA_OFF_PLAYER_HAS_BALL);
    if (oppPlayerHasBall == 0)
        return;

    int oppControlledPlayer = swosReadSignedDword(a0 + TEAMDATA_OFF_CONTROLLED_PLAYER);
    if (oppControlledPlayer == 0)
        return;

    // NOTE (preserved verbatim): the asm reads a TeamGeneralInfo offset off
    // the opponent's controlled PLAYER sprite address, not off a TeamData
    // base -- a quirk in the original, kept as written (see file header).
    int16_t oppAllowedDirs = swosReadSignedWord(oppControlledPlayer + TEAMDATA_OFF_ALLOWED_DIRECTIONS);
    int d0 = (oppAllowedDirs << 5) & 0xFFFF;
    int16_t ourDir = swosReadSignedWord(a1PlayerAddr + PLSPR_OFF_DIRECTION);
    int d1 = (ourDir << 5) & 0xFFFF;
    int8_t d0Lo = (int8_t)((d0 & 0xFF) - (d1 & 0xFF));

    if (d0Lo < -32)
        goto l_kick;

    if (d0Lo <= 32)
        return;

l_kick:;
    swosWriteWord(a6TeamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, (uint16_t)d7);
    swosWriteByte(a6TeamBase + TEAMDATA_OFF_FIRE_PRESSED, 1);
    swosWriteByte(a6TeamBase + TEAMDATA_OFF_FIRE_THIS_FRAME, 1);
    swosWriteByte(a6TeamBase + TEAMDATA_OFF_NORMAL_FIRE, 1);
}

// ===========================================================================
// AI_SetDirectionTowardOpponentsGoal -- updatePlayers.cpp:19461
// ===========================================================================
void swosAiHelpersSetDirectionTowardOpponentsGoal(int a6TeamBase)
{
    int16_t aiCounter = swosReadSignedWord(ADDR_AI_counter);
    if (aiCounter == 0)
        return;

    int16_t d0w = swosBallSpriteXPixels();

    int16_t attackHalf = swosReadSignedWord(ADDR_AI_attackHalf);
    int d1;
    if (attackHalf == 1)
        goto l_attacking_top;

    if (a6TeamBase != TEAMDATA_TOP_BASE)
        return;

    d1 = 3;
    if ((uint16_t)d0w < 300)
        goto l_set_allowed_direction;

    d1 = 5;
    if ((uint16_t)d0w > 371)
        goto l_set_allowed_direction;

    d1 = 4;
    goto l_set_allowed_direction;

l_attacking_top:;
    if (a6TeamBase != TEAMDATA_BOTTOM_BASE)
        return;

    d1 = 1;
    if ((uint16_t)d0w < 300)
        goto l_set_allowed_direction;

    d1 = 7;
    if ((uint16_t)d0w > 371)
        goto l_set_allowed_direction;

    d1 = 0;

l_set_allowed_direction:;
    swosWriteWord(a6TeamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, (uint16_t)d1);
}

// ===========================================================================
// AI_DecideWhetherToTriggerFire -- updatePlayers.cpp:19577
// ===========================================================================
bool swosAiHelpersDecideWhetherToTriggerFire(int d7Direction, int a5PlayerAddr,
                                              int a6TeamBase)
{
    int16_t playerOrdinal = swosReadSignedWord(a5PlayerAddr + PLSPR_OFF_PLAYER_ORDINAL);
    if (playerOrdinal == 1)
        goto l_no_fire;

    // The "jnz" here is an inverted naming bug in the original -- when A6 IS
    // topTeamData we fall through (we are TOP); when A6 is NOT topTeamData
    // we jump (we are BOTTOM). Preserved verbatim, including the label name.
    if (a6TeamBase != TEAMDATA_TOP_BASE)
        goto l_we_are_top;

    // ---- TOP-team branch (attacking south): valid facing dirs 3, 4, 5 ----
    if (d7Direction == 3) goto l_facing_toward_opponents_goal;
    if (d7Direction == 4) goto l_facing_toward_opponents_goal;
    if (d7Direction == 5) goto l_facing_toward_opponents_goal;
    goto l_no_fire;

l_we_are_top:;
    // ---- BOTTOM-team branch (attacking north): valid facing dirs 7, 0, 1 --
    if (d7Direction == 7) goto l_facing_toward_opponents_goal;
    if (d7Direction == 0) goto l_facing_toward_opponents_goal;
    if (d7Direction != 1) goto l_no_fire;

l_facing_toward_opponents_goal:;
    {
        int ballDistance = swosReadSignedDword(a5PlayerAddr + PLSPR_OFF_BALL_DISTANCE);
        if (ballDistance > 648)
            goto l_no_fire;
    }

    {
        int deltaZ = swosBallSpriteDeltaZ();
        if (deltaZ < 0)
            goto l_ball_falling;

        // ---- Ball rising (deltaZ >= 0): accept Z window [8, 14] ----------
        int16_t ballZ = swosBallSpriteZPixels();
        if ((uint16_t)ballZ < 8)
            goto l_no_fire;
        if ((uint16_t)ballZ > 14)
            goto l_no_fire;

        goto l_trigger_joypad;

    l_ball_falling:;
        // ---- Ball falling (deltaZ < 0): accept Z window [12, 20] ---------
        ballZ = swosBallSpriteZPixels();
        if ((uint16_t)ballZ < 12)
            goto l_no_fire;
        if ((uint16_t)ballZ > 20)
            goto l_no_fire;
    }

l_trigger_joypad:;
    swosWriteByte(a6TeamBase + TEAMDATA_OFF_FIRE_THIS_FRAME, 1);

    // 19747-19766 -- currentAllowedDirection candidate #1.
    {
        int16_t fullDir = swosReadSignedWord(a5PlayerAddr + PLSPR_OFF_FULL_DIRECTION);
        int d0 = fullDir & 0xFFFF;
        int d0LoByte = (d0 & 0xFF) + 0x80;
        d0 = (d0 & ~0xFF) | (d0LoByte & 0xFF);
        d0 &= 0xFF;
        d0 >>= 5;
        swosWriteWord(a6TeamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, (uint16_t)d0);
    }

    // 19767-19789 -- recompute with +16 added (still byte-wise) and compare
    // against D7. If they don't match -> no_fire.
    {
        int16_t fullDir = swosReadSignedWord(a5PlayerAddr + PLSPR_OFF_FULL_DIRECTION);
        int d0b = fullDir & 0xFFFF;
        int d0bLoByte = (d0b & 0xFF) + 0x80;
        d0b = (d0b & ~0xFF) | (d0bLoByte & 0xFF);
        d0b = (d0b + 16) & 0xFFFF;
        d0b &= 0xFF;
        d0b >>= 5;

        int d7Cmp = d7Direction & 0xFFFF;
        if (d0b != d7Cmp)
            goto l_no_fire;

        swosWriteWord(a6TeamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, (uint16_t)d0b);
        swosWriteWord(ADDR_AI_counter, 15);
        swosWriteWord(ADDR_AI_counterWriteOnly, (uint16_t)d0b);
        swosWriteWord(ADDR_AI_attackHalf, 2);

        if (a6TeamBase == TEAMDATA_TOP_BASE)
            goto l_out_fire;
        swosWriteWord(ADDR_AI_attackHalf, 1);
    }

l_out_fire:;
    return true;

l_no_fire:;
    return false;
}

// ===========================================================================
// AI_ResumeGameDelay -- updatePlayers.cpp:19845
// ===========================================================================
bool swosAiHelpersResumeGameDelay(int a6TeamBase, int *outOpponentsTeam)
{
    *outOpponentsTeam = swosReadSignedDword(a6TeamBase + TEAMDATA_OFF_OPPONENTS_TEAM);

    int16_t passKickTimer = swosReadSignedWord(a6TeamBase + TEAMDATA_OFF_PASS_KICK_TIMER);
    bool carry = (uint16_t)passKickTimer < (uint16_t)13;
    return carry;
}

// ===========================================================================
// findClosestPlayerToBallFacing -- updatePlayers.cpp:19870
// ===========================================================================
int swosAiHelpersFindClosestPlayerToBallFacing(int d0FullDir, int a6TeamBase,
                                                int *outBallDistance)
{
    int a0 = -1;
    int d2 = -1;
    int d3 = 1;

    int d4 = 10;
    int a3 = TEAMDATA_TOP_BASE;
    int a2 = swosReadSignedDword(a3 + TEAMDATA_OFF_PLAYERS);

l_players_loop:;
    {
        int a1Sprite = swosReadSignedDword(a2);
        a2 += 4;

        int controlledPlayer = swosReadSignedDword(a6TeamBase + TEAMDATA_OFF_CONTROLLED_PLAYER);
        if (a1Sprite == controlledPlayer)
            goto l_next_player;

        {
            int16_t sentAway = swosReadSignedWord(a1Sprite + PLSPR_OFF_SENT_AWAY);
            if (sentAway != 0)
                goto l_next_player;
        }

        {
            uint8_t playerState = swosReadByte(a1Sprite + PLSPR_OFF_PLAYER_STATE);
            if (playerState != 0)
                goto l_next_player;
        }

        {
            int16_t fullDir = swosReadSignedWord(a1Sprite + PLSPR_OFF_FULL_DIRECTION);
            int d1 = fullDir & 0xFFFF;
            int8_t d1Lo = (int8_t)((d1 & 0xFF) - (d0FullDir & 0xFF));

            if (d1Lo < -16)
                goto l_next_player;
            if (d1Lo > 16)
                goto l_next_player;
        }

        {
            int d1Dist = swosReadSignedDword(a1Sprite + PLSPR_OFF_BALL_DISTANCE);
            if ((uint32_t)d1Dist >= (uint32_t)d2)
                goto l_next_player;

            d2 = d1Dist;
            a0 = a1Sprite;
        }
    }

l_next_player:;
    d4 = (int16_t)(d4 - 1);
    if (d4 >= 0)
        goto l_players_loop;

    d4 = 10;
    a3 = TEAMDATA_BOTTOM_BASE;
    a2 = swosReadSignedDword(a3 + TEAMDATA_OFF_PLAYERS);

    d3 = (int16_t)(d3 - 1);
    if (d3 >= 0)
        goto l_players_loop;

    *outBallDistance = d2;
    return a0;
}
