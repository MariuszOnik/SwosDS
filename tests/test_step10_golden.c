// Step-10 differential test: compares FULL VM/Memory state against golden
// dumps produced by the REAL C# (tools/csharp-golden-dump/Step10Golden.cs).
// Covers SetPieces.cs, GameTime.cs, Referee.cs (joining step 7A's
// ActivateReferee), and Result.cs.
//
// RNG / static-state discipline: mirrors Step10Golden.cs's header comment
// exactly -- every scenario that depends on a specific RNG outcome re-seeds
// with the SAME seed the C# side used, immediately after swosMemoryInit(true);
// every scenario touching GameTime's s_stoppageRealTicks or Result.cs's
// scorer-list statics calls the matching reset function first (neither is
// touched by swosMemoryInit()).
//
// Regenerate build/golden/s10_*.bin with:
//   cd tools/csharp-golden-dump && dotnet run -c Release -- ../../build/golden
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "swos_addr.h"
#include "swos_ball_sprite.h"
#include "swos_game_time.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"
#include "swos_referee.h"
#include "swos_result.h"
#include "swos_rng.h"
#include "swos_set_pieces.h"
#include "swos_team_data.h"

#define GOLDEN_DIR "build/golden"

static int g_failures = 0;
static int g_checked = 0;

static void compareFullBuffer(const char *label) {
    char path[256];
    snprintf(path, sizeof(path), "%s/s10_%s.bin", GOLDEN_DIR, label);

    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "FAIL: could not open %s -- run the C# golden-dump harness first: "
                         "cd tools/csharp-golden-dump && dotnet run -c Release -- ../../build/golden\n",
                path);
        g_failures++;
        g_checked++;
        return;
    }
    uint8_t *golden = malloc(SWOS_MEM_SIZE);
    long got = (long)fread(golden, 1, SWOS_MEM_SIZE, f);
    fclose(f);
    if (got != SWOS_MEM_SIZE) {
        printf("FAIL: %s is %ld bytes, expected %d\n", path, got, SWOS_MEM_SIZE);
        g_failures++;
        g_checked++;
        free(golden);
        return;
    }

    const uint8_t *ours = swosMemoryView(0, SWOS_MEM_SIZE);
    int mismatches = 0, firstMismatch = -1;
    for (int i = 0; i < SWOS_MEM_SIZE; i++) {
        if (ours[i] != golden[i]) {
            if (firstMismatch < 0) firstMismatch = i;
            mismatches++;
            if (mismatches <= 20)
                printf("  diff @ 0x%06X: C=0x%02X golden(C#)=0x%02X\n", i, ours[i], golden[i]);
        }
    }
    free(golden);

    g_checked++;
    if (mismatches == 0) {
        printf("ok:   %s matches C# byte-for-byte (0x%X bytes)\n", label, SWOS_MEM_SIZE);
    } else {
        printf("FAIL: %s has %d mismatched byte(s), first at 0x%06X\n", label, mismatches, firstMismatch);
        g_failures++;
    }
}

// Scratch "TeamGame.players[]" base addresses -- see Step10Golden.cs's own
// comment for the rationale (reuses the established 0x4FE60/0x4FEA0
// PlayerInfo scratch convention).
#define K_TOP_TEAM_IN_GAME 0x4FE60
#define K_BOT_TEAM_IN_GAME 0x4FEA0

int main(void) {
    int top = TEAMDATA_TOP_BASE, bot = TEAMDATA_BOTTOM_BASE;
    int t1_1 = swosPlayerSpriteBase(1);
    int t1_2 = swosPlayerSpriteBase(2);
    int t2_1 = swosPlayerSpriteBase(12);

    // ==== SetPieces.SetThrowInPlayerDestinationCoordinates ====
    swosMemoryInit(true);
    swosBallSpriteSetXPixels(400); swosBallSpriteSetYPixels(300);
    swosSetPiecesSetThrowInPlayerDestinationCoordinates(t1_1);
    compareFullBuffer("set_throw_in_dest_right_half");

    swosMemoryInit(true);
    swosBallSpriteSetXPixels(100); swosBallSpriteSetYPixels(500);
    swosSetPiecesSetThrowInPlayerDestinationCoordinates(t1_1);
    compareFullBuffer("set_throw_in_dest_left_half");

    // ==== SetPieces.TickThrowIn ====
    swosMemoryInit(true);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_NUMBER, 0);
    swosWriteByte(ADDR_playerTurnFlags, 0xFF);
    swosPlayerSpriteSetDirection(1, 2);
    swosWriteWord(ADDR_gameState, 15);
    swosWriteWord(ADDR_stoppageTimerActive, 0);
    swosWriteByte(t1_1 + PLSPR_OFF_PLAYER_DOWN_TIMER, 0);
    swosWriteWord(top + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, (uint16_t)-1);
    swosWriteByte(top + TEAMDATA_OFF_NORMAL_FIRE, 1);
    swosRngReseed(20);
    swosSetPiecesTickThrowIn(t1_1, BALLSPR_BASE, top);
    compareFullBuffer("tick_throw_in_ai_thrower_normal_fire_kick");

    swosMemoryInit(true);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_NUMBER, 1);
    swosWriteByte(ADDR_playerTurnFlags, 0xFF);
    swosPlayerSpriteSetDirection(1, 2);
    swosWriteWord(ADDR_gameState, 15);
    swosWriteWord(top + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, 2);
    swosWriteByte(top + TEAMDATA_OFF_QUICK_FIRE, 1);
    swosTeamDataSetControlledPlayer(true, t1_1);
    swosPlayerSpriteSetX(2, 310 << 16); swosPlayerSpriteSetY(2, 400 << 16);
    swosPlayerSpriteSetX(1, 300 << 16); swosPlayerSpriteSetY(1, 400 << 16);
    swosSetPiecesTickThrowIn(t1_1, BALLSPR_BASE, top);
    compareFullBuffer("tick_throw_in_human_quick_fire_pass");

    swosMemoryInit(true);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_NUMBER, 1);
    swosWriteByte(ADDR_playerTurnFlags, 0xFF);
    swosPlayerSpriteSetDirection(1, 2);
    swosWriteWord(ADDR_gameState, 15);
    swosWriteByte(t1_1 + PLSPR_OFF_PLAYER_DOWN_TIMER, 5);
    swosSetPiecesTickThrowIn(t1_1, BALLSPR_BASE, top);
    compareFullBuffer("tick_throw_in_countdown_not_ready");

    swosMemoryInit(true);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_NUMBER, 1);
    swosWriteByte(ADDR_playerTurnFlags, 0xFF);
    swosPlayerSpriteSetDirection(1, 2);
    swosWriteWord(ADDR_gameState, 100);
    swosWriteByte(t1_1 + PLSPR_OFF_PLAYER_DOWN_TIMER, 0);
    swosSetPiecesTickThrowIn(t1_1, BALLSPR_BASE, top);
    compareFullBuffer("tick_throw_in_abort_wrong_game_state");

    // ==== SetPieces.DispatchByGameState ====
    swosMemoryInit(true);
    swosWriteWord(ADDR_gameState, 16);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_NUMBER, 1);
    swosWriteByte(ADDR_playerTurnFlags, 0xFF);
    swosPlayerSpriteSetDirection(1, 2);
    swosWriteByte(t1_1 + PLSPR_OFF_PLAYER_DOWN_TIMER, 0);
    swosSetPiecesDispatchByGameState(t1_1, BALLSPR_BASE, 0, top);
    compareFullBuffer("dispatch_throw_in");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameState, 13);
    swosWriteWord(ADDR_resultTimer, 0);
    swosWriteWord(top + TEAMDATA_OFF_CONTROLLED_PL_DIRECTION, 3);
    swosSetPiecesDispatchByGameState(t1_1, BALLSPR_BASE, 0, top);
    compareFullBuffer("dispatch_foul");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameState, 14);
    swosWriteWord(ADDR_resultTimer, 0);
    swosWriteWord(ADDR_AI_rand, 3);
    swosWriteByte(ADDR_playerTurnFlags, 0xFF);
    swosSetPiecesDispatchByGameState(t1_1, BALLSPR_BASE, t1_1, top);
    compareFullBuffer("dispatch_penalty");

    // ==== SetPieces.TickSetPieces ====
    swosMemoryInit(true);
    swosWriteWord(ADDR_gameState, 4);
    swosWriteWord(ADDR_resultTimer, 0);
    swosWriteWord(ADDR_foulXCoordinate, 86);
    swosWriteWord(ADDR_foulYCoordinate, 134);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)top);
    swosWriteWord(ADDR_cameraDirection, 2);
    swosPlayerSpriteSetX(2, 90 << 16); swosPlayerSpriteSetY(2, 140 << 16);
    swosSetPiecesTickSetPieces();
    compareFullBuffer("tick_set_pieces_corner");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameState, 1);
    swosWriteWord(ADDR_resultTimer, 0);
    swosWriteWord(ADDR_foulXCoordinate, 396);
    swosWriteWord(ADDR_foulYCoordinate, 744);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)bot);
    swosWriteWord(ADDR_cameraDirection, 0);
    swosSetPiecesTickSetPieces();
    compareFullBuffer("tick_set_pieces_goal_kick");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameState, 15);
    swosWriteWord(ADDR_resultTimer, 0);
    swosWriteWord(ADDR_foulXCoordinate, 590);
    swosWriteWord(ADDR_foulYCoordinate, 300);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)top);
    swosPlayerSpriteSetX(2, 585 << 16); swosPlayerSpriteSetY(2, 305 << 16);
    swosSetPiecesTickSetPieces();
    compareFullBuffer("tick_set_pieces_throw_in");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameState, 13);
    swosWriteWord(ADDR_resultTimer, 0);
    swosWriteWord(ADDR_foulXCoordinate, 250);
    swosWriteWord(ADDR_foulYCoordinate, 400);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)bot);
    swosWriteWord(ADDR_cameraDirection, 4);
    swosPlayerSpriteSetX(12, 255 << 16); swosPlayerSpriteSetY(12, 405 << 16);
    swosSetPiecesTickSetPieces();
    compareFullBuffer("tick_set_pieces_free_kick");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameState, 31);
    swosWriteWord(ADDR_gameStatePl, 101);
    swosWriteWord(ADDR_foulXCoordinate, 336);
    swosWriteWord(ADDR_foulYCoordinate, 187);
    swosWriteDword(ADDR_penaltyShooterSprite, (uint32_t)t2_1);
    swosWriteWord(ADDR_cameraDirection, 0);
    swosSetPiecesTickSetPieces();
    compareFullBuffer("tick_set_pieces_penalty_shootout");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameState, 4);
    swosWriteWord(ADDR_resultTimer, 50);
    swosSetPiecesTickSetPieces();
    compareFullBuffer("tick_set_pieces_result_timer_blocks");

    // ==== SetPieces.TickFreeKick / TickPenalty ====
    swosMemoryInit(true);
    swosWriteWord(ADDR_resultTimer, 0);
    swosWriteWord(top + TEAMDATA_OFF_CONTROLLED_PL_DIRECTION, 5);
    swosSetPiecesTickFreeKick(top);
    compareFullBuffer("tick_free_kick_direction");

    swosMemoryInit(true);
    swosWriteWord(ADDR_resultTimer, 0);
    swosWriteWord(ADDR_gameState, 14);
    swosWriteWord(ADDR_AI_rand, 3);
    swosWriteByte(ADDR_playerTurnFlags, 0xFF);
    swosSetPiecesTickPenalty(t1_1, top);
    compareFullBuffer("tick_penalty_random_direction_allowed");

    swosMemoryInit(true);
    swosWriteWord(ADDR_resultTimer, 0);
    swosWriteWord(ADDR_gameState, 14);
    swosWriteWord(ADDR_AI_rand, 3);
    swosWriteByte(ADDR_playerTurnFlags, 0x00);
    swosPlayerSpriteSetDirection(1, 0);
    swosSetPiecesTickPenalty(t1_1, top);
    compareFullBuffer("tick_penalty_direction_disallowed_zero_dir_returns");

    // ==== SetPieces.AdvancePenaltiesTimer ====
    swosMemoryInit(true);
    swosWriteWord(ADDR_playingPenalties, 1);
    swosWriteWord(ADDR_gameState, 100);
    swosWriteWord(ADDR_penaltiesTimer, 5);
    swosWriteWord(ADDR_m_penaltiesInterval, 110);
    swosSetPiecesAdvancePenaltiesTimer();
    compareFullBuffer("advance_penalties_timer_increment_only");

    swosMemoryInit(true);
    swosWriteWord(ADDR_playingPenalties, 1);
    swosWriteWord(ADDR_gameState, 100);
    swosWriteWord(ADDR_penaltiesTimer, 109);
    swosWriteWord(ADDR_m_penaltiesInterval, 110);
    swosWriteWord(ADDR_team1PenaltyAttempts, 1);
    swosWriteWord(ADDR_team2PenaltyAttempts, 1);
    swosWriteWord(ADDR_team1PenaltyGoals, 1);
    swosWriteWord(ADDR_team2PenaltyGoals, 0);
    swosWriteWord(bot + TEAMDATA_OFF_TEAM_NUMBER, 2);
    swosWriteDword(bot + TEAMDATA_OFF_PLAYERS, 0x4FE2C);
    swosSetPiecesAdvancePenaltiesTimer();
    compareFullBuffer("advance_penalties_timer_triggers_next_penalty");

    // ==== GameTime -- clock lifecycle ====
    swosMemoryInit(true);
    swosWriteWord(ADDR_gameLengthInGame, 1);
    swosGameTimeResetGameTime();
    compareFullBuffer("game_time_reset_and_showing");

    swosMemoryInit(true);
    swosGameTimeResetGameTime();
    swosWriteWord(ADDR_gameStatePl, 100);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteDword(ADDR_gt_timeDelta, 30);
    swosWriteDword(ADDR_gt_secondsSwitchAccumulator, 40);
    swosGameTimeUpdateGameTime();
    compareFullBuffer("game_time_update_normal_tick_no_rollover");

    swosMemoryInit(true);
    swosGameTimeResetGameTime();
    swosWriteWord(ADDR_gameStatePl, 100);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteDword(ADDR_gt_timeDelta, 30);
    swosWriteDword(ADDR_gt_secondsSwitchAccumulator, 0);
    swosWriteDword(ADDR_gt_gameSeconds, 59);
    swosWriteDword(ADDR_gt_gameTimeInMinutes, 10);
    swosGameTimeUpdateGameTime();
    compareFullBuffer("game_time_update_minute_rollover");

    swosMemoryInit(true);
    swosGameTimeResetGameTime();
    swosWriteWord(ADDR_gameStatePl, 0);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteDword(ADDR_gt_gameSeconds, (uint32_t)-1);
    swosWriteDword(ADDR_gt_endGameCounter, 5);
    swosGameTimeUpdateGameTime();
    compareFullBuffer("game_time_update_prolong_pin_then_refresh");

    swosMemoryInit(true);
    swosGameTimeResetGameTime();
    swosWriteWord(ADDR_gameStatePl, 100);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteDword(ADDR_gt_gameSeconds, (uint32_t)-1);
    swosWriteDword(ADDR_gt_endGameCounter, 0);
    swosWriteDword(ADDR_gt_gameTimeInMinutes, 45);
    swosWriteWord(ADDR_goalCounter, 0);
    swosGameTimeUpdateGameTime();
    compareFullBuffer("game_time_update_period_end_fires_end_first_half");

    swosMemoryInit(true);
    swosGameTimeResetGameTime();
    swosWriteWord(ADDR_gameStatePl, 100);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteDword(ADDR_gt_gameSeconds, (uint32_t)-1);
    swosWriteDword(ADDR_gt_endGameCounter, 0);
    swosWriteDword(ADDR_gt_gameTimeInMinutes, 45);
    swosWriteWord(ADDR_goalCounter, 3);
    swosGameTimeUpdateGameTime();
    compareFullBuffer("game_time_update_period_end_waits_for_goal_celebration");

    swosMemoryInit(true);
    swosGameTimeResetGameTime();
    swosWriteDword(ADDR_gt_gameTimeInMinutes, 67);
    swosWriteDword(ADDR_gt_gameTime + 1 * 4, 0);
    swosWriteDword(ADDR_gt_gameTime + 2 * 4, 6);
    swosWriteDword(ADDR_gt_gameTime + 3 * 4, 7);
    {
        int d1, d2, d3;
        (void)swosGameTimeInMinutes();
        swosGameTimeAsBcd(&d1, &d2, &d3);
        (void)swosGameTimeAtZeroMinute();
        (void)swosGameTimeShowing();
    }
    compareFullBuffer("game_time_accessors");

    // ==== GameTime.MarkPlayersHappyOrSad ====
    swosMemoryInit(true);
    swosWriteWord(ADDR_gameState, 30);
    swosWriteDword(ADDR_topTeamInGame, K_TOP_TEAM_IN_GAME);
    swosWriteDword(ADDR_bottomTeamInGame, K_BOT_TEAM_IN_GAME);
    swosWriteDword(ADDR_winningTeamPtr, K_TOP_TEAM_IN_GAME);
    swosRngReseed(1);
    swosGameTimeMarkPlayersHappyOrSad();
    compareFullBuffer("mark_players_happy_or_sad_top_wins");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameState, 30);
    swosWriteDword(ADDR_topTeamInGame, K_TOP_TEAM_IN_GAME);
    swosWriteDword(ADDR_bottomTeamInGame, K_BOT_TEAM_IN_GAME);
    swosWriteDword(ADDR_winningTeamPtr, 0);
    swosGameTimeMarkPlayersHappyOrSad();
    compareFullBuffer("mark_players_happy_or_sad_tie_no_poses");

    // ==== GameTime.NextPenalty ====
    swosMemoryInit(true);
    swosWriteWord(ADDR_team1PenaltyAttempts, 2);
    swosWriteWord(ADDR_team2PenaltyAttempts, 2);
    swosWriteWord(ADDR_team1PenaltyGoals, 1);
    swosWriteWord(ADDR_team2PenaltyGoals, 1);
    swosWriteWord(bot + TEAMDATA_OFF_TEAM_NUMBER, 2);
    swosWriteDword(bot + TEAMDATA_OFF_PLAYERS, 0x4FE2C);
    swosWriteWord(ADDR_team2PenaltyShooterIndex, 5);
    swosGameTimeNextPenalty();
    compareFullBuffer("next_penalty_continues_shootout");

    swosMemoryInit(true);
    swosWriteWord(ADDR_team1PenaltyAttempts, 5);
    swosWriteWord(ADDR_team2PenaltyAttempts, 5);
    swosWriteWord(ADDR_team1PenaltyGoals, 4);
    swosWriteWord(ADDR_team2PenaltyGoals, 3);
    swosWriteWord(ADDR_savedTeam1Goals, 2);
    swosWriteWord(ADDR_savedTeam2Goals, 1);
    swosGameTimeNextPenalty();
    compareFullBuffer("next_penalty_finishes_shootout");

    // ==== GameTime -- initMatch()-adjacent helpers ====
    swosMemoryInit(true);
    swosWriteWord(ADDR_gameLengthInGame, 2);
    swosRngReseed(7);
    swosGameTimeInitPlayerCardChance();
    compareFullBuffer("init_player_card_chance");

    swosMemoryInit(true);
    swosRngReseed(3);
    swosGameTimeDetermineStartingTeamAndTeamPlayingUp();
    compareFullBuffer("determine_starting_team_and_playing_up");

    swosMemoryInit(true);
    g_swosBallSimCurrentPitchType = 4;
    swosGameTimeInitPitchBallFactors();
    compareFullBuffer("init_pitch_ball_factors_normal");

    swosMemoryInit(true);
    g_swosBallSimCurrentPitchType = -3;
    swosGameTimeInitPitchBallFactors();
    g_swosBallSimCurrentPitchType = 4;
    compareFullBuffer("init_pitch_ball_factors_frozen_clamped_low");

    swosMemoryInit(true);
    swosWriteByte(ADDR_team1InGameTeamHeader, 0xAB);
    swosWriteByte(ADDR_team2InGameTeamHeader, 0xCD);
    swosGameTimeSaveTeams();
    swosWriteByte(ADDR_team1InGameTeamHeader, 0x00);
    swosWriteByte(ADDR_team2InGameTeamHeader, 0x00);
    swosGameTimeRestoreTeams();
    compareFullBuffer("save_and_restore_teams");

    swosMemoryInit(true);
    swosWriteWord(ADDR_secondLeg, 0);
    swosRngReseed(9);
    swosGameTimeInitGameVariables();
    compareFullBuffer("init_game_variables");

    swosMemoryInit(true);
    swosWriteWord(ADDR_secondLeg, 1);
    swosWriteWord(ADDR_team1GoalsFirstLeg, 2);
    swosWriteWord(ADDR_team2GoalsFirstLeg, 1);
    swosRngReseed(9);
    swosGameTimeInitGameVariables();
    compareFullBuffer("init_game_variables_second_leg");

    // ==== Referee -- the per-tick state machine ====
    swosMemoryInit(true);
    swosWriteWord(ADDR_foulXCoordinate, 300);
    swosWriteWord(ADDR_foulYCoordinate, 400);
    swosRefereeActivate();
    swosRefereeUpdateReferee();
    compareFullBuffer("referee_incoming_walk_tick");

    swosMemoryInit(true);
    swosWriteWord(ADDR_refState, 1);
    swosWriteWord(REFSPR_BASE + PLSPR_OFF_VISIBLE, 1);
    swosWriteWord(REFSPR_BASE + PLSPR_OFF_ON_SCREEN, 0);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosWriteWord(REFSPR_BASE + PLSPR_OFF_X + 2, 1900);
    swosWriteWord(REFSPR_BASE + PLSPR_OFF_Y + 2, 400);
    swosRefereeUpdateReferee();
    compareFullBuffer("referee_offscreen_game_in_progress_moves_toward_pitch");

    swosMemoryInit(true);
    swosWriteWord(ADDR_refState, 0);
    swosRefereeUpdateReferee();
    compareFullBuffer("referee_inactive_noop");

    swosMemoryInit(true);
    swosWriteWord(ADDR_refState, 3);
    swosWriteWord(REFSPR_BASE + PLSPR_OFF_VISIBLE, 1);
    swosWriteWord(REFSPR_BASE + PLSPR_OFF_ON_SCREEN, 1);
    swosWriteWord(ADDR_whichCard, 1);
    swosRefereeUpdateReferee();
    compareFullBuffer("referee_about_to_give_yellow_card_transitions_to_booking");

    swosMemoryInit(true);
    swosWriteWord(ADDR_refState, 3);
    swosWriteWord(REFSPR_BASE + PLSPR_OFF_VISIBLE, 1);
    swosWriteWord(REFSPR_BASE + PLSPR_OFF_ON_SCREEN, 1);
    swosWriteWord(ADDR_whichCard, 2);
    swosRefereeUpdateReferee();
    compareFullBuffer("referee_about_to_give_red_card_transitions_to_booking");

    swosMemoryInit(true);
    swosWriteWord(ADDR_refState, 5);
    swosWriteWord(REFSPR_BASE + PLSPR_OFF_VISIBLE, 1);
    swosWriteWord(REFSPR_BASE + PLSPR_OFF_ON_SCREEN, 0);
    swosWriteWord(ADDR_gameStatePl, 101);
    swosRefereeUpdateReferee();
    compareFullBuffer("referee_leaving_transitions_offscreen");

    swosMemoryInit(true);
    swosWriteWord(ADDR_whichCard, 1);
    swosWriteWord(ADDR_refState, 4);
    swosWriteDword(ADDR_bookedPlayer, (uint32_t)t1_2);
    swosWriteByte(t1_2 + PLSPR_OFF_PLAYER_STATE, 12);
    swosWriteWord(ADDR_refTimer, 0);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteDword(ADDR_lastTeamBooked, (uint32_t)top);
    swosWriteDword(top + TEAMDATA_OFF_IN_GAME_TEAM_PTR, K_TOP_TEAM_IN_GAME);
    swosPlayerSpriteSetPlayerOrdinal(2, 5);
    swosWriteByte(K_TOP_TEAM_IN_GAME + 42 + 4 * 61 + 3, 9);
    swosRefereeUpdateBookedPlayerNumberSprite();
    compareFullBuffer("referee_update_booked_player_number_sprite_blink_on");

    swosMemoryInit(true);
    swosWriteWord(ADDR_whichCard, 2);
    swosWriteWord(ADDR_refState, 4);
    swosWriteDword(ADDR_bookedPlayer, (uint32_t)t1_2);
    swosWriteByte(t1_2 + PLSPR_OFF_PLAYER_STATE, 12);
    swosWriteWord(ADDR_refTimer, 231);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteDword(ADDR_lastTeamBooked, (uint32_t)top);
    swosWriteDword(top + TEAMDATA_OFF_IN_GAME_TEAM_PTR, K_TOP_TEAM_IN_GAME);
    swosPlayerSpriteSetPlayerOrdinal(2, 3);
    swosRefereeUpdateBookedPlayerNumberSprite();
    compareFullBuffer("referee_update_booked_player_number_sprite_sentinel_sends_off");

    swosMemoryInit(true);
    swosWriteWord(ADDR_refState, 5);
    swosWriteWord(REFSPR_BASE + PLSPR_OFF_VISIBLE, 1);
    swosRefereeRemoveReferee();
    compareFullBuffer("referee_remove_referee");

    swosMemoryInit(true);
    swosWriteWord(ADDR_refState, 2);
    swosWriteWord(ADDR_whichCard, 1);
    (void)swosRefereeActive();
    (void)swosRefereeCardHandingInProgress();
    compareFullBuffer("referee_active_and_card_handing_accessors");

    // ==== Result -- scorer list + result-display timer ====
    swosMemoryInit(true);
    swosResultReset("Reds", "Blues");
    swosWriteDword(ADDR_resultTimer, SWOS_RESULT_END_OF_HALF);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosResultUpdateResult();
    compareFullBuffer("result_reset_and_show_lifecycle");

    swosMemoryInit(true);
    swosResultReset("", "");
    swosWriteDword(ADDR_resultTimer, 1);
    swosWriteWord(ADDR_lastFrameTicks, 5);
    swosWriteWord(ADDR_gameState, 25);
    swosResultUpdateResult();
    compareFullBuffer("result_countdown_hides_at_zero_halftime");

    swosMemoryInit(true);
    swosResultReset("", "");
    swosWriteDword(ADDR_resultTimer, (uint32_t)-1);
    swosResultUpdateResult();
    compareFullBuffer("result_negative_timer_hides_immediately");

    swosMemoryInit(true);
    swosResultReset("", "");
    swosWriteWord(ADDR_res_showResult, 1);
    (void)swosResultShouldDrawResult();
    swosResultHideResult();
    (void)swosResultShouldDrawResult();
    compareFullBuffer("result_hide_and_should_draw");

    swosMemoryInit(true);
    swosResultReset("", "");
    swosWriteDword(ADDR_topTeamInGame, K_TOP_TEAM_IN_GAME);
    swosWriteDword(ADDR_bottomTeamInGame, K_BOT_TEAM_IN_GAME);
    swosPlayerSpriteSetPlayerOrdinal(1, 1);
    swosWriteByte(K_TOP_TEAM_IN_GAME + 3, 7);
    swosWriteDword(ADDR_gt_gameTime + 1 * 4, 0);
    swosWriteDword(ADDR_gt_gameTime + 2 * 4, 3);
    swosWriteDword(ADDR_gt_gameTime + 3 * 4, 4);
    swosResultRegisterScorer(t1_1, 1, 0);
    compareFullBuffer("result_register_scorer_regular_goal");

    swosMemoryInit(true);
    swosResultReset("", "");
    swosWriteDword(ADDR_topTeamInGame, K_TOP_TEAM_IN_GAME);
    swosWriteDword(ADDR_bottomTeamInGame, K_BOT_TEAM_IN_GAME);
    swosPlayerSpriteSetPlayerOrdinal(1, 1);
    swosWriteByte(K_TOP_TEAM_IN_GAME + 3, 5);
    swosResultRegisterScorer(t1_1, 1, 2);
    compareFullBuffer("result_register_scorer_own_goal");

    swosMemoryInit(true);
    swosResultReset("", "");
    swosWriteDword(ADDR_topTeamInGame, K_TOP_TEAM_IN_GAME);
    swosWriteDword(ADDR_bottomTeamInGame, K_BOT_TEAM_IN_GAME);
    swosPlayerSpriteSetPlayerOrdinal(1, 1);
    swosWriteByte(K_TOP_TEAM_IN_GAME + 3, 9);
    swosResultRegisterScorer(t1_1, 1, 0);
    swosWriteDword(ADDR_gt_gameTime + 3 * 4, 55);
    swosResultRegisterScorer(t1_1, 1, 1);
    compareFullBuffer("result_register_scorer_second_goal_same_scorer");

    // ==== Integration scenarios ====
    swosMemoryInit(true);
    swosWriteWord(ADDR_gameState, 15);
    swosWriteWord(ADDR_resultTimer, 0);
    swosWriteWord(ADDR_foulXCoordinate, 590);
    swosWriteWord(ADDR_foulYCoordinate, 300);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)top);
    swosPlayerSpriteSetX(2, 585 << 16); swosPlayerSpriteSetY(2, 305 << 16);
    swosSetPiecesTickSetPieces();
    swosWriteByte(top + TEAMDATA_OFF_NORMAL_FIRE, 1);
    swosWriteByte(ADDR_playerTurnFlags, 0xFF);
    compareFullBuffer("integration_throw_in_full_cycle");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameState, 5);
    swosWriteWord(ADDR_resultTimer, 0);
    swosWriteWord(ADDR_foulXCoordinate, 585);
    swosWriteWord(ADDR_foulYCoordinate, 764);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)top);
    swosWriteWord(ADDR_cameraDirection, 6);
    swosSetPiecesTickSetPieces();
    swosWriteWord(ADDR_gameState, 13);
    swosSetPiecesTickFreeKick(top);
    compareFullBuffer("integration_corner_then_free_kick_direction_update");

    swosMemoryInit(true);
    swosWriteWord(ADDR_foulXCoordinate, 250);
    swosWriteWord(ADDR_foulYCoordinate, 600);
    swosWriteWord(ADDR_whichCard, 1);
    swosRefereeActivate();
    swosRefereeUpdateReferee();
    compareFullBuffer("integration_card_activates_referee_and_walks_in");

    swosMemoryInit(true);
    swosGameTimeResetGameTime();
    swosResultReset("", "");
    swosWriteWord(ADDR_gameStatePl, 100);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteDword(ADDR_gt_gameSeconds, (uint32_t)-1);
    swosWriteDword(ADDR_gt_endGameCounter, 0);
    swosWriteDword(ADDR_gt_gameTimeInMinutes, 45);
    swosWriteWord(ADDR_goalCounter, 0);
    swosGameTimeUpdateGameTime();
    swosWriteDword(ADDR_resultTimer, SWOS_RESULT_END_OF_HALF);
    swosResultUpdateResult();
    compareFullBuffer("integration_time_passes_to_halftime_and_result_shows");

    printf("\nchecked %d scenarios\n", g_checked);
    if (g_failures == 0) {
        printf("all %d checks passed\n", g_checked);
        return 0;
    }
    printf("%d of %d checks FAILED\n", g_failures, g_checked);
    return 1;
}
