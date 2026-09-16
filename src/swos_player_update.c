// SOURCE: openswos game/scripts/Sim/Port/PlayerUpdate.cs (full file, step
// 5.5). UpdateBallWithControllingGoalkeeper was pulled forward in step 4;
// see swos_player_update.h for the full step-5.5 scope/omission notes.
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
#include "swos_player_update.h"
#include "swos_addr.h"
#include "swos_ball_sprite.h"
#include "swos_ball_update.h"
#include "swos_memory.h"
#include "swos_player_actions.h"
#include "swos_player_energy.h"
#include "swos_player_sprite.h"
#include "swos_player_state.h"
#include "swos_rng.h"
#include "swos_team_data.h"
#include "swos_team_data_loader.h"
#include "swos_team_port.h"
#include "swos_util.h"

#include <stdint.h>

// PlayerUpdate.cs:545 -- `public const short kPortSafetyReleaseTicks = 300;`
// PORT-ONLY (no asm counterpart) -- see swos_player_update.h's
// TickGoalkeeperHoldAutoRelease doc comment.
#define SWOS_KEEPER_HOLD_AUTO_RELEASE_TICKS 300

// Private helpers (C# `private static`) -- forward-declared here since
// swosRunShotAtGoal (defined before them, matching the C# source's own
// order) calls into them.
static void setPlayerAnimationTable(int spriteAddr, int animTablePtr);
static void applyGoalScoredBranch(int a1KeeperAddr, int a2BallAddr,
                                   int a6TeamBase, int opponentBase, bool isTopTeam);
static bool applyGoalkeeperSavedBranch(int a1KeeperAddr, int a2BallAddr,
                                        int a6TeamBase, bool isTopTeam);
static int32_t ownPlayersBase(bool isTopTeam);
static int32_t opponentPlayersBase(bool isTopTeam);

void swosUpdateBallWithControllingGoalkeeper(int controllingPlayerAddr) {
    // player.cpp:202-208 -- dir = sprite.direction; byteOffset = dir << 2.
    int dir = swosReadSignedWord(controllingPlayerAddr + PLSPR_OFF_DIRECTION);
    int byteOffset = dir << 2; // each entry is 4 bytes (dx word + dy word)

    // player.cpp:210-220 -- newX = player.x.whole + kBallPlOffsets[byteOffset].
    int playerX = swosReadSignedWord(controllingPlayerAddr + PLSPR_OFF_X + 2);
    int playerY = swosReadSignedWord(controllingPlayerAddr + PLSPR_OFF_Y + 2);

    int offsX = swosReadSignedWord(ADDR_kBallPlOffsetsBase + byteOffset);
    int offsY = swosReadSignedWord(ADDR_kBallPlOffsetsBase + byteOffset + 2);

    int16_t newX = (int16_t)(playerX + offsX);
    int16_t newY = (int16_t)(playerY + offsY);

    // player.cpp:232-242 -- write ball position + destination + clear speed.
    swosBallSpriteSetSpeed(0);
    swosBallSpriteSetXPixels(newX);
    swosBallSpriteSetYPixels(newY);
    swosBallSpriteSetDestX(newX);
    swosBallSpriteSetDestY(newY);

    // player.cpp:243-263 -- sar deltaZ by 1 (preserves sign), then negate
    // ONLY if the result was positive. Net effect: dz <= 0 always (falling).
    int32_t dz = swosBallSpriteDeltaZ();
    int32_t dzHalf = swosAsr32(dz, 1);
    if (dzHalf > 0) dzHalf = -dzHalf;
    swosBallSpriteSetDeltaZ(dzHalf);

    // player.cpp:264 -- call resetBothTeamSpinTimers.
    swosResetBothTeamSpinTimers();
}


// ============================================================================
// goalkeeperClaimedTheBall -- player.cpp:2238-2336
// ============================================================================
void swosGoalkeeperClaimedTheBall(int keeperSpriteAddr, bool isTopTeam) {
    // player.cpp:2240-2241 -- lastPlayerBeforeGoalkeeper = lastPlayerPlayed.
    swosWriteDword(ADDR_lastPlayerBeforeGoalkeeper,
                    (uint32_t)swosReadSignedDword(ADDR_lastPlayerPlayed));
    // player.cpp:2242-2243 -- lastTeamScored = lastTeamPlayed.
    swosWriteDword(ADDR_lastTeamScored,
                    (uint32_t)swosReadSignedDword(ADDR_lastTeamPlayed));

    // player.cpp:2245 -- ball.speed = 0.
    swosBallSpriteSetSpeed(0);

    // player.cpp:2246-2263 -- camera direction + playerTurnFlags by team.
    int cameraDirection = isTopTeam ? 4 : 0;
    swosWriteWord(ADDR_cameraDirection, (uint16_t)cameraDirection);
    swosWriteByte(ADDR_playerTurnFlags, isTopTeam ? 0x7C : 0xC7);

    // player.cpp:2266-2279 -- CPU team additionally clears pure E/W directions.
    {
        int teamBaseTf = isTopTeam ? TEAMDATA_TOP_BASE : TEAMDATA_BOTTOM_BASE;
        int16_t pnTf = swosReadSignedWord(teamBaseTf + TEAMDATA_OFF_PLAYER_NUMBER);
        if (pnTf == 0) {
            uint8_t tf = swosReadByte(ADDR_playerTurnFlags);
            swosWriteByte(ADDR_playerTurnFlags, tf & 0xBB);
        }
    }

    // OpenSWOS fatigue: fires exactly once per catch (gameState != 3 gates
    // the first call only -- every repeat already sees 3, set just below).
    if (swosReadSignedWord(ADDR_gameState) != 3)
        swosPlayerEnergyDrainOnKeeperCatch(keeperSpriteAddr);

    // player.cpp:2297-2308.
    swosWriteWord(ADDR_gameState, 3);
    swosWriteWord(ADDR_breakCameraMode, (uint16_t)-1);
    swosWriteWord(ADDR_foulXCoordinate, (uint16_t)swosBallSpriteXPixels());
    swosWriteWord(ADDR_foulYCoordinate, (uint16_t)swosBallSpriteYPixels());
    swosWriteWord(ADDR_gameStatePl, 101);
    swosWriteWord(ADDR_gameNotInProgressCounterWriteOnly, 0);
    int teamBase = isTopTeam ? TEAMDATA_TOP_BASE : TEAMDATA_BOTTOM_BASE;
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)teamBase);
    swosWriteWord(ADDR_stoppageTimerTotal, 0);
    swosWriteWord(ADDR_stoppageTimerActive, 0);

    // player.cpp:2309 -- stopAllPlayers(). Runs even mid-dive.
    swosTeamPortStopAllPlayers();

    // player.cpp:2310-2323 -- if keeper still diving, return early. The
    // dive/catch handlers re-invoke this function once the animation ends.
    if (swosTeamDataGoalkeeperDivingRight(isTopTeam) != 0) return;
    if (swosTeamDataGoalkeeperDivingLeft(isTopTeam) != 0) return;

    // player.cpp:2325-2332.
    swosTeamDataSetControlledPlayer(isTopTeam, keeperSpriteAddr);
    swosTeamDataSetBallOutOfPlayOrKeeper(isTopTeam, 1);
    swosWriteWord(keeperSpriteAddr + PLSPR_OFF_DIRECTION, (uint16_t)cameraDirection);
    swosWriteWord(swosTeamDataBase(isTopTeam) + TEAMDATA_OFF_GOALIE_PLAYING_OR_OUT, 1);

    // player.cpp:2333 -- updatePlayerWithBall().
    swosUpdatePlayerWithBall(keeperSpriteAddr);

    // player.cpp:2334-2335.
    swosWriteWord(ADDR_cameraXVelocity, 0);
    swosWriteWord(ADDR_cameraYVelocity, 0);
}


// ============================================================================
// Mid-dive claim completion -- updatePlayers.cpp:3212-3242
// ============================================================================
bool swosTickGoalieDivingClaimCompletion(int keeperSpriteAddr, bool isTopTeam) {
    uint8_t state = swosReadByte(keeperSpriteAddr + PLSPR_OFF_PLAYER_STATE);
    if (state != PLSTATE_GOALIE_DIVING_HIGH && state != PLSTATE_GOALIE_DIVING_LOW)
        return false;

    if (swosTeamDataGoalkeeperDivingRight(isTopTeam) == 0)
        return false;

    // We run before the caller's own playerDownTimer decrement, so test
    // the PREDICTED post-decrement value (timer - 1).
    int8_t timer = (int8_t)swosReadByte(keeperSpriteAddr + PLSPR_OFF_PLAYER_DOWN_TIMER);
    int timerAfter = timer - 1;
    if (timerAfter <= 0) return false;   // rise path -- caller handles.
    if (timerAfter > 42) return false;   // still too early in the dive.

    // Consume this tick's decrement (caller skips its dive branch when we
    // return true).
    swosWriteByte(keeperSpriteAddr + PLSPR_OFF_PLAYER_DOWN_TIMER, timerAfter);

    // updatePlayers.cpp:3237 -- MUST precede the claim call so it runs its full path.
    swosTeamDataSetGoalkeeperDivingRight(isTopTeam, 0);

    swosUpdateBallWithControllingGoalkeeper(keeperSpriteAddr);
    swosGoalkeeperClaimedTheBall(keeperSpriteAddr, isTopTeam);

    // updatePlayers.cpp:3240-3241 -- ball.z.whole = 5 (keeper hand height).
    swosBallSpriteSetZPixels(5);

    // updatePlayers.cpp:3242 -> l_goalkeeper_rise + l_stop_player.
    swosWriteByte(keeperSpriteAddr + PLSPR_OFF_PLAYER_STATE, 0);
    setPlayerAnimationTable(keeperSpriteAddr, ADDR_kPlayerStandingAnimTableAddr);
    int16_t kx = swosReadSignedWord(keeperSpriteAddr + PLSPR_OFF_X + 2);
    int16_t ky = swosReadSignedWord(keeperSpriteAddr + PLSPR_OFF_Y + 2);
    swosWriteWord(keeperSpriteAddr + PLSPR_OFF_DEST_X, (uint16_t)kx);
    swosWriteWord(keeperSpriteAddr + PLSPR_OFF_DEST_Y, (uint16_t)ky);
    return true;
}


// ============================================================================
// goalkeeperCaughtTheBall -- updatePlayers.cpp:11022-11089
// ============================================================================
void swosGoalkeeperCaughtTheBall(int keeperSpriteAddr, bool isTopTeam) {
    int dir = isTopTeam ? 4 : 0;
    swosWriteWord(keeperSpriteAddr + PLSPR_OFF_DIRECTION, (uint16_t)dir);

    swosTeamDataSetGoalkeeperDivingLeft(isTopTeam, 0);

    setPlayerAnimationTable(keeperSpriteAddr, ADDR_kGoalieCatchingBallAnimTableAddr);

    swosWriteByte(keeperSpriteAddr + PLSPR_OFF_PLAYER_STATE, 4);   // kGoalieCatchingBall
    swosWriteByte(keeperSpriteAddr + PLSPR_OFF_PLAYER_DOWN_TIMER, 15);

    int catchSpeed = swosReadWord(ADDR_kGoalkeeperCatchSpeed);
    swosWriteWord(keeperSpriteAddr + PLSPR_OFF_SPEED, (uint16_t)catchSpeed);

    int16_t groundX = swosReadSignedWord(ADDR_ballNextGroundX);
    int16_t groundY = swosReadSignedWord(ADDR_ballNextGroundY);
    swosWriteWord(keeperSpriteAddr + PLSPR_OFF_DEST_X, (uint16_t)groundX);
    swosWriteWord(keeperSpriteAddr + PLSPR_OFF_DEST_Y, (uint16_t)groundY);

    if (groundY < 137) {
        swosWriteWord(keeperSpriteAddr + PLSPR_OFF_DEST_Y, 137);
    } else if (groundY > 761) {
        swosWriteWord(keeperSpriteAddr + PLSPR_OFF_DEST_Y, 761);
    }
}


// ============================================================================
// Per-tick state handlers -- kGoalieCatchingBall (4) / kGoalieClaimed (11)
// From updatePlayers.cpp:3059-3186
// ============================================================================
bool swosTickGoalieCatchingBall(int keeperSpriteAddr, bool isTopTeam) {
    int8_t timer = (int8_t)swosReadByte(keeperSpriteAddr + PLSPR_OFF_PLAYER_DOWN_TIMER);
    timer--;
    swosWriteByte(keeperSpriteAddr + PLSPR_OFF_PLAYER_DOWN_TIMER, timer);

    if (timer != 0) {
        // l_check_diving_side.
        if (swosTeamDataGoalkeeperDivingLeft(isTopTeam) != 0) return false;

        int teamBase = swosTeamDataBase(isTopTeam);
        uint8_t close = swosReadByte(teamBase + TEAMDATA_OFF_PL_VERY_CLOSE_TO_BALL);
        if (close == 0) return false;

        // RNG: D1 = (currentGameTick & 0xF0) >> 4. Compare with
        // shotChanceTable[+48]. Negative diff -> catch, else -> deflect.
        int32_t shotChanceTablePtr = swosReadSignedDword(teamBase + TEAMDATA_OFF_SHOT_CHANCE_TABLE);
        int16_t shotChance48 = shotChanceTablePtr != 0
            ? swosReadSignedWord(shotChanceTablePtr + 48)
            : 0;
        uint16_t gameTick = swosReadWord(ADDR_currentGameTick);
        int16_t d1Rnd = (int16_t)((gameTick & 0xF0) >> 4);
        int16_t diff = (int16_t)(d1Rnd - shotChance48);
        if (diff < 0) {
            // l_goalie_catches_the_ball.
            swosWriteWord(keeperSpriteAddr + PLSPR_OFF_SPEED, 0);
            swosBallSpriteSetSpeed(0);
            swosTeamDataSetGoalkeeperDivingLeft(isTopTeam, 1);
            swosGoalkeeperClaimedTheBall(keeperSpriteAddr, isTopTeam);
            // MatchAudio.KeeperClaimedComment() omitted (audio).
            return true;
        } else {
            swosGoalkeeperDeflectedBall(BALLSPR_BASE, teamBase);
            // MatchAudio.KeeperClaimedComment() omitted (audio).
            return false;
        }
    }

    // timer hit 0 -- transition to Normal.
    swosWriteByte(keeperSpriteAddr + PLSPR_OFF_PLAYER_STATE, 0);
    setPlayerAnimationTable(keeperSpriteAddr, ADDR_kPlayerStandingAnimTableAddr);

    // If goalkeeperDivingLeft was set (catch-pending flag), complete the claim.
    int16_t diving = swosTeamDataGoalkeeperDivingLeft(isTopTeam);
    if (diving != 0) {
        swosTeamDataSetGoalkeeperDivingLeft(isTopTeam, 0);
        swosUpdateBallWithControllingGoalkeeper(keeperSpriteAddr);
        swosGoalkeeperClaimedTheBall(keeperSpriteAddr, isTopTeam);
        swosBallSpriteSetZPixels(5);
    }
    return true;
}

bool swosTickGoalieClaimed(int keeperSpriteAddr) {
    int8_t timer = (int8_t)swosReadByte(keeperSpriteAddr + PLSPR_OFF_PLAYER_DOWN_TIMER);
    timer--;
    swosWriteByte(keeperSpriteAddr + PLSPR_OFF_PLAYER_DOWN_TIMER, timer);

    if (timer != 0) return false;

    swosWriteByte(keeperSpriteAddr + PLSPR_OFF_PLAYER_STATE, 0);
    setPlayerAnimationTable(keeperSpriteAddr, ADDR_kPlayerStandingAnimTableAddr);
    return true;
}


// ============================================================================
// Keeper-holds-ball release -- PORT-ONLY safety net (CPU teams only)
// ============================================================================
bool swosTickGoalkeeperHoldAutoRelease(int keeperSpriteAddr, bool isTopTeam) {
    // Faithful deferred mid-dive claim completion runs first.
    if (swosTickGoalieDivingClaimCompletion(keeperSpriteAddr, isTopTeam))
        return true;

    // Only meaningful during the keeper-holds-ball stoppage.
    if (swosReadSignedWord(ADDR_gameState) != 3) return false;

    // Only the team that's actually holding the ball releases.
    int32_t holdingTeam = swosReadSignedDword(ADDR_lastTeamPlayedBeforeBreak);
    int teamBase = swosTeamDataBase(isTopTeam);
    if (holdingTeam != 0 && holdingTeam != teamBase) return false;

    // Human-controlled team: the original waits indefinitely. Never auto-release.
    if (swosReadSignedWord(teamBase + TEAMDATA_OFF_PLAYER_NUMBER) != 0) return false;

    int16_t active = swosReadSignedWord(ADDR_stoppageTimerActive);
    if ((uint16_t)active < (uint16_t)SWOS_KEEPER_HOLD_AUTO_RELEASE_TICKS) return false;

    // Godot.GD.Print("[PORT-SAFETY] keeper hold auto-release") omitted (debug log).

    swosPlayerKickingBall(teamBase, keeperSpriteAddr);

    // Clear the pin so the kicked ball is no longer stuck to the keeper.
    swosWriteDword(teamBase + TEAMDATA_OFF_CONTROLLED_PLAYER, 0);
    swosWriteWord(teamBase + TEAMDATA_OFF_BALL_OUT_OF_PLAY_OR_KEEPER, 0);

    swosWriteWord(teamBase + TEAMDATA_OFF_PASSING_BALL, 0);
    swosWriteWord(teamBase + TEAMDATA_OFF_PASSING_TO_PLAYER, 0);
    swosWriteWord(teamBase + TEAMDATA_OFF_SHOOTING, 0);
    swosWriteWord(teamBase + TEAMDATA_OFF_PLAYER_SWITCH_TIMER, 25);

    // Resume live play.
    swosWriteWord(ADDR_gameStatePl, 100);
    swosWriteWord(ADDR_gameState, 100);

    swosWriteWord(ADDR_stoppageTimerActive, 0);
    swosWriteWord(ADDR_stoppageTimerTotal, 0);

    swosWriteByte(keeperSpriteAddr + PLSPR_OFF_PLAYER_STATE, 0);
    setPlayerAnimationTable(keeperSpriteAddr, ADDR_kPlayerStandingAnimTableAddr);
    return true;
}


// ============================================================================
// getFramesNeededToCoverDistance -- updatePlayers.cpp:11105-11191
// ============================================================================
int swosGetFramesNeededToCoverDistance(int d0, int d4) {
    if (d0 == 0) return 0;             // zero delta = no frames.
    if (d0 < 0) d0 = -d0;              // absolute value.

    // Scale d0 up (and d5 in lockstep) until d0 >= 1.0 fixed-point.
    int d5 = 1;
    while ((uint32_t)d0 < 0x10000u) {
        d0 <<= 1;
        d5 <<= 1;
    }

    // Convert d4 from "whole-pixel int" to Q16.16.
    d4 = (int)((uint32_t)(d4 & 0xFFFF) << 16);

    int d7 = 0;

l_divide_loop:;
    d7 = (int16_t)(d7 + d5);
    d4 = (int)((uint32_t)d4 - (uint32_t)d0);
    if (d4 >= 0) goto l_divide_loop;

    return d7 & 0xFFFF;
}


// ============================================================================
// shouldGoalkeeperDive -- updatePlayers.cpp:10485-10816
// ============================================================================
bool swosShouldGoalkeeperDive(int a1KeeperAddr, int a2BallAddr, int a6TeamBase) {
    int16_t ballYw = swosReadSignedWord(a2BallAddr + PLSPR_OFF_Y + 2);
    int16_t keeperYw = swosReadSignedWord(a1KeeperAddr + PLSPR_OFF_Y + 2);
    int16_t d0w = (int16_t)(ballYw - keeperYw);

    // For BOTTOM team the sign is inverted.
    bool isTopTeam = (a6TeamBase == TEAMDATA_TOP_BASE);
    if (!isTopTeam) {
        d0w = (int16_t)(-d0w);
    }

    if (d0w >= 0) goto l_ball_in_front_of_goalkeeper;

    // Behind keeper, but within 10 px -> still try.
    if (d0w < -10) goto l_goalkeeper_wont_dive;
    goto l_try_saving;

l_ball_in_front_of_goalkeeper:;
    if (d0w < 0) goto l_goalkeeper_wont_dive;   // redundant, preserved for parity.

    {
        int16_t playingPenalties = swosReadSignedWord(ADDR_playingPenalties);
        if (playingPenalties != 0) goto l_penalty_shot;
        int16_t penalty = swosReadSignedWord(ADDR_penalty);
        if (penalty == 0) goto l_normal_shot;
    }

l_penalty_shot:;
    {
        int16_t d1 = swosReadSignedWord(ADDR_kKeeperPenaltySaveDistanceFar);
        int rnd = swosRngNextByte() & 0x18;
        if (rnd != 0)
            d1 = swosReadSignedWord(ADDR_kKeeperPenaltySaveDistanceNear);

        if (d0w > d1) goto l_goalkeeper_wont_dive;
        goto l_try_saving;
    }

l_normal_shot:;
    {
        int16_t kSaveDist = swosReadSignedWord(ADDR_kKeeperSaveDistance);
        int16_t d1 = kSaveDist;
        if (d0w > d1) goto l_goalkeeper_wont_dive;

        int16_t ballYw2 = swosReadSignedWord(a2BallAddr + PLSPR_OFF_Y + 2);
        int16_t keeperYw2 = swosReadSignedWord(a1KeeperAddr + PLSPR_OFF_Y + 2);
        int16_t d4w = (int16_t)(ballYw2 - keeperYw2);
        if (d4w < 0) d4w = (int16_t)(-d4w);

        int32_t ballDeltaY = swosReadSignedDword(a2BallAddr + PLSPR_OFF_DELTA_Y);
        int d1Frames = swosGetFramesNeededToCoverDistance(ballDeltaY, d4w);
        if (d1Frames == 0) goto l_goalkeeper_wont_dive;

        int16_t ballDefX = swosReadSignedWord(ADDR_ballDefensiveX);
        int16_t keeperXw = swosReadSignedWord(a1KeeperAddr + PLSPR_OFF_X + 2);
        int16_t d4w2 = (int16_t)(ballDefX - keeperXw);
        if (d4w2 < 0) d4w2 = (int16_t)(-d4w2);

        int32_t keeperDeltaX = swosReadSignedDword(a1KeeperAddr + PLSPR_OFF_DELTA_X);
        int d2Frames = swosGetFramesNeededToCoverDistance(keeperDeltaX, d4w2);
        if (d2Frames == 0) goto l_goalkeeper_wont_dive;

        // If keeper's X-traverse takes <= ball's Y-traverse, ball arrives first.
        if ((uint32_t)d2Frames <= (uint32_t)d1Frames) goto l_goalkeeper_wont_dive;

        int16_t ballDefX2 = swosReadSignedWord(ADDR_ballDefensiveX);
        int16_t keeperXw2 = swosReadSignedWord(a1KeeperAddr + PLSPR_OFF_X + 2);
        int16_t d4w3 = (int16_t)(ballDefX2 - keeperXw2);
        if (d4w3 < 0) d4w3 = (int16_t)(-d4w3);

        int32_t shotChanceTablePtr = swosReadSignedDword(a6TeamBase + TEAMDATA_OFF_SHOT_CHANCE_TABLE);
        uint16_t gameTick = swosReadWord(ADDR_currentGameTick);
        int idx16 = ((gameTick & 0x0F) << 1);
        int16_t chanceEntry;
        if (shotChanceTablePtr != 0) {
            chanceEntry = swosReadSignedWord(shotChanceTablePtr + idx16 + 10);
        } else {
            chanceEntry = 0;
        }
        chanceEntry--;
        if (chanceEntry < 0) chanceEntry = 0;

        int32_t diveDelta = swosReadSignedDword(ADDR_kGoalkeeperDiveDeltasBase + (chanceEntry << 2));

        int16_t divDead = swosReadSignedWord(ADDR_goalkeeperDiveDeadVar);
        swosWriteWord(ADDR_goalkeeperDiveDeadVar, (uint16_t)(int16_t)(divDead + 1));

        int d3Frames = swosGetFramesNeededToCoverDistance(diveDelta, d4w3);
        if (d3Frames == 0) goto l_goalkeeper_wont_dive;

        // If keeper's dive-cover frames < ball's Y-arrival frames -> too slow.
        if ((uint32_t)d3Frames < (uint32_t)d1Frames) goto l_goalkeeper_wont_dive;
    }

l_try_saving:;
    // Telemetry omitted (BumpShouldDiveTrue -- zero Memory effect, see header).
    return true;

l_goalkeeper_wont_dive:;
    return false;
}


// ============================================================================
// goalkeeperJumping -- updatePlayers.cpp:10829-11017
// ============================================================================
void swosGoalkeeperJumping(int d0Dir, int d1SpeedFlag, int d3DestDir,
                            int a1KeeperAddr, int a2BallAddr, int a6TeamBase) {
    (void)a2BallAddr;  // read in the source for a dead side-effect-free value; unused otherwise.

    uint32_t ballDist = swosReadDword(a1KeeperAddr + PLSPR_OFF_BALL_DISTANCE);
    if (ballDist > 128u) goto l_ball_far_away;

    {
        int16_t nearSpeed = swosReadSignedWord(ADDR_kGoalkeeperNearJumpSpeed);
        swosWriteWord(a1KeeperAddr + PLSPR_OFF_SPEED, (uint16_t)nearSpeed);
        goto l_speed_setting_done;
    }

l_ball_far_away:;
    {
        int16_t farSpeed = swosReadSignedWord(ADDR_kGoalkeeperFarJumpSpeed);
        swosWriteWord(a1KeeperAddr + PLSPR_OFF_SPEED, (uint16_t)farSpeed);

        if (d1SpeedFlag == 0) goto l_speed_setting_done;

        int16_t slowerSpeed = swosReadSignedWord(ADDR_kGoalkeeperFarJumpSlowerSpeed);
        swosWriteWord(a1KeeperAddr + PLSPR_OFF_SPEED, (uint16_t)slowerSpeed);

        if (d1SpeedFlag == 1) goto l_speed_setting_done;

        uint16_t tick = swosReadWord(ADDR_currentGameTick);
        int16_t nearSpd = swosReadSignedWord(ADDR_kGoalkeeperNearJumpSpeed);
        int randomised = (tick & 0xFF) + nearSpd;
        swosWriteWord(a1KeeperAddr + PLSPR_OFF_SPEED, (uint16_t)randomised);
    }

l_speed_setting_done:;
    // 10890-10901 -- D1 = ball.x.w - keeper.x.w, captured but never used
    // outside the side effect; preserved for parity (no-op here too since
    // our reads have no side effects).

    // team.field_46 (per the C# source's own comment; offset 70, no named
    // TeamData accessor -- see swos_team_data.h's header note on this
    // class of gap) = 0; team.goalkeeperDivingRight = 0.
    swosWriteByte(a6TeamBase + 70 /* field_46 */, 0);
    swosTeamDataSetGoalkeeperDivingRight((a6TeamBase == TEAMDATA_TOP_BASE), 0);

    {
        bool isBottomTeam = (a6TeamBase == TEAMDATA_BOTTOM_BASE);

        uint16_t ballNotHighZ = swosReadWord(ADDR_ballNotHighZ);
        if (ballNotHighZ > 5) goto l_goalie_jumping_high;

        swosWriteByte(a1KeeperAddr + PLSPR_OFF_PLAYER_STATE, PLSTATE_GOALIE_DIVING_LOW);
        // Telemetry omitted (BumpDiveCounter -- zero Memory effect, see header).

        if (isBottomTeam) goto l_right_goalie_jumping_low;

        setPlayerAnimationTable(a1KeeperAddr, ADDR_kLeftGoalieJumpingLowAnimTableAddr);
        goto l_set_down_timer;

l_right_goalie_jumping_low:;
        setPlayerAnimationTable(a1KeeperAddr, ADDR_kRightGoalieJumpingLowAnimTableAddr);
        goto l_set_down_timer;

l_goalie_jumping_high:;
        swosWriteByte(a1KeeperAddr + PLSPR_OFF_PLAYER_STATE, PLSTATE_GOALIE_DIVING_HIGH);
        // Telemetry omitted (BumpDiveCounter -- zero Memory effect, see header).

        if (isBottomTeam) goto l_right_goalie_jumping_high;

        setPlayerAnimationTable(a1KeeperAddr, ADDR_kLeftGoalieJumpingHighAnimTableAddr);
        goto l_set_down_timer;

l_right_goalie_jumping_high:;
        setPlayerAnimationTable(a1KeeperAddr, ADDR_kRightGoalieJumpingHighAnimTableAddr);

l_set_down_timer:;
        swosWriteByte(a1KeeperAddr + PLSPR_OFF_PLAYER_DOWN_TIMER, 75);
        swosWriteWord(a6TeamBase + TEAMDATA_OFF_CONTROLLED_PL_DIRECTION, (uint16_t)d0Dir);

        // Unclamped, like the original -- see the header comment above for
        // why a defensive clamp here was historically wrong.
        int defDestEntry = ADDR_kDefaultDestinations + (d3DestDir << 2);
        int16_t dx = swosReadSignedWord(defDestEntry);
        int16_t keeperXw3 = swosReadSignedWord(a1KeeperAddr + PLSPR_OFF_X + 2);
        int16_t dy = swosReadSignedWord(defDestEntry + 2);
        int16_t keeperYw3 = swosReadSignedWord(a1KeeperAddr + PLSPR_OFF_Y + 2);
        int16_t keeperDestX = (int16_t)(dx + keeperXw3);
        int16_t keeperDestY = (int16_t)(dy + keeperYw3);
        swosWriteWord(a1KeeperAddr + PLSPR_OFF_DEST_X, (uint16_t)keeperDestX);
        swosWriteWord(a1KeeperAddr + PLSPR_OFF_DEST_Y, (uint16_t)keeperDestY);
    }
}


// ============================================================================
// goalkeeperDeflectedBall -- player.cpp:2342-2528
// ============================================================================
void swosGoalkeeperDeflectedBall(int a2BallAddr, int a6TeamBase) {
    bool isBottomTeam = (a6TeamBase == TEAMDATA_BOTTOM_BASE);
    int16_t d0 = isBottomTeam ? (int16_t)0 : (int16_t)4;

    int destByteOff = ((uint16_t)d0 & 0xFFFFu) << 2;
    int16_t ballXw = swosReadSignedWord(a2BallAddr + PLSPR_OFF_X + 2);
    int16_t defDx = swosReadSignedWord(ADDR_kDefaultDestinations + destByteOff);
    int16_t newDestX = (int16_t)(ballXw + defDx);
    swosWriteWord(a2BallAddr + PLSPR_OFF_DEST_X, (uint16_t)newDestX);

    int16_t ballYw = swosReadSignedWord(a2BallAddr + PLSPR_OFF_Y + 2);
    int16_t defDy = swosReadSignedWord(ADDR_kDefaultDestinations + destByteOff + 2);
    int16_t newDestY = (int16_t)(ballYw + defDy);
    swosWriteWord(a2BallAddr + PLSPR_OFF_DEST_Y, (uint16_t)newDestY);

    // X jitter.
    uint16_t gameTick = swosReadWord(ADDR_currentGameTick);
    int16_t jitterX = (int16_t)(((gameTick & 31) << 5) - 512);
    int16_t curDestX = swosReadSignedWord(a2BallAddr + PLSPR_OFF_DEST_X);
    swosWriteWord(a2BallAddr + PLSPR_OFF_DEST_X, (uint16_t)(int16_t)(curDestX + jitterX));

    // Deflect-strength thresholds from team.shotChanceTable[+52]/[+54].
    int32_t shotChanceTablePtr = swosReadSignedDword(a6TeamBase + TEAMDATA_OFF_SHOT_CHANCE_TABLE);
    int16_t d1 = (int16_t)((gameTick & 0x3C) >> 2);
    int16_t thresh1, thresh2;
    if (shotChanceTablePtr != 0) {
        thresh1 = swosReadSignedWord(shotChanceTablePtr + 52);
        thresh2 = swosReadSignedWord(shotChanceTablePtr + 54);
    } else {
        // shotChanceTable not yet populated -- always-weak deflection.
        thresh1 = 32767;
        thresh2 = 32767;
    }

    int16_t deflectSpeed;
    d1 = (int16_t)(d1 - thresh1);
    if (d1 < 0) {
        deflectSpeed = swosReadSignedWord(ADDR_kGoalkeeperStrongDeflectBallSpeed);
        goto l_set_ball_speed;
    }

    d1 = (int16_t)(d1 - thresh2);
    if (d1 < 0) {
        deflectSpeed = swosReadSignedWord(ADDR_kGoalkeeperMediumDeflectBallSpeed);
        goto l_set_ball_speed;
    }

    deflectSpeed = swosReadSignedWord(ADDR_kGoalkeeperWeakDeflectBallSpeed);

l_set_ball_speed:;
    {
        int16_t speedJitter = (int16_t)((gameTick & 0x1FF) - 256);
        int16_t newSpeed = (int16_t)(speedJitter + deflectSpeed);
        swosWriteWord(a2BallAddr + PLSPR_OFF_SPEED, (uint16_t)newSpeed);

        int32_t deflectDz = swosReadSignedDword(ADDR_kGoalkeeperDeflectDeltaZ);
        int32_t dzJitter = (int32_t)(((uint32_t)gameTick & 0x7Fu) << 8) - 16384;
        int32_t newDeltaZ = dzJitter + deflectDz;
        swosWriteDword(a2BallAddr + PLSPR_OFF_DELTA_Z, (uint32_t)newDeltaZ);
    }

    swosResetBothTeamSpinTimers();

    // PlayKickSample() omitted (audio).
}


// ============================================================================
// RunShotTripWire -- updatePlayers.cpp:2072-2194 (cseg_7F7BC)
// ============================================================================
SwosShotChainExit swosRunShotTripWire(int a1KeeperAddr, int a2BallAddr,
                                       int a6TeamBase, bool isTopTeam) {
    (void)isTopTeam;  // matches the C# signature; unused in this function's body.

    int32_t opponentBase = swosReadSignedDword(a6TeamBase + TEAMDATA_OFF_OPPONENTS_TEAM);
    int16_t opponentHasBall = swosReadSignedWord(opponentBase + TEAMDATA_OFF_PLAYER_HAS_BALL);
    if (opponentHasBall != 0) return SWOS_SHOT_CHAIN_C7FC48;

    int16_t thisHasBall = swosReadSignedWord(a6TeamBase + TEAMDATA_OFF_PLAYER_HAS_BALL);
    if (thisHasBall != 0) return SWOS_SHOT_CHAIN_C7FC48;

    uint8_t plVeryClose = swosReadByte(a6TeamBase + TEAMDATA_OFF_PL_VERY_CLOSE_TO_BALL);
    if (plVeryClose != 0) return SWOS_SHOT_CHAIN_C7FC48;

    int32_t lastTeamPlayed = swosReadSignedDword(ADDR_lastTeamPlayed);
    if (a6TeamBase == lastTeamPlayed) {
        int16_t playerHadBall = swosReadSignedWord(ADDR_playerHadBall);
        if (playerHadBall == 0)
            return SWOS_SHOT_CHAIN_C7FBEF;
        // Fall through to the ball-speed check.
    }

    {
        int16_t kMin = swosReadSignedWord(ADDR_kShotAtGoalMinumumSpeed);
        int16_t ballSpeed = swosReadSignedWord(a2BallAddr + PLSPR_OFF_SPEED);
        if ((uint16_t)kMin < (uint16_t)ballSpeed) return SWOS_SHOT_CHAIN_SHOT_AT_GOAL;
    }

    int32_t ballDistance = swosReadSignedDword(a1KeeperAddr + PLSPR_OFF_BALL_DISTANCE);
    if ((uint32_t)ballDistance > 5000u) return SWOS_SHOT_CHAIN_C7FC01;

    int16_t ballZw = swosReadSignedWord(a2BallAddr + PLSPR_OFF_Z + 2);
    if ((uint16_t)ballZw > 12) return SWOS_SHOT_CHAIN_C7FC01;

    // Only deltaZ == 0x8000 exactly falls through.
    int32_t ballDeltaZ = swosReadSignedDword(a2BallAddr + PLSPR_OFF_DELTA_Z);
    if (ballDeltaZ > 0x8000) return SWOS_SHOT_CHAIN_C7FC01;
    if (ballDeltaZ < 0x8000) return SWOS_SHOT_CHAIN_C7FC01;

    return SWOS_SHOT_CHAIN_C7FC48;
}


// ============================================================================
// RunShotAtGoal -- updatePlayers.cpp:2196-2544 (l_shot_at_goal)
// ============================================================================
SwosShotChainExit swosRunShotAtGoal(int a1KeeperAddr, int a2BallAddr,
                                     int a6TeamBase, bool isTopTeam) {
    int32_t opponentBase = swosReadSignedDword(a6TeamBase + TEAMDATA_OFF_OPPONENTS_TEAM);

    // Ball too high -> no dive.
    int16_t ballZwTop = swosReadSignedWord(a2BallAddr + PLSPR_OFF_Z + 2);
    if ((uint16_t)ballZwTop > 16) return SWOS_SHOT_CHAIN_C7FC01;

    // Positive save-comment timer -> keeper just made a save, force the dive path.
    int16_t gksTimer = swosTeamDataGoalkeeperSavedCommentTimer(isTopTeam);
    bool forceSavedPath = (gksTimer > 0);

    if (!forceSavedPath) {
        int32_t keeperBallDistance = swosReadSignedDword(a1KeeperAddr + PLSPR_OFF_BALL_DISTANCE);
        if (keeperBallDistance > 128) forceSavedPath = true;
    }

    if (!forceSavedPath) {
        uint8_t ballAbove17 = swosReadByte(a6TeamBase + TEAMDATA_OFF_BALL_ABOVE_17);
        if (ballAbove17 != 0) forceSavedPath = true;
    }

    if (!forceSavedPath) {
        // Opponent hasn't pressed fire -> ball moving but not a committed kick.
        uint8_t opFire = swosReadByte(opponentBase + TEAMDATA_OFF_FIRE_PRESSED);
        if (opFire == 0) forceSavedPath = true;
    }

    if (!forceSavedPath) {
        int16_t opHasBall = swosReadSignedWord(opponentBase + TEAMDATA_OFF_PLAYER_HAS_BALL);
        if (opHasBall == 0) {
            int16_t opPassKick = swosReadSignedWord(opponentBase + TEAMDATA_OFF_PASS_KICK_TIMER);
            if ((uint16_t)opPassKick < 22) forceSavedPath = true;
        }
    }

    // cseg_7F910 -- RNG goal-roll.
    if (!forceSavedPath) {
        int d1Finishing = 0;
        int32_t lastHeadTackle = swosReadSignedDword(opponentBase + TEAMDATA_OFF_LAST_HEADING_PLAYER);
        if (lastHeadTackle != 0) {
            int16_t ordinal = swosReadSignedWord(lastHeadTackle + PLSPR_OFF_PLAYER_ORDINAL);
            int32_t opPlayersBase = opponentPlayersBase(isTopTeam);
            if (ordinal >= 1 && ordinal <= 11 && opPlayersBase != 0) {
                int32_t piAddr = opPlayersBase + (ordinal - 1) * TDL_PLAYER_INFO_SIZE;
                d1Finishing = (int8_t)swosReadByte(piAddr + TDL_OFF_FINISHING);
            }
        }

        int d1GoalieSkill = 0;
        {
            int16_t kOrdinal = swosReadSignedWord(a1KeeperAddr + PLSPR_OFF_PLAYER_ORDINAL);
            int32_t myPlayersBase = ownPlayersBase(isTopTeam);
            if (kOrdinal >= 1 && kOrdinal <= 11 && myPlayersBase != 0) {
                int32_t piAddr = myPlayersBase + (kOrdinal - 1) * TDL_PLAYER_INFO_SIZE;
                d1GoalieSkill = (int8_t)swosReadByte(piAddr + TDL_OFF_GOALIE_SKILL);
            }
        }
        // OpenSWOS fatigue: a tired keeper saves worse.
        d1GoalieSkill -= swosPlayerEnergyKeeperSkillPenalty(a1KeeperAddr);
        if (d1GoalieSkill < 0) d1GoalieSkill = 0;
        int d1 = d1Finishing - d1GoalieSkill;
        // `cbw` -- sign-extend low byte to word.
        d1 = (int8_t)(d1 & 0xFF);
        d1 += 7;
        if (d1 < 0) d1 = 0;
        if (d1 > 14) d1 = 14;

        uint16_t gameTick = swosReadWord(ADDR_currentGameTick);
        int d0Sample = (gameTick >> 1) & 15;
        uint8_t threshold = swosReadByte(ADDR_goalScoredChances + d1);

        if ((uint8_t)d0Sample < threshold) {
            // l_goal_scored.
            applyGoalScoredBranch(a1KeeperAddr, a2BallAddr, a6TeamBase, opponentBase, isTopTeam);
            return SWOS_SHOT_CHAIN_CLAMP;
        }

        // Keeper save imminent -- commentary timer fires.
        swosTeamDataSetGoalkeeperSavedCommentTimer(isTopTeam, 5);
    }

    // l_goalkeeper_saved.
    return applyGoalkeeperSavedBranch(a1KeeperAddr, a2BallAddr, a6TeamBase, isTopTeam)
        ? SWOS_SHOT_CHAIN_CLAMP
        : SWOS_SHOT_CHAIN_C7FC01;
}

// updatePlayers.cpp:2364-2428 -- l_goal_scored branch.
static void applyGoalScoredBranch(int a1KeeperAddr, int a2BallAddr,
                                   int a6TeamBase, int opponentBase, bool isTopTeam) {
    (void)opponentBase;  // parameter carried from the caller; unused in this function's body (matches C#).

    // Negative = "scored against".
    swosTeamDataSetGoalkeeperSavedCommentTimer(isTopTeam, -5);

    // Keeper faces the side the ball beat him on.
    int16_t ballXw = swosReadSignedWord(a2BallAddr + PLSPR_OFF_X + 2);
    int16_t keeperXw = swosReadSignedWord(a1KeeperAddr + PLSPR_OFF_X + 2);
    int16_t d0Direction, d3DestDir;
    if ((uint16_t)ballXw < (uint16_t)keeperXw) {
        d0Direction = 6; d3DestDir = 6;
    } else {
        d0Direction = 2; d3DestDir = 2;
    }

    swosWriteWord(a1KeeperAddr + PLSPR_OFF_DIRECTION, (uint16_t)d0Direction);

    // ballSpeed mirror (dseg_114EA8, audio/stats only) -- STUB in the
    // source too, omitted here as well.

    // Opponent's spin timer cancelled -- no in-flight spin on a scored goal.
    swosTeamDataSetSpinTimer(!isTopTeam, -1);

    // Cap ball.speed at 1536.
    int16_t ballSpeed = swosReadSignedWord(a2BallAddr + PLSPR_OFF_SPEED);
    if ((uint16_t)ballSpeed > 1536u) {
        swosWriteWord(a2BallAddr + PLSPR_OFF_SPEED, 1536);
    }

    // Futile dive animation -- the goal itself is credited by ball.cpp when
    // the ball crosses the line, not here.
    swosGoalkeeperJumping(d0Direction, 0, d3DestDir, a1KeeperAddr, a2BallAddr, a6TeamBase);

    swosWriteWord(a6TeamBase + TEAMDATA_OFF_PASS_KICK_TIMER, 0);
    swosWriteDword(a6TeamBase + TEAMDATA_OFF_PASSING_KICKING_PLAYER, 0);
}

// updatePlayers.cpp:2430-2544 -- l_goalkeeper_saved branch. Returns true
// when the dive was committed, false when swosShouldGoalkeeperDive vetoed.
static bool applyGoalkeeperSavedBranch(int a1KeeperAddr, int a2BallAddr,
                                        int a6TeamBase, bool isTopTeam) {
    (void)isTopTeam;  // parameter carried from the caller; unused in this function's body (matches C#).
    if (!swosShouldGoalkeeperDive(a1KeeperAddr, a2BallAddr, a6TeamBase)) return false;

    int16_t ballDefX = swosReadSignedWord(ADDR_ballDefensiveX);
    int16_t keeperXw = swosReadSignedWord(a1KeeperAddr + PLSPR_OFF_X + 2);

    int16_t d0Dir, d1SpeedFlag, d3DestDir;

    if ((uint16_t)ballDefX < (uint16_t)keeperXw) {
        // W-side dive selection.
        d1SpeedFlag = 0;
        int16_t playingPenalties = swosReadSignedWord(ADDR_playingPenalties);
        int16_t penalty = swosReadSignedWord(ADDR_penalty);
        bool isPenalty = (playingPenalties != 0) || (penalty != 0);
        if (isPenalty) {
            d1SpeedFlag = 1;
            uint16_t gameTick = swosReadWord(ADDR_currentGameTick);
            if ((gameTick & 12) == 0) {
                d0Dir = 2; d3DestDir = 2;
            } else {
                d0Dir = 6; d3DestDir = 6;
                d1SpeedFlag = 0;
            }
        } else {
            d0Dir = 6; d3DestDir = 6;
        }
    } else {
        // E-side dive selection.
        d1SpeedFlag = 0;
        int16_t playingPenalties = swosReadSignedWord(ADDR_playingPenalties);
        int16_t penalty = swosReadSignedWord(ADDR_penalty);
        bool isPenalty = (playingPenalties != 0) || (penalty != 0);
        if (isPenalty) {
            d1SpeedFlag = 1;
            uint16_t gameTick = swosReadWord(ADDR_currentGameTick);
            if ((gameTick & 6) == 0) {
                d0Dir = 6; d3DestDir = 6;
            } else {
                d1SpeedFlag = 0;
                d0Dir = 2; d3DestDir = 2;
            }
        } else {
            d0Dir = 2; d3DestDir = 2;
        }
    }

    swosWriteWord(a1KeeperAddr + PLSPR_OFF_DIRECTION, (uint16_t)d0Dir);
    swosGoalkeeperJumping(d0Dir, d1SpeedFlag, d3DestDir, a1KeeperAddr, a2BallAddr, a6TeamBase);

    swosWriteWord(a6TeamBase + TEAMDATA_OFF_PASS_KICK_TIMER, 0);
    swosWriteDword(a6TeamBase + TEAMDATA_OFF_PASSING_KICKING_PLAYER, 0);
    return true;
}

// Resolve the PlayerInfo base for THIS team (TeamData.OffInGameTeamPtr).
static int32_t ownPlayersBase(bool isTopTeam) {
    int teamBase = swosTeamDataBase(isTopTeam);
    return swosReadSignedDword(teamBase + TEAMDATA_OFF_IN_GAME_TEAM_PTR);
}

static int32_t opponentPlayersBase(bool isTopTeam) {
    return ownPlayersBase(!isTopTeam);
}

// updatePlayers.cpp:10930/11048 etc -- forwards to the real port.
static void setPlayerAnimationTable(int spriteAddr, int animTablePtr) {
    swosSetPlayerAnimationTable(spriteAddr, animTablePtr);
}
