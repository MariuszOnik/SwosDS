// Step-11A differential test: compares FULL VM/Memory state against golden
// dumps produced by the REAL C# (tools/csharp-golden-dump/Step11AGolden.cs).
// Covers the real local dependencies of GameLoop.cs discovered by its
// comment-filtered dependency scan: Kickoff.cs (minimal slice), Camera.cs,
// GameSprites.cs, SpinningLogo.cs, PlayerNameDisplay.cs, Stats.cs, and the
// Bench.InBenchMenus/GetBenchState extension.
//
// Regenerate build/golden/s11a_*.bin with:
//   cd tools/csharp-golden-dump && dotnet run -c Release -- ../../build/golden
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "swos_addr.h"
#include "swos_ball_sprite.h"
#include "swos_bench.h"
#include "swos_camera.h"
#include "swos_game_sprites.h"
#include "swos_kickoff.h"
#include "swos_memory.h"
#include "swos_player_name_display.h"
#include "swos_player_sprite.h"
#include "swos_rng.h"
#include "swos_spinning_logo.h"
#include "swos_stats.h"
#include "swos_team_data.h"

#define GOLDEN_DIR "build/golden"

static int g_failures = 0;
static int g_checked = 0;

static void compareFullBuffer(const char *label) {
    char path[256];
    snprintf(path, sizeof(path), "%s/s11a_%s.bin", GOLDEN_DIR, label);

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

#define K_TOP_TEAM_IN_GAME 0x4FE60
#define K_BOT_TEAM_IN_GAME 0x4FEA0

int main(void) {
    int top = TEAMDATA_TOP_BASE, bot = TEAMDATA_BOTTOM_BASE;
    int t1_1 = swosPlayerSpriteBase(1);
    int t1_2 = swosPlayerSpriteBase(2);
    int t2_1 = swosPlayerSpriteBase(12);

    // ==== Kickoff ====
    swosMemoryInit(true);
    swosWriteWord(ADDR_teamStarting, 1);
    swosWriteWord(ADDR_teamPlayingUp, 1);
    swosKickoffPrepareForInitialKick();
    compareFullBuffer("kickoff_prepare_top_starts");

    swosMemoryInit(true);
    swosWriteWord(ADDR_teamStarting, 2);
    swosWriteWord(ADDR_teamPlayingUp, 1);
    swosKickoffPrepareForInitialKick();
    compareFullBuffer("kickoff_prepare_bottom_starts");

    swosMemoryInit(true);
    swosWriteDword(top + TEAMDATA_OFF_IN_GAME_TEAM_PTR, K_TOP_TEAM_IN_GAME);
    swosWriteDword(bot + TEAMDATA_OFF_IN_GAME_TEAM_PTR, K_BOT_TEAM_IN_GAME);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_NUMBER, 1);
    swosWriteWord(bot + TEAMDATA_OFF_PLAYER_NUMBER, 0);
    swosWriteWord(top + TEAMDATA_OFF_TEAM_NUMBER, 1);
    swosWriteWord(bot + TEAMDATA_OFF_TEAM_NUMBER, 2);
    swosKickoffReseatTeamsForNewHalf();
    compareFullBuffer("kickoff_reseat_teams_for_new_half");

    // ==== Camera ====
    swosMemoryInit(true);
    swosCameraSetX(100 << 16);
    swosCameraSetY(200 << 16);
    (void)swosCameraGetX(); (void)swosCameraGetY();
    (void)swosCameraGetXWhole(); (void)swosCameraGetYWhole();
    compareFullBuffer("camera_set_and_get_xy");

    swosMemoryInit(true);
    swosCameraSetX(50 << 16); swosCameraSetY(50 << 16);
    swosWriteWord(ADDR_showFansCounter, 5);
    swosCameraMoveCamera();
    compareFullBuffer("camera_move_fans_counter_early_out");

    swosMemoryInit(true);
    swosRngReseed(3);
    swosCameraMoveCamera();
    compareFullBuffer("camera_move_self_heal_zero_zero");

    swosMemoryInit(true);
    swosCameraSetX(176 << 16); swosCameraSetY(16 << 16);
    swosWriteWord(ADDR_whichCard, 1);
    swosWriteDword(ADDR_bookedPlayer, (uint32_t)t1_1);
    swosPlayerSpriteSetX(1, 300 << 16); swosPlayerSpriteSetY(1, 400 << 16);
    swosCameraMoveCamera();
    compareFullBuffer("camera_move_booking_mode");

    swosMemoryInit(true);
    swosCameraSetX(176 << 16); swosCameraSetY(16 << 16);
    swosWriteWord(ADDR_playingPenalties, 1);
    swosCameraMoveCamera();
    compareFullBuffer("camera_move_penalty_shootout_mode");

    swosMemoryInit(true);
    swosCameraSetX(176 << 16); swosCameraSetY(16 << 16);
    swosWriteWord(ADDR_g_waitForPlayerToGoInTimer, 5);
    swosCameraMoveCamera();
    compareFullBuffer("camera_move_bench_mode_substituting");

    swosMemoryInit(true);
    swosCameraSetX(30 << 16); swosCameraSetY(16 << 16);
    swosWriteWord(ADDR_leavingBenchMode, 1);
    swosCameraMoveCamera();
    compareFullBuffer("camera_move_leaving_bench_mode");

    swosMemoryInit(true);
    swosCameraSetX(176 << 16); swosCameraSetY(16 << 16);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosWriteWord(ADDR_gameState, 100);
    swosBallSpriteSetXPixels(300); swosBallSpriteSetYPixels(400);
    swosBallSpriteSetDeltaX(5 << 16); swosBallSpriteSetDeltaY(-3 << 16);
    swosCameraMoveCamera();
    compareFullBuffer("camera_move_standard_in_progress_follow_ball");

    swosMemoryInit(true);
    swosCameraSetX(176 << 16); swosCameraSetY(16 << 16);
    swosWriteWord(ADDR_gameStatePl, 0);
    swosWriteWord(ADDR_gameState, 21);
    swosCameraMoveCamera();
    compareFullBuffer("camera_move_standard_stopped_waiting_for_players");

    swosMemoryInit(true);
    swosCameraSetX(176 << 16); swosCameraSetY(16 << 16);
    swosWriteWord(ADDR_gameStatePl, 0);
    swosWriteWord(ADDR_gameState, 26);
    swosCameraMoveCamera();
    compareFullBuffer("camera_move_standard_result_after_game");

    swosMemoryInit(true);
    swosCameraSetX(176 << 16); swosCameraSetY(16 << 16);
    swosWriteWord(ADDR_gameStatePl, 0);
    swosWriteWord(ADDR_gameState, 30);
    swosWriteWord(ADDR_penaltiesState, (uint16_t)-1);
    swosCameraMoveCamera();
    compareFullBuffer("camera_move_standard_game_ended_penalties_unresolved");

    swosMemoryInit(true);
    swosRngReseed(3);
    swosCameraSetToInitialPosition();
    compareFullBuffer("camera_set_to_initial_position_bottom");

    swosMemoryInit(true);
    swosCameraSwitchToLeavingBenchMode();
    compareFullBuffer("camera_switch_to_leaving_bench_mode");

    // ==== GameSprites ====
    swosMemoryInit(true);
    swosWriteDword(ADDR_frameCounter, 40);
    swosGameSpritesUpdateCornerFlags();
    compareFullBuffer("game_sprites_update_corner_flags");

    swosMemoryInit(true);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_NUMBER, 0);
    swosWriteWord(top + TEAMDATA_OFF_IS_PL_COACH, 0);
    swosWriteWord(ADDR_g_trainingGame, 0);
    swosGameSpritesUpdateControlledPlayerNumbers();
    compareFullBuffer("game_sprites_controlled_numbers_gate_fails");

    swosMemoryInit(true);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_NUMBER, 1);
    swosTeamDataSetControlledPlayer(true, t1_2);
    swosPlayerSpriteSetPlayerOrdinal(2, 3);
    swosPlayerSpriteSetX(2, 250 << 16); swosPlayerSpriteSetY(2, 350 << 16); swosPlayerSpriteSetZ(2, 0);
    swosWriteDword(top + TEAMDATA_OFF_IN_GAME_TEAM_PTR, K_TOP_TEAM_IN_GAME);
    swosWriteWord(K_TOP_TEAM_IN_GAME - 22, (uint16_t)-1);
    swosWriteByte(K_TOP_TEAM_IN_GAME + 2 * 61 + 3, 9);
    swosWriteWord(ADDR_currentGameTick, 0);
    swosGameSpritesUpdateControlledPlayerNumbers();
    compareFullBuffer("game_sprites_controlled_numbers_shows_digit");

    swosMemoryInit(true);
    swosWriteWord(bot + TEAMDATA_OFF_PLAYER_NUMBER, 1);
    swosTeamDataSetControlledPlayer(false, t2_1);
    swosPlayerSpriteSetPlayerOrdinal(12, 1);
    swosWriteDword(bot + TEAMDATA_OFF_IN_GAME_TEAM_PTR, K_BOT_TEAM_IN_GAME);
    swosWriteWord(K_BOT_TEAM_IN_GAME - 22, 0);
    swosWriteWord(ADDR_currentGameTick, 0x10);
    swosGameSpritesUpdateControlledPlayerNumbers();
    compareFullBuffer("game_sprites_controlled_numbers_marked_player_hidden");

    swosMemoryInit(true);
    (void)swosGameSpritesGetPlayerSpriteOffsetFromFace(2);
    (void)swosGameSpritesGetPlayerSpriteOffsetFromFace(-1);
    (void)swosGameSpritesGetPlayerSpriteOffsetFromFace(9);
    (void)swosGameSpritesGetGoalkeeperSpriteOffset(true, 3);
    compareFullBuffer("game_sprites_face_offset_helpers");

    // ==== SpinningLogo ====
    swosMemoryInit(true);
    swosSpinningLogoSetEnabled(false);
    swosSpinningLogoUpdateSpinningLogo();
    compareFullBuffer("spinning_logo_disabled");

    swosMemoryInit(true);
    swosSpinningLogoSetEnabled(true);
    swosWriteWord(ADDR_currentGameTick, 2);
    swosSpinningLogoUpdateSpinningLogo();
    compareFullBuffer("spinning_logo_enabled_spinning_advances");

    swosMemoryInit(true);
    swosSpinningLogoSetEnabled(true);
    swosWriteWord(ADDR_currentGameTick, 2);
    swosWriteWord(ADDR_g_inSubstitutesMenu, 1);
    swosWriteWord(ADDR_m_benchState, 0);
    swosSpinningLogoUpdateSpinningLogo();
    compareFullBuffer("spinning_logo_enabled_but_bench_menus_no_advance");

    // ==== PlayerNameDisplay ====
    swosMemoryInit(true);
    swosWriteDword(ADDR_currentScorer, (uint32_t)t1_1);
    swosWriteDword(ADDR_lastTeamScored, (uint32_t)top);
    swosWriteDword(ADDR_topTeamInGame, K_TOP_TEAM_IN_GAME);
    swosWriteDword(top + TEAMDATA_OFF_IN_GAME_TEAM_PTR, K_TOP_TEAM_IN_GAME);
    swosPlayerSpriteSetPlayerOrdinal(1, 4);
    swosWriteWord(ADDR_currentGameTick, 8);
    swosPlayerNameDisplayUpdateCurrentPlayerName();
    compareFullBuffer("player_name_display_scorer_blinking_shown");

    swosMemoryInit(true);
    swosWriteWord(ADDR_whichCard, 1);
    swosWriteDword(ADDR_bookedPlayer, (uint32_t)t1_1);
    swosWriteDword(ADDR_lastTeamBooked, (uint32_t)top);
    swosWriteWord(ADDR_currentGameTick, 0);
    swosPlayerNameDisplayUpdateCurrentPlayerName();
    compareFullBuffer("player_name_display_card_blinking_hidden");

    swosMemoryInit(true);
    swosWriteDword(ADDR_lastPlayerBeforeGoalkeeper, (uint32_t)t1_1);
    swosWriteDword(ADDR_lastTeamScored, (uint32_t)top);
    swosWriteDword(ADDR_topTeamInGame, K_TOP_TEAM_IN_GAME);
    swosWriteDword(top + TEAMDATA_OFF_IN_GAME_TEAM_PTR, K_TOP_TEAM_IN_GAME);
    swosPlayerSpriteSetPlayerOrdinal(1, 2);
    swosWriteWord(ADDR_nobodysBallTimer, 10);
    swosPlayerNameDisplayUpdateCurrentPlayerName();
    compareFullBuffer("player_name_display_prolong_last_before_goalkeeper");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosWriteDword(ADDR_lastPlayerPlayed, (uint32_t)t1_1);
    swosWriteDword(ADDR_lastTeamPlayed, (uint32_t)top);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_HAS_BALL, 1);
    swosWriteDword(ADDR_topTeamInGame, K_TOP_TEAM_IN_GAME);
    swosWriteDword(top + TEAMDATA_OFF_IN_GAME_TEAM_PTR, K_TOP_TEAM_IN_GAME);
    swosPlayerSpriteSetPlayerOrdinal(1, 3);
    swosWriteWord(ADDR_nobodysBallTimer, 0);
    swosPlayerNameDisplayUpdateCurrentPlayerName();
    compareFullBuffer("player_name_display_in_progress_resets_and_shows");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosWriteDword(ADDR_lastPlayerPlayed, 0);
    swosPlayerNameDisplayUpdateCurrentPlayerName();
    compareFullBuffer("player_name_display_in_progress_no_player_hides");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 0);
    swosWriteDword(ADDR_lastPlayerPlayed, (uint32_t)t1_1);
    swosWriteDword(ADDR_lastTeamPlayed, (uint32_t)top);
    swosTeamDataSetControlledPlayer(true, t1_1);
    swosWriteWord(ADDR_pnd_nobodysBallLastFrame, 3);
    swosPlayerNameDisplayUpdateCurrentPlayerName();
    compareFullBuffer("player_name_display_stopped_hides_first_frame");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 0);
    swosWriteDword(ADDR_lastPlayerPlayed, (uint32_t)t1_1);
    swosWriteDword(ADDR_lastTeamPlayed, (uint32_t)top);
    swosTeamDataSetControlledPlayer(true, t1_1);
    swosWriteWord(ADDR_pnd_nobodysBallLastFrame, 0);
    swosWriteDword(ADDR_topTeamInGame, K_TOP_TEAM_IN_GAME);
    swosWriteDword(top + TEAMDATA_OFF_IN_GAME_TEAM_PTR, K_TOP_TEAM_IN_GAME);
    swosPlayerSpriteSetPlayerOrdinal(1, 5);
    swosPlayerNameDisplayUpdateCurrentPlayerName();
    compareFullBuffer("player_name_display_stopped_prolongs");

    // ==== Stats ====
    swosMemoryInit(true);
    swosWriteWord(ADDR_st_isGoalAttempt, 1);
    swosWriteWord(ADDR_st_showStats, 1);
    swosStatsInitStats();
    compareFullBuffer("stats_init");

    swosMemoryInit(true);
    swosWriteWord(ADDR_st_showingUserRequestedStats, 0);
    swosWriteDword(ADDR_resultTimer, 500);
    swosStatsToggleStats();
    compareFullBuffer("stats_toggle_shows_from_hidden");

    swosMemoryInit(true);
    swosWriteWord(ADDR_st_showingUserRequestedStats, 1);
    swosStatsToggleStats();
    compareFullBuffer("stats_toggle_hides_when_user_requested");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosWriteDword(ADDR_lastTeamPlayed, (uint32_t)top);
    swosBallSpriteSetYPixels(300);
    swosStatsUpdateStatistics();
    compareFullBuffer("stats_update_bumps_possession");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosWriteDword(ADDR_lastTeamPlayed, (uint32_t)top);
    swosWriteWord(ADDR_ballInGoalkeeperArea, 1);
    swosBallSpriteSetY(200 << 16);
    swosBallSpriteSetDeltaY(5 << 16);
    swosWriteWord(bot + TEAMDATA_OFF_PLAYER_HAS_BALL, 0);
    swosWriteWord(ADDR_strikeDestX, 330);
    swosStatsUpdateStatistics();
    compareFullBuffer("stats_update_registers_goal_attempt");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosWriteWord(ADDR_playingPenalties, 1);
    swosWriteDword(ADDR_lastTeamPlayed, (uint32_t)top);
    swosStatsUpdateStatistics();
    compareFullBuffer("stats_update_penalties_skip_bump");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 0);
    swosWriteWord(ADDR_statsTimer, (uint16_t)-1);
    swosWriteWord(ADDR_st_showStats, 1);
    swosStatsUpdateStatistics();
    compareFullBuffer("stats_check_timer_auto_hides");

    // ==== Bench.InBenchMenus extension ====
    swosMemoryInit(true);
    swosWriteWord(ADDR_g_inSubstitutesMenu, 1);
    swosWriteWord(ADDR_m_benchState, 0);
    (void)swosBenchInBenchMenus();
    compareFullBuffer("bench_in_bench_menus_true");

    swosMemoryInit(true);
    swosWriteWord(ADDR_g_inSubstitutesMenu, 1);
    swosWriteWord(ADDR_m_benchState, 2);
    (void)swosBenchInBenchMenus();
    compareFullBuffer("bench_in_bench_menus_false_wrong_state");

    printf("\nchecked %d scenarios\n", g_checked);
    if (g_failures == 0) {
        printf("all %d checks passed\n", g_checked);
        return 0;
    }
    printf("%d of %d checks FAILED\n", g_failures, g_checked);
    return 1;
}
