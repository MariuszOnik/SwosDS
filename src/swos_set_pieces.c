// SOURCE: openswos game/scripts/Sim/Port/SetPieces.cs (see swos_set_pieces.h
// for the boundary this closes).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes. Labels
// and goto preserved exactly where the C# used them.
#include "swos_set_pieces.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "swos_addr.h"
#include "swos_ai_brain.h"
#include "swos_ball_sprite.h"
#include "swos_game_time.h"
#include "swos_memory.h"
#include "swos_player_actions.h"
#include "swos_player_sprite.h"
#include "swos_team_data.h"

// ---- GameState enum constants (mirror swos.h) ------------------------------
#define ST_PLAYERS_TO_INITIAL_POSITIONS 0
#define ST_GOAL_OUT_LEFT                1
#define ST_GOAL_OUT_RIGHT               2
#define ST_KEEPER_HOLDS_BALL            3
#define ST_CORNER_LEFT                  4
#define ST_CORNER_RIGHT                 5
#define ST_FREE_KICK_LEFT1              6
#define ST_FREE_KICK_RIGHT3             12
#define ST_FOUL                         13
#define ST_PENALTY                      14
#define ST_THROW_IN_FORWARD_RIGHT       15
#define ST_THROW_IN_CENTER_RIGHT        16
#define ST_THROW_IN_BACK_RIGHT          17
#define ST_THROW_IN_FORWARD_LEFT        18
#define ST_THROW_IN_CENTER_LEFT         19
#define ST_THROW_IN_BACK_LEFT           20
#define ST_PENALTIES                    31
#define ST_GAME_IN_PROGRESS             100
#define ST_STOPPED                      101
#define ST_WAITING_ON_PLAYER            102

// PlayerState enum (sprites/Sprite.h:16-34).
#define PL_NORMAL   0
#define PL_THROW_IN 5

// player.cpp:24 -- kPlayerSpeedsGameStopped[1] (see SetPieces.cs comment).
#define K_PLAYER_SPEEDS_GAME_STOPPED_1 1152

// 4-pixel "stand behind the ball" offset table indexed by SWOS direction
// 0..7 (0=N, 1=NE, ... 7=NW). See SetPieces.cs's header comment.
static const int16_t kKickerWalkupOffsetX[8] = { 0, -3, -4, -3,  0, +3, +4, +3 };
static const int16_t kKickerWalkupOffsetY[8] = { +4, +3,  0, -3, -4, -3,  0, +3 };

// Forward declarations -- real implementations, wired into the hook globals
// below (static initialisers run before main()/any test harness code).
static void setThrowInPlayerDestinationCoordinatesImpl(int throwerSpriteAddr);
static void tickThrowInImpl(int a1PlayerAddr, int a2BallAddr, int a6TeamBase);

SwosSetThrowInPlayerDestHook g_swosSetThrowInPlayerDestHook = setThrowInPlayerDestinationCoordinatesImpl;
SwosTickThrowInHook g_swosTickThrowInHook = tickThrowInImpl;

void swosSetPiecesSetThrowInPlayerDestinationCoordinates(int spriteAddr)
{
    assert(g_swosSetThrowInPlayerDestHook != NULL);
    if (g_swosSetThrowInPlayerDestHook)
        g_swosSetThrowInPlayerDestHook(spriteAddr);
}

void swosSetPiecesTickThrowIn(int throwerSpriteAddr, int ballSpriteAddr,
                               int teamBase)
{
    assert(g_swosTickThrowInHook != NULL);
    if (g_swosTickThrowInHook)
        g_swosTickThrowInHook(throwerSpriteAddr, ballSpriteAddr, teamBase);
}

// ===================================================================
// setThrowInPlayerDestinationCoordinates -- updatePlayers.cpp:15322-15374
// ===================================================================
static void setThrowInPlayerDestinationCoordinatesImpl(int throwerSpriteAddr)
{
    int A0 = BALLSPR_BASE;
    int esi = A0;
    uint16_t ax = (uint16_t)swosReadSignedWord(esi + 32);   // Sprite.x + 2 -> high word
    uint16_t D1 = ax;

    bool carry = D1 < 336;
    if (carry)
        goto l_left_half;

    D1 = (uint16_t)(D1 + 3);
    goto l_set_player_x;

l_left_half:
    D1 = (uint16_t)(D1 - 3);

l_set_player_x:
    ax = D1;
    esi = throwerSpriteAddr;
    swosWriteWord(esi + 32, ax);
    swosWriteWord(esi + 58, ax);

    esi = A0;
    ax = (uint16_t)swosReadSignedWord(esi + 36);
    esi = throwerSpriteAddr;
    swosWriteWord(esi + 36, ax);

    esi = A0;
    ax = (uint16_t)swosReadSignedWord(esi + 36);
    esi = throwerSpriteAddr;
    swosWriteWord(esi + 60, ax);
}

// Forward declarations for the resolvers (used by TickSetPieces).
static void resolveCornerKick(int16_t gameState);
static void resolveGoalKick(int16_t gameState);
static void resolveThrowIn(int16_t gameState);
static void resolveFreeKick(int16_t gameState);
static void resolvePenaltyShootout(void);
static int findNearestOutfielderToPoint(int teamBase, int px, int py);
static int findKeeperSprite(int teamBase);

// ===================================================================
// DispatchByGameState
// ===================================================================
void swosSetPiecesDispatchByGameState(int a1PlayerAddr, int a2BallAddr,
                                       int a5PlayerAddr, int a6TeamBase)
{
    int16_t gameState = swosReadSignedWord(ADDR_gameState);

    if (gameState >= ST_THROW_IN_FORWARD_RIGHT && gameState <= ST_THROW_IN_BACK_LEFT)
    {
        swosSetPiecesTickThrowIn(a1PlayerAddr, a2BallAddr, a6TeamBase);
        return;
    }

    if (gameState == ST_FOUL)
    {
        swosSetPiecesTickFreeKick(a6TeamBase);
        return;
    }

    if (gameState >= ST_FREE_KICK_LEFT1 && gameState <= ST_FREE_KICK_RIGHT3)
    {
        swosSetPiecesTickFreeKick(a6TeamBase);
        return;
    }

    if (gameState == ST_PENALTY || gameState == ST_PENALTIES)
    {
        swosSetPiecesTickPenalty(a5PlayerAddr, a6TeamBase);
        return;
    }

    if (gameState == ST_CORNER_LEFT || gameState == ST_CORNER_RIGHT)
    {
        swosSetPiecesTickCorner(a6TeamBase);
        return;
    }

    if (gameState == ST_GOAL_OUT_LEFT || gameState == ST_GOAL_OUT_RIGHT)
    {
        swosSetPiecesTickGoalKick(a6TeamBase);
        return;
    }

    // ST_KEEPER_HOLDS_BALL / ST_PLAYERS_TO_INITIAL_POSITIONS / others -- no
    // per-tick set-piece action needed at this layer.
    (void)ST_PLAYERS_TO_INITIAL_POSITIONS;
    (void)ST_KEEPER_HOLDS_BALL;
}

// ===================================================================
// TickThrowIn -- updatePlayers.cpp:5780-6373 (`l_player_taking_throw_in`)
// ===================================================================
static void tickThrowInImpl(int a1PlayerAddr, int a2BallAddr, int a6TeamBase)
{
    int A1 = a1PlayerAddr;
    int A2 = a2BallAddr;
    int A6 = a6TeamBase;
    int A0 = 0;
    int16_t D0 = 0;
    int esi;
    uint16_t ax;
    uint8_t al, cl;
    int32_t eax;
    bool zeroF;

    // updatePlayers.cpp:5781-5788.
    esi = A6;
    ax = (uint16_t)swosReadSignedWord(esi + 4);            // TeamData.OffPlayerNumber
    zeroF = ax == 0;
    if (!zeroF)
        goto l_test_allowed_turn_flags;

    // updatePlayers.cpp:5790-5796 -- AI thrower.
    swosAiBrainSetControlsDirection(A6);

l_test_allowed_turn_flags:
    esi = A1;
    ax = (uint16_t)swosReadSignedWord(esi + PLSPR_OFF_DIRECTION);
    D0 = (int16_t)ax;
    cl = (uint8_t)D0;
    ax = 1;
    if ((cl & 0x1F) != 0)
        ax = (uint16_t)(ax << (cl & 0x1F));
    {
        uint8_t turnFlags = swosReadByte(ADDR_playerTurnFlags);
        uint8_t testRes = (uint8_t)(turnFlags & (uint8_t)ax);
        zeroF = testRes == 0;
    }
    if (!zeroF)
        goto l_test_turn_flags_with_camera_direction;

    ax = (uint16_t)swosReadSignedWord(ADDR_cameraDirection);
    swosWriteWord(esi + PLSPR_OFF_DIRECTION, ax);
    {
        int16_t v = swosReadSignedWord(ADDR_disallowedTurnFlagsCounter);
        swosWriteWord(ADDR_disallowedTurnFlagsCounter, (uint16_t)(v + 1));
    }

l_test_turn_flags_with_camera_direction:
    esi = A1;
    ax = (uint16_t)swosReadSignedWord(esi + PLSPR_OFF_DIRECTION);
    D0 = (int16_t)ax;
    cl = (uint8_t)D0;
    ax = 1;
    if ((cl & 0x1F) != 0)
        ax = (uint16_t)(ax << (cl & 0x1F));
    {
        uint8_t turnFlags = swosReadByte(ADDR_playerTurnFlags);
        uint8_t testRes = (uint8_t)(turnFlags & (uint8_t)ax);
        zeroF = testRes == 0;
    }
    if (!zeroF)
        goto l_check_if_throw_in_taker_substituted;

    D0 = 7;

l_next_direction:
    cl = (uint8_t)D0;
    ax = 1;
    if ((cl & 0x1F) != 0)
        ax = (uint16_t)(ax << (cl & 0x1F));
    {
        uint8_t turnFlags = swosReadByte(ADDR_playerTurnFlags);
        uint8_t testRes = (uint8_t)(turnFlags & (uint8_t)ax);
        zeroF = testRes == 0;
    }
    if (!zeroF)
        goto l_found_direction;

    D0 = (int16_t)(D0 - 1);
    if (D0 >= 0)
        goto l_next_direction;

    D0 = 0;

l_found_direction:
    ax = (uint16_t)D0;
    swosWriteWord(ADDR_cameraDirection, ax);
    esi = A1;
    swosWriteWord(esi + PLSPR_OFF_DIRECTION, ax);
    {
        int16_t v = swosReadSignedWord(ADDR_deadThrowInDirectionVar);
        swosWriteWord(ADDR_deadThrowInDirectionVar, (uint16_t)(v + 1));
    }

l_check_if_throw_in_taker_substituted:
    ax = (uint16_t)swosReadSignedWord(ADDR_g_substituteInProgress);
    if (ax == 0)
        goto l_check_throw_in_game_state;

    eax = swosReadSignedDword(ADDR_substitutedPlSprite);
    if (A1 == eax)
        goto l_abort_throw_in;

l_check_throw_in_game_state:
    {
        int16_t gs = swosReadSignedWord(ADDR_gameState);
        if ((uint16_t)gs < (uint16_t)15)
            goto l_abort_throw_in;
    }
    {
        int16_t gs = swosReadSignedWord(ADDR_gameState);
        if ((uint16_t)gs > (uint16_t)20)
            goto l_abort_throw_in;
    }

    esi = A1;
    al = swosReadByte(esi + PLSPR_OFF_PLAYER_DOWN_TIMER);
    if (al == 0)
        goto l_ready_for_throw_in;

    {
        uint8_t src = swosReadByte(esi + PLSPR_OFF_PLAYER_DOWN_TIMER);
        uint8_t res = (uint8_t)(src - 1);
        swosWriteByte(esi + PLSPR_OFF_PLAYER_DOWN_TIMER, res);
        if (res == 0)
            goto l_throw_in_over;
    }

    {
        uint8_t src = swosReadByte(esi + PLSPR_OFF_PLAYER_DOWN_TIMER);
        if (src != 18)
            return;
    }

    ax = (uint16_t)swosReadSignedWord(ADDR_g_inSubstitutesMenu);
    if (ax == 0)
        goto l_throw_in_done_check_pass_or_kick;

    A0 = ADDR_aboutToThrowInAnimTable;
    swosSetPlayerAnimationTable(A1, A0);
    esi = A1;
    swosWriteByte(esi + PLSPR_OFF_PLAYER_STATE, PL_THROW_IN);
    swosWriteByte(esi + PLSPR_OFF_PLAYER_DOWN_TIMER, 0);
    goto l_ready_for_throw_in;

l_throw_in_done_check_pass_or_kick:
    esi = A1;
    ax = (uint16_t)swosReadSignedWord(esi + PLSPR_OFF_DIRECTION);
    D0 = (int16_t)ax;
    esi = A2;
    swosWriteWord(esi + 40, 12);                         // ball.z.whole = 12
    swosWriteWord(ADDR_hideBall, 0);
    ax = (uint16_t)swosReadSignedWord(ADDR_throwInPassOrKick);
    if (ax != 0)
        goto l_do_throw_in_pass;

    goto l_do_throw_in_kick;

l_ready_for_throw_in:
    esi = A6;
    ax = (uint16_t)swosReadSignedWord(esi + 4);
    if (ax != 0)
        goto l_throw_in_check_input_direction;

    {
        int16_t stoppage = swosReadSignedWord(ADDR_stoppageTimerActive);
        if ((uint16_t)stoppage > (uint16_t)55)
            goto l_throw_in_hide_result;
    }
    goto l_throw_in_check_fire;

l_throw_in_check_input_direction:
    esi = A6;
    {
        int16_t cad = swosReadSignedWord(esi + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION);
        if (cad >= 0)
            goto l_check_if_stats_showing;
    }

l_throw_in_check_fire:
    esi = A6;
    al = swosReadByte(esi + TEAMDATA_OFF_FIRE_PRESSED);
    if (al != 0)
        goto l_check_if_stats_showing;

    al = swosReadByte(esi + TEAMDATA_OFF_QUICK_FIRE);
    if (al != 0)
        goto l_check_if_stats_showing;

    al = swosReadByte(esi + TEAMDATA_OFF_NORMAL_FIRE);
    if (al == 0)
        goto l_throw_in_check_direction;

l_check_if_stats_showing:
    ax = (uint16_t)swosReadSignedWord(ADDR_statsTimer);
    if (ax == 0)
        goto l_throw_in_hide_result;

    swosWriteWord(ADDR_fireBlocked, 1);

l_throw_in_hide_result:
    {
        int16_t v = swosReadSignedWord(ADDR_timeVar);
        swosWriteWord(ADDR_timeVar, (uint16_t)(-v));
    }
    {
        int16_t v = swosReadSignedWord(ADDR_resultTimer);
        swosWriteWord(ADDR_resultTimer, (uint16_t)(-v));
    }

l_throw_in_check_direction:
    esi = A6;
    ax = (uint16_t)swosReadSignedWord(esi + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION);
    D0 = (int16_t)ax;
    if (D0 >= 0)
        goto l_throw_in_got_input_direction;

l_throw_in_use_sprite_direction:
    esi = A1;
    ax = (uint16_t)swosReadSignedWord(esi + PLSPR_OFF_DIRECTION);
    esi = A6;
    swosWriteWord(esi + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, ax);
    ax = (uint16_t)swosReadSignedWord(esi + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION);
    D0 = (int16_t)ax;
    goto l_throw_in_check_quick_fire;

l_throw_in_got_input_direction:
    cl = (uint8_t)D0;
    ax = 1;
    if ((cl & 0x1F) != 0)
        ax = (uint16_t)(ax << (cl & 0x1F));
    {
        uint8_t turnFlags = swosReadByte(ADDR_playerTurnFlags);
        uint8_t testRes = (uint8_t)(turnFlags & (uint8_t)ax);
        zeroF = testRes == 0;
    }
    if (zeroF)
        goto l_throw_in_use_sprite_direction;

    esi = A1;
    ax = (uint16_t)swosReadSignedWord(esi + PLSPR_OFF_DIRECTION);
    if (D0 == (int16_t)ax)
        goto l_throw_in_check_quick_fire;

    ax = (uint16_t)D0;
    swosWriteWord(esi + PLSPR_OFF_DIRECTION, ax);
    setThrowInPlayerDestinationCoordinatesImpl(A1);
    A0 = ADDR_aboutToThrowInAnimTable;
    swosSetPlayerAnimationTable(A1, A0);
    esi = A1;
    swosWriteByte(esi + PLSPR_OFF_PLAYER_STATE, PL_THROW_IN);
    swosWriteByte(esi + PLSPR_OFF_PLAYER_DOWN_TIMER, 0);

l_throw_in_check_quick_fire:
    esi = A6;
    al = swosReadByte(esi + TEAMDATA_OFF_QUICK_FIRE);
    if (al == 0)
        goto l_throw_in_check_normal_fire;

    cl = (uint8_t)D0;
    ax = 1;
    if ((cl & 0x1F) != 0)
        ax = (uint16_t)(ax << (cl & 0x1F));
    {
        uint16_t turnFlagsW = (uint16_t)swosReadSignedWord(ADDR_playerTurnFlags);
        uint16_t testRes = (uint16_t)(turnFlagsW & ax);
        zeroF = testRes == 0;
    }
    if (zeroF)
        goto l_throw_in_check_normal_fire;

    swosWriteWord(ADDR_throwInPassOrKick, 1);
    A0 = ADDR_throwInPassAnimTable;
    swosSetPlayerAnimationTable(A1, A0);
    esi = A1;
    swosWriteByte(esi + PLSPR_OFF_PLAYER_DOWN_TIMER, 20);
    return;

l_throw_in_check_normal_fire:
    esi = A6;
    al = swosReadByte(esi + TEAMDATA_OFF_NORMAL_FIRE);
    if (al == 0)
        return;

    cl = (uint8_t)D0;
    ax = 1;
    if ((cl & 0x1F) != 0)
        ax = (uint16_t)(ax << (cl & 0x1F));
    {
        uint16_t turnFlagsW = (uint16_t)swosReadSignedWord(ADDR_playerTurnFlags);
        uint16_t testRes = (uint16_t)(turnFlagsW & ax);
        zeroF = testRes == 0;
    }
    if (zeroF)
        return;

    swosWriteWord(ADDR_throwInPassOrKick, 0);
    A0 = ADDR_throwInKickAnimTable;
    swosSetPlayerAnimationTable(A1, A0);
    esi = A1;
    swosWriteByte(esi + PLSPR_OFF_PLAYER_DOWN_TIMER, 25);
    return;

l_do_throw_in_pass:
    eax = A6;
    swosWriteDword(ADDR_lastTeamPlayed, (uint32_t)eax);
    eax = A1;
    swosWriteDword(ADDR_lastPlayerPlayed, (uint32_t)eax);
    swosWriteWord(ADDR_playerHadBall, 1);
    swosDoPass(A6, A1);

    eax = A1;
    esi = A6;
    swosWriteDword(esi + TEAMDATA_OFF_LAST_HEADING_PLAYER, (uint32_t)eax);
    esi = A2;
    swosWriteDword(esi + 54, 1);                         // ball.deltaZ = 1 (raw dword write)
    {
        int16_t gsp = swosReadSignedWord(ADDR_gameStatePl);
        if (gsp == 100)
            goto l_throw_in_ball_passed;
    }
    swosWriteWord(ADDR_gameNotInProgressCounterWriteOnly, 0);

l_throw_in_ball_passed:
    swosWriteWord(ADDR_gameStatePl, ST_GAME_IN_PROGRESS);
    swosWriteWord(ADDR_gameState, ST_GAME_IN_PROGRESS);
    esi = A6;
    swosWriteWord(esi + TEAMDATA_OFF_BALL_IN_PLAY, 1);
    swosWriteWord(esi + TEAMDATA_OFF_BALL_OUT_OF_PLAY, 1);
    eax = swosReadSignedDword(esi + TEAMDATA_OFF_OPPONENTS_TEAM);
    A0 = eax;
    esi = A0;
    swosWriteWord(esi + TEAMDATA_OFF_BALL_IN_PLAY, 1);
    swosWriteWord(esi + TEAMDATA_OFF_BALL_OUT_OF_PLAY, 1);
    swosWriteWord(esi + TEAMDATA_OFF_SPIN_TIMER, (uint16_t)-1);
    esi = A6;
    swosWriteDword(esi + TEAMDATA_OFF_CONTROLLED_PLAYER, 0);
    eax = A1;
    swosWriteDword(esi + TEAMDATA_OFF_PASSING_KICKING_PLAYER, (uint32_t)eax);
    swosWriteWord(esi + TEAMDATA_OFF_PASS_KICK_TIMER, 25);
    swosWriteWord(esi + TEAMDATA_OFF_BALL_CAN_BE_CONTROLLED, 0);
    eax = swosReadSignedDword(esi + TEAMDATA_OFF_OPPONENTS_TEAM);
    esi = A0;
    swosWriteWord(esi + TEAMDATA_OFF_PASSING_TO_PLAYER, 0);
    swosWriteDword(esi + TEAMDATA_OFF_PASSING_KICKING_PLAYER, 0);
    return;

l_do_throw_in_kick:
    eax = A6;
    swosWriteDword(ADDR_lastTeamPlayed, (uint32_t)eax);
    eax = A1;
    swosWriteDword(ADDR_lastPlayerPlayed, (uint32_t)eax);
    swosWriteWord(ADDR_playerHadBall, 1);
    swosPlayerKickingBall(A6, A1);

    eax = A1;
    esi = A6;
    swosWriteDword(esi + TEAMDATA_OFF_LAST_HEADING_PLAYER, (uint32_t)eax);
    {
        int16_t gsp = swosReadSignedWord(ADDR_gameStatePl);
        if (gsp == 100)
            goto l_throw_in_ball_kicked;
    }
    swosWriteWord(ADDR_gameNotInProgressCounterWriteOnly, 0);

l_throw_in_ball_kicked:
    swosWriteWord(ADDR_gameStatePl, ST_GAME_IN_PROGRESS);
    swosWriteWord(ADDR_gameState, ST_GAME_IN_PROGRESS);
    esi = A6;
    swosWriteWord(esi + TEAMDATA_OFF_BALL_IN_PLAY, 1);
    swosWriteWord(esi + TEAMDATA_OFF_BALL_OUT_OF_PLAY, 1);
    eax = swosReadSignedDword(esi + TEAMDATA_OFF_OPPONENTS_TEAM);
    A0 = eax;
    esi = A0;
    swosWriteWord(esi + TEAMDATA_OFF_BALL_IN_PLAY, 1);
    swosWriteWord(esi + TEAMDATA_OFF_BALL_OUT_OF_PLAY, 1);
    swosWriteWord(esi + TEAMDATA_OFF_SPIN_TIMER, (uint16_t)-1);
    esi = A6;
    swosWriteDword(esi + TEAMDATA_OFF_CONTROLLED_PLAYER, 0);
    eax = A1;
    swosWriteDword(esi + TEAMDATA_OFF_PASSING_KICKING_PLAYER, (uint32_t)eax);
    swosWriteWord(esi + TEAMDATA_OFF_PASS_KICK_TIMER, 25);
    swosWriteWord(esi + TEAMDATA_OFF_BALL_CAN_BE_CONTROLLED, 0);
    eax = swosReadSignedDword(esi + TEAMDATA_OFF_OPPONENTS_TEAM);
    esi = A0;
    swosWriteWord(esi + TEAMDATA_OFF_PASSING_TO_PLAYER, 0);
    swosWriteDword(esi + TEAMDATA_OFF_PASSING_KICKING_PLAYER, 0);
    return;

l_abort_throw_in:
    swosWriteWord(ADDR_hideBall, 0);

l_throw_in_over:
    esi = A1;
    swosWriteByte(esi + PLSPR_OFF_PLAYER_STATE, PL_NORMAL);
    A0 = ADDR_playerNormalStandingAnimTable;
    swosSetPlayerAnimationTable(A1, A0);
    return;
}

// ===================================================================
// TickSetPieces -- per-tick corner / goal-kick auto-resolver
// ===================================================================
void swosSetPiecesTickSetPieces(void)
{
    int16_t gameState = swosReadSignedWord(ADDR_gameState);
    if (gameState == ST_CORNER_LEFT || gameState == ST_CORNER_RIGHT)
    {
        resolveCornerKick(gameState);
        return;
    }
    if (gameState == ST_GOAL_OUT_LEFT || gameState == ST_GOAL_OUT_RIGHT)
    {
        resolveGoalKick(gameState);
        return;
    }
    if (gameState == ST_PENALTIES)
    {
        int16_t gsp = swosReadSignedWord(ADDR_gameStatePl);
        if (gsp == ST_STOPPED)
            resolvePenaltyShootout();
        return;
    }
    if (gameState >= ST_THROW_IN_FORWARD_RIGHT && gameState <= ST_THROW_IN_BACK_LEFT)
    {
        resolveThrowIn(gameState);
        return;
    }
    if (gameState == ST_FOUL ||
        (gameState >= ST_FREE_KICK_LEFT1 && gameState <= ST_FREE_KICK_RIGHT3))
    {
        resolveFreeKick(gameState);
    }
}

// ===================================================================
// TickCorner / TickGoalKick -- per-tick AI turn / fire update
// ===================================================================
void swosSetPiecesTickCorner(int a6TeamBase)
{
    int16_t rt = swosReadSignedWord(ADDR_resultTimer);
    if (rt != 0)
        return;

    (void)a6TeamBase;
    int16_t gameState = swosReadSignedWord(ADDR_gameState);
    resolveCornerKick(gameState);
}

void swosSetPiecesTickGoalKick(int a6TeamBase)
{
    int16_t rt = swosReadSignedWord(ADDR_resultTimer);
    if (rt != 0)
        return;

    (void)a6TeamBase;
    int16_t gameState = swosReadSignedWord(ADDR_gameState);
    resolveGoalKick(gameState);
}

// ===================================================================
// ResolveCornerKick -- spawn ball at corner flag, pick kicker, fire
// ===================================================================
static void resolveCornerKick(int16_t gameState)
{
    int16_t rt = swosReadSignedWord(ADDR_resultTimer);
    if (rt != 0)
        return;

    int16_t ballX = swosReadSignedWord(ADDR_foulXCoordinate);
    int16_t ballY = swosReadSignedWord(ADDR_foulYCoordinate);

    int32_t kickerTeamBase = swosReadSignedDword(ADDR_lastTeamPlayedBeforeBreak);
    if (kickerTeamBase == 0)
        kickerTeamBase = TEAMDATA_TOP_BASE;

    swosBallSpriteSetSpeed(0);
    swosBallSpriteSetX((int32_t)ballX << 16);
    swosBallSpriteSetY((int32_t)ballY << 16);
    swosBallSpriteSetZ(0);
    swosBallSpriteSetDeltaX(0);
    swosBallSpriteSetDeltaY(0);
    swosBallSpriteSetDeltaZ(0);
    swosBallSpriteSetDestX(ballX);
    swosBallSpriteSetDestY(ballY);

    int kickerSpriteAddr = findNearestOutfielderToPoint(kickerTeamBase, ballX, ballY);
    if (kickerSpriteAddr == 0)
        return;

    int16_t cameraDir = swosReadSignedWord(ADDR_cameraDirection);
    int16_t cdIdx = (int16_t)(cameraDir & 7);
    int16_t destX = (int16_t)(ballX + kKickerWalkupOffsetX[cdIdx]);
    int16_t destY = (int16_t)(ballY + kKickerWalkupOffsetY[cdIdx]);
    swosWriteWord(kickerSpriteAddr + PLSPR_OFF_DEST_X, (uint16_t)destX);
    swosWriteWord(kickerSpriteAddr + PLSPR_OFF_DEST_Y, (uint16_t)destY);
    swosWriteWord(kickerSpriteAddr + PLSPR_OFF_DIRECTION, (uint16_t)cameraDir);
    swosWriteWord(kickerSpriteAddr + PLSPR_OFF_FULL_DIRECTION, (uint16_t)(cameraDir * 32));
    swosWriteWord(kickerSpriteAddr + PLSPR_OFF_SPEED, K_PLAYER_SPEEDS_GAME_STOPPED_1);

    swosWriteWord(kickerTeamBase + TEAMDATA_OFF_CONTROLLED_PL_DIRECTION, (uint16_t)cameraDir);
    swosWriteDword(kickerTeamBase + TEAMDATA_OFF_CONTROLLED_PLAYER, (uint32_t)kickerSpriteAddr);

    swosWriteDword(ADDR_lastTeamPlayed, (uint32_t)kickerTeamBase);
    swosWriteDword(ADDR_lastPlayerPlayed, (uint32_t)kickerSpriteAddr);
    swosWriteWord(ADDR_playerHadBall, 1);

    // CEREMONY DEFER -- see SetPieces.cs's header comment.
    swosWriteWord(ADDR_gameStatePl, ST_WAITING_ON_PLAYER);
    swosWriteDword(kickerTeamBase + TEAMDATA_OFF_PASSING_KICKING_PLAYER, (uint32_t)kickerSpriteAddr);

    (void)gameState;
}

// ===================================================================
// ResolveGoalKick -- spawn ball at 6-yard line, keeper takes the kick
// ===================================================================
static void resolveGoalKick(int16_t gameState)
{
    int16_t rt = swosReadSignedWord(ADDR_resultTimer);
    if (rt != 0)
        return;

    int16_t ballX = swosReadSignedWord(ADDR_foulXCoordinate);
    int16_t ballY = swosReadSignedWord(ADDR_foulYCoordinate);

    int32_t kickerTeamBase = swosReadSignedDword(ADDR_lastTeamPlayedBeforeBreak);
    if (kickerTeamBase == 0)
        kickerTeamBase = TEAMDATA_TOP_BASE;

    swosBallSpriteSetSpeed(0);
    swosBallSpriteSetX((int32_t)ballX << 16);
    swosBallSpriteSetY((int32_t)ballY << 16);
    swosBallSpriteSetZ(0);
    swosBallSpriteSetDeltaX(0);
    swosBallSpriteSetDeltaY(0);
    swosBallSpriteSetDeltaZ(0);
    swosBallSpriteSetDestX(ballX);
    swosBallSpriteSetDestY(ballY);

    int kickerSpriteAddr = findKeeperSprite(kickerTeamBase);
    if (kickerSpriteAddr == 0)
        return;

    int16_t cameraDir = swosReadSignedWord(ADDR_cameraDirection);
    int16_t cdIdx = (int16_t)(cameraDir & 7);
    int16_t destX = (int16_t)(ballX + kKickerWalkupOffsetX[cdIdx]);
    int16_t destY = (int16_t)(ballY + kKickerWalkupOffsetY[cdIdx]);
    swosWriteWord(kickerSpriteAddr + PLSPR_OFF_DEST_X, (uint16_t)destX);
    swosWriteWord(kickerSpriteAddr + PLSPR_OFF_DEST_Y, (uint16_t)destY);
    swosWriteWord(kickerSpriteAddr + PLSPR_OFF_DIRECTION, (uint16_t)cameraDir);
    swosWriteWord(kickerSpriteAddr + PLSPR_OFF_FULL_DIRECTION, (uint16_t)(cameraDir * 32));
    swosWriteWord(kickerSpriteAddr + PLSPR_OFF_SPEED, K_PLAYER_SPEEDS_GAME_STOPPED_1);

    swosWriteWord(kickerTeamBase + TEAMDATA_OFF_CONTROLLED_PL_DIRECTION, (uint16_t)cameraDir);
    swosWriteDword(kickerTeamBase + TEAMDATA_OFF_CONTROLLED_PLAYER, (uint32_t)kickerSpriteAddr);

    swosWriteDword(ADDR_lastTeamPlayed, (uint32_t)kickerTeamBase);
    swosWriteDword(ADDR_lastPlayerPlayed, (uint32_t)kickerSpriteAddr);
    swosWriteWord(ADDR_playerHadBall, 1);

    swosWriteWord(ADDR_gameStatePl, ST_WAITING_ON_PLAYER);
    swosWriteDword(kickerTeamBase + TEAMDATA_OFF_PASSING_KICKING_PLAYER, (uint32_t)kickerSpriteAddr);

    (void)gameState;
}

// ===================================================================
// ResolveThrowIn -- spawn ball at touchline spot, pick thrower, release
// ===================================================================
static void resolveThrowIn(int16_t gameState)
{
    int16_t rt = swosReadSignedWord(ADDR_resultTimer);
    if (rt != 0)
        return;

    int16_t ballX = swosReadSignedWord(ADDR_foulXCoordinate);
    int16_t ballY = swosReadSignedWord(ADDR_foulYCoordinate);

    int32_t throwerTeamBase = swosReadSignedDword(ADDR_lastTeamPlayedBeforeBreak);
    if (throwerTeamBase == 0)
        throwerTeamBase = TEAMDATA_TOP_BASE;

    swosBallSpriteSetSpeed(0);
    swosBallSpriteSetX((int32_t)ballX << 16);
    swosBallSpriteSetY((int32_t)ballY << 16);
    swosBallSpriteSetZ(0);
    swosBallSpriteSetDeltaX(0);
    swosBallSpriteSetDeltaY(0);
    swosBallSpriteSetDeltaZ(0);
    swosBallSpriteSetDestX(ballX);
    swosBallSpriteSetDestY(ballY);

    int throwerSpriteAddr = findNearestOutfielderToPoint(throwerTeamBase, ballX, ballY);
    if (throwerSpriteAddr == 0)
        return;

    // updatePlayers.cpp:16608-16638 -- AI_throwInDirections[gameState-15],
    // rotated by 4 (swap nibbles) for the top team. See SetPieces.cs's
    // header comment for the full derivation.
    int tableIdx = gameState - ST_THROW_IN_FORWARD_RIGHT;
    if (tableIdx < 0 || tableIdx > 5) tableIdx = 0;
    uint8_t dirBitmap = swosReadByte(ADDR_AI_throwInDirections + tableIdx);
    if (throwerTeamBase == TEAMDATA_TOP_BASE)
        dirBitmap = (uint8_t)(((dirBitmap >> 4) & 0x0F) | ((dirBitmap & 0x0F) << 4));
    int16_t releaseDir = 0;
    for (int d = 0; d < 8; d++)
    {
        if ((dirBitmap & (1 << d)) != 0)
        {
            releaseDir = (int16_t)d;
            break;
        }
    }

    int16_t throwOffsetX = (ballX < 336) ? (int16_t)-3 : (int16_t)+3;
    int16_t destX = (int16_t)(ballX + throwOffsetX);
    int16_t destY = ballY;
    swosWriteWord(throwerSpriteAddr + PLSPR_OFF_DEST_X, (uint16_t)destX);
    swosWriteWord(throwerSpriteAddr + PLSPR_OFF_DEST_Y, (uint16_t)destY);
    swosWriteWord(throwerSpriteAddr + PLSPR_OFF_DIRECTION, (uint16_t)releaseDir);
    swosWriteWord(throwerSpriteAddr + PLSPR_OFF_FULL_DIRECTION, (uint16_t)(releaseDir * 32));
    swosWriteWord(throwerSpriteAddr + PLSPR_OFF_SPEED, K_PLAYER_SPEEDS_GAME_STOPPED_1);

    swosWriteWord(throwerTeamBase + TEAMDATA_OFF_CONTROLLED_PL_DIRECTION, (uint16_t)releaseDir);
    swosWriteDword(throwerTeamBase + TEAMDATA_OFF_CONTROLLED_PLAYER, (uint32_t)throwerSpriteAddr);

    swosWriteDword(ADDR_lastTeamPlayed, (uint32_t)throwerTeamBase);
    swosWriteDword(ADDR_lastPlayerPlayed, (uint32_t)throwerSpriteAddr);
    swosWriteWord(ADDR_playerHadBall, 1);

    swosWriteWord(ADDR_gameStatePl, ST_WAITING_ON_PLAYER);
    swosWriteDword(throwerTeamBase + TEAMDATA_OFF_PASSING_KICKING_PLAYER, (uint32_t)throwerSpriteAddr);

    (void)gameState;
}

// ===================================================================
// ResolveFreeKick -- spawn ball at foul spot, pick taker, release
// ===================================================================
static void resolveFreeKick(int16_t gameState)
{
    int16_t rt = swosReadSignedWord(ADDR_resultTimer);
    if (rt != 0)
        return;

    int16_t ballX = swosReadSignedWord(ADDR_foulXCoordinate);
    int16_t ballY = swosReadSignedWord(ADDR_foulYCoordinate);

    int32_t kickerTeamBase = swosReadSignedDword(ADDR_lastTeamPlayedBeforeBreak);
    if (kickerTeamBase == 0)
        kickerTeamBase = TEAMDATA_TOP_BASE;

    swosBallSpriteSetSpeed(0);
    swosBallSpriteSetX((int32_t)ballX << 16);
    swosBallSpriteSetY((int32_t)ballY << 16);
    swosBallSpriteSetZ(0);
    swosBallSpriteSetDeltaX(0);
    swosBallSpriteSetDeltaY(0);
    swosBallSpriteSetDeltaZ(0);
    swosBallSpriteSetDestX(ballX);
    swosBallSpriteSetDestY(ballY);

    int kickerSpriteAddr = findNearestOutfielderToPoint(kickerTeamBase, ballX, ballY);
    if (kickerSpriteAddr == 0)
        return;

    int16_t cameraDir = swosReadSignedWord(ADDR_cameraDirection);
    if (cameraDir < 0 || cameraDir > 7) cameraDir = 0;

    int16_t cdIdx = (int16_t)(cameraDir & 7);
    int16_t destX = (int16_t)(ballX + kKickerWalkupOffsetX[cdIdx]);
    int16_t destY = (int16_t)(ballY + kKickerWalkupOffsetY[cdIdx]);
    swosWriteWord(kickerSpriteAddr + PLSPR_OFF_DEST_X, (uint16_t)destX);
    swosWriteWord(kickerSpriteAddr + PLSPR_OFF_DEST_Y, (uint16_t)destY);
    swosWriteWord(kickerSpriteAddr + PLSPR_OFF_DIRECTION, (uint16_t)cameraDir);
    swosWriteWord(kickerSpriteAddr + PLSPR_OFF_FULL_DIRECTION, (uint16_t)(cameraDir * 32));
    swosWriteWord(kickerSpriteAddr + PLSPR_OFF_SPEED, K_PLAYER_SPEEDS_GAME_STOPPED_1);

    swosWriteWord(kickerTeamBase + TEAMDATA_OFF_CONTROLLED_PL_DIRECTION, (uint16_t)cameraDir);
    swosWriteDword(kickerTeamBase + TEAMDATA_OFF_CONTROLLED_PLAYER, (uint32_t)kickerSpriteAddr);

    swosWriteDword(ADDR_lastTeamPlayed, (uint32_t)kickerTeamBase);
    swosWriteDword(ADDR_lastPlayerPlayed, (uint32_t)kickerSpriteAddr);
    swosWriteWord(ADDR_playerHadBall, 1);

    swosWriteWord(ADDR_gameStatePl, ST_WAITING_ON_PLAYER);
    swosWriteDword(kickerTeamBase + TEAMDATA_OFF_PASSING_KICKING_PLAYER, (uint32_t)kickerSpriteAddr);

    (void)gameState;
}

// Find the nearest outfielder of `teamBase` to (px, py). Returns 0 if none
// found. Skips goalkeeper (ordinal slot 0) -- a regular outfielder takes
// corners in SWOS.
static int findNearestOutfielderToPoint(int teamBase, int px, int py)
{
    int32_t tableAddr = swosReadSignedDword(teamBase + TEAMDATA_OFF_PLAYERS);
    if (tableAddr == 0)
        return 0;

    int bestSprite = 0;
    int64_t bestDistSq = INT64_MAX;
    for (int slotInTeam = 1; slotInTeam < PLSPR_TEAM_SIZE; slotInTeam++)
    {
        int32_t spriteAddr = swosReadSignedDword(tableAddr + slotInTeam * 4);
        if (spriteAddr == 0)
            continue;
        int16_t sentAway = swosReadSignedWord(spriteAddr + PLSPR_OFF_SENT_AWAY);
        if (sentAway != 0)
            continue;

        int16_t x = swosReadSignedWord(spriteAddr + PLSPR_OFF_X + 2);
        int16_t y = swosReadSignedWord(spriteAddr + PLSPR_OFF_Y + 2);
        int64_t dx = x - px;
        int64_t dy = y - py;
        int64_t distSq = dx * dx + dy * dy;
        if (distSq < bestDistSq)
        {
            bestDistSq = distSq;
            bestSprite = spriteAddr;
        }
    }
    return bestSprite;
}

// Find the keeper sprite of `teamBase`. Slot 0 of each team table.
static int findKeeperSprite(int teamBase)
{
    int32_t tableAddr = swosReadSignedDword(teamBase + TEAMDATA_OFF_PLAYERS);
    if (tableAddr == 0)
        return 0;
    return swosReadSignedDword(tableAddr);
}

// ===================================================================
// TickFreeKick -- updatePlayers.cpp:16385-16448
// ===================================================================
void swosSetPiecesTickFreeKick(int a6TeamBase)
{
    int16_t rt = swosReadSignedWord(ADDR_resultTimer);
    if (rt != 0)
        return;

    int esi = a6TeamBase;
    uint16_t ax = (uint16_t)swosReadSignedWord(esi + TEAMDATA_OFF_CONTROLLED_PL_DIRECTION);
    uint16_t D0 = ax;

    D0 = (uint16_t)(D0 + 16);
    D0 = (uint16_t)(D0 & 0xFF);
    D0 = (uint16_t)(D0 >> 5);

    ax = D0;
    swosWriteWord(esi + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, ax);
}

// ===================================================================
// TickPenalty -- updatePlayers.cpp:16859-16914
// ===================================================================
void swosSetPiecesTickPenalty(int a5PlayerAddr, int a6TeamBase)
{
    int16_t rt = swosReadSignedWord(ADDR_resultTimer);
    if (rt != 0)
        return;

    int16_t gameState = swosReadSignedWord(ADDR_gameState);
    if (gameState == ST_PENALTIES)
    {
        int16_t gsp = swosReadSignedWord(ADDR_gameStatePl);
        if (gsp == ST_STOPPED)
            resolvePenaltyShootout();
    }

    int A5 = a5PlayerAddr;
    int A6 = a6TeamBase;
    int esi;
    uint16_t ax;
    uint8_t cl;
    bool zeroF;

    ax = (uint16_t)swosReadSignedWord(ADDR_AI_rand);
    uint16_t D0 = (uint16_t)(ax & 7);

    cl = (uint8_t)D0;
    ax = 1;
    if ((cl & 0x1F) != 0)
        ax = (uint16_t)(ax << (cl & 0x1F));
    {
        uint16_t turnFlagsW = (uint16_t)swosReadSignedWord(ADDR_playerTurnFlags);
        uint16_t testRes = (uint16_t)(turnFlagsW & ax);
        zeroF = testRes == 0;
    }
    if (zeroF)
        goto l_penalty_random_direction_disallowed;

    ax = D0;
    esi = A6;
    swosWriteWord(esi + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, ax);
    return;

l_penalty_random_direction_disallowed:
    esi = A5;
    ax = (uint16_t)swosReadSignedWord(esi + PLSPR_OFF_DIRECTION);
    if (ax == 0)
        return;

    ax = (uint16_t)swosReadSignedWord(esi + PLSPR_OFF_DIRECTION);
    if (ax == 4)
        return;

    // 16914 -- falls into l_apply_after_touch, owned by AiBrain in this port.
}

// ===================================================================
// ResolvePenaltyShootout -- walk shooter to penalty spot + park
// ===================================================================
static void resolvePenaltyShootout(void)
{
    int16_t ballX = swosReadSignedWord(ADDR_foulXCoordinate);
    int16_t ballY = swosReadSignedWord(ADDR_foulYCoordinate);

    int32_t shooterSprite = swosReadSignedDword(ADDR_penaltyShooterSprite);
    if (shooterSprite == 0)
        return;

    swosBallSpriteSetSpeed(0);
    swosBallSpriteSetX((int32_t)ballX << 16);
    swosBallSpriteSetY((int32_t)ballY << 16);
    swosBallSpriteSetZ(0);
    swosBallSpriteSetDeltaX(0);
    swosBallSpriteSetDeltaY(0);
    swosBallSpriteSetDeltaZ(0);
    swosBallSpriteSetDestX(ballX);
    swosBallSpriteSetDestY(ballY);

    int16_t cameraDir = swosReadSignedWord(ADDR_cameraDirection);
    if (cameraDir < 0 || cameraDir > 7) cameraDir = 0;
    swosWriteWord(shooterSprite + PLSPR_OFF_X + 2, (uint16_t)ballX);
    swosWriteWord(shooterSprite + PLSPR_OFF_Y + 2, (uint16_t)ballY);
    swosWriteWord(shooterSprite + PLSPR_OFF_DEST_X, (uint16_t)ballX);
    swosWriteWord(shooterSprite + PLSPR_OFF_DEST_Y, (uint16_t)ballY);
    swosWriteWord(shooterSprite + PLSPR_OFF_DIRECTION, (uint16_t)cameraDir);
    swosWriteWord(shooterSprite + PLSPR_OFF_FULL_DIRECTION, (uint16_t)(cameraDir * 32));

    // NextPenalty resolved A6 = bottomTeamData unconditionally; the shooter
    // was pulled from bottomTeamData.spritesTable -- mirror that here.
    int shooterTeamBase = TEAMDATA_BOTTOM_BASE;

    swosWriteWord(shooterTeamBase + TEAMDATA_OFF_CONTROLLED_PL_DIRECTION, (uint16_t)cameraDir);
    swosWriteDword(shooterTeamBase + TEAMDATA_OFF_CONTROLLED_PLAYER, (uint32_t)shooterSprite);

    swosWriteDword(ADDR_lastTeamPlayed, (uint32_t)shooterTeamBase);
    swosWriteDword(ADDR_lastPlayerPlayed, (uint32_t)shooterSprite);
    swosWriteWord(ADDR_playerHadBall, 1);

    swosWriteWord(ADDR_gameStatePl, ST_WAITING_ON_PLAYER);
    swosWriteDword(shooterTeamBase + TEAMDATA_OFF_PASSING_KICKING_PLAYER, (uint32_t)shooterSprite);
}

// ===================================================================
// AdvancePenaltiesTimer -- inter-pen pause + NextPenalty trigger
// ===================================================================
void swosSetPiecesAdvancePenaltiesTimer(void)
{
    int16_t pp = swosReadSignedWord(ADDR_playingPenalties);
    if (pp == 0)
        return;

    int16_t gs = swosReadSignedWord(ADDR_gameState);
    if (gs == ST_PENALTIES)
        return;

    int16_t timer = swosReadSignedWord(ADDR_penaltiesTimer);
    timer = (int16_t)(timer + 1);
    swosWriteWord(ADDR_penaltiesTimer, (uint16_t)timer);

    int16_t interval = swosReadSignedWord(ADDR_m_penaltiesInterval);
    if (timer != interval)
        return;

    swosGameTimeNextPenalty();
}
