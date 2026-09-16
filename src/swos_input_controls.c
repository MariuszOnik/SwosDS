// SOURCE: openswos game/scripts/Sim/Port/InputControls.cs (full file, step 8).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
// See include/swos_input_controls.h for the dependency scan and omission
// rationale.
#include "swos_input_controls.h"

#include <assert.h>
#include <stdint.h>

#include "swos_addr.h"
#include "swos_ball_sprite.h"
#include "swos_bench.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"
#include "swos_team_data.h"

SwosInputControlsTelemetry g_swosIcTelemetry;

void swosInputControlsResetTelemetry(void)
{
    g_swosIcTelemetry = (SwosInputControlsTelemetry){0};
}

// ---- forward declarations (definition order matches the C# source) -------
static void updateControlledPlayer(bool top);
static void updatePlayerBeingPassedTo(bool top);
static void updatePlayerBeingPassedToStopped(int a6);
static int32_t filterOverlappedEvents(int player, int32_t events);
static void updateGameControls(int player, int32_t events);
static void updateTeamControlsInternal(bool top, int player, int32_t events);
static void updatePlayerFire(int player, int32_t events);

void swosInputControlsSetJoystickState(int teamIndex, int direction,
                                        bool fireDown, bool fireTriggered)
{
    int32_t events = swosDirectionToEvents((int16_t)direction);
    if (fireDown)
        events |= IC_EVENT_KICK;

    int addr = teamIndex == IC_PLAYER2 ? ADDR_ic_pl2Events : ADDR_ic_pl1Events;
    swosWriteDword(addr, (uint32_t)events);

    (void)fireTriggered; // reserved -- see header comment.
}

// gameControls.cpp:33-46 -- resetGameControls.
void swosResetGameControls(void)
{
    swosWriteDword(ADDR_teamSwitchCounter, 0);
    swosWriteByte(ADDR_ic_pl1LastFired, 0);
    swosWriteByte(ADDR_ic_pl2LastFired, 0);
    swosWriteDword(ADDR_ic_pl1FireCounter, 0);
    swosWriteDword(ADDR_ic_pl2FireCounter, 0);
    swosWriteDword(ADDR_ic_oldPl1Events, 0);
    swosWriteDword(ADDR_ic_oldPl2Events, 0);
    swosWriteDword(ADDR_ic_pl1LastVertical, 0);
    swosWriteDword(ADDR_ic_pl1LastHorizontal, 0);
    swosWriteDword(ADDR_ic_pl2LastVertical, 0);
    swosWriteDword(ADDR_ic_pl2LastHorizontal, 0);
}

// gameControls.cpp:48-56 -- updateFireBlocked.
bool swosUpdateFireBlocked(void)
{
    if (swosReadWord(ADDR_fireBlocked) != 0)
    {
        if (!swosIsAnyPlayerFiring())
            swosWriteWord(ADDR_fireBlocked, 0);
        return true;
    }
    return false;
}

// gameControls.cpp:58-62 -- selectTeamForUpdate.
bool swosSelectTeamForUpdate(void)
{
    int counter = swosReadSignedDword(ADDR_teamSwitchCounter) + 1;
    swosWriteDword(ADDR_teamSwitchCounter, (uint32_t)counter);
    return (counter & 1) != 0; // odd -> top team
}

// gameControls.cpp:67-90 -- updateTeamControls(team).
void swosUpdateTeamControls(bool top)
{
    updateControlledPlayer(top);
    updatePlayerBeingPassedTo(top);

    int teamBase = swosTeamDataBase(top);
    int16_t playerNumber = swosReadSignedWord(teamBase + TEAMDATA_OFF_PLAYER_NUMBER);

    if (playerNumber != 0)
    {
        int player = playerNumber == 2 ? IC_PLAYER2 : IC_PLAYER1;
        int32_t events = swosGetPlayerEvents(player);
        updateGameControls(player, events);
        updateTeamControlsInternal(top, player, events);
    }

    // gameControls.cpp:80-89 -- if (!team->resetControls) { if (inBench()) {...} }
    int16_t resetControls = swosReadSignedWord(teamBase + TEAMDATA_OFF_RESET_CONTROLS);
    if (resetControls == 0)
    {
        if (swosBenchInBench())
        {
            swosWriteWord(teamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, (uint16_t)IC_NO_DIRECTION);
            swosWriteByte(teamBase + TEAMDATA_OFF_QUICK_FIRE, 0);
            swosWriteByte(teamBase + TEAMDATA_OFF_NORMAL_FIRE, 0);
            swosWriteByte(teamBase + TEAMDATA_OFF_FIRE_PRESSED, 0);
            swosWriteByte(teamBase + TEAMDATA_OFF_FIRE_THIS_FRAME, 0);
            swosWriteWord(teamBase + TEAMDATA_OFF_FIRE_COUNTER, 0);
        }
    }
}

// gameControls.cpp:92-99 -- postUpdateTeamControls.
void swosPostUpdateTeamControls(bool top)
{
    int teamBase = swosTeamDataBase(top);
    int16_t headerOrTackle = swosReadSignedWord(teamBase + TEAMDATA_OFF_HEADER_OR_TACKLE);
    if (headerOrTackle != 0)
    {
        swosWriteWord(teamBase + TEAMDATA_OFF_HEADER_OR_TACKLE, 0);
        int16_t playerNumber = swosReadSignedWord(teamBase + TEAMDATA_OFF_PLAYER_NUMBER);
        int fireCounterAddr = playerNumber == 2 ? ADDR_ic_pl2FireCounter : ADDR_ic_pl1FireCounter;
        swosWriteDword(fireCounterAddr, 0);
    }
}

// gameControls.cpp:101-127 -- getPlayerEvents.
int32_t swosGetPlayerEvents(int player)
{
    assert(player == IC_PLAYER1 || player == IC_PLAYER2);
    int addr = player == IC_PLAYER2 ? ADDR_ic_pl2Events : ADDR_ic_pl1Events;
    int32_t events = swosReadSignedDword(addr);
    return filterOverlappedEvents(player, events);
}

// gameControls.cpp:129-133 -- isPlayerFiring.
bool swosIsPlayerFiring(int player)
{
    int32_t events = swosGetPlayerEvents(player);
    return (events & IC_EVENT_KICK) != 0;
}

// gameControls.cpp:135-161 -- getFireStartedAndBumpFireCounter.
bool swosGetFireStartedAndBumpFireCounter(bool currentFire, int player)
{
    int fireCounterAddr = player == IC_PLAYER1 ? ADDR_ic_pl1FireCounter : ADDR_ic_pl2FireCounter;
    int lastFiredAddr   = player == IC_PLAYER1 ? ADDR_ic_pl1LastFired   : ADDR_ic_pl2LastFired;

    int fireCounter = swosReadSignedDword(fireCounterAddr);
    bool lastFired = swosReadByte(lastFiredAddr) != 0;
    bool fireStartedThisFrame = false;

    if (lastFired)
    {
        if (currentFire)
        {
            if (fireCounter != 0)
                fireCounter--;
        }
        else
        {
            lastFired = false;
            fireCounter = -fireCounter;
        }
    }
    else
    {
        if (currentFire)
        {
            fireStartedThisFrame = true;
            lastFired = true;
            fireCounter = -1;
        }
        else
        {
            lastFired = false;
        }
    }

    swosWriteDword(fireCounterAddr, (uint32_t)fireCounter);
    swosWriteByte(lastFiredAddr, lastFired ? 1 : 0);
    return fireStartedThisFrame;
}

// gameControls.cpp:163-190 -- eventsToDirection.
int16_t swosEventsToDirection(int32_t events)
{
    int16_t direction = IC_NO_DIRECTION;

    bool left  = (events & IC_EVENT_LEFT)  != 0;
    bool right = (events & IC_EVENT_RIGHT) != 0;
    bool up    = (events & IC_EVENT_UP)    != 0;
    bool down  = (events & IC_EVENT_DOWN)  != 0;

    if      (up   && right) direction = IC_FACING_TOP_RIGHT;
    else if (down && right) direction = IC_FACING_BOTTOM_RIGHT;
    else if (down && left)  direction = IC_FACING_BOTTOM_LEFT;
    else if (up   && left)  direction = IC_FACING_TOP_LEFT;
    else if (up)            direction = IC_FACING_TOP;
    else if (right)         direction = IC_FACING_RIGHT;
    else if (down)          direction = IC_FACING_BOTTOM;
    else if (left)          direction = IC_FACING_LEFT;

    return direction;
}

// gameControls.cpp:192-216 -- directionToEvents.
int32_t swosDirectionToEvents(int16_t direction)
{
    switch (direction)
    {
        case IC_FACING_TOP:          return IC_EVENT_UP;
        case IC_FACING_TOP_RIGHT:    return IC_EVENT_UP | IC_EVENT_RIGHT;
        case IC_FACING_RIGHT:        return IC_EVENT_RIGHT;
        case IC_FACING_BOTTOM_RIGHT: return IC_EVENT_DOWN | IC_EVENT_RIGHT;
        case IC_FACING_BOTTOM:       return IC_EVENT_DOWN;
        case IC_FACING_BOTTOM_LEFT:  return IC_EVENT_DOWN | IC_EVENT_LEFT;
        case IC_FACING_LEFT:         return IC_EVENT_LEFT;
        case IC_FACING_TOP_LEFT:     return IC_EVENT_UP | IC_EVENT_LEFT;
        default:
            // Original asserts; in production it falls through to kNoDirection.
            assert(direction == IC_NO_DIRECTION);
            return IC_EVENT_NONE;
    }
}

// gameControls.cpp:218-221 -- isAnyPlayerFiring.
bool swosIsAnyPlayerFiring(void)
{
    return ((swosGetPlayerEvents(IC_PLAYER1) | swosGetPlayerEvents(IC_PLAYER2)) & IC_EVENT_KICK) != 0;
}

// ----- File-static helpers (gameControls.cpp:225-332) ----------------------

// gameControls.cpp:225-258 -- filterOverlappedEvents.
static int32_t filterOverlappedEvents(int player, int32_t events)
{
    int oldEventsAddr     = player == IC_PLAYER1 ? ADDR_ic_oldPl1Events      : ADDR_ic_oldPl2Events;
    int forceVerticalAddr = player == IC_PLAYER1 ? ADDR_ic_pl1LastVertical   : ADDR_ic_pl2LastVertical;
    int forceHorizAddr    = player == IC_PLAYER1 ? ADDR_ic_pl1LastHorizontal : ADDR_ic_pl2LastHorizontal;

    int32_t oldEvents       = swosReadSignedDword(oldEventsAddr);
    int32_t forceVertical   = swosReadSignedDword(forceVerticalAddr);
    int32_t forceHorizontal = swosReadSignedDword(forceHorizAddr);

    // Vertical axis.
    if ((events & IC_EVENT_UP) != 0 && (events & IC_EVENT_DOWN) != 0)
    {
        events &= ~(IC_EVENT_UP | IC_EVENT_DOWN);
        if (forceVertical == IC_EVENT_NONE)
        {
            forceVertical = (oldEvents & IC_EVENT_UP) != 0 ? IC_EVENT_DOWN : IC_EVENT_UP;
        }
        events |= forceVertical;
    }
    else
    {
        forceVertical = IC_EVENT_NONE;
    }

    // Horizontal axis.
    if ((events & IC_EVENT_LEFT) != 0 && (events & IC_EVENT_RIGHT) != 0)
    {
        events &= ~(IC_EVENT_LEFT | IC_EVENT_RIGHT);
        if (forceHorizontal == IC_EVENT_NONE)
        {
            forceHorizontal = (oldEvents & IC_EVENT_LEFT) != 0 ? IC_EVENT_RIGHT : IC_EVENT_LEFT;
        }
        events |= forceHorizontal;
    }
    else
    {
        forceHorizontal = IC_EVENT_NONE;
    }

    // gameControls.cpp:257 -- `return oldEvents = events;` (assign + return).
    swosWriteDword(oldEventsAddr, (uint32_t)events);
    swosWriteDword(forceVerticalAddr, (uint32_t)forceVertical);
    swosWriteDword(forceHorizAddr, (uint32_t)forceHorizontal);
    return events;
}

// gameControls.cpp:260-277 -- updateGameControls (file-static).
static void updateGameControls(int player, int32_t events)
{
    updatePlayerFire(player, events);

    if ((events & IC_EVENT_REPLAY) != 0)
        swosInputControlsRequestFadeAndInstantReplay();

    if ((events & IC_EVENT_SAVE_HIGHLIGHT) != 0)
        swosInputControlsRequestFadeAndSaveReplay();

    // gameControls.cpp:270-275 -- zoomIn()/zoomOut() (camera.cpp). The
    // original bodies are empty (`/* TODO */` in the C# stand-in) -- a
    // genuine no-op ported as a no-op, not an omission.
    if ((events & IC_EVENT_ZOOM_IN) != 0)
        { /* camera.cpp zoomIn() -- no-op, see header comment */ }

    if ((events & IC_EVENT_ZOOM_OUT) != 0)
        { /* camera.cpp zoomOut() -- no-op, see header comment */ }
}

// gameControls.cpp:291-326 -- updateTeamControls(team, player, events) (file-static).
static void updateTeamControlsInternal(bool top, int player, int32_t events)
{
    int teamBase = swosTeamDataBase(top);
    assert(swosReadSignedWord(teamBase + TEAMDATA_OFF_PLAYER_NUMBER) != 0);

    int16_t direction = swosEventsToDirection(events);
    bool fire = (events & IC_EVENT_KICK) != 0;

    swosWriteWord(teamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, (uint16_t)direction);
    swosWriteWord(teamBase + TEAMDATA_OFF_DIRECTION,                 (uint16_t)direction);
    swosWriteByte(teamBase + TEAMDATA_OFF_FIRE_THIS_FRAME,
        swosGetFireStartedAndBumpFireCounter(fire, player) ? 1 : 0);
    swosWriteByte(teamBase + TEAMDATA_OFF_SECONDARY_FIRE,
        (events & IC_EVENT_BENCH) != 0 ? 1 : 0);

    // gameControls.cpp:303-306 -- firePressed = (fire ? -1 : 0). If fire
    // pressed, bump fireCounter; otherwise reset it.
    int firePressed = fire ? -1 : 0;
    swosWriteByte(teamBase + TEAMDATA_OFF_FIRE_PRESSED, firePressed);
    if (firePressed != 0)
    {
        int16_t fc = swosReadSignedWord(teamBase + TEAMDATA_OFF_FIRE_COUNTER);
        swosWriteWord(teamBase + TEAMDATA_OFF_FIRE_COUNTER, (uint16_t)(int16_t)(fc + 1));
    }
    else
    {
        swosWriteWord(teamBase + TEAMDATA_OFF_FIRE_COUNTER, 0);
    }

    // gameControls.cpp:308-309 -- clear quick/normal fire flags before redecide.
    swosWriteByte(teamBase + TEAMDATA_OFF_QUICK_FIRE, 0);
    swosWriteByte(teamBase + TEAMDATA_OFF_NORMAL_FIRE, 0);

    int fireCounterAddr = player == IC_PLAYER1 ? ADDR_ic_pl1FireCounter : ADDR_ic_pl2FireCounter;
    int fireCounter = swosReadSignedDword(fireCounterAddr);

    // gameControls.cpp:313-325 -- quick / normal fire latching.
    if (fireCounter < 0)
    {
        if (fireCounter < -4)
        {
            swosWriteByte(teamBase + TEAMDATA_OFF_NORMAL_FIRE, -1);
            fireCounter = 0;
        }
    }
    else if (fireCounter > 0)
    {
        if (fireCounter > 4)
            swosWriteByte(teamBase + TEAMDATA_OFF_NORMAL_FIRE, -1);
        else
            swosWriteByte(teamBase + TEAMDATA_OFF_QUICK_FIRE, -1);
        fireCounter = 0;
    }
    swosWriteDword(fireCounterAddr, (uint32_t)fireCounter);
}

// gameControls.cpp:328-332 -- updatePlayerFire (file-static).
static void updatePlayerFire(int player, int32_t events)
{
    int fireAddr = player == IC_PLAYER1 ? ADDR_ic_pl1Fire : ADDR_ic_pl2Fire;
    int value = (events & IC_EVENT_KICK) != 0 ? -1 : 0;
    swosWriteWord(fireAddr, (uint16_t)value);
}

// ===========================================================================
// updateControlledPlayer -- swos.asm:100851-101034
// ===========================================================================
static void updateControlledPlayer(bool top)
{
    int a6 = swosTeamDataBase(top);

    int16_t d3BallX = swosBallSpriteXPixels();
    int16_t d4BallY = swosBallSpriteYPixels();

    int a1Table = swosReadSignedDword(a6 + TEAMDATA_OFF_PLAYERS);
    int a3Closest = 0;
    uint32_t d5BestDist = 0xFFFFFFFFu;
    int d6 = 10;

    while (1)
    {
        int a2Sprite = swosReadSignedDword(a1Table);
        a1Table += 4;

        int16_t pX = swosReadSignedWord(a2Sprite + PLSPR_OFF_X + 2);
        int16_t pY = swosReadSignedWord(a2Sprite + PLSPR_OFF_Y + 2);
        int d1 = (int16_t)(pX - d3BallX);
        int d2 = (int16_t)(pY - d4BallY);

        int d1sq = d1 * d1;
        int d2sq = d2 * d2;
        int d1Dist = d1sq + d2sq;

        swosWriteDword(a2Sprite + PLSPR_OFF_BALL_DISTANCE, (uint32_t)d1Dist);

        int16_t gameStatePl = swosReadSignedWord(ADDR_gameStatePl);
        if (gameStatePl == 100)
        {
            int16_t onScreen = swosReadSignedWord(a2Sprite + PLSPR_OFF_ON_SCREEN);
            if (onScreen == 0)
                goto l_next;
        }

        {
            int16_t sentAway = swosReadSignedWord(a2Sprite + PLSPR_OFF_SENT_AWAY);
            if (sentAway != 0)
                goto l_next;
        }

        {
            int16_t goaliePlayingOrOut = swosReadSignedWord(a6 + TEAMDATA_OFF_GOALIE_PLAYING_OR_OUT);
            if (goaliePlayingOrOut == 0)
            {
                int16_t ordinal = swosReadSignedWord(a2Sprite + PLSPR_OFF_PLAYER_ORDINAL);
                if (ordinal == 1)
                    goto l_next;
            }
        }

        {
            int passingKickingPlayer = swosReadSignedDword(a6 + TEAMDATA_OFF_PASSING_KICKING_PLAYER);
            if (a2Sprite == passingKickingPlayer)
                goto l_next;
        }

        {
            uint8_t plState = swosReadByte(a2Sprite + PLSPR_OFF_PLAYER_STATE);
            if (plState == 1) goto l_next;   // PL_TACKLING
            if (plState == 3) goto l_next;   // PL_TACKLED
            if (plState == 9) goto l_next;   // PL_JUMP_HEADING
            if (plState == 8) goto l_next;   // PL_STATIC_HEADING
            if (plState == 13) goto l_next;  // PL_ROLLING_INJURED
        }

        {
            int passToPlayerPtr = swosReadSignedDword(a6 + TEAMDATA_OFF_PASS_TO_PLAYER_PTR);
            if (a2Sprite == passToPlayerPtr)
                goto l_next;
        }

        if ((uint32_t)d1Dist >= d5BestDist)
            goto l_next;

        d5BestDist = (uint32_t)d1Dist;
        a3Closest = a2Sprite;

    l_next:
        d6--;
        if (d6 < 0)
            break;
    }

    int16_t playingPenalties = swosReadSignedWord(ADDR_playingPenalties);
    if (playingPenalties != 0)
    {
        int16_t gameStatePl2 = swosReadSignedWord(ADDR_gameStatePl);
        if (gameStatePl2 == 100)
            return;
    }

    if (a3Closest == 0)
        return;

    int16_t ballOutOfPlay = swosReadSignedWord(a6 + TEAMDATA_OFF_BALL_OUT_OF_PLAY);
    if (ballOutOfPlay == 0)
        return;

    int currentControlled = swosReadSignedDword(a6 + TEAMDATA_OFF_CONTROLLED_PLAYER);
    if (a3Closest != currentControlled)
    {
        int oldCtrl = currentControlled;
        if (oldCtrl != 0)
        {
            uint8_t oldState = swosReadByte(oldCtrl + PLSPR_OFF_PLAYER_STATE);
            if (oldState == 0)
            {
                int16_t oldX = swosReadSignedWord(oldCtrl + PLSPR_OFF_X + 2);
                int16_t oldY = swosReadSignedWord(oldCtrl + PLSPR_OFF_Y + 2);
                swosWriteWord(oldCtrl + PLSPR_OFF_DEST_X, (uint16_t)oldX);
                swosWriteWord(oldCtrl + PLSPR_OFF_DEST_Y, (uint16_t)oldY);
            }
        }
        // Diagnostic -- count the ballOutOfPlay-gated swap (real port-only
        // telemetry, see header comment).
        int16_t pnOop = swosReadSignedWord(a6 + TEAMDATA_OFF_PLAYER_NUMBER);
        if (pnOop == 0)
        {
            if (top) g_swosIcTelemetry.ctrlSwapAiTop++;
            else     g_swosIcTelemetry.ctrlSwapAiBot++;
        }
        else
        {
            if (top) g_swosIcTelemetry.ctrlSwapHumanTop++;
            else     g_swosIcTelemetry.ctrlSwapHumanBot++;
        }
    }

    swosWriteDword(a6 + TEAMDATA_OFF_CONTROLLED_PLAYER, (uint32_t)a3Closest);

    int passToPlayer = swosReadSignedDword(a6 + TEAMDATA_OFF_PASS_TO_PLAYER_PTR);
    if (a3Closest != passToPlayer)
        return;

    int passTarget = passToPlayer;
    if (passTarget != 0)
    {
        uint8_t tgtState = swosReadByte(passTarget + PLSPR_OFF_PLAYER_STATE);
        if (tgtState == 0)
        {
            int16_t tgtX = swosReadSignedWord(passTarget + PLSPR_OFF_X + 2);
            int16_t tgtY = swosReadSignedWord(passTarget + PLSPR_OFF_Y + 2);
            swosWriteWord(passTarget + PLSPR_OFF_DEST_X, (uint16_t)tgtX);
            swosWriteWord(passTarget + PLSPR_OFF_DEST_Y, (uint16_t)tgtY);
        }
    }

    swosWriteDword(a6 + TEAMDATA_OFF_PASS_TO_PLAYER_PTR, 0);
}

// ===========================================================================
// updatePlayerBeingPassedTo -- swos.asm:101045-101321
// ===========================================================================
static void updatePlayerBeingPassedTo(bool top)
{
    int a6 = swosTeamDataBase(top);

    int16_t gameStatePl = swosReadSignedWord(ADDR_gameStatePl);
    if (gameStatePl != 100) // ST_GAME_IN_PROGRESS
    {
        updatePlayerBeingPassedToStopped(a6);
        return;
    }

    // ---- GAME_IN_PROGRESS path (101049-101201) -----------------------------
    int16_t ballX = swosBallSpriteXPixels();
    int16_t ballY = swosBallSpriteYPixels();
    swosWriteWord(a6 + TEAMDATA_OFF_BALL_X, (uint16_t)ballX);
    swosWriteWord(a6 + TEAMDATA_OFF_BALL_Y, (uint16_t)ballY);

    int16_t ballInPlay = swosReadSignedWord(a6 + TEAMDATA_OFF_BALL_IN_PLAY);
    if (ballInPlay == 0)
        return;

    int16_t playerSwitchTimer = swosReadSignedWord(a6 + TEAMDATA_OFF_PLAYER_SWITCH_TIMER);
    if (playerSwitchTimer != 0)
        return;

    int16_t passingToPlayer = swosReadSignedWord(a6 + TEAMDATA_OFF_PASSING_TO_PLAYER);
    if (passingToPlayer != 0)
    {
        int existing = swosReadSignedDword(a6 + TEAMDATA_OFF_PASS_TO_PLAYER_PTR);
        if (existing != 0)
            return;
    }

    // 101075-101085 -- penalties branch. Penalty shooter sprite isn't wired
    // in our port yet (same disposition as UpdatePlayers.cs's own penalty
    // branches) -- the equivalent asm path always jumps to @@out here.
    int16_t playingPenalties = swosReadSignedWord(ADDR_playingPenalties);
    if (playingPenalties != 0)
    {
        return;
    }

    int a1Table = swosReadSignedDword(a6 + TEAMDATA_OFF_PLAYERS);
    int a3Closest = 0;
    uint32_t d5BestDist = 0xFFFFFFFFu;
    int d6 = 10;

    while (1)
    {
        int a2Sprite = swosReadSignedDword(a1Table);
        a1Table += 4;

        int16_t onScreen = swosReadSignedWord(a2Sprite + PLSPR_OFF_ON_SCREEN);
        if (onScreen == 0)
            goto l_next;

        {
            int16_t sentAway = swosReadSignedWord(a2Sprite + PLSPR_OFF_SENT_AWAY);
            if (sentAway != 0)
                goto l_next;
        }

        {
            int16_t ballOutOfPlayOrKeeper = swosReadSignedWord(a6 + TEAMDATA_OFF_BALL_OUT_OF_PLAY_OR_KEEPER);
            if (ballOutOfPlayOrKeeper == 0)
            {
                int16_t ordinal = swosReadSignedWord(a2Sprite + PLSPR_OFF_PLAYER_ORDINAL);
                if (ordinal == 1)
                    goto l_next;
            }
        }

        {
            int controlledPlayer = swosReadSignedDword(a6 + TEAMDATA_OFF_CONTROLLED_PLAYER);
            if (a2Sprite == controlledPlayer)
                goto l_next;
        }

        {
            int passingKickingPlayer = swosReadSignedDword(a6 + TEAMDATA_OFF_PASSING_KICKING_PLAYER);
            if (a2Sprite == passingKickingPlayer)
                goto l_next;
        }

        {
            uint8_t plState = swosReadByte(a2Sprite + PLSPR_OFF_PLAYER_STATE);
            if (plState != 0)
                goto l_next;
        }

        {
            int d1Dist = swosReadSignedDword(a2Sprite + PLSPR_OFF_BALL_DISTANCE);
            if ((uint32_t)d1Dist >= d5BestDist)
                goto l_next;
            d5BestDist = (uint32_t)d1Dist;
            a3Closest = a2Sprite;
        }

    l_next:
        d6--;
        if (d6 < 0)
            break;
    }

    if (a3Closest == 0)
        return;

    int currentPassTo = swosReadSignedDword(a6 + TEAMDATA_OFF_PASS_TO_PLAYER_PTR);
    if (a3Closest != currentPassTo)
    {
        int prev = currentPassTo;
        if (prev != 0)
        {
            uint8_t prevState = swosReadByte(prev + PLSPR_OFF_PLAYER_STATE);
            if (prevState == 0)
            {
                int16_t prevX = swosReadSignedWord(prev + PLSPR_OFF_X + 2);
                int16_t prevY = swosReadSignedWord(prev + PLSPR_OFF_Y + 2);
                swosWriteWord(prev + PLSPR_OFF_DEST_X, (uint16_t)prevX);
                swosWriteWord(prev + PLSPR_OFF_DEST_Y, (uint16_t)prevY);
            }
        }
    }

    swosWriteDword(a6 + TEAMDATA_OFF_PASS_TO_PLAYER_PTR, (uint32_t)a3Closest);
}

// swos.asm:101204-101321 -- @@game_stopped path.
static void updatePlayerBeingPassedToStopped(int a6)
{
    int16_t ballX = swosBallSpriteXPixels();
    int16_t ballY = swosBallSpriteYPixels();
    swosWriteWord(a6 + TEAMDATA_OFF_BALL_X, (uint16_t)ballX);
    swosWriteWord(a6 + TEAMDATA_OFF_BALL_Y, (uint16_t)ballY);

    int16_t ballInPlay = swosReadSignedWord(a6 + TEAMDATA_OFF_BALL_IN_PLAY);
    if (ballInPlay == 0)
        return;

    int16_t pst = swosReadSignedWord(a6 + TEAMDATA_OFF_PLAYER_SWITCH_TIMER);
    if (pst != 0)
        return;

    int16_t passingToPlayer = swosReadSignedWord(a6 + TEAMDATA_OFF_PASSING_TO_PLAYER);
    if (passingToPlayer != 0)
    {
        int existing = swosReadSignedDword(a6 + TEAMDATA_OFF_PASS_TO_PLAYER_PTR);
        if (existing != 0)
            return;
    }

    int16_t playingPenalties = swosReadSignedWord(ADDR_playingPenalties);
    if (playingPenalties != 0)
    {
        return;
    }

    int a1Table = swosReadSignedDword(a6 + TEAMDATA_OFF_PLAYERS);
    int a3Closest = 0;
    uint32_t d5BestDist = 0xFFFFFFFFu;
    int d6 = 10;

    while (1)
    {
        int a2Sprite = swosReadSignedDword(a1Table);
        a1Table += 4;

        {
            int16_t sentAway = swosReadSignedWord(a2Sprite + PLSPR_OFF_SENT_AWAY);
            if (sentAway != 0)
                goto l_next;
        }

        {
            int16_t ballOutOfPlayOrKeeper = swosReadSignedWord(a6 + TEAMDATA_OFF_BALL_OUT_OF_PLAY_OR_KEEPER);
            if (ballOutOfPlayOrKeeper == 0)
            {
                int16_t ordinal = swosReadSignedWord(a2Sprite + PLSPR_OFF_PLAYER_ORDINAL);
                if (ordinal == 1)
                    goto l_next;
            }
        }

        {
            int controlledPlayer = swosReadSignedDword(a6 + TEAMDATA_OFF_CONTROLLED_PLAYER);
            if (a2Sprite == controlledPlayer)
                goto l_next;
            int passingKickingPlayer = swosReadSignedDword(a6 + TEAMDATA_OFF_PASSING_KICKING_PLAYER);
            if (a2Sprite == passingKickingPlayer)
                goto l_next;
        }

        {
            uint8_t plState = swosReadByte(a2Sprite + PLSPR_OFF_PLAYER_STATE);
            if (plState != 0)
                goto l_next;
        }

        {
            int d1Dist = swosReadSignedDword(a2Sprite + PLSPR_OFF_BALL_DISTANCE);
            if ((uint32_t)d1Dist >= d5BestDist)
                goto l_next;
            d5BestDist = (uint32_t)d1Dist;
            a3Closest = a2Sprite;
        }

    l_next:
        d6--;
        if (d6 < 0)
            break;
    }

    if (a3Closest == 0)
        return;

    // cseg_735CE (101313-101316) -- commit without stopping previous.
    swosWriteDword(a6 + TEAMDATA_OFF_PASS_TO_PLAYER_PTR, (uint32_t)a3Closest);
}

// gameLoop.cpp:155-158 -- requestFadeAndInstantReplay.
void swosInputControlsRequestFadeAndInstantReplay(void)
{
    swosWriteWord(ADDR_m_fadeAndInstantReplay, 1);
}

// gameLoop.cpp:150-153 -- requestFadeAndSaveReplay.
void swosInputControlsRequestFadeAndSaveReplay(void)
{
    swosWriteWord(ADDR_m_fadeAndSaveReplay, 1);
}
