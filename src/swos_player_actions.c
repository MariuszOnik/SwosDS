// SOURCE: openswos game/scripts/Sim/Port/PlayerActions.cs (full file).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes. See
// swos_player_actions.h for what was deliberately omitted (telemetry,
// audio, PlayerControlled counters) and what was forward-pulled as a
// minimal slice (TeamDataLoader offsets, PlayerEnergy).
#include "swos_player_actions.h"
#include "swos_player_controlled.h"
#include "swos_addr.h"
#include "swos_ball_sprite.h"
#include "swos_ball_update.h"
#include "swos_memory.h"
#include "swos_player_energy.h"
#include "swos_player_sprite.h"
#include "swos_rng.h"
#include "swos_sprite_update.h"
#include "swos_team_data.h"
#include "swos_team_data_loader.h"
#include "swos_util.h"

#include <stdint.h>

void swosSetPlayerAnimationTable(int playerAddr, int animTable) {
    // 104312-104314 -- sprite.animationTable = A0.
    swosWriteDword(playerAddr + PLSPR_OFF_ANIM_TABLE_PTR, (uint32_t)animTable);

    // 104316-104324 -- D0 = (teamNumber - 1); if ordinal == 1 -> D0 += 2.
    int16_t teamNum = swosReadSignedWord(playerAddr + PLSPR_OFF_TEAM_NUMBER);
    int16_t d0Index = (int16_t)(teamNum - 1);
    int16_t ord = swosReadSignedWord(playerAddr + PLSPR_OFF_PLAYER_ORDINAL);
    if (ord == 1) {
        d0Index = (int16_t)(d0Index + 2);
    }

    // 104326-104333 -- D0 = (D0 << 3) + direction, then D0 <<= 2 (byte
    // offset into the 32-entry pointer table).
    d0Index = (int16_t)(d0Index << 3);
    int16_t pDir = swosReadSignedWord(playerAddr + PLSPR_OFF_DIRECTION);
    d0Index = (int16_t)(d0Index + pDir);
    int d0Off = ((uint16_t)d0Index & 0xFFFFu) << 2;

    // 104334-104337 -- sprite.frameDelay = *(word*)A0.
    int16_t frameDelay = swosReadSignedWord(animTable);
    swosWriteWord(playerAddr + PLSPR_OFF_FRAME_DELAY, (uint16_t)frameDelay);

    // 104338-104342 -- sprite.frameIndicesTable = *(dword*)(A0 + 2 + D0).
    int32_t fitPtr = swosReadSignedDword(animTable + 2 + d0Off);
    swosWriteDword(playerAddr + PLSPR_OFF_FRAME_INDICES_TABLE, (uint32_t)fitPtr);

    // 104343-104344 -- fatal_error path: animation table has a null pointer
    // at the requested (team, ordinal, direction) slot. The asm `int 3`s
    // here; OpenSWOS's port treats it as a soft no-op -- leaves the sprite
    // in whatever animation it had before (animationTable was already
    // written above, matching the asm which also leaves it updated).
    if (fitPtr == 0) {
        return;
    }

    // 104345-104354 -- reset cycle bookkeeping + cache startingDirection.
    swosWriteWord(playerAddr + PLSPR_OFF_FRAME_SWITCH_COUNTER, (uint16_t)-1);
    swosWriteWord(playerAddr + PLSPR_OFF_FRAME_INDEX, (uint16_t)-1);
    swosWriteWord(playerAddr + PLSPR_OFF_CYCLE_FRAMES_TIMER, 1);
    swosWriteWord(playerAddr + PLSPR_OFF_STARTING_DIRECTION, (uint16_t)pDir);
}


// ============================================================================
// setPlayerAnimationTableAndPictureIndex -- player.cpp:3450-3558 (static)
// ============================================================================
void swosSetPlayerAnimationTableAndPictureIndex(int animTable, int playerAddr) {
    // player.cpp:3452-3454 -- player.animationTable = A0.
    swosWriteDword(playerAddr + PLSPR_OFF_ANIM_TABLE_PTR, (uint32_t)animTable);

    // player.cpp:3456-3463 -- D0 = teamNumber - 1.
    int16_t teamNum = swosReadSignedWord(playerAddr + PLSPR_OFF_TEAM_NUMBER);
    int16_t d0Index = (int16_t)(teamNum - 1);

    // player.cpp:3465-3474 -- if ordinal == 1 -> D0 += 2.
    int16_t ord = swosReadSignedWord(playerAddr + PLSPR_OFF_PLAYER_ORDINAL);
    if (ord == 1) {
        d0Index = (int16_t)(d0Index + 2);
    }

    // l_calc_table_index (player.cpp:3483-3499).
    d0Index = (int16_t)(d0Index << 3);
    int16_t pDir = swosReadSignedWord(playerAddr + PLSPR_OFF_DIRECTION);
    d0Index = (int16_t)(d0Index + pDir);
    int d0Off = ((uint16_t)d0Index & 0xFFFFu) << 2;

    // player.cpp:3500-3508 -- frameDelay = [A0]; frameIndicesTable = [A0 + d0Off + 2].
    if (animTable < 0 || animTable > 0x60000) return;  // Bail if outside Memory.

    int16_t fdelay = swosReadSignedWord(animTable);
    swosWriteWord(playerAddr + PLSPR_OFF_FRAME_DELAY, (uint16_t)fdelay);
    int32_t fitPtr = swosReadSignedDword(animTable + d0Off + 2);
    swosWriteDword(playerAddr + PLSPR_OFF_FRAME_INDICES_TABLE, (uint32_t)fitPtr);

    // player.cpp:3509-3513 -- if frameIndicesTable == 0 -> fatal_error.
    if (fitPtr == 0) return;

    // player.cpp:3515-3516 -- startingDirection = direction.
    swosWriteWord(playerAddr + PLSPR_OFF_STARTING_DIRECTION, (uint16_t)pDir);

    // player.cpp:3517-3528 -- D0 = [frameIndicesTable + frameIndex*2].
    int16_t frameIndex = swosReadSignedWord(playerAddr + PLSPR_OFF_FRAME_INDEX);
    if (fitPtr < 0 || fitPtr > 0x60000) return;  // Bail if outside Memory.

    int16_t rawImg = swosReadSignedWord(fitPtr + (frameIndex << 1));
    if (rawImg < 0) return;  // Stay at the existing imageIndex.

    // player.cpp:3535-3547 -- imageIndex = D0 + player.frameOffset.
    int16_t frameOff = swosReadSignedWord(playerAddr + PLSPR_OFF_FRAME_OFFSET);
    swosWriteWord(playerAddr + PLSPR_OFF_IMAGE_INDEX, (uint16_t)(int16_t)(rawImg + frameOff));
}


// ============================================================================
// getPlayerPointerFromShirtNumber -- player.cpp:3245-3251
// ============================================================================
int swosGetPlayerInfoForSprite(int teamBase, int playerAddr) {
    int32_t inGameTeamPtr = swosReadSignedDword(teamBase + TEAMDATA_OFF_IN_GAME_TEAM_PTR);
    if (inGameTeamPtr <= 0) return 0;

    int16_t ordinal = swosReadSignedWord(playerAddr + PLSPR_OFF_PLAYER_ORDINAL);
    if (ordinal < 1 || ordinal > 16) return 0;

    // PlayerInfoSize = 61. inGameTeamPtr already points at players[0].
    return inGameTeamPtr + (ordinal - 1) * TDL_PLAYER_INFO_SIZE;
}


// ============================================================================
// updatePlayerWithBall -- player.cpp:85-130
// ============================================================================
void swosUpdatePlayerWithBall(int playerAddr) {
    // player.cpp:87-95 -- read ball + player coords, then direction.
    int16_t ballX = swosBallSpriteXPixels();
    int16_t ballY = swosBallSpriteYPixels();
    int16_t dir = swosReadSignedWord(playerAddr + PLSPR_OFF_DIRECTION);

    // player.cpp:97-99 -- direction <<= 2 (4 bytes per entry).
    int dirOffset = ((uint16_t)dir & 0xFFFFu) << 2;

    // player.cpp:100-119 -- newX = ballX + table[dir*4]; newY += table+2.
    int16_t tableDx = swosReadSignedWord(ADDR_kPlayerWithBallOffsets + dirOffset);
    int16_t tableDy = swosReadSignedWord(ADDR_kPlayerWithBallOffsets + dirOffset + 2);

    int16_t newX = (int16_t)(ballX + tableDx);
    int16_t newY = (int16_t)(ballY + tableDy);

    // player.cpp:120-128 -- write player.x/y + destX/destY.
    swosWriteWord(playerAddr + PLSPR_OFF_X + 2, (uint16_t)newX);
    swosWriteWord(playerAddr + PLSPR_OFF_Y + 2, (uint16_t)newY);
    swosWriteWord(playerAddr + PLSPR_OFF_DEST_X, (uint16_t)newX);
    swosWriteWord(playerAddr + PLSPR_OFF_DEST_Y, (uint16_t)newY);

    // player.cpp:129 -- resetBothTeamSpinTimers.
    swosResetBothTeamSpinTimers();
}


// ============================================================================
// updateControllingPlayer -- player.cpp:135-191
// ============================================================================
void swosUpdateControllingPlayer(int playerAddr) {
    // player.cpp:137-143 -- direction <<= 2.
    int16_t dir = swosReadSignedWord(playerAddr + PLSPR_OFF_DIRECTION);
    int dirOffset = ((uint16_t)dir & 0xFFFFu) << 2;

    // player.cpp:144-166 -- newX = player.x + kBallPlOffsets[dir*4].
    int16_t playerX = swosReadSignedWord(playerAddr + PLSPR_OFF_X + 2);
    int16_t playerY = swosReadSignedWord(playerAddr + PLSPR_OFF_Y + 2);

    int16_t offsX = swosReadSignedWord(ADDR_kBallPlOffsetsBase + dirOffset);
    int16_t offsY = swosReadSignedWord(ADDR_kBallPlOffsetsBase + dirOffset + 2);

    int16_t newX = (int16_t)(playerX + offsX);
    int16_t newY = (int16_t)(playerY + offsY);

    // player.cpp:167-178 -- ball.speed = 0; ball.x/y/destX/destY = new.
    swosBallSpriteSetSpeed(0);
    swosBallSpriteSetXPixels(newX);
    swosBallSpriteSetYPixels(newY);
    swosBallSpriteSetDestX(newX);
    swosBallSpriteSetDestY(newY);

    // player.cpp:178 -- ball.z.whole = 0 (clamp to ground).
    swosBallSpriteSetZPixels(0);

    // player.cpp:179-189 -- deltaZ sar 1 (no negation, unlike the keeper variant).
    int32_t dz = swosBallSpriteDeltaZ();
    swosBallSpriteSetDeltaZ(swosAsr32(dz, 1));

    // player.cpp:190 -- resetBothTeamSpinTimers.
    swosResetBothTeamSpinTimers();
}


// ============================================================================
// calculateIfPlayerWinsBall -- player.cpp:276-744
// ============================================================================
void swosCalculateIfPlayerWinsBall(int direction, int teamBase, int playerAddr) {
    // player.cpp:279 -- team.passInProgress = 0.
    swosWriteWord(teamBase + TEAMDATA_OFF_PASS_IN_PROGRESS, 0);

    // player.cpp:280-289 -- read opponent's team. if opponent.wonTheBallTimer != 0 -> set_team_direction.
    // player.cpp:283 reads [esi+138] = TeamGeneralInfo.wonTheBallTimer -- NOT
    // AI_timer (+130). See swos_team_data.h's header note on this offset.
    int32_t oppTeamBase = swosReadSignedDword(teamBase + TEAMDATA_OFF_OPPONENTS_TEAM);
    int16_t wonBallTimer = swosReadSignedWord(oppTeamBase + 138 /* wonTheBallTimer */);
    if (wonBallTimer != 0) goto l_set_team_direction;

    {
    // player.cpp:291-297 -- if opponent.playerHasBall == 0 -> set_team_direction.
    int16_t oppPlayerHasBall = swosReadSignedWord(oppTeamBase + TEAMDATA_OFF_PLAYER_HAS_BALL);
    if (oppPlayerHasBall == 0) goto l_set_team_direction;

    // player.cpp:299-305 -- if opponent.currentAllowedDirection < 0 -> set_team_direction.
    int16_t oppAllowedDir = swosReadSignedWord(oppTeamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION);
    if (oppAllowedDir < 0) goto l_set_team_direction;

    // player.cpp:307-323 -- A1 = opponent.controlledPlayer. If null -> no_opponent_controlled_player.
    int32_t oppControlledPlayer = swosReadSignedDword(oppTeamBase + TEAMDATA_OFF_CONTROLLED_PLAYER);
    if (oppControlledPlayer == 0) goto l_no_opponent_controlled_player;

    // player.cpp:325-340 -- D1 = (own's player_at_opp_ordinal tackling + ballControl) / 2.
    // A6 is still the OWN team here (the swap doesn't happen until line 343),
    // so this resolves OWN team's roster slot at the opp's controlled-player
    // ordinal. Strange but exact -- see the C# source's comment for the full
    // asm-register derivation.
    int32_t a4D1 = swosGetPlayerInfoForSprite(teamBase, oppControlledPlayer);
    int d1Tack = a4D1 != 0 ? swosReadByte(a4D1 + TDL_OFF_TACKLING) : 0;
    int d1Bc   = a4D1 != 0 ? swosReadByte(a4D1 + TDL_OFF_BALL_CONTROL) : 0;
    // 8-bit signed add then arithmetic shr 1 -- matches asm `add bl,al`/`shr bl,1`.
    int8_t d1Sum = (int8_t)((int8_t)d1Tack + (int8_t)d1Bc);
    int8_t d1OwnAvg = (int8_t)(((uint8_t)d1Sum) >> 1);

    // player.cpp:342-357 -- A6 swap to opponent for the comparison; A1 = opp.controlledPlayer.
    if (oppControlledPlayer == 0) goto l_no_opponent_controlled_player;

    // player.cpp:359-374 -- D0 = (opp's controlled player tackling + ballControl) / 2.
    // After the A6 swap, A6=opp team; A1 is still opp.controlledPlayer -- so
    // this is the meaningful read: the dribbler's own tackling+ballControl.
    int32_t a4D0 = swosGetPlayerInfoForSprite(oppTeamBase, oppControlledPlayer);
    int d0Tack = a4D0 != 0 ? swosReadByte(a4D0 + TDL_OFF_TACKLING) : 0;
    int d0Bc   = a4D0 != 0 ? swosReadByte(a4D0 + TDL_OFF_BALL_CONTROL) : 0;
    int8_t d0Sum = (int8_t)((int8_t)d0Tack + (int8_t)d0Bc);
    int8_t d0OppAvg = (int8_t)(((uint8_t)d0Sum) >> 1);

    // player.cpp:376-396 / swos.asm:108109-108151 -- pre-roll: A6 = the
    // LOWER-average team (tie -> opponent). The stronger player is
    // protected: P(weaker side loses) = (16+|diff|)/32 = 50..72%.
    int d1Diff = d1OwnAvg - d0OppAvg;
    int32_t a6LoserTeam;
    if (d1Diff < 0) {
        // own side has the LOWER avg -- abs the diff, own is the pre-roll loser.
        d1Diff = -d1Diff;
        a6LoserTeam = teamBase;
    } else {
        // opponent side has the lower (or equal) avg -- pre-roll loser.
        a6LoserTeam = oppTeamBase;
    }

    // cseg_7A26D (player.cpp:398-410).
    int rnd = swosRngNextByte() & 31;
    int tableIdx = d1Diff & 0xFFFF;
    uint8_t chance = swosReadByte(ADDR_kPlAvgTacklingBallControlDiffChance + tableIdx);
    if ((uint8_t)rnd < chance) {
        // l_init_ball_winner_team. The pre-roll loser stays the loser.
    } else {
        // player.cpp:423-425 -- upset: swap A6 to the opponent side.
        a6LoserTeam = swosReadSignedDword(a6LoserTeam + TEAMDATA_OFF_OPPONENTS_TEAM);
    }

    // PlayerControlled.cs telemetry, now available since step 6A.
    if (a6LoserTeam != teamBase)
        swosPlayerControlledIncSkillDuelOwnWin();
    else
        swosPlayerControlledIncSkillDuelOppWin();

    // l_init_ball_winner_team (player.cpp:427-440).
    swosWriteWord(a6LoserTeam + 138 /* wonTheBallTimer */, 12);
    swosBallSpriteSetSpeed(0);
    swosBallSpriteSetDestX(swosBallSpriteXPixels());
    swosBallSpriteSetDestY(swosBallSpriteYPixels());
    return;
    }

l_no_opponent_controlled_player:;
    // player.cpp:442-447 -- pop & fall to set_team_direction.

l_set_team_direction:;
    // player.cpp:448-451 -- team.controlledPlDirection = D0.
    swosWriteWord(teamBase + TEAMDATA_OFF_CONTROLLED_PL_DIRECTION, (uint16_t)direction);

    // player.cpp:452-462 -- if ball.deltaZ > 0 -> clamp to -1.
    int32_t ballDz = swosBallSpriteDeltaZ();
    if (ballDz > 0) {
        swosBallSpriteSetDeltaZ(-1);
    }

    // player.cpp:464-475 -- if D0 != 0 (direction has horizontal component) -> set_player_destination.
    if (direction != 0) goto l_set_player_destination;

    {
    // player.cpp:477-498 -- nudge ball X by 1 px when player straight-up.
    int16_t bxNudge = swosBallSpriteXPixels();
    int16_t pxNudge = swosReadSignedWord(playerAddr + PLSPR_OFF_X + 2);
    int16_t d2NudgeDiff = (int16_t)(bxNudge - pxNudge);
    if (d2NudgeDiff >= 4) goto l_set_player_destination;
    if (d2NudgeDiff <= -4) goto l_set_player_destination;
    if (d2NudgeDiff < 0) {
        // l_nudge_ball_left (player.cpp:520-533).
        swosBallSpriteSetXPixels((int16_t)(bxNudge - 1));
        goto l_set_player_destination;
    }
    // l_nudge_ball_right (player.cpp:535-544).
    swosBallSpriteSetXPixels((int16_t)(bxNudge + 1));
    }

l_set_player_destination:;
    {
    // player.cpp:547-584 -- destX/Y = player.x/y + kDefaultDestinations[dir*4].
    int destIdx = ((uint16_t)direction & 0xFFFFu) << 2;
    int16_t plDestX = swosReadSignedWord(playerAddr + PLSPR_OFF_X + 2);
    int16_t plDestY = swosReadSignedWord(playerAddr + PLSPR_OFF_Y + 2);
    int16_t defDx = swosReadSignedWord(ADDR_kDefaultDestinations + destIdx);
    int16_t defDy = swosReadSignedWord(ADDR_kDefaultDestinations + destIdx + 2);
    swosBallSpriteSetDestX((int16_t)(plDestX + defDx));
    swosBallSpriteSetDestY((int16_t)(plDestY + defDy));

    // player.cpp:585-587 -- A4 = PlayerGameHeader pointer.
    int32_t a4PlayerInfo = swosGetPlayerInfoForSprite(teamBase, playerAddr);

    // player.cpp:588-597 -- test currentTick bit 1; if zero -> update_ball_speed (d1=0).
    int16_t d1Inc = 0;
    uint8_t currentTickByte = swosReadByte(ADDR_currentGameTick);
    if ((currentTickByte & 2) != 0) {
        // player.cpp:599-611 -- read kBallSpeedDeltaWhenControlled[ballControl*2].
        int ballControlSkill = 4;
        if (a4PlayerInfo != 0) {
            int bc = swosReadByte(a4PlayerInfo + TDL_OFF_BALL_CONTROL);
            if (bc >= 0 && bc <= 7) ballControlSkill = bc;
        }
        d1Inc = swosReadSignedWord(ADDR_kBallSpeedDeltaWhenControlled + ballControlSkill * 2);
    }

    // l_update_ball_speed (player.cpp:613-624).
    int16_t playerSpeed = swosReadSignedWord(playerAddr + PLSPR_OFF_SPEED);
    swosBallSpriteSetSpeed((int16_t)(d1Inc + playerSpeed));

    // player.cpp:625-685 -- direction-diff between player's full-angle pose
    // (fullDirection + 128, low byte) and the move direction (direction*32, low byte).
    int16_t playerFullDir = swosReadSignedWord(playerAddr + PLSPR_OFF_FULL_DIRECTION);
    uint8_t d0Byte = (uint8_t)((playerFullDir & 0xFF) + 128);
    int16_t playerDir = swosReadSignedWord(playerAddr + PLSPR_OFF_DIRECTION);
    uint8_t d1Byte = (uint8_t)(((uint16_t)playerDir << 5) & 0xFF);
    int8_t signedDiff = (int8_t)(d0Byte - d1Byte);
    if (signedDiff >= 64 || signedDiff <= -64) {
        // cseg_7A517 -- speed += 256.
        swosBallSpriteSetSpeed((int16_t)(swosBallSpriteSpeed() + 256));
    }

    // cseg_7A523 (player.cpp:686-739).
    int16_t controlledDir = swosReadSignedWord(teamBase + TEAMDATA_OFF_CONTROLLED_PL_DIRECTION);
    int16_t pDir = swosReadSignedWord(playerAddr + PLSPR_OFF_DIRECTION);
    if (controlledDir == pDir) goto l_reset_ball_spin;

    // player.cpp:704-741 -- unkBallTimer++. Compare dseg_17E276[ballControl*2] with timer.
    int16_t unkBallTimer = swosReadSignedWord(teamBase + TEAMDATA_OFF_OFS108);
    unkBallTimer++;
    swosWriteWord(teamBase + TEAMDATA_OFF_OFS108, (uint16_t)unkBallTimer);

    int dsegBallControlSkill = 4;
    if (a4PlayerInfo != 0) {
        int bc2 = swosReadByte(a4PlayerInfo + TDL_OFF_BALL_CONTROL);
        if (bc2 >= 0 && bc2 <= 7) dsegBallControlSkill = bc2;
    }
    int16_t entry = swosReadSignedWord(ADDR_dseg_17E276 + dsegBallControlSkill * 2);
    if (entry > unkBallTimer) {
        // Counter hasn't crossed -- wonTheBallTimer not updated.
    } else {
        // Counter crossed -> wonTheBallTimer = 8. player.cpp:740 writes
        // [esi+138], not AI_timer (+130).
        swosWriteWord(teamBase + 138 /* wonTheBallTimer */, 8);
    }
    }

l_reset_ball_spin:;
    // player.cpp:743 -- resetBothTeamSpinTimers.
    swosResetBothTeamSpinTimers();
}


// ============================================================================
// playerKickingBall -- player.cpp:750-1136
// ============================================================================
void swosPlayerKickingBall(int teamBase, int playerAddr) {
    // player.cpp:752 -- stateGoal = 0.
    swosWriteWord(ADDR_stateGoal, 0);

    // player.cpp:756-761 -- d0 = d2 = player.direction; team.controlledPlDirection = direction.
    int16_t dir = swosReadSignedWord(playerAddr + PLSPR_OFF_DIRECTION);
    swosWriteWord(teamBase + TEAMDATA_OFF_CONTROLLED_PL_DIRECTION, (uint16_t)dir);

    // player.cpp:762 -- A0 = getBallDestCoordinatesTable() (per-state table).
    int a0DestTable = swosGetBallDestCoordinatesTable();

    // player.cpp:763-799 -- ball.destX/Y = ball.x/y + table[dir*4].
    int dirOff = ((uint16_t)dir & 0xFFFFu) << 2;
    int16_t tableDx = swosReadSignedWord(a0DestTable + dirOff);
    int16_t tableDy = swosReadSignedWord(a0DestTable + dirOff + 2);
    swosBallSpriteSetDestX((int16_t)(swosBallSpriteXPixels() + tableDx));
    swosBallSpriteSetDestY((int16_t)(swosBallSpriteYPixels() + tableDy));

    // player.cpp:800-805 -- ball.speed = kBallKickingSpeed; ball.deltaZ = kBallKickingDeltaZ; reset spin.
    int16_t kickSpeed = swosReadSignedWord(ADDR_kBallKickingSpeed);
    swosBallSpriteSetSpeed(kickSpeed);
    int32_t kickDz = swosReadSignedDword(ADDR_kBallKickingDeltaZ);
    swosBallSpriteSetDeltaZ(kickDz);
    swosResetBothTeamSpinTimers();

    // player.cpp:806-817 -- if gameStatePl == ST_GAME_IN_PROGRESS (100) -> game_in_progress.
    int16_t gameStatePl = swosReadSignedWord(ADDR_gameStatePl);
    if (gameStatePl == 100) goto l_game_in_progress;

    {
    // player.cpp:819-830 -- if gameState < ST_THROW_IN_FORWARD_RIGHT (15) -> game_in_progress.
    int16_t gameState = swosReadSignedWord(ADDR_gameState);
    if (gameState < 15) goto l_game_in_progress;

    // player.cpp:832-843 -- if gameState <= ST_THROW_IN_BACK_LEFT (20) -> return.
    if (gameState <= 20) return;
    }

l_game_in_progress:;
    {
    // player.cpp:846-856 -- if A6 == topTeam -> left_team.
    bool isTopTeam = (teamBase == TEAMDATA_TOP_BASE);
    if (isTopTeam) goto l_left_team;

    // player.cpp:858-870 -- BOTTOM team. ball.y > 342 -> not a shot.
    if (swosBallSpriteYPixels() > 342) goto l_not_a_shot_on_goal;

    // player.cpp:872-907 -- controlledPlDirection in {0, 1, 7} -> possible shot.
    int16_t ctrlDir = swosReadSignedWord(teamBase + TEAMDATA_OFF_CONTROLLED_PL_DIRECTION);
    if (ctrlDir == 0) goto l_possible_shot_on_goal;
    if (ctrlDir == 1) goto l_possible_shot_on_goal;
    if (ctrlDir == 7) goto l_possible_shot_on_goal;
    goto l_not_a_shot_on_goal;
    }

l_left_team:;
    {
    // player.cpp:910-922 -- TOP team. ball.y < 556 -> not a shot.
    if (swosBallSpriteYPixels() < 556) goto l_not_a_shot_on_goal;

    // player.cpp:924-962 -- controlledPlDirection in {3, 4, 5} -> possible shot.
    int16_t ctrlDirTop = swosReadSignedWord(teamBase + TEAMDATA_OFF_CONTROLLED_PL_DIRECTION);
    if (ctrlDirTop == 4) goto l_possible_shot_on_goal;
    if (ctrlDirTop == 3) goto l_possible_shot_on_goal;
    if (ctrlDirTop != 5) goto l_not_a_shot_on_goal;
    }

l_possible_shot_on_goal:;
    // player.cpp:964-977 -- if ball.x < 241 -> long shot.
    if (swosBallSpriteXPixels() < 241) goto l_its_a_long_shot;
    // player.cpp:979-990 -- if ball.x > 431 -> long shot.
    if (swosBallSpriteXPixels() > 431) goto l_its_a_long_shot;
    // player.cpp:992-1003 -- if ball.y < 204 -> finishing shot.
    if (swosBallSpriteYPixels() < 204) goto l_its_a_finishing_shot;
    // player.cpp:1005-1016 -- if ball.y < 694 -> long shot.
    if (swosBallSpriteYPixels() < 694) goto l_its_a_long_shot;
    // Else -> finishing.

l_its_a_finishing_shot:;
    {
    // Telemetry omitted (RecordShot -- zero Memory effect, see header).
    // player.cpp:1019-1047 -- speed += kBallSpeedFinishing[finishing*2].
    // Asm at player.cpp:1023 reads [esi+75] = PlayerGameHeader.finishing
    // (= PlayerInfo +33, TDL_OFF_FINISHING).
    int finishingSkill = 4;
    int32_t playerInfoFin = swosGetPlayerInfoForSprite(teamBase, playerAddr);
    if (playerInfoFin != 0) {
        int f = swosReadByte(playerInfoFin + TDL_OFF_FINISHING);
        if (f >= 0 && f <= 7) finishingSkill = f;
    }
    // OpenSWOS fatigue: exhausted (<10%) shooter loses 1 finishing point too.
    finishingSkill -= swosPlayerEnergyShotPenalty(playerAddr);
    if (finishingSkill < 0) finishingSkill = 0;
    int16_t finishBoost = swosReadSignedWord(ADDR_kBallSpeedFinishing + finishingSkill * 2);
    swosBallSpriteSetSpeed((int16_t)(swosBallSpriteSpeed() + finishBoost));
    goto l_not_a_shot_on_goal;
    }

l_its_a_long_shot:;
    {
    // Telemetry omitted (RecordShot -- zero Memory effect, see header).
    // player.cpp:1049-1073 -- speed += kBallSpeedKicking[shooting*2].
    // Asm at player.cpp:1054 reads [esi+70] = PlayerGameHeader.shooting
    // (= PlayerInfo +28, TDL_OFF_SHOOTING).
    int shootingSkill = 4;
    int32_t playerInfoShoot = swosGetPlayerInfoForSprite(teamBase, playerAddr);
    if (playerInfoShoot != 0) {
        int sh = swosReadByte(playerInfoShoot + TDL_OFF_SHOOTING);
        if (sh >= 0 && sh <= 7) shootingSkill = sh;
    }
    shootingSkill -= swosPlayerEnergyShotPenalty(playerAddr);
    if (shootingSkill < 0) shootingSkill = 0;
    int16_t shootBoost = swosReadSignedWord(ADDR_kBallSpeedKicking + shootingSkill * 2);
    swosBallSpriteSetSpeed((int16_t)(swosBallSpriteSpeed() + shootBoost));
    }

l_not_a_shot_on_goal:;
    {
    // player.cpp:1076-1114 -- if player.playerOrdinal == 1 (keeper) AND
    // direction in {2, 6} (left/right) -> skip spin clear.
    int16_t orderTest = swosReadSignedWord(playerAddr + PLSPR_OFF_PLAYER_ORDINAL);
    if (orderTest != 1) goto cseg_7AE0E;

    int16_t dirTest = swosReadSignedWord(playerAddr + PLSPR_OFF_DIRECTION);
    if (dirTest == 2) goto l_play_kick_sample_and_leave;
    if (dirTest == 6) goto l_play_kick_sample_and_leave;
    }

cseg_7AE0E:;
    {
    // player.cpp:1116-1130 -- if opp.goalkeeperSavedCommentTimer < 0 -> leave.
    int32_t oppTeam = swosReadSignedDword(teamBase + TEAMDATA_OFF_OPPONENTS_TEAM);
    int16_t oppKeepTimer = swosReadSignedWord(oppTeam + TEAMDATA_OFF_GOALKEEPER_SAVED_COMMENT_TIMER);
    if (oppKeepTimer < 0) goto l_play_kick_sample_and_leave;

    // player.cpp:1129-1130 -- team.spinTimer = 0.
    swosWriteWord(teamBase + TEAMDATA_OFF_SPIN_TIMER, 0);
    }

l_play_kick_sample_and_leave:;
    // player.cpp:1133-1135 -- team.passInProgress = 0; PlayKickSample (omitted, audio).
    swosWriteWord(teamBase + TEAMDATA_OFF_PASS_IN_PROGRESS, 0);
}


// ============================================================================
// playerHittingStaticHeader -- player.cpp:1146-1388
// ============================================================================
void swosPlayerHittingStaticHeader(int teamBase, int playerAddr) {
    // player.cpp:1148 -- passInProgress = 0.
    swosWriteWord(teamBase + TEAMDATA_OFF_PASS_IN_PROGRESS, 0);

    // player.cpp:1150-1161 -- if player.animationTable already == staticHeaderHitAnimTable
    // -> skip direction-shift block.
    int32_t animTablePtr = swosReadSignedDword(playerAddr + PLSPR_OFF_ANIM_TABLE_PTR);
    if (animTablePtr == ADDR_kStaticHeaderHitAnimTableAddr) goto l_set_static_header_anim_table;

    {
    // player.cpp:1163-1178 -- D0 = team.currentAllowedDirection - player.direction.
    int16_t curAllowedDir = swosReadSignedWord(teamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION);
    int16_t pDir = swosReadSignedWord(playerAddr + PLSPR_OFF_DIRECTION);
    int16_t d0DirDiff = (int16_t)(curAllowedDir - pDir);
    if (d0DirDiff == 0) goto l_set_static_header_anim_table;

    // player.cpp:1180-1193 -- D0 &= 7; if == 4 -> set anim table.
    d0DirDiff = (int16_t)(d0DirDiff & 7);
    if (d0DirDiff == 4) goto l_set_static_header_anim_table;

    // player.cpp:1195-1196 -- if diff < 4 -> turn right; else -> turn left (no carry).
    if (d0DirDiff < 4) goto l_turn_player_right;

    // Turn left: pDir = (pDir - 1) & 7; if still wrong, pDir -= 1.
    pDir = (int16_t)((pDir - 1) & 7);
    swosWriteWord(playerAddr + PLSPR_OFF_DIRECTION, (uint16_t)pDir);
    if (pDir == d0DirDiff) goto l_fix_sprite_direction;
    swosWriteWord(playerAddr + PLSPR_OFF_DIRECTION, (uint16_t)(int16_t)(pDir - 1));
    goto l_fix_sprite_direction;

l_turn_player_right:;
    // player.cpp:1241-1280 -- pDir = (pDir + 1) & 7; if still wrong, pDir += 1.
    pDir = swosReadSignedWord(playerAddr + PLSPR_OFF_DIRECTION);
    pDir = (int16_t)((pDir + 1) & 7);
    swosWriteWord(playerAddr + PLSPR_OFF_DIRECTION, (uint16_t)pDir);
    if (pDir == d0DirDiff) goto l_fix_sprite_direction;
    swosWriteWord(playerAddr + PLSPR_OFF_DIRECTION, (uint16_t)(int16_t)(pDir + 1));

l_fix_sprite_direction:;
    // player.cpp:1282-1291 -- direction &= 7 (sanity wrap).
    {
    int16_t fixedDir = (int16_t)(swosReadSignedWord(playerAddr + PLSPR_OFF_DIRECTION) & 7);
    swosWriteWord(playerAddr + PLSPR_OFF_DIRECTION, (uint16_t)fixedDir);
    }
    }

l_set_static_header_anim_table:;
    // player.cpp:1294 -- A0 = staticHeaderHitAnimTable.
    swosSetPlayerAnimationTableAndPictureIndex(ADDR_kStaticHeaderHitAnimTableAddr, playerAddr);

    {
    // player.cpp:1296-1304 -- D1 = team.currentAllowedDirection or player.direction.
    int16_t ad = swosReadSignedWord(teamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION);
    int16_t d1Dir = (ad < 0) ? swosReadSignedWord(playerAddr + PLSPR_OFF_DIRECTION) : ad;

    // player.cpp:1310-1311 -- team.controlledPlDirection = D1.
    swosWriteWord(teamBase + TEAMDATA_OFF_CONTROLLED_PL_DIRECTION, (uint16_t)d1Dir);

    // player.cpp:1312-1346 -- destX/Y = ball.x/y + kDefaultDestinations[D1*4].
    int destIdx = ((uint16_t)d1Dir & 0xFFFFu) << 2;
    int16_t defDx = swosReadSignedWord(ADDR_kDefaultDestinations + destIdx);
    int16_t defDy = swosReadSignedWord(ADDR_kDefaultDestinations + destIdx + 2);
    swosBallSpriteSetDestX((int16_t)(swosBallSpriteXPixels() + defDx));
    swosBallSpriteSetDestY((int16_t)(swosBallSpriteYPixels() + defDy));

    // player.cpp:1347-1348 -- ball.speed = kStaticHeaderBallSpeed.
    int16_t headSpeed = swosReadSignedWord(ADDR_kStaticHeaderBallSpeed);
    swosBallSpriteSetSpeed(headSpeed);

    // player.cpp:1349-1371 -- speed += kPlayerHeaderSpeedIncrease[heading*2].
    // Real lookup: PlayerGameHeader.heading at +71 === PlayerInfo+29 (TDL_OFF_HEADING).
    int headingSkill = 4;
    {
        int32_t piAddr = swosGetPlayerInfoForSprite(teamBase, playerAddr);
        if (piAddr != 0) {
            int hs = swosReadByte(piAddr + TDL_OFF_HEADING);
            if (hs >= 0 && hs <= 7) headingSkill = hs;
        }
    }
    int16_t headBoost = swosReadSignedWord(ADDR_kPlayerHeaderSpeedIncrease + headingSkill * 2);
    swosBallSpriteSetSpeed((int16_t)(swosBallSpriteSpeed() + headBoost));

    // player.cpp:1372-1383 -- deltaZ = -(deltaZ) >> 1 (i.e. -|dz|/2 -- invert AND halve).
    int32_t curDz = swosBallSpriteDeltaZ();
    curDz = -curDz;
    curDz = swosAsr32(curDz, 1);
    swosBallSpriteSetDeltaZ(curDz);

    // player.cpp:1385-1387 -- player.heading = 1; PlayKickSample (omitted,
    // audio); resetBothTeamSpinTimers.
    swosWriteWord(playerAddr + 98 /* heading */, 1);
    swosResetBothTeamSpinTimers();
    }
}


// ============================================================================
// playerHittingJumpHeader -- player.cpp:1396-1671
// ============================================================================
void swosPlayerHittingJumpHeader(int teamBase, int playerAddr) {
    // player.cpp:1399 -- passInProgress = 0.
    swosWriteWord(teamBase + TEAMDATA_OFF_PASS_IN_PROGRESS, 0);

    // player.cpp:1400-1410 -- D1 = team.currentAllowedDirection or player.direction.
    int16_t ad = swosReadSignedWord(teamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION);
    int16_t d1Dir = (ad >= 0) ? ad : swosReadSignedWord(playerAddr + PLSPR_OFF_DIRECTION);

    // l_set_player_z_and_ball_speed:
    // player.cpp:1413-1421 -- ball.deltaZ = kBallJumpHeaderDeltaZ; speed = player.speed.
    int32_t jumpDz = swosReadSignedDword(ADDR_kBallJumpHeaderDeltaZ);
    swosBallSpriteSetDeltaZ(jumpDz);
    int16_t pSpeed = swosReadSignedWord(playerAddr + PLSPR_OFF_SPEED);
    swosBallSpriteSetSpeed(pSpeed);

    // player.cpp:1422-1437 -- ball.speed += player.speed >> 2.
    int16_t bumpSpeed = (int16_t)swosAsr32((int32_t)pSpeed, 2);
    swosBallSpriteSetSpeed((int16_t)(swosBallSpriteSpeed() + bumpSpeed));

    // player.cpp:1438-1447 -- D0 = player.direction - D1.
    int16_t pDir = swosReadSignedWord(playerAddr + PLSPR_OFF_DIRECTION);
    int16_t d0DirDiff = (int16_t)(pDir - d1Dir);

    // player.cpp:1448-1454 -- if team.currentAllowedDirection < 0 -> static header path.
    ad = swosReadSignedWord(teamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION);
    if (ad < 0) goto l_do_static_header;

    // player.cpp:1456-1465 -- D0 &= 7; if == 0 -> use_allowed_direction.
    d0DirDiff = (int16_t)(d0DirDiff & 7);
    if (d0DirDiff == 0) goto l_use_allowed_direction;

    // player.cpp:1466-1475 -- if D0 == 4 -> lob_header.
    if (d0DirDiff == 4) goto l_lob_header;

    // player.cpp:1477-1486 -- if D0 == 1 -> aim_left.
    if (d0DirDiff == 1) goto l_aim_left;

    // player.cpp:1488-1497 -- if D0 == 7 -> aim_right.
    if (d0DirDiff == 7) goto l_aim_right;

    // player.cpp:1499-1508 -- if D0 == 2 -> left_held.
    if (d0DirDiff == 2) goto l_left_held;

    // player.cpp:1510-1519 -- if D0 == 6 -> right_held.
    if (d0DirDiff == 6) goto l_right_held;

    // player.cpp:1521-1530 -- if D0 == 3 -> down_left_held.
    if (d0DirDiff == 3) goto l_down_left_held;

    // Else (D0 == 5) -- doLobHeader + aim_right.
    swosDoLobHeader(playerAddr);
    goto l_aim_right;

l_down_left_held:;
    swosDoLobHeader(playerAddr);
    goto l_aim_left;

l_right_held:;
    swosDoFlyingHeader(playerAddr);
    goto l_aim_right;

l_left_held:;
    swosDoFlyingHeader(playerAddr);
    goto l_aim_left;

l_do_static_header:;
    swosDoFlyingHeader(playerAddr);
    goto l_use_allowed_direction;

l_lob_header:;
    swosDoLobHeader(playerAddr);
    goto l_use_allowed_direction;

l_aim_right:;
    // player.cpp:1555-1568 -- D0 = player.direction + 1.
    d0DirDiff = (int16_t)(swosReadSignedWord(playerAddr + PLSPR_OFF_DIRECTION) + 1);
    goto l_update_player_direction;

l_aim_left:;
    // player.cpp:1570-1583 -- D0 = player.direction - 1.
    d0DirDiff = (int16_t)(swosReadSignedWord(playerAddr + PLSPR_OFF_DIRECTION) - 1);
    goto l_update_player_direction;

l_use_allowed_direction:;
    // player.cpp:1585-1588 -- D0 = player.direction (uses player.direction not team's).
    d0DirDiff = swosReadSignedWord(playerAddr + PLSPR_OFF_DIRECTION);

l_update_player_direction:;
    // player.cpp:1590-1597 -- direction &= 7; team.controlledPlDirection = direction.
    d0DirDiff = (int16_t)(d0DirDiff & 7);
    swosWriteWord(teamBase + TEAMDATA_OFF_CONTROLLED_PL_DIRECTION, (uint16_t)d0DirDiff);

    {
    // player.cpp:1598-1632 -- ball.destX/Y = ball.x/y + kDefaultDestinations[D0*4].
    int destIdx = ((uint16_t)d0DirDiff & 0xFFFFu) << 2;
    int16_t defDx = swosReadSignedWord(ADDR_kDefaultDestinations + destIdx);
    int16_t defDy = swosReadSignedWord(ADDR_kDefaultDestinations + destIdx + 2);
    swosBallSpriteSetDestX((int16_t)(swosBallSpriteXPixels() + defDx));
    swosBallSpriteSetDestY((int16_t)(swosBallSpriteYPixels() + defDy));

    // player.cpp:1633-1656 -- speed += kPlayerHeaderSpeedIncrease[heading*2].
    int headingSkill = 4;
    {
        int32_t piAddr = swosGetPlayerInfoForSprite(teamBase, playerAddr);
        if (piAddr != 0) {
            int hs = swosReadByte(piAddr + TDL_OFF_HEADING);
            if (hs >= 0 && hs <= 7) headingSkill = hs;
        }
    }
    int16_t headBoost = swosReadSignedWord(ADDR_kPlayerHeaderSpeedIncrease + headingSkill * 2);
    swosBallSpriteSetSpeed((int16_t)(swosBallSpriteSpeed() + headBoost));

    // player.cpp:1657-1666 -- player.speed >>= 1; player.heading = 1.
    int16_t ps = swosReadSignedWord(playerAddr + PLSPR_OFF_SPEED);
    swosWriteWord(playerAddr + PLSPR_OFF_SPEED, (uint16_t)((uint16_t)ps >> 1));
    swosWriteWord(playerAddr + 98 /* heading */, 1);

    // player.cpp:1668-1670 -- PlayKickSample; playHeaderComment (both omitted,
    // audio); ResetSpinTimers.
    swosResetBothTeamSpinTimers();
    }
}


// ============================================================================
// playerTackledTheBallStrong -- player.cpp:1684-1966
// ============================================================================
void swosPlayerTackledTheBallStrong(int teamBase, int playerAddr) {
    // player.cpp:1687-1697 -- D1 = team.currentAllowedDirection or player.direction.
    int16_t ad = swosReadSignedWord(teamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION);
    int16_t d1Dir = (ad < 0) ? swosReadSignedWord(playerAddr + PLSPR_OFF_DIRECTION) : ad;

    // l_current_direction_allowed:
    // player.cpp:1699-1715 -- D0 = player.direction - D1.
    int16_t pDir = swosReadSignedWord(playerAddr + PLSPR_OFF_DIRECTION);
    int16_t d0DirDiff = (int16_t)(pDir - d1Dir);
    if (d0DirDiff == 0) goto l_set_controlled_player_direction;

    // player.cpp:1717-1733 -- D0 &= 7; if == 4 -> same/opposite; else branch.
    d0DirDiff = (int16_t)(d0DirDiff & 7);
    if (d0DirDiff == 4) goto l_set_controlled_player_direction;
    if (d0DirDiff < 4) goto l_controls_leaning_leftward;

    // Right lean: D0 = player.direction + 1.
    d0DirDiff = (int16_t)(swosReadSignedWord(playerAddr + PLSPR_OFF_DIRECTION) + 1);
    goto l_set_new_direction;

l_controls_leaning_leftward:;
    // player.cpp:1748-1761 -- D0 = player.direction - 1.
    d0DirDiff = (int16_t)(swosReadSignedWord(playerAddr + PLSPR_OFF_DIRECTION) - 1);
    goto l_set_new_direction;

l_set_controlled_player_direction:;
    // player.cpp:1763-1767 -- D0 = player.direction.
    d0DirDiff = swosReadSignedWord(playerAddr + PLSPR_OFF_DIRECTION);

l_set_new_direction:;
    // player.cpp:1768-1775 -- D0 &= 7; team.controlledPlDirection = D0.
    d0DirDiff = (int16_t)(d0DirDiff & 7);
    swosWriteWord(teamBase + TEAMDATA_OFF_CONTROLLED_PL_DIRECTION, (uint16_t)d0DirDiff);

    {
    // player.cpp:1776-1807 -- destX/Y = ball.x/y + kDefaultDestinations[D0*4].
    int destIdx = ((uint16_t)d0DirDiff & 0xFFFFu) << 2;
    int16_t defDx = swosReadSignedWord(ADDR_kDefaultDestinations + destIdx);
    int16_t defDy = swosReadSignedWord(ADDR_kDefaultDestinations + destIdx + 2);
    swosBallSpriteSetDestX((int16_t)(swosBallSpriteXPixels() + defDx));
    swosBallSpriteSetDestY((int16_t)(swosBallSpriteYPixels() + defDy));
    }

    // player.cpp:1808-1814 -- if team.playerNumber == 0 (CPU) -> player_not_cpu (yes, inverted).
    {
    int16_t pNum = swosReadSignedWord(teamBase + TEAMDATA_OFF_PLAYER_NUMBER);
    if (pNum != 0) goto l_player_not_cpu;
    }

    // CPU path (player.cpp:1816-1824) -- ball.speed = player.speed; halve player.speed below.
    swosBallSpriteSetSpeed(swosReadSignedWord(playerAddr + PLSPR_OFF_SPEED));
    goto l_halve_player_speed;

l_player_not_cpu:;
    {
    // player.cpp:1826-1860 -- Human player. ball.speed = player.speed + (player.speed >> 2)
    // applied TWICE.
    int16_t pSp = swosReadSignedWord(playerAddr + PLSPR_OFF_SPEED);
    int16_t d0Shr2 = (int16_t)((uint16_t)pSp >> 2);
    pSp = swosReadSignedWord(playerAddr + PLSPR_OFF_SPEED);
    d0Shr2 = (int16_t)(d0Shr2 + pSp);
    swosBallSpriteSetSpeed(d0Shr2);
    // Second pass.
    pSp = swosReadSignedWord(playerAddr + PLSPR_OFF_SPEED);
    d0Shr2 = (int16_t)((uint16_t)pSp >> 2);
    pSp = swosReadSignedWord(playerAddr + PLSPR_OFF_SPEED);
    d0Shr2 = (int16_t)(d0Shr2 + pSp);
    swosBallSpriteSetSpeed(d0Shr2);
    }

l_halve_player_speed:;
    {
    // player.cpp:1862-1870 -- player.speed >>= 1; tackleState = 1 (TS_TACKLING_THE_BALL).
    int16_t pSpFinal = swosReadSignedWord(playerAddr + PLSPR_OFF_SPEED);
    swosWriteWord(playerAddr + PLSPR_OFF_SPEED, (uint16_t)((uint16_t)pSpFinal >> 1));
    swosWriteWord(playerAddr + PLSPR_OFF_TACKLE_STATE, 1);

    // player.cpp:1871-1899 -- if opp.controlledPlayer is null OR ballDistance < 9 -> out.
    int32_t oppTeam = swosReadSignedDword(teamBase + TEAMDATA_OFF_OPPONENTS_TEAM);
    int32_t oppCtrl = swosReadSignedDword(oppTeam + TEAMDATA_OFF_CONTROLLED_PLAYER);
    if (oppCtrl == 0) goto l_out_strong;
    int32_t oppBallDist = swosReadSignedDword(oppCtrl + PLSPR_OFF_BALL_DISTANCE);
    if (oppBallDist < 9) goto l_out_strong;

    // player.cpp:1901-1957 -- distance^2 between players. If > 32 -> TS_GOOD_TACKLE.
    int16_t px = swosReadSignedWord(playerAddr + PLSPR_OFF_X + 2);
    int16_t ox = swosReadSignedWord(oppCtrl + PLSPR_OFF_X + 2);
    int16_t dx = (int16_t)(px - ox);
    int32_t dxSq = (int32_t)dx * (int32_t)dx;
    int16_t py = swosReadSignedWord(playerAddr + PLSPR_OFF_Y + 2);
    int16_t oy = swosReadSignedWord(oppCtrl + PLSPR_OFF_Y + 2);
    int16_t dy = (int16_t)(py - oy);
    int32_t dySq = (int32_t)dy * (int32_t)dy;
    int32_t distSq = dxSq + dySq;
    if (distSq <= 32) goto l_out_strong;

    // PlayGoodTackleComment() omitted (audio).
    swosWriteWord(playerAddr + PLSPR_OFF_TACKLE_STATE, 2);  // TS_GOOD_TACKLE
    }

l_out_strong:;
    // PlayKickSample() omitted (audio).
    swosResetBothTeamSpinTimers();
}


// ============================================================================
// playerTackledTheBallWeak -- player.cpp:1974-2231
// ============================================================================
void swosPlayerTackledTheBallWeak(int teamBase, int playerAddr) {
    // player.cpp:1976 -- passInProgress = 0.
    swosWriteWord(teamBase + TEAMDATA_OFF_PASS_IN_PROGRESS, 0);

    // player.cpp:1977-1988 -- D1 = team.currentAllowedDirection or player.direction.
    int16_t ad = swosReadSignedWord(teamBase + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION);
    int16_t d1Dir = (ad < 0) ? swosReadSignedWord(playerAddr + PLSPR_OFF_DIRECTION) : ad;

    // l_controls_something:
    // player.cpp:1990-2006 -- D0 = player.direction - D1.
    int16_t pDir = swosReadSignedWord(playerAddr + PLSPR_OFF_DIRECTION);
    int16_t d0DirDiff = (int16_t)(pDir - d1Dir);
    if (d0DirDiff == 0) goto l_tackling_in_same_or_oposite_direction;

    // player.cpp:2008-2024 -- D0 &= 7; if == 4 -> same/opposite; else lean.
    d0DirDiff = (int16_t)(d0DirDiff & 7);
    if (d0DirDiff == 4) goto l_tackling_in_same_or_oposite_direction;
    if (d0DirDiff < 4) goto l_strive_left;

    // Right strive: D0 = player.direction + 1.
    d0DirDiff = (int16_t)(swosReadSignedWord(playerAddr + PLSPR_OFF_DIRECTION) + 1);
    goto l_set_new_ball_direction_and_speed;

l_strive_left:;
    // player.cpp:2039-2052 -- D0 = player.direction - 1.
    d0DirDiff = (int16_t)(swosReadSignedWord(playerAddr + PLSPR_OFF_DIRECTION) - 1);
    goto l_set_new_ball_direction_and_speed;

l_tackling_in_same_or_oposite_direction:;
    // player.cpp:2054-2058 -- D0 = player.direction.
    d0DirDiff = swosReadSignedWord(playerAddr + PLSPR_OFF_DIRECTION);

l_set_new_ball_direction_and_speed:;
    // player.cpp:2059-2066 -- D0 &= 7; team.controlledPlDirection = D0.
    d0DirDiff = (int16_t)(d0DirDiff & 7);
    swosWriteWord(teamBase + TEAMDATA_OFF_CONTROLLED_PL_DIRECTION, (uint16_t)d0DirDiff);

    {
    // player.cpp:2067-2098 -- destX/Y = ball.x/y + kDefaultDestinations[D0*4].
    int destIdx = ((uint16_t)d0DirDiff & 0xFFFFu) << 2;
    int16_t defDx = swosReadSignedWord(ADDR_kDefaultDestinations + destIdx);
    int16_t defDy = swosReadSignedWord(ADDR_kDefaultDestinations + destIdx + 2);
    swosBallSpriteSetDestX((int16_t)(swosBallSpriteXPixels() + defDx));
    swosBallSpriteSetDestY((int16_t)(swosBallSpriteYPixels() + defDy));
    }

    {
    // player.cpp:2099-2135 -- player.speed -= (speed>>1)|1; ball.speed = 1.5 * player.speed.
    int16_t pSp = swosReadSignedWord(playerAddr + PLSPR_OFF_SPEED);
    int16_t halfDecrement = (int16_t)((uint16_t)pSp >> 1);
    pSp = (int16_t)(pSp - halfDecrement);
    pSp = (int16_t)(pSp | 1);
    swosWriteWord(playerAddr + PLSPR_OFF_SPEED, (uint16_t)pSp);

    // Now player.speed = pSp. ball.speed = pSp + (pSp >> 1).
    int16_t halfP = (int16_t)((uint16_t)pSp >> 1);
    int16_t ballNewSpeed = (int16_t)(halfP + pSp);
    swosBallSpriteSetSpeed(ballNewSpeed);
    }

    // player.cpp:2137 -- tackleState = TS_TACKLING_THE_BALL (1).
    swosWriteWord(playerAddr + PLSPR_OFF_TACKLE_STATE, 1);

    {
    // player.cpp:2138-2153 -- if opp.controlledPlayer is null OR ballDist<9 -> out.
    int32_t oppTeam = swosReadSignedDword(teamBase + TEAMDATA_OFF_OPPONENTS_TEAM);
    int32_t oppCtrl = swosReadSignedDword(oppTeam + TEAMDATA_OFF_CONTROLLED_PLAYER);
    if (oppCtrl == 0) goto l_out_weak;
    int32_t oppBallDist = swosReadSignedDword(oppCtrl + PLSPR_OFF_BALL_DISTANCE);
    if (oppBallDist < 9) goto l_out_weak;

    // player.cpp:2168-2223 -- distance^2 check. >32 -> TS_GOOD_TACKLE.
    int16_t px = swosReadSignedWord(playerAddr + PLSPR_OFF_X + 2);
    int16_t ox = swosReadSignedWord(oppCtrl + PLSPR_OFF_X + 2);
    int16_t dx = (int16_t)(px - ox);
    int32_t dxSq = (int32_t)dx * (int32_t)dx;
    int16_t py = swosReadSignedWord(playerAddr + PLSPR_OFF_Y + 2);
    int16_t oy = swosReadSignedWord(oppCtrl + PLSPR_OFF_Y + 2);
    int16_t dy = (int16_t)(py - oy);
    int32_t dySq = (int32_t)dy * (int32_t)dy;
    int32_t distSq = dxSq + dySq;
    if (distSq <= 32) goto l_out_weak;

    swosWriteWord(playerAddr + PLSPR_OFF_TACKLE_STATE, 2);  // TS_GOOD_TACKLE
    }

l_out_weak:;
    // PlayKickSample() omitted (audio).
    swosResetBothTeamSpinTimers();
}


// ============================================================================
// doFlyingHeader -- player.cpp:2530-2554
// ============================================================================
void swosDoFlyingHeader(int playerAddr) {
    // player.cpp:2532-2534 -- ball.deltaZ = kHeaderLowJumpHeight.
    int32_t dz = swosReadSignedDword(ADDR_kHeaderLowJumpHeight);
    swosBallSpriteSetDeltaZ(dz);

    // player.cpp:2535-2552 -- speed -= speed >> 2 (i.e. 75% of original).
    int16_t sp = swosBallSpriteSpeed();
    int16_t shr2 = (int16_t)((uint16_t)sp >> 2);
    swosBallSpriteSetSpeed((int16_t)(sp - shr2));

    // player.cpp:2553 -- SetPlayerJumpHeaderHitAnimationTable.
    swosSetPlayerJumpHeaderHitAnimationTable(playerAddr);
}


// ============================================================================
// doPass -- player.cpp:2560-3123
// ============================================================================
void swosDoPass(int teamBase, int playerAddr) {
    // player.cpp:2562-2563 -- goodPassSampleCommand = 0; stateGoal = 0.
    swosWriteWord(ADDR_goodPassSampleCommand, 0);
    swosWriteWord(ADDR_stateGoal, 0);

    // player.cpp:2564-2566 -- A0 = team.controlledPlayer. Read but never
    // used again in the C# source either (mechanically dead, no side
    // effects -- kept for fidelity, same pattern as `isTopTeam` above).
    int32_t a0CtrlPlayer = swosReadSignedDword(teamBase + TEAMDATA_OFF_CONTROLLED_PLAYER);
    (void)a0CtrlPlayer;

    // player.cpp:2567-2572 -- D0 = D7 = player.direction; team.controlledPlDirection = direction.
    int16_t pDir = swosReadSignedWord(playerAddr + PLSPR_OFF_DIRECTION);
    int16_t d7Dir = pDir;
    swosWriteWord(teamBase + TEAMDATA_OFF_CONTROLLED_PL_DIRECTION, (uint16_t)pDir);

    // player.cpp:2574-2580 -- A4 = PlayerGameHeader of A1; getClosestNonControlledPlayerInDirection.
    int32_t a0Closest = swosGetClosestNonControlledPlayerInDirection(pDir, teamBase, playerAddr);
    if (a0Closest == -1) goto l_no_closest_player;

    // player.cpp:2592-2596 -- team.passToPlayerPtr = closest; passingBall=1; passingToPlayer=1.
    swosWriteDword(teamBase + TEAMDATA_OFF_PASS_TO_PLAYER_PTR, (uint32_t)a0Closest);
    swosWriteWord(teamBase + TEAMDATA_OFF_PASSING_BALL, 1);
    swosWriteWord(teamBase + TEAMDATA_OFF_PASSING_TO_PLAYER, 1);

    // player.cpp:2597-2608 -- if gameStatePl != ST_GAME_IN_PROGRESS (100) -> calculate_pass_to_player_delta_x_y.
    if (swosReadSignedWord(ADDR_gameStatePl) != 100) goto l_calculate_pass_to_player_delta_x_y;

    // player.cpp:2610-2616 -- if team.playerNumber != 0 -> calculate_pass_to_player_delta_x_y.
    if (swosReadSignedWord(teamBase + TEAMDATA_OFF_PLAYER_NUMBER) != 0) goto l_calculate_pass_to_player_delta_x_y;

    {
    // player.cpp:2618-2650 -- AI failed-pass check. D7 = (currentGameTick & 0x1E) >> 1;
    // chance = kAIFailedPassChance[passing*2]. If D7 >= chance -> good pass.
    int passingSkill = 4;
    {
        int32_t piAddrPass = swosGetPlayerInfoForSprite(teamBase, playerAddr);
        if (piAddrPass != 0) {
            int ps = swosReadByte(piAddrPass + TDL_OFF_PASSING);
            if (ps >= 0 && ps <= 7) passingSkill = ps;
        }
    }
    uint16_t tick = swosReadWord(ADDR_currentGameTick);
    int d7Rnd = (tick & 0x1E) >> 1;
    int16_t chance = swosReadSignedWord(ADDR_kAIFailedPassChance + passingSkill * 2);
    if (d7Rnd >= chance) goto l_calculate_pass_to_player_delta_x_y;
    }

    {
    // player.cpp:2652-2742 -- Failed pass: destX/Y = target + dx/dy where
    // dx/dy = (target - ball) * either (>>1) or 1 then << 5, depending on
    // currentGameTick & 0x20.
    swosWriteWord(ADDR_goodPassSampleCommand, (uint16_t)-2);

    int16_t txp = swosReadSignedWord(a0Closest + PLSPR_OFF_X + 2);
    int16_t bxp = swosBallSpriteXPixels();
    int16_t dx = (int16_t)(txp - bxp);
    bool jitter1 = (swosReadByte(ADDR_currentGameTick) & 0x20) != 0;
    if (!jitter1) {
        dx = (int16_t)swosAsr32((int32_t)dx, 1);  // sar
    }
    dx = (int16_t)(dx << 5);
    swosBallSpriteSetDestX((int16_t)(swosBallSpriteXPixels() + dx));

    int16_t typ = swosReadSignedWord(a0Closest + PLSPR_OFF_Y + 2);
    int16_t byp = swosBallSpriteYPixels();
    int16_t dy = (int16_t)(typ - byp);
    bool jitter2 = (swosReadByte(ADDR_currentGameTick) & 0x20) != 0;
    if (jitter2) {
        dy = (int16_t)swosAsr32((int32_t)dy, 1);  // sar
    }
    dy = (int16_t)(dy << 5);
    swosBallSpriteSetDestY((int16_t)(swosBallSpriteYPixels() + dy));
    goto l_determine_ball_speed;
    }

l_calculate_pass_to_player_delta_x_y:;
    {
    // player.cpp:2744-2856 -- D1 = target.x - ball.x; D2 = target.y - ball.y.
    // Then doubling loop until destX/Y land outside playable area.
    int16_t ctxp = swosReadSignedWord(a0Closest + PLSPR_OFF_X + 2);
    int16_t cbxp = swosBallSpriteXPixels();
    int16_t d1Dx = (int16_t)(ctxp - cbxp);
    int16_t ctyp = swosReadSignedWord(a0Closest + PLSPR_OFF_Y + 2);
    int16_t cbyp = swosBallSpriteYPixels();
    int16_t d2Dy = (int16_t)(ctyp - cbyp);
    if (d1Dx == 0 && d2Dy == 0) {
        d1Dx = 1;
    }

l_increase_distances_loop:;
    {
    int16_t testDestX = (int16_t)(swosBallSpriteXPixels() + d1Dx);
    if (testDestX < 0) goto l_set_dest_x_y;
    if (testDestX >= 672) goto l_set_dest_x_y;

    int16_t testDestY = (int16_t)(swosBallSpriteYPixels() + d2Dy);
    if (testDestY < 0) goto l_set_dest_x_y;
    if (testDestY >= 880) goto l_set_dest_x_y;

    d1Dx = (int16_t)(d1Dx << 1);
    d2Dy = (int16_t)(d2Dy << 1);
    goto l_increase_distances_loop;
    }

l_set_dest_x_y:;
    swosBallSpriteSetDestX((int16_t)(swosBallSpriteXPixels() + d1Dx));
    swosBallSpriteSetDestY((int16_t)(swosBallSpriteYPixels() + d2Dy));
    }

l_determine_ball_speed:;
    {
    // player.cpp:2883-2992 -- pick speed by ballDistance (squared).
    int16_t d1Sp;
    int32_t dist = swosReadSignedDword(a0Closest + PLSPR_OFF_BALL_DISTANCE);
    if (dist < 2500) { d1Sp = swosReadSignedWord(ADDR_kPassingSpeedCloserThan2500); goto l_set_ball_speed; }
    if (dist < 10000) { d1Sp = swosReadSignedWord(ADDR_kPassingSpeed_2500_10000); goto l_set_ball_speed; }
    if (dist < 22500) { d1Sp = swosReadSignedWord(ADDR_kPassingSpeed_10000_22500); goto l_set_ball_speed; }
    if (dist < 40000) { d1Sp = swosReadSignedWord(ADDR_kPassingSpeed_22500_40000); goto l_set_ball_speed; }
    if (dist < 62500) { d1Sp = swosReadSignedWord(ADDR_kPassingSpeed_40000_62500); goto l_set_ball_speed; }
    if (dist < 90000) { d1Sp = swosReadSignedWord(ADDR_kPassingSpeed_62500_90000); goto l_set_ball_speed; }
    if (dist < 122500) { d1Sp = swosReadSignedWord(ADDR_kPassingSpeed_90000_122500); goto l_set_ball_speed; }
    d1Sp = swosReadSignedWord(ADDR_kPassingSpeedFurtherThan122500);

l_set_ball_speed:;
    // player.cpp:2994-3020 -- ball.speed = D1 + kBallSpeedPassingIncrease[passing*2].
    {
    int passingSkill2 = 4;
    int16_t incr = swosReadSignedWord(ADDR_kBallSpeedPassingIncrease + passingSkill2 * 2);
    swosBallSpriteSetSpeed((int16_t)(d1Sp + incr));
    }
    swosWriteWord(ADDR_goodPassSampleCommand, (uint16_t)-1);
    goto l_reset_spin_timers;
    }

l_no_closest_player:;
    {
    // player.cpp:3023-3063 -- no candidate -- kick toward kDefaultDestinations[D7*4]
    // (or per-state table).
    int a0DestTable = swosGetBallDestCoordinatesTable();
    int destIdx2 = ((uint16_t)d7Dir & 0xFFFFu) << 2;
    int16_t defDx = swosReadSignedWord(a0DestTable + destIdx2);
    int16_t defDy = swosReadSignedWord(a0DestTable + destIdx2 + 2);
    swosBallSpriteSetDestX((int16_t)(swosBallSpriteXPixels() + defDx));
    swosBallSpriteSetDestY((int16_t)(swosBallSpriteYPixels() + defDy));
    int16_t freeSp = swosReadSignedWord(ADDR_kFreePassReleasingBallSpeed);
    swosBallSpriteSetSpeed(freeSp);
    }

l_reset_spin_timers:;
    // player.cpp:3066 -- resetBothTeamSpinTimers.
    swosResetBothTeamSpinTimers();

    // player.cpp:3067-3076 -- if team.playerNumber == 0 (CPU) -> player_passing
    // skipping spinTimer = 0.
    if (swosReadSignedWord(teamBase + TEAMDATA_OFF_PLAYER_NUMBER) == 0) goto l_player_passing;

    // Human player -- clear spinTimer.
    swosWriteWord(teamBase + TEAMDATA_OFF_SPIN_TIMER, 0);

l_player_passing:;
    // player.cpp:3079-3080 -- team.passInProgress = 1.
    swosWriteWord(teamBase + TEAMDATA_OFF_PASS_IN_PROGRESS, 1);

    // player.cpp:3081-3118 -- if gameStatePl == ST_GAME_IN_PROGRESS (100)
    // OR gameState < 15 OR gameState > 20 -> play kick & pass samples.
    if (swosReadSignedWord(ADDR_gameStatePl) == 100) goto l_play_kick_and_pass_samples;
    {
    int16_t gameStateEnd = swosReadSignedWord(ADDR_gameState);
    if (gameStateEnd < 15) goto l_play_kick_and_pass_samples;
    if (gameStateEnd <= 20) return;
    }

l_play_kick_and_pass_samples:;
    // player.cpp:3121-3122 -- PlayKickSample (omitted, audio); PlayStopGoodPassSampleIfNeeded.
    swosPlayStopGoodPassSampleIfNeeded();
}


// ============================================================================
// setPlayerDowntimeAfterTackle -- player.cpp:3132-3170
// ============================================================================
void swosSetPlayerDowntimeAfterTackle(int teamBase, int playerAddr) {
    // player.cpp:3138-3148 -- if player.tacklingTimer == -1 -> CPU path (kComputerTacklingDownTime).
    int16_t tackleTimer = swosReadSignedWord(playerAddr + PLSPR_OFF_TACKLING_TIMER);
    int a0Table = (tackleTimer == -1) ? ADDR_kComputerTacklingDownTime : ADDR_kPlayerTacklingDownTime;

    // player.cpp:3152-3169 -- playerDownTimer = table[tackling*2].
    // Real lookup: [esi+72] = PlayerGameHeader.tackling === PlayerInfo+30 (TDL_OFF_TACKLING).
    int tacklingSkill = 4;
    {
        int32_t piAddr = swosGetPlayerInfoForSprite(teamBase, playerAddr);
        if (piAddr != 0) {
            int ts = swosReadByte(piAddr + TDL_OFF_TACKLING);
            if (ts >= 0 && ts <= 7) tacklingSkill = ts;
        }
    }
    int16_t downTime = swosReadSignedWord(a0Table + tacklingSkill * 2);
    swosWriteByte(playerAddr + PLSPR_OFF_PLAYER_DOWN_TIMER, downTime);
}


// ============================================================================
// setJumpHeaderHitAnimTable -- player.cpp:3175-3243 (static)
// ============================================================================
void swosSetJumpHeaderHitAnimTable(int playerAddr) {
    // player.cpp:3177-3188 -- if animTable already == jumpHeaderHitAnimTable -> out.
    int32_t animTable = swosReadSignedDword(playerAddr + PLSPR_OFF_ANIM_TABLE_PTR);
    if (animTable == ADDR_kJumpHeaderHitAnimTableAddr) return;

    // player.cpp:3190-3195 -- if player.heading != 0 -> out.
    int16_t heading = swosReadSignedWord(playerAddr + 98 /* heading */);
    if (heading != 0) return;

    // player.cpp:3197-3207 -- if playerDownTimer != 40 -> out.
    uint8_t dt = swosReadByte(playerAddr + PLSPR_OFF_PLAYER_DOWN_TIMER);
    if (dt != 40) return;

    // player.cpp:3209-3219 -- if frameSwitchCounter > 2 -> out.
    int16_t fsCounter = swosReadSignedWord(playerAddr + PLSPR_OFF_FRAME_SWITCH_COUNTER);
    if (fsCounter > 2) return;

    // player.cpp:3221-3229 -- if (currentTick+1) bit 1 not set -> out.
    uint8_t tickHi = swosReadByte(ADDR_currentGameTick + 1);
    if ((tickHi & 2) == 0) return;

    // player.cpp:3231-3242 -- A0 = jumpHeaderHitAnimTable; player.speed >>= 1.
    swosSetPlayerAnimationTableAndPictureIndex(ADDR_kJumpHeaderHitAnimTableAddr, playerAddr);
    int16_t sp = swosReadSignedWord(playerAddr + PLSPR_OFF_SPEED);
    swosWriteWord(playerAddr + PLSPR_OFF_SPEED, (uint16_t)((uint16_t)sp >> 1));
}


// ============================================================================
// doLobHeader -- player.cpp:3257-3281 (static)
// ============================================================================
void swosDoLobHeader(int playerAddr) {
    // player.cpp:3259-3261 -- ball.deltaZ = kHeaderHighJumpHeight.
    int32_t dz = swosReadSignedDword(ADDR_kHeaderHighJumpHeight);
    swosBallSpriteSetDeltaZ(dz);

    // player.cpp:3262-3279 -- speed -= speed >> 4 (i.e. 93.75% of original).
    int16_t sp = swosBallSpriteSpeed();
    int16_t shr4 = (int16_t)((uint16_t)sp >> 4);
    swosBallSpriteSetSpeed((int16_t)(sp - shr4));

    // player.cpp:3280 -- SetPlayerJumpHeaderHitAnimationTable.
    swosSetPlayerJumpHeaderHitAnimationTable(playerAddr);
}


// ============================================================================
// getClosestNonControlledPlayerInDirection -- player.cpp:3293-3420 (static)
// ============================================================================
int swosGetClosestNonControlledPlayerInDirection(int dir, int teamBase, int playerAddr) {
    (void)playerAddr;  // Matches the C# signature; unused in the source body too.

    // player.cpp:3295-3309 -- D3 = ball.x, D4 = ball.y, D0 = d0Dir << 5.
    // (ball.x/ball.y are read in the source but not subsequently used --
    // kept for fidelity, matching the `isTopTeam`/`a0CtrlPlayer` pattern
    // elsewhere in this file.)
    int16_t ballX = swosBallSpriteXPixels();
    int16_t ballY = swosBallSpriteYPixels();
    (void)ballX;
    (void)ballY;
    int d0Shift = ((uint16_t)dir & 0xFFFFu) << 5;

    // player.cpp:3310-3312 -- A0 = -1, D2 = -1, D7 = 10 (loop counter).
    int32_t a0Best = -1;
    int32_t d2BestDist = -1;
    int loopCount = 10;

    // A2 = team.spritesTable (TeamData +20).
    int32_t spritesTableAddr = swosReadSignedDword(teamBase + TEAMDATA_OFF_PLAYERS);

l_players_loop:;
    {
    // player.cpp:3315-3322 -- A1 = spritesTable[i]; spritesTable += 4.
    int32_t a1Slot = swosReadSignedDword(spritesTableAddr);
    spritesTableAddr += 4;

    // player.cpp:3324-3336 -- skip if A1 == team.controlledPlayer.
    int32_t ctrlPlayer = swosReadSignedDword(teamBase + TEAMDATA_OFF_CONTROLLED_PLAYER);
    if (a1Slot == ctrlPlayer) goto l_next_iter;

    // player.cpp:3338-3345 -- skip if player.sentAway != 0.
    if (swosReadSignedWord(a1Slot + PLSPR_OFF_SENT_AWAY) != 0) goto l_next_iter;

    // player.cpp:3347-3358 -- skip if playerState != PL_NORMAL (0).
    if (swosReadByte(a1Slot + PLSPR_OFF_PLAYER_STATE) != 0) goto l_next_iter;

    // player.cpp:3360-3391 -- D1 = player.fullDirection - d0Shift (byte). Skip if |D1| > 16.
    int16_t fullDir = swosReadSignedWord(a1Slot + PLSPR_OFF_FULL_DIRECTION);
    int8_t d1Byte = (int8_t)((fullDir & 0xFF) - (d0Shift & 0xFF));
    if (d1Byte < -16) goto l_next_iter;
    if (d1Byte > 16) goto l_next_iter;

    // player.cpp:3393-3406 -- D1 = ballDistance. If D1 >= best -> skip.
    int32_t ballDist = swosReadSignedDword(a1Slot + PLSPR_OFF_BALL_DISTANCE);
    if ((uint32_t)ballDist >= (uint32_t)d2BestDist) goto l_next_iter;

    // player.cpp:3408-3411 -- best updated.
    d2BestDist = ballDist;
    a0Best = a1Slot;
    }

l_next_iter:;
    // player.cpp:3414-3419 -- dec D7; loop while >= 0.
    loopCount--;
    if (loopCount >= 0) goto l_players_loop;

    return a0Best;
}


// ============================================================================
// setPlayerJumpHeaderHitAnimationTable -- player.cpp:3427-3444 (static)
// ============================================================================
void swosSetPlayerJumpHeaderHitAnimationTable(int playerAddr) {
    // Defensive: callers may still pass 0 if A1 wasn't wired yet.
    if (playerAddr == 0) return;

    // player.cpp:3429-3440 -- if frameSwitchCounter > 2 -> out.
    int16_t fsCounter = swosReadSignedWord(playerAddr + PLSPR_OFF_FRAME_SWITCH_COUNTER);
    if (fsCounter > 2) return;

    // player.cpp:3442-3443 -- set jumpHeaderHitAnimTable.
    swosSetPlayerAnimationTableAndPictureIndex(ADDR_kJumpHeaderHitAnimTableAddr, playerAddr);
}


// ============================================================================
// getBallDestCoordinatesTable -- player.cpp:3567-3720 (static)
// ============================================================================
int swosGetBallDestCoordinatesTable(void) {
    int16_t gameState = swosReadSignedWord(ADDR_gameState);

    // player.cpp:3568-3591 -- gameState 15..20 -> throw-in.
    if (gameState >= 15 && gameState <= 20) {
        // player.cpp:3593-3603 -- if foulX > 336 -> right throw; else -> left throw.
        int16_t foulX = swosReadSignedWord(ADDR_foulXCoordinate);
        if (foulX > 336) return ADDR_kRightThrowInBallDestDelta;
        return ADDR_kLeftThrowInBallDestDelta;
    }

    // player.cpp:3612-3623 -- gameState 14 (PENALTY) -> penalty.
    if (gameState == 14) return ADDR_kPenaltyBallDestDelta;
    // player.cpp:3625-3635 -- gameState 31 (PENALTIES) -> penalty.
    if (gameState == 31) return ADDR_kPenaltyBallDestDelta;

    // player.cpp:3641-3664 -- gameState 4 or 5 -> corner.
    if (gameState == 4 || gameState == 5) {
        // player.cpp:3666-3712 -- foulY > 449 -> lower; foulX > 336 -> right-side.
        int16_t foulY = swosReadSignedWord(ADDR_foulYCoordinate);
        int16_t foulX2 = swosReadSignedWord(ADDR_foulXCoordinate);
        if (foulY > 449) {
            // Lower corner.
            if (foulX2 > 336) return ADDR_kLowerRightCornerBallDestDelta;
            return ADDR_kLowerLeftCornerBallDestDelta;
        }
        // Upper corner.
        if (foulX2 > 336) return ADDR_kUpperRightCornerBallDestDelta;
        return ADDR_kUpperLeftCornerBallDestDelta;
    }

    // l_not_corner -- default destinations table.
    return ADDR_kDefaultDestinations;
}


// ============================================================================
// playStopGoodPassSampleIfNeeded -- player.cpp:3722-3762 (static)
// ============================================================================
void swosPlayStopGoodPassSampleIfNeeded(void) {
    // player.cpp:3724-3734 -- if cmd == -1 -> enqueue_good_pass_sample.
    int16_t cmd = swosReadSignedWord(ADDR_goodPassSampleCommand);
    if (cmd == -1) {
        swosWriteWord(ADDR_goodPassSampleCommand, 0);
        swosEnqueuePlayingGoodPassSample();
        return;
    }

    // player.cpp:3736-3746 -- if cmd == -2 -> stop_sample.
    if (cmd == -2) {
        swosWriteWord(ADDR_goodPassSampleCommand, 0);
        swosStopGoodPassSample();
        return;
    }

    // player.cpp:3748 -- nothing to do.
}


// ============================================================================
// stopGoodPassSample -- player.cpp:3764-3767 (static)
// ============================================================================
void swosStopGoodPassSample(void) {
    swosWriteWord(ADDR_playingGoodPassTimer, (uint16_t)-1);
    // player.cpp:3764 stopGoodPassSample -- MatchAudio.CancelGoodPass() omitted (audio).
}


// ============================================================================
// enqueuePlayingGoodPassSample -- player.cpp:3769-3793 (static)
// ============================================================================
void swosEnqueuePlayingGoodPassSample(void) {
    // player.cpp:3771 -- playingGoodPassTimer = -1.
    swosWriteWord(ADDR_playingGoodPassTimer, (uint16_t)-1);

    // player.cpp:3772-3779 -- goodPassTimer += 1.
    int16_t timer = swosReadSignedWord(ADDR_goodPassTimer);
    timer = (int16_t)(timer + 1);
    swosWriteWord(ADDR_goodPassTimer, (uint16_t)timer);

    // player.cpp:3780-3793 -- if timer == 5 -> set playing=10, timer=0.
    if (timer != 5) return;
    swosWriteWord(ADDR_goodPassTimer, 0);
    swosWriteWord(ADDR_playingGoodPassTimer, 10);
    // player.cpp:3788-3793 -- every 5th good pass arms the 10-tick good-pass
    // comment. MatchAudio.EnqueueGoodPass() omitted (audio).
}


// ============================================================================
// updatePlayerSpeedAndFrameDelay -- player.cpp:17-77
// ============================================================================
void swosUpdatePlayerSpeedAndFrameDelay(int teamBase, int playerAddr) {
    // player.cpp:19 -- early-out unless player.state == PlayerState::kNormal
    // AND (game is in progress AND player.playerOrdinal == 1 AND not the
    // controlled player). The early-out skips speed/frameDelay writes BUT
    // the asm caller still unconditionally runs the delta-recompute step
    // using whatever speed is currently on the sprite -- mirrored here by
    // calling swosRecomputeSpriteDeltas before every early-out return.
    uint8_t playerState = swosReadByte(playerAddr + PLSPR_OFF_PLAYER_STATE);
    if (playerState != 0) {
        // PL_NORMAL = 0. Anything else -> bail (but still recompute deltas).
        swosRecomputeSpriteDeltas(teamBase, playerAddr);
        return;
    }
    int16_t gameStatePl = swosReadSignedWord(ADDR_gameStatePl);
    int16_t playerOrdinal = swosReadSignedWord(playerAddr + PLSPR_OFF_PLAYER_ORDINAL);
    int32_t controlledPlayer = swosReadSignedDword(teamBase + TEAMDATA_OFF_CONTROLLED_PLAYER);
    if (gameStatePl == 100 /*kInProgress*/ && playerOrdinal == 1 && playerAddr != controlledPlayer) {
        // Keeper early-out -- same delta-recompute responsibility.
        swosRecomputeSpriteDeltas(teamBase, playerAddr);
        return;
    }

    // player.cpp:23-24 -- per-state speed tables.
    static const int kPlayerSpeedsGameInProgress[8] = {
        928, 974, 1020, 1066, 1112, 1158, 1204, 1250
    };
    static const int kPlayerSpeedsGameStopped[8] = {
        1136, 1152, 1168, 1184, 1200, 1216, 1232, 1248
    };
    const int *speedTable = (gameStatePl == 100) ? kPlayerSpeedsGameInProgress : kPlayerSpeedsGameStopped;

    // player.cpp:29 -- auto& playerInfo = getPlayerPointerFromShirtNumber(team, player).
    // Reads PlayerInfo.speed (byte at +32, TDL_OFF_SPEED). Fallback to
    // skill=4 if PlayerInfo hasn't been wired (early-tick safety).
    int32_t playerInfoAddr = swosGetPlayerInfoForSprite(teamBase, playerAddr);
    int playerInfoSpeedSkill = 4;
    if (playerInfoAddr != 0) {
        int s = swosReadByte(playerInfoAddr + TDL_OFF_SPEED);
        if (s >= 0 && s <= 7) playerInfoSpeedSkill = s;
    }

    // player.cpp:32 -- player.speed = speedTable[playerInfo.speed].
    int newSpeed = speedTable[playerInfoSpeedSkill];

    // player.cpp:34-37 -- `runSlower` flag (post-goal slowdown).
    if (swosReadSignedWord(ADDR_runSlower) != 0) {
        newSpeed = 5 * newSpeed / 8;
    }

    // player.cpp:39-43 -- injury speed handicap. Gate: only applies to
    // human/coach-controlled teams (playerNumber != 0 OR playerCoachNumber != 0).
    int16_t playerNumberHandicap = swosReadSignedWord(teamBase + TEAMDATA_OFF_PLAYER_NUMBER);
    int16_t playerCoachNumberHandicap = swosReadSignedWord(teamBase + TEAMDATA_OFF_PLAYER_COACH_NUMBER);
    if (playerNumberHandicap != 0 || playerCoachNumberHandicap != 0) {
        int16_t injuryLevel = swosReadSignedWord(playerAddr + PLSPR_OFF_INJURY_LEVEL);
        if (injuryLevel != 0) {
            static const int kInjuriesSpeedHandicap[8] = {
                0, -96, -128, -160, -192, -224, -256, -288
            };
            int bucket = injuryLevel / 32;
            if (bucket < 0) bucket = 0;
            else if (bucket >= 8) bucket = 7;
            newSpeed += kInjuriesSpeedHandicap[bucket];
        }
    }

    // OpenSWOS OPTIONAL fatigue penalty (see swos_player_energy.h). Reuse
    // the same 46-per-skill-point step the speed table uses.
    if (g_swosPlayerEnergyEffectEnabled) {
        newSpeed -= 46 * swosPlayerEnergySpeedStep(playerAddr);
    }

    // player.cpp:45-48 -- controlled player carrying the ball runs at 87.5%.
    int16_t playerHasBall = swosTeamDataPlayerHasBall(teamBase == TEAMDATA_TOP_BASE);
    if (playerAddr == controlledPlayer && playerHasBall != 0) {
        newSpeed -= newSpeed / 8;
    }

    // player.cpp:50-60 -- pass-overlap speed boost.
    int16_t teamPlayerNumber = swosReadSignedWord(teamBase + TEAMDATA_OFF_PLAYER_NUMBER);
    if (teamPlayerNumber != 0) {
        int32_t passToPtr = swosReadSignedDword(teamBase + TEAMDATA_OFF_PASS_TO_PLAYER_PTR);
        if (passToPtr == playerAddr) {
            int16_t passingToPlayer = swosReadSignedWord(teamBase + TEAMDATA_OFF_PASSING_TO_PLAYER);
            if (passingToPlayer != 0) {
                int16_t longPassFlag = swosReadSignedWord(teamBase + TEAMDATA_OFF_LONG_PASS);
                int16_t leftSpinFlag = swosReadSignedWord(teamBase + TEAMDATA_OFF_LEFT_SPIN);
                int16_t rightSpinFlag = swosReadSignedWord(teamBase + TEAMDATA_OFF_RIGHT_SPIN);
                if (longPassFlag != 0 || leftSpinFlag != 0 || rightSpinFlag != 0) {
                    int16_t ballSpeed = swosBallSpriteSpeed();
                    if (ballSpeed != 0) {
                        // 8-bit signed wrap of fullDirection diff.
                        uint8_t bFD = (uint8_t)(swosBallSpriteFullDirection() & 0xFF);
                        uint8_t pFD = (uint8_t)(swosReadSignedWord(playerAddr + PLSPR_OFF_FULL_DIRECTION) & 0xFF);
                        int8_t directionDiff = (int8_t)(bFD - pFD);
                        if (directionDiff >= -7 && directionDiff <= 7) {
                            newSpeed = (directionDiff >= -5 && directionDiff <= 5) ? 256 : 512;
                        }
                    }
                }
            }
        }
    }

    // player.cpp:62-72 -- stoppage-time slowdown.
    // GameState constants per swos.h: kGoingToHalftime=23,
    // kPlayersGoingToShower=24, kFirstHalfEnded=29, kGameEnded=30.
    if (gameStatePl != 100) {
        int16_t gameState = swosReadSignedWord(ADDR_gameState);
        int16_t stoppageTimerTotal = swosReadSignedWord(ADDR_stoppageTimerTotal);
        if (gameState == 29 || gameState == 30) {
            // player.cpp:63-65 -- slow down and stop players gradually.
            int slowed = newSpeed - stoppageTimerTotal * 32;
            newSpeed = slowed > 0 ? slowed : 0;
        } else if (gameState == 23 || gameState == 24) {
            // player.cpp:66-71 -- speed weighted with time since game/half end.
            int factor = stoppageTimerTotal * 4;
            if (factor > 100) factor = 100;
            newSpeed = newSpeed * factor / 100;
        }
    }

    swosWriteWord(playerAddr + PLSPR_OFF_SPEED, (uint16_t)newSpeed);

    // player.cpp:74-76 -- frameDelay derived from speed.
    {
        const int kMaxSpeed = 1280;
        int diff = kMaxSpeed - newSpeed;
        if (diff < 0) diff = 0;
        int frameDelay = diff / 128 + 6;
        swosWriteWord(playerAddr + PLSPR_OFF_FRAME_DELAY, (uint16_t)frameDelay);
    }

    // updatePlayers.cpp:10088-10356 -- l_update_player_speed_and_deltas
    // post-UpdatePlayerSpeed: unconditional delta/direction/anim-table tail.
    swosRecomputeSpriteDeltas(teamBase, playerAddr);
}


// ============================================================================
// RecomputeSpriteDeltas -- updatePlayers.cpp:10088-10106
// ============================================================================
void swosRecomputeSpriteDeltas(int teamBase, int playerAddr) {
    int16_t curSpeed = swosReadSignedWord(playerAddr + PLSPR_OFF_SPEED);
    int16_t destX = swosReadSignedWord(playerAddr + PLSPR_OFF_DEST_X);
    int16_t destY = swosReadSignedWord(playerAddr + PLSPR_OFF_DEST_Y);
    int16_t curX  = swosReadSignedWord(playerAddr + PLSPR_OFF_X + 2);
    int16_t curY  = swosReadSignedWord(playerAddr + PLSPR_OFF_Y + 2);
    // updatePlayers.cpp:10088-10101 -- CalculateDeltaXAndY(D0=speed, ...).
    SwosDeltasAndAngle deltas = swosCalculateDeltaXAndY(curSpeed, curX, curY, destX, destY);
    // updatePlayers.cpp:10102-10106 -- write deltaX/deltaY.
    swosWriteDword(playerAddr + PLSPR_OFF_DELTA_X, (uint32_t)deltas.deltaX);
    swosWriteDword(playerAddr + PLSPR_OFF_DELTA_Y, (uint32_t)deltas.deltaY);

    int16_t gameStatePl = swosReadSignedWord(ADDR_gameStatePl);
    int16_t ordinal = swosReadSignedWord(playerAddr + PLSPR_OFF_PLAYER_ORDINAL);

    // ----- updatePlayers.cpp:10107-10148 -- keeper facing override --------
    bool calcDirection = false;
    if (ordinal == 1 && gameStatePl == 100) {
        int16_t gkPlaying = swosReadSignedWord(teamBase + TEAMDATA_OFF_GOALKEEPER_PLAYING);
        if (gkPlaying == 0) {
            uint16_t upPa = swosReadWord(ADDR_ballInUpperPenaltyArea);
            uint16_t loPa = swosReadWord(ADDR_ballInLowerPenaltyArea);
            if ((upPa | loPa) != 0)
                calcDirection = true;
        }
    }

    // ----- updatePlayers.cpp:10150-10218 -- direction (0..7) update -------
    int32_t d0Dir = deltas.direction;
    bool skipSettingDirection = false;
    if (!calcDirection) {
        if (d0Dir < 0) {
            int32_t ctrlDir = swosReadSignedDword(teamBase + TEAMDATA_OFF_CONTROLLED_PLAYER);
            int32_t pkpDir = swosReadSignedDword(teamBase + TEAMDATA_OFF_PASSING_KICKING_PLAYER);
            if (playerAddr == ctrlDir || playerAddr == pkpDir)
                skipSettingDirection = true;
            else
                calcDirection = true;
        }
    }
    if (!skipSettingDirection) {
        if (calcDirection) {
            // l_calc_direction -- :10186-10199: D0 = (fullDirection + 128) & 255.
            int16_t oldFull = swosReadSignedWord(playerAddr + PLSPR_OFF_FULL_DIRECTION);
            d0Dir = (oldFull + 128) & 0xFF;
        }
        // l_got_movement -- :10201-10218: direction = ((D0 + 16) & 255) >> 5.
        int dir8 = ((d0Dir + 16) & 0xFF) >> 5;
        swosWriteWord(playerAddr + PLSPR_OFF_DIRECTION, (uint16_t)dir8);
    }

    {
    // ----- updatePlayers.cpp:10220-10278 -- fullDirection refresh ---------
    int16_t d3Target, d4Target;
    int16_t gsTail = swosReadSignedWord(ADDR_gameState);
    if (gameStatePl != 100 && (uint16_t)gsTail <= 20) {
        d3Target = swosReadSignedWord(ADDR_foulXCoordinate);
        d4Target = swosReadSignedWord(ADDR_foulYCoordinate);
    } else {
        // l_ball_going_to_player -- :10253-10258.
        d3Target = swosBallSpriteXPixels();
        d4Target = swosBallSpriteYPixels();
    }
    // l_calculate_player_ball_direction -- :10260-10278.
    SwosDeltasAndAngle face = swosCalculateDeltaXAndY(256, d3Target, d4Target, curX, curY);
    int16_t newFull = face.direction < 0 ? (int16_t)0 : (int16_t)face.direction;
    swosWriteWord(playerAddr + PLSPR_OFF_FULL_DIRECTION, (uint16_t)newFull);
    }

    {
    // ----- updatePlayers.cpp:10279-10354 -- anim-table switch -------------
    uint8_t plState = swosReadByte(playerAddr + PLSPR_OFF_PLAYER_STATE);
    if (plState != 0) return;  // :10279-10290 jnz l_next_player

    bool moving = deltas.deltaX != 0 || deltas.deltaY != 0;  // :10292-10304
    uint8_t newMoving = moving ? (uint8_t)0xFF : (uint8_t)0x00;
    uint8_t stampedMoving = swosReadByte(playerAddr + PLSPR_OFF_IS_MOVING);
    if (newMoving == stampedMoving) {
        // l_no_change_in_movement -- :10339-10354.
        int16_t stampedDir = swosReadSignedWord(playerAddr + PLSPR_OFF_PLAYER_DIRECTION);
        int16_t dirNow = swosReadSignedWord(playerAddr + PLSPR_OFF_DIRECTION);
        if (stampedDir == dirNow) return;
    }
    // cseg_83D7F -- :10321-10337.
    if (moving) {
        // l_player_is_running -- :10334-10336.
        swosSetPlayerAnimationTable(playerAddr, ADDR_kPlayerRunningAnimTableAddr);
    } else {
        // :10330-10332.
        swosSetPlayerAnimationTable(playerAddr, ADDR_kPlayerStandingAnimTableAddr);
    }
    }
}
