// SOURCE: openswos game/scripts/Sim/Port/UpdatePlayers.cs (full file, step
// 7B of the porting order).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes. Labels
// and goto preserved exactly where the C# used them (the C# is itself a
// disassembly-faithful translation of swos.asm/updatePlayers.cpp, with the
// original asm labels kept as comments/goto targets).
//
// Statics kept (matches the C#'s own distinction, see project memory):
//   - s_zeroTicksTop/Bot, s_carrierStallTicksTop/Bot: REAL, gameplay-
//     affecting debounce counters (gate a real Memory write, the
//     controlledPlayer re-election). Ported as genuine C statics.
//   - kChaseFallbackEnabled: a compile-time-disabled-but-retained C# `const
//     bool` gating a whole heuristic block ("kept so the A/B can be
//     re-run"). Ported as a real #define-gated dead branch, not omitted.
// Statics NOT kept (pure C#-side telemetry, zero Memory effect, same
// pattern as every prior step's telemetry omissions -- verified by reading
// each one's only use, the disabled chase-fallback block and TickAiControlled's
// removed off-ball kick fallback comment block):
//   s_fallbackChasesTop/Bot, s_kickFallbackTop/Bot, s_reaimAppliedTop/Bot,
//   s_kickFallbackTop/BotDef/Mid/Att/Box, and UpdatePlayers.FallbackChasesTop
//   etc.'s public getters (Main.cs smoke-test reporting only).
//
// Hook boundaries (reuse the g_swosAiSetControlsDirectionHook /
// g_swosAiKickHook globals from swos_player_controlled.h, established step
// 6A):
//   - AiBrain.SetControlsDirection / AiHelpers.AI_Kick -- wired to the real
//     implementations as of step 9 (swos_ai_brain.h/swos_ai_helpers.h).
//   - SetPieces.SetThrowInPlayerDestinationCoordinates / SetPieces.TickThrowIn
//     -- still step 10, via the NEW swos_set_pieces.h hooks (7B), still
//     assert-backed.
//
// Audio omitted (pure playback, zero Memory effect, verified by reading
// MatchAudio.KeeperSavedComment/PlayMissGoal's bodies -- Audio/MatchAudio.cs):
// both calls in TickGoalieDiving's dive-outcome tail.
// Telemetry omitted (pure C#-side counters, zero Memory effect, verified by
// reading the body): Referee.NotifyEnteredAboutToGiveCard
// (DbgEnteredAboutToGive++ only) in CheckIfThisPlayerGettingBooked.
//
// Design decision for the three `try { SetPlayerWithNoBallDestination(...) }
// catch (System.Exception) { }` C# blocks: see swos_update_players.h.
#include "swos_update_players.h"

#include <assert.h>
#include <stdint.h>
#include <stddef.h>

#include "swos_addr.h"
#include "swos_ball_sprite.h"
#include "swos_ball_update.h"
#include "swos_ball_variables.h"
#include "swos_memory.h"
#include "swos_player_actions.h"
#include "swos_player_controlled.h"
#include "swos_player_energy.h"
#include "swos_player_header.h"
#include "swos_player_sprite.h"
#include "swos_player_state.h"
#include "swos_player_tackle.h"
#include "swos_player_update.h"
#include "swos_set_pieces.h"
#include "swos_sprite_update.h"
#include "swos_team_data.h"
#include "swos_team_port.h"
#include "swos_util.h"

#include "generated/swos_update_players_tables.h"
#include "generated/swos_team_port_tables.h"

// ---- Constants from swos-port (verified against swos.h / updatePlayers.cpp) ---
#define K_ST_GAME_IN_PROGRESS   100
#define K_ST_KEEPER_HOLDS_BALL  3

#define K_RE_ELECT_DEBOUNCE_TICKS 4

// updatePlayers.cpp:210-271 -- pitch coordinate constants for ball-location tests.
#define K_PENALTY_AREA_X_MIN            193
#define K_PENALTY_AREA_X_MAX            478
#define K_PENALTY_AREA_Y_MIN            129
#define K_PENALTY_AREA_Y_MAX            769
#define K_UPPER_PENALTY_AREA_Y_BOUNDARY 216
#define K_LOWER_PENALTY_AREA_Y_BOUNDARY 682
#define K_GOALKEEPER_AREA_X_MIN         273
#define K_GOALKEEPER_AREA_X_MAX         398
#define K_GOALKEEPER_AREA_UPPER_Y       158
#define K_GOALKEEPER_AREA_LOWER_Y       740

#define K_REF_WAITING_PLAYER    2
#define K_REF_ABOUT_TO_GIVE_CARD 3

// ---- port-only chase heuristic: OFF (task #242, 2026-08-18) ---------------
// See the C# source's own header comment (kChaseFallbackEnabled) -- kept
// (not deleted) so the A/B can be re-run; do not re-enable without
// re-running it.
#define K_CHASE_FALLBACK_ENABLED 0

// 2026-06-06 -- debounce state for the controlledPlayer re-establishment
// heuristic (see UpdatePlayers.cs's own comment, mirrored in the header).
// REAL gameplay state (gates a real Memory write), not telemetry.
static int s_zeroTicksTop = 0;
static int s_zeroTicksBot = 0;
static int s_carrierStallTicksTop = 0;
static int s_carrierStallTicksBot = 0;

void swosUpdatePlayersResetState(void)
{
    s_zeroTicksTop = 0;
    s_zeroTicksBot = 0;
    s_carrierStallTicksTop = 0;
    s_carrierStallTicksBot = 0;
}

// ---- AI hook wrappers (reuse the EXISTING globals from step 6A) -----------
// Step 9 wired g_swosAiSetControlsDirectionHook/g_swosAiKickHook to the real
// AiBrain.SetControlsDirection/AiHelpers.AI_Kick (see
// swos_player_controlled.c's static initializer, the one definition site
// for both globals) -- the hook is always non-NULL now. Kept as named
// wrapper functions (stable call points), assert relaxed to a plain
// defensive null-check rather than an "unimplemented" failure.
static void requireAiSetControlsDirection(int teamBase)
{
    assert(g_swosAiSetControlsDirectionHook != NULL);
    if (g_swosAiSetControlsDirectionHook)
        g_swosAiSetControlsDirectionHook(teamBase);
}

static void requireAiKick(int spriteAddr, int teamBase)
{
    assert(g_swosAiKickHook != NULL);
    if (g_swosAiKickHook)
        g_swosAiKickHook(spriteAddr, teamBase);
}

// ---- forward declarations (definition order matches the C# source) --------
static bool checkIfThisPlayerGettingBooked(int spriteAddr, int teamBase);
static void tickGoalkeeper(int spriteAddr, bool topTeam, int slotInTeam);
static void runGoalkeeperInAreaChain(int spriteAddr, int teamBase, bool topTeam,
                                      bool ballInOurArea);
static bool runGoalieCantCatchBallPickup(int spriteAddr, int teamBase, bool topTeam);
static void tickGoalieDiving(int spriteAddr, int teamBase, bool topTeam);
static void goalkeeperRise(int spriteAddr, int teamBase);
static void tickHumanControlled(int spriteAddr, bool topTeam);
static void overrideDestToBallIfChaser(int spriteAddr, int teamBase);
static void tickAiControlled(int spriteAddr, bool topTeam, int slotInTeam);
static void setPlayerPositionsForGameBreak(int spriteAddr, int teamBase, bool topTeam);
static void setPlayerWithNoBallDestinationForBreak(int spriteAddr, int teamBase,
                                                     int16_t d6, int16_t d7,
                                                     int16_t gameStatePl);
static void tickPassExpectingStopped(int spriteAddr, bool topTeam);
static void tickTackledPlayer(int spriteAddr, bool topTeam);
static void tickTacklingPlayer(int spriteAddr, bool topTeam);
static void tickInjuredRollingPlayer(int spriteAddr, bool topTeam);
static void updatePlayerBallDistanceAndHeight(int teamBase, int spriteAddr);
static void tickEpilogueKeeperTransition(bool topTeam);
static int ownGoaliePlayersBase(bool isTopTeam);
static void headerExitToNormal(int spriteAddr);
static void tickJumpHeader(int spriteAddr, bool topTeam);
static void tickStaticHeader(int spriteAddr, bool topTeam);
static void dispatchByPlayerState(uint8_t state, int spriteAddr, int slotInTeam,
                                   bool topTeam);

// ===========================================================================
// swosUpdatePlayersUpdate -- updatePlayers.cpp:47 (C# UpdatePlayers.Update)
// ===========================================================================
void swosUpdatePlayersUpdate(int teamIndex)
{
    if (teamIndex < 0 || teamIndex > 1) return;
    bool topTeam = teamIndex == 0;
    int teamBase = swosTeamDataBase(topTeam);

    // === BUG FIX (2026-06-02) -- re-establish controlledPlayer if cleared. ==
    {
        int16_t playerNumberAtStart = swosReadSignedWord(teamBase + TEAMDATA_OFF_PLAYER_NUMBER);
        int ctrlAtStart = swosReadSignedDword(teamBase + TEAMDATA_OFF_CONTROLLED_PLAYER);
        int16_t gameStatePlAtStart = swosReadSignedWord(ADDR_gameStatePl);
        if (playerNumberAtStart == 0 && ctrlAtStart == 0 && gameStatePlAtStart == K_ST_GAME_IN_PROGRESS)
        {
            int zeroTicks;
            if (topTeam) { s_zeroTicksTop++; zeroTicks = s_zeroTicksTop; }
            else         { s_zeroTicksBot++; zeroTicks = s_zeroTicksBot; }

            int16_t topHas = swosReadSignedWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PLAYER_HAS_BALL);
            int16_t botHas = swosReadSignedWord(TEAMDATA_BOTTOM_BASE + TEAMDATA_OFF_PLAYER_HAS_BALL);
            bool ballHeldBySomeone = topHas != 0 || botHas != 0;

            if (zeroTicks >= K_RE_ELECT_DEBOUNCE_TICKS && !ballHeldBySomeone)
            {
                int slotBase = topTeam ? 0 : PLSPR_TEAM_SIZE;
                int16_t bxFix = swosBallSpriteXPixels();
                int16_t byFix = swosBallSpriteYPixels();
                int bestSlot = -1;
                long long bestDist = 0x7FFFFFFFFFFFFFFFLL;
                for (int s = slotBase + 1; s < slotBase + 11; s++)
                {
                    int sa = swosPlayerSpriteBase(s);
                    int16_t sx = swosReadSignedWord(sa + PLSPR_OFF_X + 2);
                    int16_t sy = swosReadSignedWord(sa + PLSPR_OFF_Y + 2);
                    int dxF = sx - bxFix;
                    int dyF = sy - byFix;
                    long long sq = (long long)dxF * dxF + (long long)dyF * dyF;
                    if (sq < bestDist) { bestDist = sq; bestSlot = s; }
                }
                if (bestSlot >= 0)
                {
                    int newAddr = swosPlayerSpriteBase(bestSlot);
                    swosWriteDword(teamBase + TEAMDATA_OFF_CONTROLLED_PLAYER, (uint32_t)newAddr);
                    if (topTeam) s_zeroTicksTop = 0;
                    else         s_zeroTicksBot = 0;
                }
            }
        }
        else
        {
            if (topTeam) s_zeroTicksTop = 0;
            else         s_zeroTicksBot = 0;
        }
    }

    // === DIAG (2026-06-06) -- carrier-stall tracker (per-team-per-tick) ====
    {
        int curCarrier = swosReadSignedDword(teamBase + TEAMDATA_OFF_CONTROLLED_PLAYER);
        int16_t pkt2 = swosReadSignedWord(teamBase + TEAMDATA_OFF_PASS_KICK_TIMER);
        if (curCarrier == 0 || pkt2 != 0)
        {
            if (topTeam) s_carrierStallTicksTop = 0;
            else         s_carrierStallTicksBot = 0;
        }
        else
        {
            int carrierBallDistTick = swosReadSignedDword(curCarrier + PLSPR_OFF_BALL_DISTANCE);
            if (carrierBallDistTick <= 32)
            {
                if (topTeam) s_carrierStallTicksTop++;
                else         s_carrierStallTicksBot++;
            }
            else
            {
                if (topTeam) s_carrierStallTicksTop = 0;
                else         s_carrierStallTicksBot = 0;
            }
        }
    }

    // ----- updatePlayers.cpp:50-88 -------------------------------------
    int lastPlayerPlayed = swosReadSignedDword(ADDR_lastPlayerPlayed);
    int lastTeamPlayed   = swosReadSignedDword(ADDR_lastTeamPlayed);
    int lastKeeperPlayed = swosReadSignedDword(ADDR_lastKeeperPlayed);

    swosWriteDword(ADDR_prevLastPlayer, (uint32_t)lastPlayerPlayed);
    swosWriteDword(ADDR_prevLastTeamPlayed, (uint32_t)lastTeamPlayed);

    if (lastKeeperPlayed != 0)
    {
        int goalie1Addr = swosPlayerSpriteBase(PLSPR_SLOT_GOALIE1);
        int goalie2Addr = swosPlayerSpriteBase(PLSPR_SLOT_GOALIE2);
        if (lastPlayerPlayed != goalie1Addr && lastPlayerPlayed != goalie2Addr)
        {
            swosWriteDword(ADDR_lastKeeperPlayed, 0);
        }
    }

    // ----- updatePlayers.cpp:90-126 -------------------------------------
    int16_t gksTimer = swosTeamDataGoalkeeperSavedCommentTimer(topTeam);
    if (gksTimer != 0)
    {
        if (gksTimer < 0) gksTimer++;
        else              gksTimer--;
        swosTeamDataSetGoalkeeperSavedCommentTimer(topTeam, gksTimer);
    }

    // ----- updatePlayers.cpp:128-153 -------------------------------------
    int16_t passKickTimer = swosReadSignedWord(teamBase + TEAMDATA_OFF_PASS_KICK_TIMER);
    if (passKickTimer != 0)
    {
        passKickTimer--;
        swosWriteWord(teamBase + TEAMDATA_OFF_PASS_KICK_TIMER, (uint16_t)passKickTimer);
        if (passKickTimer == 0)
        {
            swosWriteDword(teamBase + TEAMDATA_OFF_PASSING_KICKING_PLAYER, 0);
        }
    }

    // ----- updatePlayers.cpp:155-172 -------------------------------------
    // wonTheBallTimer -- no named TeamData accessor (same as the C#): raw
    // offset +138.
    {
        const int kOffWonTheBallTimer = 138;
        int16_t wonBallTimer = swosReadSignedWord(teamBase + kOffWonTheBallTimer);
        if (wonBallTimer != 0)
        {
            wonBallTimer--;
            swosWriteWord(teamBase + kOffWonTheBallTimer, (uint16_t)wonBallTimer);
        }
    }

    // ----- updatePlayers.cpp:174-195 -------------------------------------
    {
        int16_t pswTimer = swosReadSignedWord(teamBase + TEAMDATA_OFF_PLAYER_SWITCH_TIMER);
        if (pswTimer != 0)
        {
            pswTimer--;
            swosWriteWord(teamBase + TEAMDATA_OFF_PLAYER_SWITCH_TIMER, (uint16_t)pswTimer);
        }
    }

    // === STALE-playerHasBall RECONCILIATION (2026-06-02) ===================
    {
        int ctrlReconcile = swosReadSignedDword(teamBase + TEAMDATA_OFF_CONTROLLED_PLAYER);
        if (ctrlReconcile == 0)
        {
            int16_t stalePhb = swosReadSignedWord(teamBase + TEAMDATA_OFF_PLAYER_HAS_BALL);
            if (stalePhb != 0)
            {
                swosWriteWord(teamBase + TEAMDATA_OFF_PLAYER_HAS_BALL, 0);
            }
        }
    }

    // ----- updatePlayers.cpp:197-198 -------------------------------------
    // BallUpdate.ApplyBallAfterTouch(topTeam) is BallUpdate.cs's own
    // per-team entry point (already ported, step 4) -- not part of this
    // step's scope; called here to preserve exact call order.
    swosApplyBallAfterTouch(topTeam);

    // ----- Pre-loop ball-variable predictor wiring ------------------------
    swosCalculateBallNextGroundXYPositions(BALLSPR_BASE);

    // ----- updatePlayers.cpp:199-336 -------------------------------------
    int16_t ballX = swosBallSpriteXPixels();
    int16_t ballY = swosBallSpriteYPixels();
    swosWriteWord(ADDR_ballInUpperPenaltyArea, 0);
    swosWriteWord(ADDR_ballInLowerPenaltyArea, 0);
    swosWriteWord(ADDR_ballInGoalkeeperArea, 0);

    bool inPenaltyXRange = ballX >= K_PENALTY_AREA_X_MIN && ballX <= K_PENALTY_AREA_X_MAX;
    bool inPenaltyYRange = ballY >= K_PENALTY_AREA_Y_MIN && ballY <= K_PENALTY_AREA_Y_MAX;
    bool inPenaltyArea = false;
    if (inPenaltyXRange && inPenaltyYRange)
    {
        if (ballY <= K_UPPER_PENALTY_AREA_Y_BOUNDARY)
        {
            swosWriteWord(ADDR_ballInUpperPenaltyArea, 1);
            inPenaltyArea = true;
        }
        else if (ballY >= K_LOWER_PENALTY_AREA_Y_BOUNDARY)
        {
            swosWriteWord(ADDR_ballInLowerPenaltyArea, 1);
            inPenaltyArea = true;
        }
    }

    if (inPenaltyArea)
    {
        bool inGkXRange = ballX >= K_GOALKEEPER_AREA_X_MIN && ballX <= K_GOALKEEPER_AREA_X_MAX;
        if (inGkXRange &&
            (ballY <= K_GOALKEEPER_AREA_UPPER_Y || ballY >= K_GOALKEEPER_AREA_LOWER_Y))
        {
            swosWriteWord(ADDR_ballInGoalkeeperArea, 1);
        }
    }

    // ----- updatePlayers.cpp:338-368 -------------------------------------
    {
        int16_t gameStatePl = swosReadSignedWord(ADDR_gameStatePl);
        if (gameStatePl == K_ST_GAME_IN_PROGRESS)
        {
            int16_t goalOut = swosReadSignedWord(ADDR_goalOut);
            if (goalOut != 0)
            {
                uint32_t upPa = swosReadDword(ADDR_ballInUpperPenaltyArea);
                if (upPa == 0)
                {
                    swosWriteWord(ADDR_goalOut, 0);
                }
            }
        }
    }

    // ----- updatePlayers.cpp:370-393 -------------------------------------
    {
        int16_t upIdx = swosReadSignedWord(teamBase + TEAMDATA_OFF_UPDATE_PLAYER_INDEX);
        upIdx++;
        if (upIdx == 11) upIdx = 0;
        swosWriteWord(teamBase + TEAMDATA_OFF_UPDATE_PLAYER_INDEX, (uint16_t)upIdx);
    }

    // ----- updatePlayers.cpp:395-... --------------------------------------
    for (int slotInTeam = 0; slotInTeam < 11; slotInTeam++)
    {
        int spriteAddr = swosTeamDataGetTeamSpriteAddr(topTeam, slotInTeam);
        if (spriteAddr == 0) continue;

        int16_t direction = swosReadSignedWord(spriteAddr + PLSPR_OFF_DIRECTION);
        swosWriteWord(spriteAddr + PLSPR_OFF_PLAYER_DIRECTION, (uint16_t)direction);

        int dx = swosReadSignedDword(spriteAddr + PLSPR_OFF_DELTA_X);
        int dy = swosReadSignedDword(spriteAddr + PLSPR_OFF_DELTA_Y);
        uint8_t isMoving = (uint8_t)((dx == 0 && dy == 0) ? 0x00 : 0xFF);
        swosWriteByte(spriteAddr + PLSPR_OFF_IS_MOVING, isMoving);

        swosPlayerEnergyDrainSlot(spriteAddr);

        uint8_t state = swosReadByte(spriteAddr + PLSPR_OFF_PLAYER_STATE);
        if (state == PLSTATE_TACKLED)
        {
            tickTackledPlayer(spriteAddr, topTeam);
            continue;
        }
        if (state == PLSTATE_INJURED)
        {
            tickInjuredRollingPlayer(spriteAddr, topTeam);
            continue;
        }

        updatePlayerBallDistanceAndHeight(teamBase, spriteAddr);

        swosUpdateBallVariables(spriteAddr, BALLSPR_BASE, teamBase);

        dispatchByPlayerState(state, spriteAddr, slotInTeam, topTeam);
    }

    // ----- updatePlayers.cpp:10367-10456 ----------------------------------
    tickEpilogueKeeperTransition(topTeam);
}

// ===========================================================================
// checkIfThisPlayerGettingBooked -- updatePlayers.cpp:8933-9049
// ===========================================================================
static bool checkIfThisPlayerGettingBooked(int spriteAddr, int teamBase)
{
    (void)teamBase; // matches the C#: teamBase is unused inside this function.
    int bookedPlayer = swosReadSignedDword(ADDR_bookedPlayer);
    if (bookedPlayer == 0 || bookedPlayer != spriteAddr) return false;

    int16_t foulX = swosReadSignedWord(ADDR_foulXCoordinate);
    int16_t foulY = swosReadSignedWord(ADDR_foulYCoordinate);
    int16_t destD1 = (int16_t)(foulX + 21);
    int16_t destD2 = foulY;

    int16_t curX = swosReadSignedWord(spriteAddr + PLSPR_OFF_X + 2);
    int16_t curY = swosReadSignedWord(spriteAddr + PLSPR_OFF_Y + 2);
    if (curX != destD1 || curY != destD2)
    {
        swosWriteWord(spriteAddr + PLSPR_OFF_DEST_X, (uint16_t)destD1);
        swosWriteWord(spriteAddr + PLSPR_OFF_DEST_Y, (uint16_t)destD2);
        return true;
    }

    int16_t refState = swosReadSignedWord(ADDR_refState);
    if (refState != K_REF_WAITING_PLAYER)
        return true;

    swosWriteWord(ADDR_refState, K_REF_ABOUT_TO_GIVE_CARD);
    // Referee.NotifyEnteredAboutToGiveCard() omitted: pure telemetry
    // (DbgEnteredAboutToGive++ only, Referee.cs:136-139) -- verified by
    // reading the body.

    swosSetPlayerAnimationTable(spriteAddr, ADDR_kPlayerStandingAnimTableAddr);

    swosWriteByte(spriteAddr + PLSPR_OFF_PLAYER_STATE, PLSTATE_BOOKED);
    swosWriteByte(spriteAddr + PLSPR_OFF_PLAYER_DOWN_TIMER, 1);
    return true;
}

// ===========================================================================
// tickGoalkeeper -- updatePlayers.cpp:~880-1300 (l_player_goalkeeper)
// ===========================================================================
static void tickGoalkeeper(int spriteAddr, bool topTeam, int slotInTeam)
{
    int teamBase = swosTeamDataBase(topTeam);
    uint8_t state = swosReadByte(spriteAddr + PLSPR_OFF_PLAYER_STATE);

    if (state == PLSTATE_GOALIE_DIVING_HIGH || state == PLSTATE_GOALIE_DIVING_LOW)
    {
        tickGoalieDiving(spriteAddr, teamBase, topTeam);
        return;
    }

    {
        int16_t gkPlaying = swosReadSignedWord(teamBase + TEAMDATA_OFF_GOALKEEPER_PLAYING);
        if (gkPlaying != 0)
        {
            swosWriteByte(teamBase + TEAMDATA_OFF_GOALIE_PLAYING_OR_OUT, 1);
            swosWriteByte(teamBase + TEAMDATA_OFF_BALL_OUT_OF_PLAY_OR_KEEPER, 0);
            int gkCtrl = swosReadSignedDword(teamBase + TEAMDATA_OFF_CONTROLLED_PLAYER);
            if (spriteAddr == gkCtrl)
            {
                if (state == PLSTATE_NORMAL)
                {
                    int piBaseCtrl = ownGoaliePlayersBase(topTeam);
                    if (piBaseCtrl != 0)
                        swosTeamPortUpdatePlayerShotChanceTable(topTeam, piBaseCtrl);
                    tickHumanControlled(spriteAddr, topTeam);
                    return;
                }
            }
            else
            {
                swosWriteWord(teamBase + TEAMDATA_OFF_GOALKEEPER_PLAYING, 0);
            }
        }
    }

    // ----- updatePlayers.cpp:943-969 -- l_update_ball_out_or_keepers -------
    {
        const int16_t kGoalkeeperSpeedWhenGameStopped = 1024;
        const int16_t kGoalkeeperGameSpeed = 1024;
        swosWriteByte(teamBase + TEAMDATA_OFF_BALL_OUT_OF_PLAY_OR_KEEPER, 0);
        swosWriteByte(teamBase + TEAMDATA_OFF_GOALIE_PLAYING_OR_OUT, 0);
        int16_t gsPlFlags = swosReadSignedWord(ADDR_gameStatePl);
        if (gsPlFlags != K_ST_GAME_IN_PROGRESS)
        {
            swosWriteByte(teamBase + TEAMDATA_OFF_GOALIE_PLAYING_OR_OUT, 0xFF);
            swosWriteByte(teamBase + TEAMDATA_OFF_BALL_OUT_OF_PLAY_OR_KEEPER, 0xFF);
            swosWriteWord(spriteAddr + PLSPR_OFF_SPEED, (uint16_t)kGoalkeeperSpeedWhenGameStopped);
        }
        swosWriteWord(spriteAddr + PLSPR_OFF_SPEED, (uint16_t)kGoalkeeperGameSpeed);
    }

    if (swosTickGoalkeeperHoldAutoRelease(spriteAddr, topTeam))
    {
        swosUpdatePlayerSpeedAndFrameDelay(teamBase, spriteAddr);
        return;
    }

    if (state == PLSTATE_GOALIE_CATCHING_BALL)
    {
        swosTickGoalieCatchingBall(spriteAddr, topTeam);
        swosUpdatePlayerSpeedAndFrameDelay(teamBase, spriteAddr);
        return;
    }

    if (state == PLSTATE_GOALIE_CLAIMED)
    {
        swosTickGoalieClaimed(spriteAddr);
        swosUpdatePlayerSpeedAndFrameDelay(teamBase, spriteAddr);
        return;
    }

    {
        int gkControlled = swosTeamDataControlledPlayer(topTeam);
        if (spriteAddr == gkControlled)
        {
            tickHumanControlled(spriteAddr, topTeam);
            return;
        }
        int gkPassTo = swosReadSignedDword(teamBase + TEAMDATA_OFF_PASS_TO_PLAYER_PTR);
        if (spriteAddr == gkPassTo && gkPassTo != 0)
        {
            int16_t gkGsPl = swosReadSignedWord(ADDR_gameStatePl);
            if (gkGsPl != K_ST_GAME_IN_PROGRESS)
            {
                tickPassExpectingStopped(spriteAddr, topTeam);
                return;
            }
            swosRunPassExpectingBranch(spriteAddr, topTeam);
            swosUpdatePlayerSpeedAndFrameDelay(teamBase, spriteAddr);
            return;
        }
        int16_t gsPlRoute = swosReadSignedWord(ADDR_gameStatePl);
        if (gsPlRoute != K_ST_GAME_IN_PROGRESS)
        {
            tickAiControlled(spriteAddr, topTeam, slotInTeam);
            return;
        }
    }

    {
        int16_t ordinal = swosReadSignedWord(spriteAddr + PLSPR_OFF_PLAYER_ORDINAL);
        int piBase = ownGoaliePlayersBase(topTeam);
        if (ordinal >= 1 && ordinal <= 11 && piBase != 0)
        {
            int piAddr = piBase + (ordinal - 1) * 61;
            swosTeamPortUpdatePlayerShotChanceTable(topTeam, piAddr);
        }
    }

    uint16_t upPa = swosReadWord(ADDR_ballInUpperPenaltyArea);
    uint16_t loPa = swosReadWord(ADDR_ballInLowerPenaltyArea);
    bool ballInOurArea = (topTeam && upPa != 0) || (!topTeam && loPa != 0);

    runGoalkeeperInAreaChain(spriteAddr, teamBase, topTeam, ballInOurArea);
}

// Local accessor for the shot-chance row (mirrors the C#'s local `Row`
// closure inside RunGoalkeeperInAreaChain). byteOff is the asm byte offset.
static int16_t shotChanceRow(int scTab, int byteOff)
{
    return scTab != 0
        ? swosReadSignedWord(scTab + byteOff)
        : swos_kPlayerShotChanceTable[byteOff >> 1];
}

// ===========================================================================
// runGoalkeeperInAreaChain -- updatePlayers.cpp:1143-2940
// (l_ball_in_penalty_area .. l_clamp_ball_y_inside_pitch, plus the
//  l_check_pass_to_player / l_this_player_last_played tails)
// ===========================================================================
static void runGoalkeeperInAreaChain(int spriteAddr, int teamBase, bool topTeam,
                                      bool ballInOurArea)
{
    int a2Ball = BALLSPR_BASE;
    int scTab = swosReadSignedDword(teamBase + TEAMDATA_OFF_SHOT_CHANCE_TABLE);

    int16_t d0w, d1w, d2w;

    // ----- updatePlayers.cpp:970-1008 -- wrong-half guard -----------------
    {
        uint16_t gkYw0 = (uint16_t)swosReadSignedWord(spriteAddr + PLSPR_OFF_Y + 2);
        if (gkYw0 <= 449)
        {
            if (!topTeam) goto l_this_player_last_played;
        }
        else
        {
            if (topTeam) goto l_this_player_last_played;
        }
    }

    // ----- updatePlayers.cpp:1143-1177 -- ball in OUR penalty area? -------
    if (!ballInOurArea) goto l_check_pass_to_player;

    // ----- l_ball_in_penalty_area (:1179-1191) ---------------------------
    if (spriteAddr == swosReadSignedDword(ADDR_lastPlayerPlayed))
        goto l_this_player_last_played;

    swosUpdateBallVariables(spriteAddr, a2Ball, teamBase);
    swosCalculateBallNextGroundXYPositions(a2Ball);

    if (swosReadWord(ADDR_ballInGoalkeeperArea) != 0)
        goto l_ball_in_lower_goalkeeper_area;

    {
        uint16_t tick = swosReadWord(ADDR_currentGameTick);
        if ((uint16_t)((tick & 0xF0) >> 4) >= (uint16_t)shotChanceRow(scTab, 58))
            goto l_ball_standing_in_goalkeeper_area;
    }

l_ball_in_lower_goalkeeper_area:;
    d1w = swosReadSignedWord(ADDR_ballNextGroundX);
    if (d1w < 0) goto l_ball_standing_in_goalkeeper_area;

    d2w = swosReadSignedWord(ADDR_ballNextGroundY);

    if (topTeam)
    {
        if (d2w < 137) goto l_ball_standing_in_goalkeeper_area;
        if (d2w > 216) goto l_ball_standing_in_goalkeeper_area;
    }
    else
    {
        if (d2w < 682) goto l_ball_standing_in_goalkeeper_area;
        if (d2w > 761) goto l_ball_standing_in_goalkeeper_area;
    }
    if (d1w < 193) goto l_ball_standing_in_goalkeeper_area;
    if (d1w > 478) goto l_ball_standing_in_goalkeeper_area;

    // ----- l_in_penalty_area (:1354-1467) ----------------------------------
    {
        int16_t gkXw = swosReadSignedWord(spriteAddr + PLSPR_OFF_X + 2);
        int16_t gkYw = swosReadSignedWord(spriteAddr + PLSPR_OFF_Y + 2);
        int16_t bXw = swosBallSpriteXPixels();
        int16_t bYw = swosBallSpriteYPixels();
        int dxK = (int16_t)(d1w - gkXw); int dyK = (int16_t)(d2w - gkYw);
        int d1Acc = dxK * dxK + dyK * dyK;
        int dxB = (int16_t)(d1w - bXw); int dyB = (int16_t)(d2w - bYw);
        int d2Acc = dxB * dxB + dyB * dyB;
        if ((uint32_t)(d1Acc << 2) > (uint32_t)d2Acc)
            goto l_ball_standing_in_goalkeeper_area;

        swosWriteWord(spriteAddr + PLSPR_OFF_DEST_X, (uint16_t)d1w);
        swosWriteWord(spriteAddr + PLSPR_OFF_DEST_Y, (uint16_t)d2w);
        swosWriteWord(spriteAddr + PLSPR_OFF_SPEED,
            (uint16_t)swosReadSignedWord(ADDR_kGoalkeeperMoveToBallSpeed));
        goto cseg_7FCD0;
    }

l_ball_standing_in_goalkeeper_area:;
    if (swosReadSignedWord(ADDR_gameStatePl) != K_ST_GAME_IN_PROGRESS)
        goto l_this_player_last_played;

    {
        uint16_t strikeX = swosReadWord(ADDR_strikeDestX);
        if (strikeX < 295) goto l_shot_on_goal_or_close;
        if (strikeX <= 376) goto l_goal_attempt;
    }

l_shot_on_goal_or_close:;
    if ((uint32_t)swosReadSignedDword(spriteAddr + PLSPR_OFF_BALL_DISTANCE) >= 512u)
        goto l_goalkeeper_dont_throw;
    if (swosReadByte(teamBase + TEAMDATA_OFF_BALL_ABOVE_17) != 0)
        goto l_goalkeeper_dont_throw;
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_X, (uint16_t)swosBallSpriteXPixels());
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_Y, (uint16_t)swosBallSpriteYPixels());
    goto cseg_7FCD0;

l_goalkeeper_dont_throw:;
    if (swosReadByte(teamBase + TEAMDATA_OFF_BALL_ABOVE_17) != 0)
        goto cseg_7F511;

    if (swosReadWord(ADDR_ballInGoalkeeperArea) != 0)
        goto l_center_goalkeeper_on_ball_x;

    if ((uint16_t)swosBallSpriteYPixels() < 449)
    {
        if (swosBallSpriteDeltaY() >= 0) goto cseg_7F511;
    }
    else
    {
        if (swosBallSpriteDeltaY() <= 0) goto cseg_7F511;
    }

    {
        int16_t dx0 = (int16_t)(swosBallSpriteXPixels() - 336);
        dx0 = (int16_t)(dx0 >> 1);
        swosWriteWord(spriteAddr + PLSPR_OFF_DEST_X, (uint16_t)(int16_t)(dx0 + 336));
    }
    goto cseg_7F458;

l_center_goalkeeper_on_ball_x:;
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_X, (uint16_t)swosBallSpriteXPixels());

cseg_7F458:;
    {
        uint16_t bY = (uint16_t)swosBallSpriteYPixels();
        if (bY < 449)
        {
            d0w = (int16_t)((uint16_t)((uint16_t)(bY - 129) >> 1) + 129);
        }
        else
        {
            d0w = (int16_t)((uint16_t)((uint16_t)(769 - bY) >> 1) + bY);
        }
    }
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_Y, (uint16_t)d0w);
    swosWriteWord(spriteAddr + PLSPR_OFF_SPEED, (uint16_t)shotChanceRow(scTab, 6));
    goto cseg_7FCD0;

cseg_7F511:;
    if ((uint16_t)swosBallSpriteZPixels() > 16) goto cseg_7F626;
    if (swosReadSignedWord(ADDR_ballDefensiveX) < 0) goto cseg_7F626;

    {
        uint32_t d0Dist = (uint32_t)swosReadSignedDword(spriteAddr + PLSPR_OFF_BALL_DISTANCE);
        int opp = swosReadSignedDword(teamBase + TEAMDATA_OFF_OPPONENTS_TEAM);
        int cand;
        cand = swosReadSignedDword(teamBase + TEAMDATA_OFF_CONTROLLED_PLAYER);
        if (cand != 0 && d0Dist > (uint32_t)swosReadSignedDword(cand + PLSPR_OFF_BALL_DISTANCE))
            goto cseg_7F626;
        cand = swosReadSignedDword(teamBase + TEAMDATA_OFF_PASS_TO_PLAYER_PTR);
        if (cand != 0 && d0Dist > (uint32_t)swosReadSignedDword(cand + PLSPR_OFF_BALL_DISTANCE))
            goto cseg_7F626;
        cand = swosReadSignedDword(opp + TEAMDATA_OFF_CONTROLLED_PLAYER);
        if (cand != 0 && d0Dist > (uint32_t)swosReadSignedDword(cand + PLSPR_OFF_BALL_DISTANCE))
            goto cseg_7F626;
        cand = swosReadSignedDword(opp + TEAMDATA_OFF_PASS_TO_PLAYER_PTR);
        if (cand != 0 && d0Dist > (uint32_t)swosReadSignedDword(cand + PLSPR_OFF_BALL_DISTANCE))
            goto cseg_7F626;
    }
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_X, (uint16_t)swosReadSignedWord(ADDR_ballDefensiveX));
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_Y, (uint16_t)swosReadSignedWord(ADDR_ballDefensiveY));
    goto cseg_7FCD0;

cseg_7F626:;
    goto l_this_player_last_played;

l_goal_attempt:;
    if (teamBase == swosReadSignedDword(ADDR_lastTeamPlayed) &&
        swosReadSignedWord(ADDR_playerHadBall) == 0)
        goto cseg_7FBEF;

    if ((uint32_t)swosReadSignedDword(spriteAddr + PLSPR_OFF_BALL_DISTANCE) < 512u &&
        swosReadByte(teamBase + TEAMDATA_OFF_BALL_ABOVE_17) == 0)
    {
        swosWriteWord(spriteAddr + PLSPR_OFF_DEST_X, (uint16_t)swosBallSpriteXPixels());
        swosWriteWord(spriteAddr + PLSPR_OFF_DEST_Y, (uint16_t)swosBallSpriteYPixels());
        goto l_shot_at_goal;
    }

    if ((uint32_t)swosReadSignedDword(spriteAddr + PLSPR_OFF_BALL_DISTANCE) > 2048u)
        goto cseg_7F7BC;
    if (swosReadByte(teamBase + TEAMDATA_OFF_BALL_ABOVE_17) != 0)
        goto cseg_7F7BC;

    {
        int16_t dx0 = (int16_t)(swosBallSpriteXPixels() - 336);
        dx0 = (int16_t)(dx0 >> 1);
        swosWriteWord(spriteAddr + PLSPR_OFF_DEST_X, (uint16_t)(int16_t)(dx0 + 336));
    }
    {
        uint16_t bY = (uint16_t)swosBallSpriteYPixels();
        if (bY < 449)
        {
            d0w = (int16_t)((uint16_t)((uint16_t)(bY - 129) >> 1) + 129);
        }
        else
        {
            d0w = (int16_t)((uint16_t)((uint16_t)(769 - bY) >> 1) + bY);
        }
    }
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_Y, (uint16_t)d0w);
    swosWriteWord(spriteAddr + PLSPR_OFF_SPEED, (uint16_t)shotChanceRow(scTab, 6));
    goto l_shot_at_goal;

cseg_7F7BC:;
    switch (swosRunShotTripWire(spriteAddr, a2Ball, teamBase, topTeam))
    {
        case SWOS_SHOT_CHAIN_SHOT_AT_GOAL: goto l_shot_at_goal;
        case SWOS_SHOT_CHAIN_C7FBEF:       goto cseg_7FBEF;
        case SWOS_SHOT_CHAIN_C7FC01:       goto cseg_7FC01;
        default:                           goto cseg_7FC48;
    }

l_shot_at_goal:;
    switch (swosRunShotAtGoal(spriteAddr, a2Ball, teamBase, topTeam))
    {
        case SWOS_SHOT_CHAIN_C7FC01: goto cseg_7FC01;
        default:                     goto l_clamp_ball_y_inside_pitch;
    }

cseg_7FBEF:;
    swosWriteWord(spriteAddr + PLSPR_OFF_SPEED,
        (uint16_t)swosReadSignedWord(ADDR_dseg_1105EF));
    goto cseg_7FC23;

cseg_7FC01:;
    swosWriteWord(spriteAddr + PLSPR_OFF_SPEED, (uint16_t)shotChanceRow(scTab, 8));

cseg_7FC23:;
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_X, (uint16_t)swosReadSignedWord(ADDR_ballNotHighX));
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_Y, (uint16_t)swosReadSignedWord(ADDR_ballNotHighY));
    goto cseg_7FCD0;

cseg_7FC48:;
    if (swosReadByte(teamBase + TEAMDATA_OFF_BALL_ABOVE_17) != 0)
        goto cseg_7FC01;
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_X, (uint16_t)swosBallSpriteXPixels());
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_Y, (uint16_t)swosBallSpriteYPixels());
    swosWriteWord(spriteAddr + PLSPR_OFF_SPEED, (uint16_t)shotChanceRow(scTab, 6));
    goto cseg_7FCD0;

cseg_7FCD0:;
    // ----- :2605-2778 -- the CATCH chain -----------------------------------
    if (teamBase == swosReadSignedDword(ADDR_lastTeamPlayed) &&
        swosReadSignedWord(ADDR_playerHadBall) == 0)
        goto l_goalie_cant_catch_ball;

    if (swosReadWord(ADDR_ballInGoalkeeperArea) == 0)
    {
        uint16_t tick = swosReadWord(ADDR_currentGameTick);
        if ((uint16_t)((tick & 0xF0) >> 4) >= (uint16_t)shotChanceRow(scTab, 58))
            goto l_goalie_cant_catch_ball;
    }

    if (swosReadSignedWord(ADDR_ballNextGroundX) < 0)
        goto l_goalie_cant_catch_ball;
    {
        int16_t defZ = swosReadSignedWord(ADDR_ballDefensiveZ);
        if (defZ > 27) goto l_goalie_cant_catch_ball;
        if (defZ <= 12) goto l_goalie_cant_catch_ball;
    }

    if ((uint32_t)swosReadSignedDword(spriteAddr + PLSPR_OFF_BALL_DISTANCE) > 2116u)
        goto l_goalie_cant_catch_ball;

    {
        int16_t dxw = (int16_t)(swosReadSignedWord(spriteAddr + PLSPR_OFF_X + 2)
                            - swosReadSignedWord(ADDR_ballDefensiveX));
        if (dxw > 12) goto l_goalie_cant_catch_ball;
        if (dxw < -12) goto l_goalie_cant_catch_ball;
        int16_t dyw = (int16_t)(swosReadSignedWord(spriteAddr + PLSPR_OFF_Y + 2)
                            - swosReadSignedWord(ADDR_ballDefensiveY));
        if (dyw > 12) goto l_goalie_cant_catch_ball;
        if (dyw < -12) goto l_goalie_cant_catch_ball;
    }

    swosGoalkeeperCaughtTheBall(spriteAddr, topTeam);

l_goalie_cant_catch_ball:;
    if (runGoalieCantCatchBallPickup(spriteAddr, teamBase, topTeam))
    {
        swosUpdatePlayerSpeedAndFrameDelay(teamBase, spriteAddr);
        return;
    }
    goto l_clamp_ball_y_inside_pitch;

l_check_pass_to_player:;
    swosWriteWord(teamBase + TEAMDATA_OFF_BALL_OUT_OF_PLAY_OR_KEEPER, 0);
    if (spriteAddr == swosReadSignedDword(teamBase + TEAMDATA_OFF_PASS_TO_PLAYER_PTR))
    {
        swosWriteDword(teamBase + TEAMDATA_OFF_PASS_TO_PLAYER_PTR, 0);
        swosWriteWord(teamBase + TEAMDATA_OFF_PASSING_BALL, 0);
        swosWriteWord(teamBase + TEAMDATA_OFF_PASSING_TO_PLAYER, 0);
    }
    // fall through

l_this_player_last_played:;
    {
        int frame = swosReadSignedDword(ADDR_frameCounter);
        if ((frame & 0x0E) != 0) goto l_clamp_ball_y_inside_pitch;
        // Design decision (see swos_update_players.h): direct call, no
        // exception emulation.
        swosSetPlayerWithNoBallDestination(spriteAddr, teamBase,
            swosBallSpriteXPixels(), swosBallSpriteYPixels());
    }

l_clamp_ball_y_inside_pitch:;
    {
        int16_t destYc = swosReadSignedWord(spriteAddr + PLSPR_OFF_DEST_Y);
        if (destYc < 129)
            swosWriteWord(spriteAddr + PLSPR_OFF_DEST_Y, 129);
        destYc = swosReadSignedWord(spriteAddr + PLSPR_OFF_DEST_Y);
        if (destYc > 769)
            swosWriteWord(spriteAddr + PLSPR_OFF_DEST_Y, 769);
    }
    swosUpdatePlayerSpeedAndFrameDelay(teamBase, spriteAddr);
}

// ===========================================================================
// runGoalieCantCatchBallPickup -- updatePlayers.cpp:2780-2905
// (l_goalie_cant_catch_ball .. l_opponent_player_touched_the_ball)
// ===========================================================================
static bool runGoalieCantCatchBallPickup(int spriteAddr, int teamBase, bool topTeam)
{
    int lastPlayerPlayed = swosReadSignedDword(ADDR_lastPlayerPlayed);
    if (spriteAddr == lastPlayerPlayed)
        return false;

    int ballDist = swosReadSignedDword(spriteAddr + PLSPR_OFF_BALL_DISTANCE);
    if ((uint32_t)ballDist > 72)
        return false;

    if (swosReadByte(teamBase + TEAMDATA_OFF_BALL_ABOVE_17) != 0)
        return false;

    uint8_t plState = swosReadByte(spriteAddr + PLSPR_OFF_PLAYER_STATE);
    if (plState != PLSTATE_GOALIE_CATCHING_BALL)
    {
        if (swosReadByte(teamBase + TEAMDATA_OFF_BALL_12_TO_17) != 0)
            return false;
    }

    int16_t gkX = swosReadSignedWord(spriteAddr + PLSPR_OFF_X + 2);
    int16_t gkY = swosReadSignedWord(spriteAddr + PLSPR_OFF_Y + 2);
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_X, (uint16_t)gkX);
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_Y, (uint16_t)gkY);

    int lastTeamPlayed = swosReadSignedDword(ADDR_lastTeamPlayed);
    int16_t playerHadBall = swosReadSignedWord(ADDR_playerHadBall);
    if (teamBase == lastTeamPlayed && playerHadBall == 0)
    {
        // ----- :2867-2893 -- BACK-PASS: keeper plays on with his feet ------
        swosUpdateBallWithControllingGoalkeeper(spriteAddr);
        swosWriteDword(teamBase + TEAMDATA_OFF_CONTROLLED_PLAYER, (uint32_t)spriteAddr);
        swosWriteDword(teamBase + TEAMDATA_OFF_PASS_TO_PLAYER_PTR, 0);
        swosWriteWord(teamBase + TEAMDATA_OFF_PLAYER_SWITCH_TIMER, 25);
        swosWriteWord(teamBase + TEAMDATA_OFF_PASSING_BALL, 0);
        swosWriteWord(teamBase + TEAMDATA_OFF_PASSING_TO_PLAYER, 0);
        swosWriteWord(spriteAddr + PLSPR_OFF_DEST_X, (uint16_t)gkX);
        swosWriteWord(spriteAddr + PLSPR_OFF_DEST_Y, (uint16_t)gkY);
        swosWriteDword(ADDR_lastTeamPlayed, (uint32_t)teamBase);
        swosWriteDword(ADDR_lastPlayerPlayed, (uint32_t)spriteAddr);
        swosWriteWord(ADDR_penalty, 0);
        swosWriteWord(ADDR_playerHadBall, 0);
        swosWriteWord(teamBase + TEAMDATA_OFF_PASS_KICK_TIMER, 0);
        swosWriteDword(teamBase + TEAMDATA_OFF_PASSING_KICKING_PLAYER, 0);
        swosWriteWord(teamBase + 116, 0);
        swosWriteWord(teamBase + TEAMDATA_OFF_GOALKEEPER_PLAYING, 1);
        swosWriteByte(teamBase + TEAMDATA_OFF_GOALIE_PLAYING_OR_OUT, 1);
        swosWriteByte(teamBase + TEAMDATA_OFF_BALL_OUT_OF_PLAY_OR_KEEPER, 1);
        return true;
    }

    // ----- l_opponent_player_touched_the_ball :2895-2905 -- hand claim -----
    swosWriteDword(ADDR_lastKeeperPlayed, (uint32_t)lastPlayerPlayed);
    swosWriteDword(ADDR_lastTeamPlayed, (uint32_t)teamBase);
    swosWriteDword(ADDR_lastPlayerPlayed, (uint32_t)spriteAddr);
    swosWriteWord(ADDR_penalty, 0);
    swosWriteWord(ADDR_playerHadBall, 0);
    swosUpdateBallWithControllingGoalkeeper(spriteAddr);
    swosGoalkeeperClaimedTheBall(spriteAddr, topTeam);
    return false;
}

// ===========================================================================
// tickGoalieDiving -- l_goalie_diving, updatePlayers.cpp:3188-3435
// (+ simplified collision stand-in for 3537-4090)
// ===========================================================================
static void tickGoalieDiving(int spriteAddr, int teamBase, bool topTeam)
{
    uint8_t downTimer = swosReadByte(spriteAddr + PLSPR_OFF_PLAYER_DOWN_TIMER);
    downTimer = (uint8_t)(downTimer - 1);
    swosWriteByte(spriteAddr + PLSPR_OFF_PLAYER_DOWN_TIMER, downTimer);
    if (downTimer == 0)
    {
        goalkeeperRise(spriteAddr, teamBase);
        return;
    }

    int16_t divingRight = swosReadSignedWord(teamBase + TEAMDATA_OFF_GOALKEEPER_DIVING_RIGHT);
    if (divingRight != 0 && downTimer <= 42)
    {
        swosWriteWord(teamBase + TEAMDATA_OFF_GOALKEEPER_DIVING_RIGHT, 0);
        swosUpdateBallWithControllingGoalkeeper(spriteAddr);
        swosGoalkeeperClaimedTheBall(spriteAddr, topTeam);
        swosBallSpriteSetZPixels(5);
        goalkeeperRise(spriteAddr, teamBase);
        return;
    }

    int16_t gsPl = swosReadSignedWord(ADDR_gameStatePl);
    if (gsPl == K_ST_GAME_IN_PROGRESS && downTimer <= 60)
    {
        goalkeeperRise(spriteAddr, teamBase);
        return;
    }

    // cseg_80282 -- pick the dive speed-decay constant.
    int16_t d0Decay = 112;
    uint8_t field46 = swosReadByte(teamBase + 70);
    if (field46 == 0)
    {
        int16_t img = swosReadSignedWord(spriteAddr + PLSPR_OFF_IMAGE_INDEX);
        if (img == 976 || img == 978 || img == 1106 || img == 1108 ||
            img == 1092 || img == 1094 || img == 990 || img == 992)
        {
            d0Decay = 192;
        }
        else
        {
            int16_t ballYd = swosBallSpriteYPixels();
            int16_t keepYd = swosReadSignedWord(spriteAddr + PLSPR_OFF_Y + 2);
            int16_t d1Diff = (int16_t)(ballYd - keepYd);
            if (teamBase == TEAMDATA_TOP_BASE)
            {
                if (d1Diff < -5) d0Decay = 128;
            }
            else
            {
                if (d1Diff > 5) d0Decay = 128;
            }
        }
    }

    int16_t diveSpeed = swosReadSignedWord(spriteAddr + PLSPR_OFF_SPEED);
    diveSpeed = (int16_t)(diveSpeed - d0Decay);
    if (diveSpeed < 0) diveSpeed = 0;
    swosWriteWord(spriteAddr + PLSPR_OFF_SPEED, (uint16_t)diveSpeed);

    if (gsPl != K_ST_GAME_IN_PROGRESS || field46 != 0)
    {
        swosUpdatePlayerSpeedAndFrameDelay(teamBase, spriteAddr);
        return;
    }

    int ballDeltaY = swosBallSpriteDeltaY();
    if (teamBase == TEAMDATA_TOP_BASE)
    {
        if (ballDeltaY >= 0)
        {
            swosUpdatePlayerSpeedAndFrameDelay(teamBase, spriteAddr);
            return;
        }
    }
    else
    {
        if (ballDeltaY <= 0)
        {
            swosUpdatePlayerSpeedAndFrameDelay(teamBase, spriteAddr);
            return;
        }
    }

    // ===== cseg_80404 -- diving-keeper ball contact + outcome verdict ======
    {
        int16_t ballXw = swosBallSpriteXPixels();
        int16_t ballYw = swosBallSpriteYPixels();
        int16_t keeperXw = swosReadSignedWord(spriteAddr + PLSPR_OFF_X + 2);
        int16_t keeperYw = swosReadSignedWord(spriteAddr + PLSPR_OFF_Y + 2);
        uint16_t tick = swosReadWord(ADDR_currentGameTick);
        int oppBase = swosReadSignedDword(teamBase + TEAMDATA_OFF_OPPONENTS_TEAM);
        int16_t commentTimer = 0;

        int16_t img = swosReadSignedWord(spriteAddr + PLSPR_OFF_IMAGE_INDEX);
        if (img < 0) goto l_dive_no_contact;

        // STUB (cited): SpriteGraphics tables not ported yet -- keeper dive
        // frames span ~24 px. TODO from updatePlayers.cpp:3548.
        {
            int16_t pixWidth = 24;
            int keeperDeltaX = swosReadSignedDword(spriteAddr + PLSPR_OFF_DELTA_X);
            if (keeperDeltaX < 0)
            {
                int16_t d1 = (int16_t)(keeperXw - 2 - 3);
                int16_t d0 = (int16_t)(pixWidth + 6);
                if ((uint16_t)d1 >= (uint16_t)ballXw) goto l_dive_no_contact;
                d1 = (int16_t)(d1 + d0);
                if ((uint16_t)d1 <= (uint16_t)ballXw) goto l_dive_no_contact;
            }
            else
            {
                int16_t d1 = (int16_t)(keeperXw + 2 + 3);
                int16_t d0 = (int16_t)(pixWidth + 6);
                if ((uint16_t)d1 < (uint16_t)ballXw) goto l_dive_no_contact;
                d1 = (int16_t)(d1 - d0);
                if ((uint16_t)d1 > (uint16_t)ballXw) goto l_dive_no_contact;
            }
        }

        if ((uint16_t)swosBallSpriteZPixels() > 20) goto l_dive_no_contact;

        bool isPenaltyDive =
            swosReadSignedWord(ADDR_playingPenalties) != 0 ||
            swosReadSignedWord(ADDR_penalty) != 0;
        if (isPenaltyDive)
        {
            int16_t dy0 = (int16_t)(keeperYw - ballYw);
            if (dy0 > 5) goto l_dive_no_contact;
            if (dy0 < -5) goto l_dive_no_contact;
            int16_t spd = swosReadSignedWord(spriteAddr + PLSPR_OFF_SPEED);
            spd = (int16_t)(spd - (int16_t)((uint16_t)spd >> 2));
            swosWriteWord(spriteAddr + PLSPR_OFF_SPEED, (uint16_t)spd);
        }
        else
        {
            int16_t dy0 = (int16_t)(keeperYw - ballYw);
            if (dy0 > 7) goto l_dive_no_contact;
            if (dy0 < -7) goto l_dive_no_contact;
            swosWriteWord(teamBase + TEAMDATA_OFF_PASS_KICK_TIMER, 25);
            swosWriteWord(oppBase + TEAMDATA_OFF_PASSING_TO_PLAYER, 0);
            swosWriteDword(oppBase + TEAMDATA_OFF_PASSING_KICKING_PLAYER, 0);
        }

        swosWriteByte(teamBase + 70, 1);
        {
            int16_t spd = swosReadSignedWord(spriteAddr + PLSPR_OFF_SPEED);
            uint8_t st = swosReadByte(spriteAddr + PLSPR_OFF_PLAYER_STATE);
            int16_t cut = st == PLSTATE_GOALIE_DIVING_HIGH
                ? (int16_t)(spd >> 2)
                : (int16_t)(spd >> 1);
            swosWriteWord(spriteAddr + PLSPR_OFF_SPEED, (uint16_t)(int16_t)(spd - cut));
        }

        int rowPtr = swosReadSignedDword(teamBase + TEAMDATA_OFF_SHOT_CHANCE_TABLE);
        if (isPenaltyDive) rowPtr = ADDR_dseg_17EECC;

        swosWriteWord(ADDR_penalty, 0);
        swosWriteWord(ADDR_playerHadBall, 0);
        swosWriteWord(teamBase + TEAMDATA_OFF_PASS_KICK_TIMER, 25);
        swosWriteWord(oppBase + TEAMDATA_OFF_PASSING_TO_PLAYER, 0);

        commentTimer = swosTeamDataGoalkeeperSavedCommentTimer(topTeam);
        if (commentTimer >= 0)
            swosWriteDword(oppBase + TEAMDATA_OFF_PASSING_KICKING_PLAYER, 0);

        if (swosReadWord(ADDR_ballInGoalkeeperArea) == 0)
        {
            int statsPtr = swosReadSignedDword(oppBase + TEAMDATA_OFF_TEAM_STATS_PTR);
            if (statsPtr != 0)
            {
                int16_t ga = swosReadSignedWord(statsPtr + 10);
                swosWriteWord(statsPtr + 10, (uint16_t)(int16_t)(ga + 1));
            }
        }

        swosResetBothTeamSpinTimers();

        // ----- the outcome verdict (:3934-4024) -----------------------------
        {
            int16_t sample = (int16_t)((tick & 0xF0) >> 4);
            int16_t e21 = rowPtr != 0 ? swosReadSignedWord(rowPtr + 42) : (int16_t)1;
            int16_t e22 = rowPtr != 0 ? swosReadSignedWord(rowPtr + 44) : (int16_t)6;
            sample = (int16_t)(sample - e21);
            if (sample < 0) goto cseg_807CB;
            sample = (int16_t)(sample - e22);
            if (commentTimer < 0)
            {
                if (sample < 0) goto cseg_80A1B;
                goto cseg_808BF;
            }
            if (sample < 0) goto cseg_808BF;
            goto cseg_80A1B;
        }

    cseg_807CB:;
        {
            int16_t dx0 = (int16_t)(keeperXw - ballXw);
            if (dx0 > 6) goto cseg_80A1B;
            if (dx0 < -6) goto cseg_80A1B;
            swosWriteDword(ADDR_lastTeamPlayed, (uint32_t)teamBase);
            swosWriteDword(ADDR_lastPlayerPlayed, (uint32_t)spriteAddr);
            swosBallSpriteSetXPixels(keeperXw);
            swosBallSpriteSetYPixels(keeperYw);
            swosBallSpriteSetDestX(keeperXw);
            swosBallSpriteSetDestY(keeperYw);
            swosBallSpriteSetSpeed(0);
            swosWriteWord(teamBase + TEAMDATA_OFF_GOALKEEPER_DIVING_RIGHT, 1);
            swosGoalkeeperClaimedTheBall(spriteAddr, topTeam);
            // MatchAudio.KeeperSavedComment()/PlayMissGoal() omitted: pure
            // audio, zero Memory effect (Audio/MatchAudio.cs).
            goto l_dive_done;
        }

    cseg_808BF:;
        {
            swosWriteDword(ADDR_lastKeeperPlayed,
                (uint32_t)swosReadSignedDword(ADDR_lastPlayerPlayed));
            swosWriteDword(ADDR_lastTeamPlayed, (uint32_t)teamBase);
            swosWriteDword(ADDR_lastPlayerPlayed, (uint32_t)spriteAddr);
            int16_t bspd = swosBallSpriteSpeed();
            bspd = (int16_t)(bspd - (int16_t)((uint16_t)bspd >> 2));
            swosBallSpriteSetSpeed(bspd);
            if ((uint16_t)swosBallSpriteSpeed() > 1792)
                swosBallSpriteSetSpeed(1792);
            if (commentTimer < 0)
            {
                int16_t bend = (int16_t)(((tick & 15) << 7) - 960);
                swosBallSpriteSetDestX((int16_t)(swosBallSpriteDestX() + bend));
                goto cseg_80B1D;
            }
            int16_t d1y = swosBallSpriteYPixels();
            int16_t d0y = (int16_t)(swosBallSpriteDestY() - d1y);
            int16_t shift = swosReadSignedWord(ADDR_dseg_110BDB + (tick & 0x0E));
            d0y = swosAsr32(d0y, shift & 0x1F);
            swosBallSpriteSetDestY((int16_t)(d1y + d0y));
            swosBallSpriteSetDestX(swosReadSignedWord(spriteAddr + PLSPR_OFF_DEST_X));
            goto cseg_80B1D;
        }

    cseg_80A1B:;
        {
            swosWriteDword(ADDR_lastTeamPlayed, (uint32_t)teamBase);
            swosWriteDword(ADDR_lastPlayerPlayed, (uint32_t)spriteAddr);
            int16_t d1y = swosBallSpriteYPixels();
            int16_t d0y = (int16_t)(swosBallSpriteDestY() - d1y);
            d1y = (int16_t)(d1y - d0y);
            int16_t noise = (int16_t)(((tick & 31) << 4) - 256);
            d1y = (int16_t)(d1y + noise);
            swosBallSpriteSetDestY(d1y);
            swosBallSpriteSetDestX(swosReadSignedWord(spriteAddr + PLSPR_OFF_DEST_X));
            int16_t bspd = swosBallSpriteSpeed();
            int16_t quarter = (int16_t)((uint16_t)bspd >> 2);
            bspd = (int16_t)(bspd - quarter);
            swosBallSpriteSetSpeed(bspd);
            if ((tick & 0x10) != 0)
            {
                swosBallSpriteSetSpeed((int16_t)(swosBallSpriteSpeed() - (int16_t)((uint16_t)quarter >> 1)));
            }
            if ((uint16_t)swosBallSpriteSpeed() > 1536)
                swosBallSpriteSetSpeed(1536);
            goto cseg_80B1D;
        }

    cseg_80B1D:;
        {
            int16_t d0 = (int16_t)(topTeam ? 1 : -1);
            swosBallSpriteSetYPixels((int16_t)(keeperYw + d0));
        }
        swosWriteWord(teamBase + TEAMDATA_OFF_PASS_KICK_TIMER, 25);
        swosWriteWord(oppBase + TEAMDATA_OFF_PASSING_TO_PLAYER, 0);
        if (swosTeamDataGoalkeeperSavedCommentTimer(topTeam) >= 0)
            swosWriteDword(oppBase + TEAMDATA_OFF_PASSING_KICKING_PLAYER, 0);
        // MatchAudio.KeeperSavedComment()/PlayMissGoal() omitted (audio).

    l_dive_done:;
    l_dive_no_contact:;
        swosUpdatePlayerSpeedAndFrameDelay(teamBase, spriteAddr);
    }
}

// l_goalkeeper_rise -- updatePlayers.cpp:3205-3210 -> l_stop_player
// (10079-10084) -> l_update_player_speed_and_deltas.
static void goalkeeperRise(int spriteAddr, int teamBase)
{
    swosWriteByte(spriteAddr + PLSPR_OFF_PLAYER_STATE, PLSTATE_NORMAL);
    swosSetPlayerAnimationTable(spriteAddr, ADDR_kPlayerStandingAnimTableAddr);
    int16_t sx = swosReadSignedWord(spriteAddr + PLSPR_OFF_X + 2);
    int16_t sy = swosReadSignedWord(spriteAddr + PLSPR_OFF_Y + 2);
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_X, (uint16_t)sx);
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_Y, (uint16_t)sy);
    swosUpdatePlayerSpeedAndFrameDelay(teamBase, spriteAddr);
}

// bottomBallOutOfPlayPositions / topBallOutOfPlayPositions -- swos.asm:
// 245963-246006. "indexed by gameState, states < 32; each pointer points to
// a table of 22 elements, x & y position". A NULL entry means "no fixed
// table for this gameState -- fall back to SetPlayerWithNoBallDestination".
// The pointed-to tables themselves are mechanically extracted (see
// generated/swos_update_players_tables.h); only this dispatch-by-index
// structure (which name goes at which slot, and which slots are NULL) is
// hand-written, mirroring the C# `short[]?[]` initializer -- extract_table.py
// has no jagged-array support, and the underlying numeric data is still 100%
// mechanically extracted.
#define K_BALL_OUT_OF_PLAY_POSITIONS_COUNT 32
static const int16_t *const kBottomBallOutOfPlayPositions[K_BALL_OUT_OF_PLAY_POSITIONS_COUNT] = {
    kBottomStartingPositions,   //  0 -- post-goal / kick-off walk-back
    kDseg17E831,                //  1 -- ST_GOAL_OUT_LEFT (bypassed at 9289-9300)
    kDseg17E889,                //  2 -- ST_GOAL_OUT_RIGHT (bypassed)
    kDseg17E8E1,                //  3 -- ST_KEEPER_HOLDS_BALL (bypassed)
    kDseg17E781,                //  4 -- ST_CORNER_LEFT
    kDseg17E7D9,                //  5 -- ST_CORNER_RIGHT
    kDseg17EA6D,                //  6 -- ST_FREE_KICK_LEFT1
    kDseg17EB1D,                //  7
    kDseg17EBCD,                //  8
    kDseg17E9BD,                //  9
    kDseg17EB75,                // 10
    kDseg17EAC5,                // 11
    kDseg17EA15,                // 12 -- ST_FREE_KICK_RIGHT3
    NULL,                       // 13
    kDseg17EBF9,                // 14 -- throw-in
    kDseg17EC51,                // 15
    kDseg17EC7D,                // 16
    kDseg17ECA9,                // 17
    kDseg17ECD5,                // 18
    kDseg17ED01,                // 19
    kDseg17ED31,                // 20
    kDseg17ED5D,                // 21 -- penalty kick
    kDseg17ED5D,                // 22
    kPlayersLeavingPitchBottom, // 23 -- going to halftime
    kPlayersLeavingPitchBottom, // 24 -- going to shower
    kPlayersLeavingPitchBottom, // 25
    kPlayersLeavingPitchBottom, // 26
    kDseg17EE0D,                // 27 -- second-half restart
    kDseg17EE0D,                // 28
    NULL,                       // 29 -- ST_FIRST_HALF_ENDED (bailed earlier)
    NULL,                       // 30 -- ST_GAME_ENDED (bailed earlier)
    kBottomPenaltyPositions,    // 31 -- ST_PENALTIES
};

static const int16_t *const kTopBallOutOfPlayPositions[K_BALL_OUT_OF_PLAY_POSITIONS_COUNT] = {
    kTopStartingPositions,      //  0 -- post-goal / kick-off walk-back
    kDseg17E85D,                //  1
    kDseg17E8B5,                //  2
    kDseg17E90D,                //  3
    kDseg17E7AD,                //  4 -- ST_CORNER_LEFT
    kDseg17E805,                //  5 -- ST_CORNER_RIGHT
    kDseg17EA41,                //  6 -- ST_FREE_KICK_LEFT1
    kDseg17EAF1,                //  7
    kDseg17EBA1,                //  8
    kDseg17E991,                //  9
    kDseg17EB49,                // 10
    kDseg17EA99,                // 11
    kDseg17E9E9,                // 12 -- ST_FREE_KICK_RIGHT3
    NULL,                       // 13
    kDseg17EC25,                // 14 -- throw-in
    NULL,                       // 15
    NULL,                       // 16
    NULL,                       // 17
    NULL,                       // 18
    NULL,                       // 19
    NULL,                       // 20
    kDseg17ED89,                // 21 -- penalty kick
    kDseg17ED89,                // 22
    kPlayersLeavingPitchTop,    // 23
    kPlayersLeavingPitchTop,    // 24
    kPlayersLeavingPitchTop,    // 25
    kPlayersLeavingPitchTop,    // 26
    kDseg17EE39,                // 27
    kDseg17EE39,                // 28
    NULL,                       // 29
    NULL,                       // 30
    kTopPenaltyPositions,       // 31 -- ST_PENALTIES
};

// updatePlayers.cpp:9209-9934 (l_set_player_positions_if_game_break chain).
static void setPlayerPositionsForGameBreak(int spriteAddr, int teamBase, bool topTeam)
{
    int16_t gameStatePl = swosReadSignedWord(ADDR_gameStatePl);
    int16_t gameState   = swosReadSignedWord(ADDR_gameState);

    int16_t d0State;
    int16_t d6;
    int16_t d7;

    int16_t playingPenalties = swosReadSignedWord(ADDR_playingPenalties);
    if (playingPenalties != 0)
    {
        d0State = 31;
    }
    else
    {
        if (gameStatePl == K_ST_GAME_IN_PROGRESS)
        {
            d6 = swosBallSpriteXPixels();
            d7 = swosBallSpriteYPixels();
            setPlayerWithNoBallDestinationForBreak(spriteAddr, teamBase, d6, d7, gameStatePl);
            return;
        }
        d0State = gameState;
    }

    d6 = swosReadSignedWord(ADDR_foulXCoordinate);
    d7 = swosReadSignedWord(ADDR_foulYCoordinate);

    if (gameState == 29 || gameState == 30) return;

    if (gameState == 3 || gameState == 1 || gameState == 2)
    {
        setPlayerWithNoBallDestinationForBreak(spriteAddr, teamBase, d6, d7, gameStatePl);
        return;
    }

    bool useTopTable;
    int16_t forceLeftTeam = swosReadSignedWord(ADDR_forceLeftTeam);
    int lastTeamBeforeBreak = swosReadSignedDword(ADDR_lastTeamPlayedBeforeBreak);
    if (forceLeftTeam == 1 && (gameState == 4 || gameState == 5) && d7 < 449)
    {
        useTopTable = true;
    }
    else if (playingPenalties != 0)
    {
        useTopTable = teamBase != TEAMDATA_BOTTOM_BASE;
    }
    else
    {
        useTopTable = teamBase != lastTeamBeforeBreak;
    }

    const int16_t *const *positionsList = useTopTable ? kTopBallOutOfPlayPositions
                                                        : kBottomBallOutOfPlayPositions;
    const int16_t *positions = (d0State >= 0 && d0State < K_BALL_OUT_OF_PLAY_POSITIONS_COUNT)
        ? positionsList[d0State]
        : NULL;
    if (positions == NULL)
    {
        setPlayerWithNoBallDestinationForBreak(spriteAddr, teamBase, d6, d7, gameStatePl);
        return;
    }

    int16_t freeKickDestX = 0;
    int16_t myOrdinal = swosReadSignedWord(spriteAddr + PLSPR_OFF_PLAYER_ORDINAL);
    int16_t d0Slot;

    bool inFreeKickWall =
        teamBase != lastTeamBeforeBreak &&
        gameState > 6 && gameState < 12 &&
        myOrdinal >= 2 && myOrdinal <= 5;
    if (inFreeKickWall)
    {
        int16_t d0Count = 2;
        int walkSlot = 1;
        for (int16_t d1Ord = 2; d1Ord != myOrdinal && walkSlot <= 10; d1Ord++)
        {
            int sa = swosTeamDataGetTeamSpriteAddr(topTeam, walkSlot);
            walkSlot++;
            if (sa == 0) continue;
            int16_t cards = swosReadSignedWord(sa + PLSPR_OFF_CARDS);
            if (cards >= 0) d0Count++;
        }

        int16_t factor = kFreeKickFactorsX[gameState - 6];
        freeKickDestX = (int16_t)(factor << 2);

        int takerSlot = 1;
        for (int16_t d1Taker = 3; d1Taker >= 0; d1Taker--)
        {
            int sa = swosTeamDataGetTeamSpriteAddr(topTeam, takerSlot);
            takerSlot++;
            if (sa == 0) continue;
            int16_t cards = swosReadSignedWord(sa + PLSPR_OFF_CARDS);
            if (cards >= 0)
                freeKickDestX = (int16_t)(freeKickDestX - factor);
        }

        d0Slot = d0Count;
    }
    else
    {
        d0Slot = myOrdinal;
    }

    int pairIdx = (d0Slot - 1) * 2;
    if (pairIdx < 0 || pairIdx + 1 >= 22)
    {
        setPlayerWithNoBallDestinationForBreak(spriteAddr, teamBase, d6, d7, gameStatePl);
        return;
    }
    int16_t tblX = positions[pairIdx];
    int16_t tblY = positions[pairIdx + 1];
    if (tblX == 22222)
    {
        setPlayerWithNoBallDestinationForBreak(spriteAddr, teamBase, d6, d7, gameStatePl);
        return;
    }

    int16_t d1 = tblX;
    int16_t d2 = tblY;
    bool foulRelative;

    if (teamBase == TEAMDATA_TOP_BASE)
    {
        if (d1 <= -1000)
        {
            d1 = (int16_t)(d1 + 1000);
            d1 = (int16_t)(d1 + freeKickDestX);
            foulRelative = true;
        }
        else if (d1 >= 1000)
        {
            d1 = (int16_t)(d1 - 1000);
            d1 = (int16_t)(d1 + freeKickDestX);
            foulRelative = true;
        }
        else
        {
            d1 = (int16_t)(d1 + 5);
            foulRelative = false;
        }
    }
    else
    {
        if (d1 <= -1000)
        {
            d1 = (int16_t)(d1 + 1000);
            d1 = (int16_t)(-d1);
            d2 = (int16_t)(-d2);
            d1 = (int16_t)(d1 - freeKickDestX);
            foulRelative = true;
        }
        else if (d1 >= 1000)
        {
            d1 = (int16_t)(d1 - 1000);
            d1 = (int16_t)(-d1);
            d2 = (int16_t)(-d2);
            d1 = (int16_t)(d1 - freeKickDestX);
            foulRelative = true;
        }
        else
        {
            d1 = (int16_t)(509 - tblX - 5);
            d2 = (int16_t)(640 - tblY);
            foulRelative = false;
        }
    }

    int16_t destX, destY;
    if (foulRelative)
    {
        destX = (int16_t)(d1 + d6);
        destY = (int16_t)(d2 + d7);
    }
    else
    {
        destX = (int16_t)(d1 + 81);
        destY = (int16_t)(d2 + 129);
    }
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_X, (uint16_t)destX);
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_Y, (uint16_t)destY);
}

// l_set_player_with_no_ball_destination + cseg_83A41 --
// updatePlayers.cpp:9936-10077.
static void setPlayerWithNoBallDestinationForBreak(int spriteAddr, int teamBase,
                                                     int16_t d6, int16_t d7,
                                                     int16_t gameStatePl)
{
    // Design decision (see swos_update_players.h): direct call, no exception
    // emulation. Unlike the C# catch block (which is documented as "DO NOT
    // return" -- the cseg_83A41 pushback below must still run even if the
    // tactics dest write failed), our direct call has no failure path to
    // skip, so the fall-through here is unconditional -- equivalent net
    // effect.
    swosSetPlayerWithNoBallDestination(spriteAddr, teamBase, d6, d7);

    if (gameStatePl == K_ST_GAME_IN_PROGRESS) return;

    int16_t ordinal = swosReadSignedWord(spriteAddr + PLSPR_OFF_PLAYER_ORDINAL);
    if (ordinal == 1) return;

    int lastTeamBeforeBreak = swosReadSignedDword(ADDR_lastTeamPlayedBeforeBreak);
    if (teamBase == lastTeamBeforeBreak) return;

    int16_t destX = swosReadSignedWord(spriteAddr + PLSPR_OFF_DEST_X);
    int16_t destY = swosReadSignedWord(spriteAddr + PLSPR_OFF_DEST_Y);
    int16_t foulX = swosReadSignedWord(ADDR_foulXCoordinate);
    int16_t foulY = swosReadSignedWord(ADDR_foulYCoordinate);
    int dx = (int16_t)(destX - foulX);
    int dy = (int16_t)(destY - foulY);
    int distSq = dx * dx + dy * dy;
    if ((uint32_t)distSq > 4225u) return;

    int16_t pushedX = foulX;
    if (pushedX >= 336) pushedX = (int16_t)(pushedX - 70);
    else                pushedX = (int16_t)(pushedX + 70);
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_X, (uint16_t)pushedX);
}

// ===========================================================================
// tickPassExpectingStopped -- l_player_expecting_pass, GAME-STOPPED slice.
// updatePlayers.cpp:7604-8067 + cseg_82EC2 (8312-8430) + cseg_8308D
// (8563-8634) + l_stop_player (10079-10084).
// ===========================================================================
static void tickPassExpectingStopped(int spriteAddr, bool topTeam)
{
    int teamBase = swosTeamDataBase(topTeam);

    int16_t px = swosReadSignedWord(spriteAddr + PLSPR_OFF_X + 2);
    int16_t py = swosReadSignedWord(spriteAddr + PLSPR_OFF_Y + 2);
    if (px < 81 || px > 590 || py < 129 || py > 769)
    {
        swosWriteWord(teamBase + TEAMDATA_OFF_PASSING_TO_PLAYER, 0);
        swosWriteWord(teamBase + TEAMDATA_OFF_PASSING_BALL, 0);
        goto l_stop_player;
    }

    // ----- updatePlayers.cpp:7892-7908 -- l_cpu_passing_to -----------------
    {
        int16_t pnPass = swosReadSignedWord(teamBase + TEAMDATA_OFF_PLAYER_NUMBER);
        if (pnPass == 0)
        {
            requireAiKick(spriteAddr, teamBase);
        }
    }

    // ----- cseg_82AAF -- updatePlayers.cpp:7910-7935 ------------------------
    {
        int lastBeforeBreak = swosReadSignedDword(ADDR_lastTeamPlayedBeforeBreak);
        if (lastBeforeBreak != teamBase)
            goto l_update_player_speed_and_deltas;
    }

    // ----- updatePlayers.cpp:7937-7972 --------------------------------------
    {
        int16_t gsPass = swosReadSignedWord(ADDR_gameState);
        int16_t ordPass = swosReadSignedWord(spriteAddr + PLSPR_OFF_PLAYER_ORDINAL);
        if (gsPass == 3 && ordPass == 1)
            goto l_pass_success_entry;
        int bdPass = swosReadSignedDword(spriteAddr + PLSPR_OFF_BALL_DISTANCE);
        if (bdPass != 0)
            goto l_chase_or_hold;
    }

l_pass_success_entry:;
    // ----- cseg_82AF6 -- updatePlayers.cpp:7974-8002 ------------------------
    {
        int16_t camDir = swosReadSignedWord(ADDR_cameraDirection);
        if (camDir >= 8)
        {
            int16_t sprDir = swosReadSignedWord(spriteAddr + PLSPR_OFF_DIRECTION);
            swosWriteWord(ADDR_cameraDirection, (uint16_t)sprDir);
            int16_t cnt = swosReadSignedWord(ADDR_dseg_132804);
            swosWriteWord(ADDR_dseg_132804, (uint16_t)(int16_t)(cnt + 1));
        }
    }

    // ----- l_pass_success -- updatePlayers.cpp:8004-8028 ---------------------
    {
        int16_t camDirNow = swosReadSignedWord(ADDR_cameraDirection);
        swosWriteWord(spriteAddr + PLSPR_OFF_DIRECTION, (uint16_t)camDirNow);
        int passToPtr = swosReadSignedDword(teamBase + TEAMDATA_OFF_PASS_TO_PLAYER_PTR);
        swosWriteDword(teamBase + TEAMDATA_OFF_CONTROLLED_PLAYER, (uint32_t)passToPtr);
        swosWriteDword(teamBase + TEAMDATA_OFF_PASS_TO_PLAYER_PTR, 0);
        swosWriteWord(teamBase + TEAMDATA_OFF_PLAYER_SWITCH_TIMER, 25);
        swosWriteWord(teamBase + TEAMDATA_OFF_PASSING_BALL, 0);
        swosWriteWord(teamBase + TEAMDATA_OFF_PASSING_TO_PLAYER, 0);
        swosWriteWord(teamBase + TEAMDATA_OFF_SHOOTING, 0);
        swosBallSpriteSetSpeed(0);
        swosWriteDword(ADDR_lastTeamPlayed, (uint32_t)teamBase);
        swosWriteDword(ADDR_lastPlayerPlayed, (uint32_t)spriteAddr);
        swosWriteWord(ADDR_penalty, 0);
        swosWriteWord(ADDR_playerHadBall, 0);
        swosUpdatePlayerWithBall(spriteAddr);
        swosWriteWord(teamBase + TEAMDATA_OFF_PASS_KICK_TIMER, 0);
        swosWriteDword(teamBase + TEAMDATA_OFF_PASSING_KICKING_PLAYER, 0);
        swosWriteWord(teamBase + 116, 0);
    }

    // ----- throw-in tail -- updatePlayers.cpp:8029-8067 ----------------------
    {
        int16_t gsThrow = swosReadSignedWord(ADDR_gameState);
        if (gsThrow >= 15 && gsThrow <= 20)
        {
            swosSetPiecesSetThrowInPlayerDestinationCoordinates(spriteAddr);
            swosSetPlayerAnimationTable(spriteAddr, ADDR_aboutToThrowInAnimTable);
            swosWriteByte(spriteAddr + PLSPR_OFF_PLAYER_STATE, PLSTATE_THROW_IN);
            swosWriteByte(spriteAddr + PLSPR_OFF_PLAYER_DOWN_TIMER, 0);
            swosWriteWord(ADDR_hideBall, 1);
        }
    }
    goto l_update_player_speed_and_deltas;

l_chase_or_hold:;
    // ----- cseg_82EC2 -- updatePlayers.cpp:8312-8391 -------------------------
    {
        int16_t passingBall = swosReadSignedWord(teamBase + TEAMDATA_OFF_PASSING_BALL);
        if (passingBall == 0)
        {
            int a5Ctrl = swosReadSignedDword(teamBase + TEAMDATA_OFF_CONTROLLED_PLAYER);
            bool itsAPass = false;
            if (a5Ctrl == 0)
            {
                itsAPass = true;
            }
            else
            {
                int ctrlBd = swosReadSignedDword(a5Ctrl + PLSPR_OFF_BALL_DISTANCE);
                int myBd = swosReadSignedDword(spriteAddr + PLSPR_OFF_BALL_DISTANCE);
                if ((uint32_t)ctrlBd > 3200u)
                {
                    itsAPass = true;
                }
                else if ((uint32_t)ctrlBd > (uint32_t)myBd)
                {
                    itsAPass = true;
                }
                else if ((uint32_t)myBd <= 3200u)
                {
                    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_X, (uint16_t)px);
                    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_Y, (uint16_t)py);
                    goto l_update_player_speed_and_deltas;
                }
            }
            if (itsAPass)
            {
                swosWriteWord(teamBase + TEAMDATA_OFF_PASSING_BALL, 1);
            }
        }
    }

    // ----- l_player_chase_ball -- updatePlayers.cpp:8393-8430 ----------------
    {
        int16_t tBallX = swosReadSignedWord(teamBase + TEAMDATA_OFF_BALL_X);
        int16_t tBallY = swosReadSignedWord(teamBase + TEAMDATA_OFF_BALL_Y);
        swosWriteWord(spriteAddr + PLSPR_OFF_DEST_X, (uint16_t)tBallX);
        swosWriteWord(spriteAddr + PLSPR_OFF_DEST_Y, (uint16_t)tBallY);
        if (px == tBallX && py == tBallY)
            goto l_update_player_speed_and_deltas;
    }

    // ----- l_player_still_moving -> cseg_8308D --------------------------------
    {
        int16_t goalOutFlag = swosReadSignedWord(ADDR_goalOut);
        if (goalOutFlag == 0)
            goto l_update_player_speed_and_deltas;
        int16_t ordGoalOut = swosReadSignedWord(spriteAddr + PLSPR_OFF_PLAYER_ORDINAL);
        if (ordGoalOut == 1)
            goto l_update_player_speed_and_deltas;
        if (px < 183)  goto l_update_player_speed_and_deltas;
        if (px > 488)  goto l_update_player_speed_and_deltas;
        if (py <= 226) goto l_stop_player;
        if (py >= 672) goto l_stop_player;
        goto l_update_player_speed_and_deltas;
    }

l_stop_player:;
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_X, (uint16_t)px);
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_Y, (uint16_t)py);

l_update_player_speed_and_deltas:;
    swosUpdatePlayerSpeedAndFrameDelay(teamBase, spriteAddr);
}

// updatePlayers.cpp:4398-5779 -- l_its_controlled_player handler.
static void tickHumanControlled(int spriteAddr, bool topTeam)
{
    int teamBase = swosTeamDataBase(topTeam);

    SwosPlayerControlledExit exit = swosRunControlledBranch(spriteAddr, topTeam);

    if (exit == SWOS_PC_STOP_PLAYER)
    {
        int16_t sx = swosReadSignedWord(spriteAddr + PLSPR_OFF_X + 2);
        int16_t sy = swosReadSignedWord(spriteAddr + PLSPR_OFF_Y + 2);
        swosWriteWord(spriteAddr + PLSPR_OFF_DEST_X, (uint16_t)sx);
        swosWriteWord(spriteAddr + PLSPR_OFF_DEST_Y, (uint16_t)sy);
    }

    // === PASS-RECEIPT TRIGGER (call-site preserved; guarded no-op for now) ===
    int16_t passReceiptGsp = swosReadSignedWord(ADDR_gameStatePl);
    if (passReceiptGsp == K_ST_GAME_IN_PROGRESS)
    {
        swosRunPassReceiptTrigger(spriteAddr, topTeam);
    }

    // === BALL-CHASE OVERRIDE for the team's controlled player ===
    overrideDestToBallIfChaser(spriteAddr, teamBase);

    swosUpdatePlayerSpeedAndFrameDelay(teamBase, spriteAddr);
}

// Helper for the ball-chase override (see tickHumanControlled comment).
static void overrideDestToBallIfChaser(int spriteAddr, int teamBase)
{
    int16_t gameStatePl = swosReadSignedWord(ADDR_gameStatePl);
    if (gameStatePl != 100) return;

    int16_t pNum = swosReadSignedWord(teamBase + TEAMDATA_OFF_PLAYER_NUMBER);
    if (pNum != 0) return;

    int controlled = swosReadSignedDword(teamBase + TEAMDATA_OFF_CONTROLLED_PLAYER);
    if (controlled != spriteAddr) return;

    int16_t px = swosReadSignedWord(spriteAddr + PLSPR_OFF_X + 2);
    int16_t py = swosReadSignedWord(spriteAddr + PLSPR_OFF_Y + 2);
    int16_t bxPx = swosBallSpriteXPixels();
    int16_t byPx = swosBallSpriteYPixels();
    int dpx = px - bxPx;
    int dpy = py - byPx;
    if ((dpx >= -2 && dpx <= 2) && (dpy >= -2 && dpy <= 2)) return;

    int16_t destX = bxPx;
    int16_t destY = byPx;
    if (destX < 81)  destX = 81;
    if (destX > 590) destX = 590;
    if (destY < 129) destY = 129;
    if (destY > 769) destY = 769;
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_X, (uint16_t)destX);
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_Y, (uint16_t)destY);
}

// updatePlayers.cpp:8654 (l_not_controlled_player) + 7604 (l_player_expecting_pass) --
// off-ball outfielder handler.
static void tickAiControlled(int spriteAddr, bool topTeam, int slotInTeam)
{
    int teamBase = swosTeamDataBase(topTeam);

    // === CPU-TEAM GATE (2026-07-02, USER-VISIBLE BUG FIX) ==================
    int16_t pNumAiGate = swosReadSignedWord(teamBase + TEAMDATA_OFF_PLAYER_NUMBER);
    bool myTurn = false;
    if (pNumAiGate != 0)
        goto l_skip_ai_controls;

    {
        int ctrlAiGate = swosReadSignedDword(teamBase + TEAMDATA_OFF_CONTROLLED_PLAYER);
        if (ctrlAiGate == 0)
        {
            requireAiSetControlsDirection(teamBase);
        }
    }

    // === FALLBACK heuristic (NOT a port) -- DISABLED, see
    // K_CHASE_FALLBACK_ENABLED above (task #242, 2026-08-18). A runtime `if`
    // (not #if) so the dead branch stays compiled and warning-checked like
    // the rest of the file -- flip the #define to 1 to re-run the A/B. =====
    {
        int16_t postDir = swosReadSignedWord(teamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION);
        if (K_CHASE_FALLBACK_ENABLED && postDir == -1)
        {
            int16_t t1Has = swosReadSignedWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PLAYER_HAS_BALL);
            int16_t t2Has = swosReadSignedWord(TEAMDATA_BOTTOM_BASE + TEAMDATA_OFF_PLAYER_HAS_BALL);
            if (t1Has == 0 && t2Has == 0)
            {
                int16_t bx = swosBallSpriteXPixels();
                int16_t by = swosBallSpriteYPixels();
                int16_t px = swosReadSignedWord(spriteAddr + PLSPR_OFF_X + 2);
                int16_t py = swosReadSignedWord(spriteAddr + PLSPR_OFF_Y + 2);
                int dx = bx - px;
                int dy = by - py;
                int distSq = dx * dx + dy * dy;
                if (distSq <= 250 * 250)
                {
                    SwosDeltasAndAngle chaseResult =
                        swosCalculateDeltaXAndY(256, px, py, bx, by);
                    int chaseAngle = chaseResult.direction;
                    if (chaseAngle >= 0)
                    {
                        int chaseDir = ((chaseAngle + 16) & 0xFF) >> 5;
                        swosWriteWord(teamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, (uint16_t)chaseDir);
                    }
                }
            }
        }
    }
    // ========================================================================

l_skip_ai_controls:;
    // Human-team entry point -- see the CPU-TEAM GATE at the top of this
    // function. From here down the original runs for BOTH teams.
    {
        int16_t updatePlIdx = swosReadSignedWord(teamBase + TEAMDATA_OFF_UPDATE_PLAYER_INDEX);
        myTurn = (updatePlIdx == slotInTeam);
    }

    {
        int16_t gameStatePlAi = swosReadSignedWord(ADDR_gameStatePl);
        if (gameStatePlAi == K_ST_GAME_IN_PROGRESS)
        {
            if (myTurn)
            {
                uint16_t inGameCounter = swosReadWord(ADDR_inGameCounter);
                if (inGameCounter <= 25)
                {
                    uint16_t breakStateGate = swosReadWord(ADDR_breakState);
                    if (breakStateGate == 4 || breakStateGate == 5 ||
                        (breakStateGate >= 6 && breakStateGate <= 12))
                    {
                        swosUpdatePlayerSpeedAndFrameDelay(teamBase, spriteAddr);
                        return;
                    }
                }

                int16_t drsInPlay = swosReadSignedWord(spriteAddr + PLSPR_OFF_DEST_REACHED_STATE);
                if (drsInPlay == 1)
                    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_REACHED_STATE, 2);

                // l_set_player_positions_if_game_break -- updatePlayers.cpp:9234-9239.
                // Design decision (see swos_update_players.h): direct call,
                // no exception emulation.
                {
                    int16_t ballXPx = swosBallSpriteXPixels();
                    int16_t ballYPx = swosBallSpriteYPixels();
                    swosSetPlayerWithNoBallDestination(spriteAddr, teamBase, ballXPx, ballYPx);
                }
            }

            swosUpdatePlayerSpeedAndFrameDelay(teamBase, spriteAddr);
            return;
        }
    }

    // ===== STOPPAGE PATH (gameStatePl != 100) ===============================
    if (!myTurn) return;

    int16_t drs = swosReadSignedWord(spriteAddr + PLSPR_OFF_DEST_REACHED_STATE);
    if (drs == 1)
        swosWriteWord(spriteAddr + PLSPR_OFF_DEST_REACHED_STATE, 2);

    if (checkIfThisPlayerGettingBooked(spriteAddr, teamBase))
    {
        swosUpdatePlayerSpeedAndFrameDelay(teamBase, spriteAddr);
        return;
    }

    int16_t sentAway = swosReadSignedWord(spriteAddr + PLSPR_OFF_SENT_AWAY);
    if (sentAway == 0)
    {
        setPlayerPositionsForGameBreak(spriteAddr, teamBase, topTeam);
    }

    swosUpdatePlayerSpeedAndFrameDelay(teamBase, spriteAddr);
}

// updatePlayers.cpp:7462 -- l_player_tackled (PL_TACKLED, state=3).
static void tickTackledPlayer(int spriteAddr, bool topTeam)
{
    int teamBase = swosTeamDataBase(topTeam);

    int8_t downTimer = (int8_t)swosReadByte(spriteAddr + PLSPR_OFF_PLAYER_DOWN_TIMER);
    downTimer = (int8_t)(downTimer - 1);
    swosWriteByte(spriteAddr + PLSPR_OFF_PLAYER_DOWN_TIMER, (uint8_t)downTimer);

    if (downTimer != 0)
    {
        int16_t speed = swosReadSignedWord(spriteAddr + PLSPR_OFF_SPEED);
        if (speed > 0)
        {
            int16_t py = swosReadSignedWord(spriteAddr + PLSPR_OFF_Y + 2);
            bool inAreaByY = (py < 159) || (py > 739);
            if (inAreaByY)
            {
                int16_t px = swosReadSignedWord(spriteAddr + PLSPR_OFF_X + 2);
                if (px >= 265 && px <= 406)
                {
                    int sub = (uint16_t)speed >> 2;
                    speed = (int16_t)(speed - sub);
                }
            }

            // swos.asm:203803 `kPlayerGroundConstant dw 96` -- shared with
            // tickTacklingPlayer.
            const int kPlayerGroundConstant = 96;
            speed = (int16_t)(speed - kPlayerGroundConstant);
            if (speed < 0) speed = 0;
            swosWriteWord(spriteAddr + PLSPR_OFF_SPEED, (uint16_t)speed);
        }
        swosUpdatePlayerSpeedAndFrameDelay(teamBase, spriteAddr);
        return;
    }

    int16_t injuryLevel = swosReadSignedWord(spriteAddr + PLSPR_OFF_INJURY_LEVEL);
    if (injuryLevel != 0)
    {
        swosWriteWord(spriteAddr + PLSPR_OFF_SPEED, 0);
        swosWriteByte(spriteAddr + PLSPR_OFF_PLAYER_STATE, 13);
        swosWriteByte(spriteAddr + PLSPR_OFF_PLAYER_DOWN_TIMER, (uint8_t)injuryLevel);
        swosSetPlayerAnimationTable(spriteAddr, ADDR_kPlInjuredAnimTableAddr);
        swosUpdatePlayerSpeedAndFrameDelay(teamBase, spriteAddr);
        return;
    }

    swosWriteByte(spriteAddr + PLSPR_OFF_PLAYER_STATE, PLSTATE_NORMAL);
    swosSetPlayerAnimationTable(spriteAddr, ADDR_kPlayerStandingAnimTableAddr);

    int16_t sx = swosReadSignedWord(spriteAddr + PLSPR_OFF_X + 2);
    int16_t sy = swosReadSignedWord(spriteAddr + PLSPR_OFF_Y + 2);
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_X, (uint16_t)sx);
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_Y, (uint16_t)sy);
    swosUpdatePlayerSpeedAndFrameDelay(teamBase, spriteAddr);
}

// updatePlayers.cpp:6375 -- l_player_tackling (PL_TACKLING, state=1).
static void tickTacklingPlayer(int spriteAddr, bool topTeam)
{
    int teamBase = swosTeamDataBase(topTeam);

    swosPlayerTacklingTestFoul(spriteAddr, teamBase);

    int16_t tackTimer = swosReadSignedWord(spriteAddr + PLSPR_OFF_TACKLING_TIMER);
    if (tackTimer >= 0)
    {
        swosWriteWord(spriteAddr + PLSPR_OFF_TACKLING_TIMER, (uint16_t)(int16_t)(tackTimer + 1));
    }

    int8_t downTimer = (int8_t)swosReadByte(spriteAddr + PLSPR_OFF_PLAYER_DOWN_TIMER);
    downTimer = (int8_t)(downTimer - 1);
    swosWriteByte(spriteAddr + PLSPR_OFF_PLAYER_DOWN_TIMER, (uint8_t)downTimer);

    if (downTimer == 0)
    {
        swosWriteByte(spriteAddr + PLSPR_OFF_PLAYER_STATE, PLSTATE_NORMAL);
        swosSetPlayerAnimationTable(spriteAddr, ADDR_playerNormalStandingAnimTable);

        int16_t sx = swosReadSignedWord(spriteAddr + PLSPR_OFF_X + 2);
        int16_t sy = swosReadSignedWord(spriteAddr + PLSPR_OFF_Y + 2);
        swosWriteWord(spriteAddr + PLSPR_OFF_DEST_X, (uint16_t)sx);
        swosWriteWord(spriteAddr + PLSPR_OFF_DEST_Y, (uint16_t)sy);
        swosUpdatePlayerSpeedAndFrameDelay(teamBase, spriteAddr);
        return;
    }

    // updatePlayers.cpp:6416-6462 -- l_player_down_tackling.
    tackTimer = swosReadSignedWord(spriteAddr + PLSPR_OFF_TACKLING_TIMER);
    if (tackTimer >= 0)
    {
        int16_t playerNumber = swosReadSignedWord(teamBase + TEAMDATA_OFF_PLAYER_NUMBER);
        uint8_t firePressed = swosReadByte(teamBase + TEAMDATA_OFF_FIRE_PRESSED);
        if (playerNumber != 0 && firePressed == 0)
        {
            int16_t neg = (int16_t)(-tackTimer);
            swosWriteWord(spriteAddr + PLSPR_OFF_TACKLING_TIMER, (uint16_t)neg);
            if (neg >= -2)
            {
                swosWriteWord(spriteAddr + PLSPR_OFF_TACKLING_TIMER, (uint16_t)(int16_t)-1);
            }
        }
    }

    // updatePlayers.cpp:6464-6493 -- l_computer_tackling.
    const int kPlayerGroundConstant = 96;
    int16_t speed = swosReadSignedWord(spriteAddr + PLSPR_OFF_SPEED);
    if (speed != 0)
    {
        speed = (int16_t)(speed - kPlayerGroundConstant);
        swosWriteWord(spriteAddr + PLSPR_OFF_SPEED, (uint16_t)speed);
        if (speed <= 0)
        {
            swosWriteWord(spriteAddr + PLSPR_OFF_SPEED, 0);
            swosSetPlayerDowntimeAfterTackle(teamBase, spriteAddr);
            swosUpdatePlayerSpeedAndFrameDelay(teamBase, spriteAddr);
            return;
        }
    }
    else
    {
        swosUpdatePlayerSpeedAndFrameDelay(teamBase, spriteAddr);
        return;
    }

    // updatePlayers.cpp:6495-6661 -- l_player_still_tackling_and_moving.
    int16_t tackTimer2 = swosReadSignedWord(spriteAddr + PLSPR_OFF_TACKLING_TIMER);
    int16_t tackState  = swosReadSignedWord(spriteAddr + 96);
    int ballDist    = swosReadSignedDword(spriteAddr + 74);
    uint8_t ballLe4    = swosReadByte(teamBase + 64);
    uint8_t ball4To8   = swosReadByte(teamBase + 65);
    int16_t gsPl      = swosReadSignedWord(ADDR_gameStatePl);
    if (gsPl == K_ST_GAME_IN_PROGRESS
        && tackState == 0
        && (uint32_t)ballDist <= 64u
        && tackTimer2 != -1
        && (ballLe4 != 0 || ball4To8 != 0))
    {
        // l_player_tackling_the_ball -- updatePlayers.cpp:6730-6752.
        swosWriteDword(ADDR_lastTeamPlayed, (uint32_t)teamBase);
        swosWriteDword(ADDR_lastPlayerPlayed, (uint32_t)spriteAddr);
        swosWriteWord(ADDR_penalty, 0);
        swosWriteWord(ADDR_playerHadBall, 0);
        int tackleOppBase = swosReadSignedDword(teamBase + TEAMDATA_OFF_OPPONENTS_TEAM);
        int16_t tackleOppHasBall = swosReadSignedWord(tackleOppBase + TEAMDATA_OFF_PLAYER_HAS_BALL);
        if (tackleOppHasBall != 0)
        {
            swosWriteWord(ADDR_playerHadBall, 1);
        }

        swosPlayersTackledTheBallStrong(spriteAddr, teamBase);

        // cseg_821F5 -- updatePlayers.cpp:6791-6803. NOTE: the original
        // applies this epilogue to BOTH weak and strong tackle branches; the
        // weak variant (tacklingTimer == -1, updatePlayers.cpp:6756-6779) is
        // still un-ported IN OPENSWOS ITSELF (the C# source's own comment
        // says so) and must share it when OpenSWOS lands it -- not this
        // port's gap to fill.
        swosWriteDword(teamBase + TEAMDATA_OFF_LAST_HEADING_PLAYER, (uint32_t)spriteAddr);
        swosWriteWord(teamBase + TEAMDATA_OFF_PASS_KICK_TIMER, 25);
        swosWriteWord(tackleOppBase + TEAMDATA_OFF_PASSING_TO_PLAYER, 0);
        swosWriteDword(tackleOppBase + TEAMDATA_OFF_PASSING_KICKING_PLAYER, 0);
        swosWriteWord(tackleOppBase + TEAMDATA_OFF_SPIN_TIMER, (uint16_t)(int16_t)-1);
        swosWriteWord(tackleOppBase + 138, 12);   // wonTheBallTimer (+138)
    }
    swosUpdatePlayerSpeedAndFrameDelay(teamBase, spriteAddr);
}

// updatePlayers.cpp:7432 -- l_player_injured (PL_ROLLING_INJURED, state=13).
static void tickInjuredRollingPlayer(int spriteAddr, bool topTeam)
{
    int teamBase = swosTeamDataBase(topTeam);

    int16_t injuriesForever = swosReadSignedWord(ADDR_injuriesForever);
    if (injuriesForever != 0)
    {
        swosUpdatePlayerSpeedAndFrameDelay(teamBase, spriteAddr);
        return;
    }

    int8_t downTimer = (int8_t)swosReadByte(spriteAddr + PLSPR_OFF_PLAYER_DOWN_TIMER);
    downTimer = (int8_t)(downTimer - 1);
    swosWriteByte(spriteAddr + PLSPR_OFF_PLAYER_DOWN_TIMER, (uint8_t)downTimer);

    if (downTimer != 0)
    {
        swosUpdatePlayerSpeedAndFrameDelay(teamBase, spriteAddr);
        return;
    }

    swosWriteByte(spriteAddr + PLSPR_OFF_PLAYER_STATE, PLSTATE_NORMAL);
    swosSetPlayerAnimationTable(spriteAddr, ADDR_kPlayerStandingAnimTableAddr);

    int16_t sx = swosReadSignedWord(spriteAddr + PLSPR_OFF_X + 2);
    int16_t sy = swosReadSignedWord(spriteAddr + PLSPR_OFF_Y + 2);
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_X, (uint16_t)sx);
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_Y, (uint16_t)sy);
    swosUpdatePlayerSpeedAndFrameDelay(teamBase, spriteAddr);
}

// updatePlayers.cpp:537-679 -- update ball-distance and ball-height flags.
static void updatePlayerBallDistanceAndHeight(int teamBase, int spriteAddr)
{
    // === Sprite.ballDistance compute (was missing) =========================
    int16_t pxSp = swosReadSignedWord(spriteAddr + PLSPR_OFF_X + 2);
    int16_t pySp = swosReadSignedWord(spriteAddr + PLSPR_OFF_Y + 2);
    int16_t bxSp = swosBallSpriteXPixels();
    int16_t bySp = swosBallSpriteYPixels();
    int dxSp = (int16_t)(pxSp - bxSp);
    int dySp = (int16_t)(pySp - bySp);
    int sqrDist = dxSp * dxSp + dySp * dySp;
    swosWriteDword(spriteAddr + PLSPR_OFF_BALL_DISTANCE, (uint32_t)sqrDist);

    uint8_t plVeryClose = swosReadByte(teamBase + 61);
    swosWriteByte(teamBase + 69, plVeryClose);   // prevPlVeryCloseToBall
    swosWriteByte(teamBase + 61, 0);             // plVeryCloseToBall
    swosWriteByte(teamBase + 62, 0);             // plCloseToBall
    swosWriteByte(teamBase + 63, 0);             // plNotFarFromBall

    int ballDistance = swosReadSignedDword(spriteAddr + PLSPR_OFF_BALL_DISTANCE);
    if ((uint32_t)ballDistance <= 32u)
    {
        swosWriteByte(teamBase + 61, 1);
    }
    else if ((uint32_t)ballDistance <= 72u)
    {
        swosWriteByte(teamBase + 62, 1);
    }
    else if ((uint32_t)ballDistance <= 2450u)
    {
        swosWriteByte(teamBase + 63, 1);
    }

    swosWriteByte(teamBase + 64, 0);   // ballLessEqual4
    swosWriteByte(teamBase + 65, 0);   // ball4To8
    swosWriteByte(teamBase + 66, 0);   // ball8To12
    swosWriteByte(teamBase + 67, 0);   // ball12To17
    swosWriteByte(teamBase + 68, 0);   // ballAbove17

    int16_t ballZ = swosBallSpriteZPixels();
    if (ballZ > 17)
    {
        swosWriteByte(teamBase + 68, 1);
    }
    else if (ballZ > 12)
    {
        swosWriteByte(teamBase + 67, 1);
    }
    else if (ballZ > 8)
    {
        swosWriteByte(teamBase + 66, 1);
    }
    else if (ballZ > 4)
    {
        swosWriteByte(teamBase + 65, 1);
    }
    else
    {
        swosWriteByte(teamBase + 64, 1);
    }
}

// updatePlayers.cpp:10367-10456 -- loop epilogue.
static void tickEpilogueKeeperTransition(bool topTeam)
{
    (void)topTeam; // matches the C#: topTeam is unused inside this function.
    int16_t gameStatePl = swosReadSignedWord(ADDR_gameStatePl);
    int16_t gameState = swosReadSignedWord(ADDR_gameState);
    if (gameStatePl != K_ST_GAME_IN_PROGRESS && gameState != K_ST_KEEPER_HOLDS_BALL)
    {
        return;
    }

    int lastPlayer = swosReadSignedDword(ADDR_lastPlayerPlayed);
    int prevPlayer = swosReadSignedDword(ADDR_prevLastPlayer);
    int goalie1Addr = swosPlayerSpriteBase(PLSPR_SLOT_GOALIE1);
    int goalie2Addr = swosPlayerSpriteBase(PLSPR_SLOT_GOALIE2);

    bool lastIsGoalie1 = (lastPlayer == goalie1Addr);
    bool lastIsGoalie2 = (lastPlayer == goalie2Addr);
    if (!lastIsGoalie1 && !lastIsGoalie2) return;

    if (lastIsGoalie1 && prevPlayer == goalie1Addr) return;
    if (lastIsGoalie2 && prevPlayer == goalie2Addr) return;

    swosWriteDword(ADDR_lastPlayerBeforeGoalkeeper, (uint32_t)prevPlayer);
    int prevTeam = swosReadSignedDword(ADDR_prevLastTeamPlayed);
    swosWriteDword(ADDR_lastTeamScored, (uint32_t)prevTeam);
    swosWriteWord(ADDR_nobodysBallTimer, 50);
}

// Resolve the PlayerInfo base for THIS team.
static int ownGoaliePlayersBase(bool isTopTeam)
{
    int teamBase = swosTeamDataBase(isTopTeam);
    return swosReadSignedDword(teamBase + TEAMDATA_OFF_IN_GAME_TEAM_PTR);
}

// Shared "header finished -> back to PL_NORMAL" tail.
static void headerExitToNormal(int spriteAddr)
{
    swosWriteByte(spriteAddr + PLSPR_OFF_PLAYER_STATE, PLSTATE_NORMAL);
    swosSetPlayerAnimationTable(spriteAddr, ADDR_kPlayerStandingAnimTableAddr);
    int16_t sx = swosReadSignedWord(spriteAddr + PLSPR_OFF_X + 2);
    int16_t sy = swosReadSignedWord(spriteAddr + PLSPR_OFF_Y + 2);
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_X, (uint16_t)sx);
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_Y, (uint16_t)sy);
}

// updatePlayers.cpp:6972-7430 -- l_player_jump_heading, ported in full.
static void tickJumpHeader(int spriteAddr, bool topTeam)
{
    int teamBase = swosTeamDataBase(topTeam);
    int a1 = spriteAddr;

    int8_t down = (int8_t)swosReadByte(a1 + PLSPR_OFF_PLAYER_DOWN_TIMER);
    down = (int8_t)(down - 1);
    swosWriteByte(a1 + PLSPR_OFF_PLAYER_DOWN_TIMER, (uint8_t)down);
    if (down == 0)
    {
        headerExitToNormal(a1);
        goto Done;
    }

    {
        int16_t sp = swosReadSignedWord(a1 + PLSPR_OFF_SPEED);
        if (sp != 0)
        {
            swosSetJumpHeaderHitAnimTable(a1);
            int animTable = swosReadSignedDword(a1 + PLSPR_OFF_ANIM_TABLE_PTR);
            int8_t halveThreshold =
                (animTable == ADDR_kJumpHeaderAttemptAnimTableAddr) ? (int8_t)37 : (int8_t)17;
            if (down > halveThreshold)
            {
                sp = (int16_t)(sp - 72);
                swosWriteWord(a1 + PLSPR_OFF_SPEED, (uint16_t)sp);
                if (sp < 0)
                {
                    swosWriteWord(a1 + PLSPR_OFF_SPEED, 0);
                    goto Done;
                }
            }
            else
            {
                sp = (int16_t)(sp >> 1);
                swosWriteWord(a1 + PLSPR_OFF_SPEED, (uint16_t)sp);
                goto Done;
            }
        }
    }

    // :7095-7327 -- l_check_if_jump_header_inside_pitch ladder.
    {
        int16_t x = swosReadSignedWord(a1 + PLSPR_OFF_X + 2);
        if (x < 73) goto SlowDownAndLeave;
        if (x > 598) goto SlowDownAndLeave;
        int16_t y = swosReadSignedWord(a1 + PLSPR_OFF_Y + 2);
        if (y < 132) goto HeadingCloseToGoalLines;
        if (y <= 766) goto WindedUp;
    }

HeadingCloseToGoalLines:;
    {
        int16_t x = swosReadSignedWord(a1 + PLSPR_OFF_X + 2);
        if (x < 296) goto Cseg8253D;
        if (x > 375) goto Cseg8253D;
        if (x < 305) goto HeadingFinished;
        if (x <= 366) goto Cseg8253D;
    }

HeadingFinished:;
    swosWriteWord(a1 + PLSPR_OFF_SPEED, 0);
    swosWriteByte(a1 + PLSPR_OFF_PLAYER_DOWN_TIMER, 0);
    headerExitToNormal(a1);
    goto Done;

Cseg8253D:;
    {
        int16_t y = swosReadSignedWord(a1 + PLSPR_OFF_Y + 2);
        if (y < 129) goto HeadingIntoTheGoal;
        if (y <= 769) goto WindedUp;
    }

HeadingIntoTheGoal:;
    {
        int16_t x = swosReadSignedWord(a1 + PLSPR_OFF_X + 2);
        if (x < 290) goto CheckInsideGoal;
        if (x <= 381) goto HeadingFinished;
    }

CheckInsideGoal:;
    {
        int16_t y = swosReadSignedWord(a1 + PLSPR_OFF_Y + 2);
        if (y < 121) goto SlowDownAndLeave;
        if (y <= 777) goto WindedUp;
    }

SlowDownAndLeave:;
    {
        int16_t s = swosReadSignedWord(a1 + PLSPR_OFF_SPEED);
        int16_t d0 = (int16_t)(s >> 2);
        s = (int16_t)(s - d0);
        d0 = (int16_t)(d0 >> 1);
        s = (int16_t)(s - d0);
        swosWriteWord(a1 + PLSPR_OFF_SPEED, (uint16_t)s);
    }
    goto Done;

WindedUp:;
    if (swosReadByte(a1 + PLSPR_OFF_PLAYER_DOWN_TIMER) < 42)
        goto Done;
    if (swosReadSignedWord(ADDR_gameStatePl) != K_ST_GAME_IN_PROGRESS)
        goto Done;
    if (swosReadSignedWord(a1 + 98 /*heading*/) != 0)
        goto Done;
    if ((uint32_t)swosReadSignedDword(a1 + PLSPR_OFF_BALL_DISTANCE) > 64u)
        goto Done;
    {
        int16_t bz = swosBallSpriteZPixels();
        if (bz < 8) goto Done;
        if (bz > 15) goto Done;
    }
    // :7405-7430 -- strike commit.
    swosWriteDword(ADDR_lastTeamPlayed, (uint32_t)teamBase);
    swosWriteDword(ADDR_lastPlayerPlayed, (uint32_t)a1);
    swosWriteWord(ADDR_penalty, 0);
    swosWriteWord(ADDR_playerHadBall, 1);
    swosPlayerHittingJumpHeader(teamBase, a1);
    swosWriteWord(teamBase + TEAMDATA_OFF_PASS_KICK_TIMER, 25);
    {
        int opp = swosReadSignedDword(teamBase + TEAMDATA_OFF_OPPONENTS_TEAM);
        swosWriteWord(opp + TEAMDATA_OFF_PASSING_TO_PLAYER, 0);
        swosWriteDword(opp + TEAMDATA_OFF_PASSING_KICKING_PLAYER, 0);
    }
    goto Done;

Done:;
    swosUpdatePlayerSpeedAndFrameDelay(teamBase, a1);
}

// updatePlayers.cpp:6831-6970 -- l_player_doing_static_header, ported in full.
static void tickStaticHeader(int spriteAddr, bool topTeam)
{
    int teamBase = swosTeamDataBase(topTeam);
    int a1 = spriteAddr;

    int8_t down = (int8_t)swosReadByte(a1 + PLSPR_OFF_PLAYER_DOWN_TIMER);
    down = (int8_t)(down - 1);
    swosWriteByte(a1 + PLSPR_OFF_PLAYER_DOWN_TIMER, (uint8_t)down);
    if (down == 0)
    {
        headerExitToNormal(a1);
        goto Done;
    }

    {
        int16_t sp = swosReadSignedWord(a1 + PLSPR_OFF_SPEED);
        sp = (int16_t)(sp - 16);
        swosWriteWord(a1 + PLSPR_OFF_SPEED, (uint16_t)sp);
        if (sp < 0) swosWriteWord(a1 + PLSPR_OFF_SPEED, 0);
    }

    // :6873 -- SetStaticHeaderDirection runs UNCONDITIONALLY each wind-up tick.
    swosSetStaticHeaderDirection(a1, teamBase);

    // :6874-6943 -- winded-up CONTACT gates. ALL must hold to strike.
    if (swosReadSignedWord(ADDR_gameStatePl) != K_ST_GAME_IN_PROGRESS)
        goto Done;
    if (swosReadSignedWord(a1 + 98 /*heading*/) != 0)
        goto Done;
    if ((uint32_t)swosReadSignedDword(a1 + PLSPR_OFF_BALL_DISTANCE) > 64u)
        goto Done;
    {
        int16_t bz = swosBallSpriteZPixels();
        if (bz < 8) goto Done;
        if (bz > 15) goto Done;
    }
    if (swosReadSignedWord(teamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION) < 0)
        goto Done;

    // :6945-6969 -- strike commit.
    swosWriteDword(ADDR_lastTeamPlayed, (uint32_t)teamBase);
    swosWriteDword(ADDR_lastPlayerPlayed, (uint32_t)a1);
    swosWriteWord(ADDR_penalty, 0);
    swosWriteWord(ADDR_playerHadBall, 1);
    swosPlayerHittingStaticHeader(teamBase, a1);
    swosWriteWord(teamBase + TEAMDATA_OFF_PASS_KICK_TIMER, 25);
    {
        int opp = swosReadSignedDword(teamBase + TEAMDATA_OFF_OPPONENTS_TEAM);
        swosWriteWord(opp + TEAMDATA_OFF_PASSING_TO_PLAYER, 0);
        swosWriteDword(opp + TEAMDATA_OFF_PASSING_KICKING_PLAYER, 0);
    }

Done:;
    swosUpdatePlayerSpeedAndFrameDelay(teamBase, a1);
}

// Dispatch table-equivalent for the per-state switch in
// updatePlayers.cpp:680-836. TACKLED + ROLLING are already handled before
// getting here (swosUpdatePlayersUpdate's loop), so they don't appear here.
static void dispatchByPlayerState(uint8_t state, int spriteAddr, int slotInTeam,
                                   bool topTeam)
{
    // updatePlayers.cpp:847-849 -- for the GOALKEEPER (playerOrdinal == 1),
    // dispatch goes straight to l_player_goalkeeper REGARDLESS of playerState.
    int16_t ordinal = swosReadSignedWord(spriteAddr + PLSPR_OFF_PLAYER_ORDINAL);
    if (ordinal == 1)
    {
        tickGoalkeeper(spriteAddr, topTeam, slotInTeam);
        return;
    }

    switch (state)
    {
        case PLSTATE_NORMAL:
        {
            // updatePlayers.cpp:851-876:
            //   if A1 == team.controlledPlayer -> l_its_controlled_player
            //   if A1 == team.passToPlayerPtr  -> l_player_expecting_pass
            //   else                            -> l_not_controlled_player
            int controlled = swosTeamDataControlledPlayer(topTeam);
            int passTo = swosReadSignedDword(swosTeamDataBase(topTeam) + TEAMDATA_OFF_PASS_TO_PLAYER_PTR);

            if (spriteAddr == controlled)
            {
                tickHumanControlled(spriteAddr, topTeam);
            }
            else if (spriteAddr == passTo && passTo != 0)
            {
                // STOPPAGE ROUTE (2026-07-02): during a break (gameStatePl
                // != 100) route to the faithful cseg_82AAF..cseg_82EC2 body
                // in tickPassExpectingStopped.
                int16_t gsPlPass = swosReadSignedWord(ADDR_gameStatePl);
                if (gsPlPass != K_ST_GAME_IN_PROGRESS)
                {
                    tickPassExpectingStopped(spriteAddr, topTeam);
                }
                else
                {
                    swosRunPassExpectingBranch(spriteAddr, topTeam);
                    swosUpdatePlayerSpeedAndFrameDelay(swosTeamDataBase(topTeam), spriteAddr);
                }
            }
            else
            {
                tickAiControlled(spriteAddr, topTeam, slotInTeam);
            }
            break;
        }

        case PLSTATE_TACKLING:
            // updatePlayers.cpp:6375 -- l_player_tackling.
            tickTacklingPlayer(spriteAddr, topTeam);
            break;

        case PLSTATE_JUMP_HEADER:
            // updatePlayers.cpp:6972 -- l_player_jump_heading.
            tickJumpHeader(spriteAddr, topTeam);
            break;

        case PLSTATE_STATIC_HEADER:
            // updatePlayers.cpp:6831 -- l_player_doing_static_header.
            tickStaticHeader(spriteAddr, topTeam);
            break;

        case PLSTATE_THROW_IN:
            // updatePlayers.cpp:5780 -- l_player_taking_throw_in. Real,
            // executed SetPieces dependency -- deferred to step 10 via the
            // assert-backed hook (swos_set_pieces.h).
            swosSetPiecesTickThrowIn(spriteAddr, BALLSPR_BASE, swosTeamDataBase(topTeam));
            swosUpdatePlayerSpeedAndFrameDelay(swosTeamDataBase(topTeam), spriteAddr);
            break;

        case PLSTATE_GOALIE_DIVING_HIGH:
        case PLSTATE_GOALIE_DIVING_LOW:
            // updatePlayers.cpp:3188 -- l_goalie_diving. Dive completion is
            // handled inside tickGoalkeeper (ordinal==1 routes there first).
            // Outfielders cannot enter these states; safe no-op.
            swosUpdatePlayerSpeedAndFrameDelay(swosTeamDataBase(topTeam), spriteAddr);
            break;

        case PLSTATE_GOALIE_CATCHING_BALL:
            // updatePlayers.cpp:3059 -- l_goalie_catching_the_ball.
            swosTickGoalieCatchingBall(spriteAddr, topTeam);
            swosUpdatePlayerSpeedAndFrameDelay(swosTeamDataBase(topTeam), spriteAddr);
            break;

        case PLSTATE_GOALIE_CLAIMED:
            // updatePlayers.cpp:3163 -- l_goalie_claimed.
            swosTickGoalieClaimed(spriteAddr);
            swosUpdatePlayerSpeedAndFrameDelay(swosTeamDataBase(topTeam), spriteAddr);
            break;

        case PLSTATE_DOWN:
        {
            // updatePlayers.cpp:3037-3057 -- l_player_down_st_10.
            int8_t downTimer = (int8_t)swosReadByte(spriteAddr + PLSPR_OFF_PLAYER_DOWN_TIMER);
            downTimer = (int8_t)(downTimer - 1);
            swosWriteByte(spriteAddr + PLSPR_OFF_PLAYER_DOWN_TIMER, (uint8_t)downTimer);
            if (downTimer == 0)
            {
                swosWriteByte(spriteAddr + PLSPR_OFF_PLAYER_STATE, PLSTATE_NORMAL);
                swosSetPlayerAnimationTable(spriteAddr, ADDR_kPlayerStandingAnimTableAddr);
            }
            swosUpdatePlayerSpeedAndFrameDelay(swosTeamDataBase(topTeam), spriteAddr);
            break;
        }

        case PLSTATE_BOOKED:
        {
            // updatePlayers.cpp:2992-3006 -- l_player_booked.
            int16_t whichCard = swosReadSignedWord(ADDR_whichCard);
            if (whichCard == 0)
            {
                swosWriteByte(spriteAddr + PLSPR_OFF_PLAYER_STATE, PLSTATE_NORMAL);
                swosSetPlayerAnimationTable(spriteAddr, ADDR_kPlayerStandingAnimTableAddr);
            }
            swosUpdatePlayerSpeedAndFrameDelay(swosTeamDataBase(topTeam), spriteAddr);
            break;
        }

        case PLSTATE_SAD:
        case PLSTATE_HAPPY:
        {
            // updatePlayers.cpp:3008-3035 -- l_player_sad_or_happy, plus the
            // OpenSWOS extension clear-triggers (ST_GAME_IN_PROGRESS restart,
            // destReachedState==1 bcm-ladder arm) -- see UpdateGoals.cs.
            int16_t sadDrs = swosReadSignedWord(spriteAddr + PLSPR_OFF_DEST_REACHED_STATE);
            int16_t gsSad = swosReadSignedWord(ADDR_gameState);
            if (gsSad == 23 /* ST_GOING_TO_HALFTIME */ ||
                gsSad == 24 /* ST_PLAYERS_GOING_TO_SHOWER */ ||
                gsSad == K_ST_GAME_IN_PROGRESS /* OpenSWOS: post-goal exit */ ||
                sadDrs == 1 /* OpenSWOS: bcm ladder armed the walk-back */)
            {
                swosWriteByte(spriteAddr + PLSPR_OFF_PLAYER_STATE, PLSTATE_NORMAL);
                swosSetPlayerAnimationTable(spriteAddr, ADDR_kPlayerStandingAnimTableAddr);
            }
            swosUpdatePlayerSpeedAndFrameDelay(swosTeamDataBase(topTeam), spriteAddr);
            break;
        }

        default:
            // updatePlayers.cpp:878 has debugBreak() here. Our port silently
            // no-ops so unported state values don't crash (matches the C#'s
            // own comment/behaviour -- not a fidelity gap this step owns).
            break;
    }
}
