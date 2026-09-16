// Step-11 (full Bench.cs port) differential test: compares FULL VM/Memory
// state against golden dumps produced by the REAL C# (tools/csharp-golden-
// dump/Step11BenchGolden.cs).
//
// Regenerate build/golden/s11bench_*.bin with:
//   cd tools/csharp-golden-dump && dotnet run -c Release -- ../../build/golden
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "swos_addr.h"
#include "swos_bench.h"
#include "swos_input_controls.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"
#include "swos_referee.h"
#include "swos_team_data.h"
#include "swos_team_data_loader.h"

#define GOLDEN_DIR "build/golden"

static int g_failures = 0;
static int g_checked = 0;

static void compareFullBuffer(const char *label) {
    char path[256];
    snprintf(path, sizeof(path), "%s/s11bench_%s.bin", GOLDEN_DIR, label);

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

static void wireTeams(int top, int bot) {
    swosWriteDword(ADDR_topTeamInGame, K_TOP_TEAM_IN_GAME);
    swosWriteDword(ADDR_bottomTeamInGame, K_BOT_TEAM_IN_GAME);
    swosWriteWord(top + TEAMDATA_OFF_TEAM_NUMBER, 1);
    swosWriteWord(bot + TEAMDATA_OFF_TEAM_NUMBER, 2);
    swosWriteDword(top + TEAMDATA_OFF_OPPONENTS_TEAM, (uint32_t)bot);
    swosWriteDword(bot + TEAMDATA_OFF_OPPONENTS_TEAM, (uint32_t)top);
}

int main(void) {
    int top = TEAMDATA_TOP_BASE, bot = TEAMDATA_BOTTOM_BASE;
    int t1_1 = swosPlayerSpriteBase(1);

    // ==== Simple accessors ====
    swosMemoryInit(true);
    swosBenchInitBenchControls();
    wireTeams(top, bot);
    (void)swosBenchInBench();
    (void)swosBenchGetBenchState();
    (void)swosBenchInBenchMenus();
    (void)swosBenchGetBenchY();
    (void)swosBenchGetOpponentBenchY();
    (void)swosBenchTrainingTopTeam();
    swosBenchSetTrainingTopTeam(true);
    (void)swosBenchGetBenchPlayerIndex();
    (void)swosBenchGetBenchMenuSelectedPlayer();
    (void)swosBenchGetSelectedFormationEntry();
    (void)swosBenchPlayerToEnterGameIndex();
    (void)swosBenchPlayerToBeSubstitutedIndex();
    (void)swosBenchPlayerToBeSubstitutedPos();
    (void)swosBenchGetBenchPlayerShirtNumber(true, 3);
    (void)swosBenchInBenchOrGoingTo();
    (void)swosBenchGoingToBenchDelay();
    (void)swosBenchSubstituteInProgress();
    (void)swosBenchNewPlayerAboutToGoIn();
    (void)swosBenchGetBenchTeamBase();
    (void)swosBenchGetBenchTeamGameBase();
    (void)swosBenchTeamIsTop();
    (void)swosBenchGetBenchPlayerInfoAddr(2);
    (void)swosBenchGetBenchPlayerPosition(2);
    compareFullBuffer("accessors_basic");

    swosMemoryInit(true);
    swosBenchInitBenchControls();
    swosBenchSetSubstituteInProgress();
    compareFullBuffer("set_substitute_in_progress");

    swosMemoryInit(true);
    swosBenchInitBenchControls();
    wireTeams(top, bot);
    swosWriteWord(ADDR_teamPlayingUp, 1);
    swosBenchInitBenchControls();
    swosWriteWord(top + TEAMDATA_OFF_TEAM_NUMBER, 9);
    (void)swosBenchGetBenchTeamBase();
    compareFullBuffer("get_bench_team_base_resyncs_on_team_number_mismatch");

    // ==== InitBenchBeforeMatch / InitBenchControls ====
    swosMemoryInit(true);
    swosBenchInitBenchControls();
    wireTeams(top, bot);
    swosWriteWord(ADDR_teamPlayingUp, 1);
    swosBenchInitBenchBeforeMatch();
    compareFullBuffer("init_bench_before_match_top_starts");

    swosMemoryInit(true);
    swosBenchInitBenchControls();
    wireTeams(top, bot);
    swosWriteWord(ADDR_teamPlayingUp, 2);
    swosWriteWord(ADDR_g_trainingGame, 1);
    swosBenchInitBenchBeforeMatch();
    compareFullBuffer("init_bench_before_match_bottom_starts_training");

    // ==== UpdateBench / BenchCheckControls -- out-of-bench polling ====
    swosMemoryInit(true);
    swosBenchInitBenchControls();
    wireTeams(top, bot);
    swosWriteWord(ADDR_g_waitForPlayerToGoInTimer, 5);
    swosBenchUpdateBench();
    compareFullBuffer("update_bench_blocked_by_wait_timer_ticks_down");

    swosMemoryInit(true);
    swosBenchInitBenchControls();
    wireTeams(top, bot);
    swosWriteWord(ADDR_refState, 2);
    swosBenchUpdateBench();
    compareFullBuffer("update_bench_blocked_by_referee_active");

    swosMemoryInit(true);
    swosBenchInitBenchControls();
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosBenchUpdateBench();
    compareFullBuffer("update_bench_unavailable_game_in_progress");

    swosMemoryInit(true);
    swosBenchInitBenchControls();
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 0);
    swosWriteWord(ADDR_gameState, 25);
    swosBenchUpdateBench();
    compareFullBuffer("update_bench_unavailable_ceremony_gamestate");

    swosMemoryInit(true);
    swosBenchInitBenchControls();
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 0);
    swosWriteWord(ADDR_gameState, 0);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_NUMBER, 0);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_COACH_NUMBER, 0);
    swosWriteWord(bot + TEAMDATA_OFF_PLAYER_NUMBER, 0);
    swosWriteWord(bot + TEAMDATA_OFF_PLAYER_COACH_NUMBER, 0);
    swosBenchUpdateBench();
    compareFullBuffer("update_bench_cpu_team_never_invokes");

    swosMemoryInit(true);
    swosBenchInitBenchControls();
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 0);
    swosWriteWord(ADDR_gameState, 0);
    swosBenchRequestBench1();
    swosBenchUpdateBench();
    compareFullBuffer("update_bench_invoked_by_secondary_fire");

    swosMemoryInit(true);
    swosBenchInitBenchControls();
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 0);
    swosWriteWord(ADDR_gameState, 0);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_NUMBER, 1);
    swosWriteWord(top + TEAMDATA_OFF_DIRECTION, 2);
    swosBenchUpdateBench();
    compareFullBuffer("update_bench_single_tap_does_not_invoke");

    swosMemoryInit(true);
    swosBenchInitBenchControls();
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 0);
    swosWriteWord(ADDR_gameState, 0);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_NUMBER, 1);
    for (int i = 0; i < 2; i++) {
        swosWriteWord(top + TEAMDATA_OFF_DIRECTION, 2);
        swosBenchUpdateBench();
        swosWriteWord(top + TEAMDATA_OFF_DIRECTION, (uint16_t)-1);
        swosBenchUpdateBench();
    }
    swosWriteWord(top + TEAMDATA_OFF_DIRECTION, 2);
    swosBenchUpdateBench();
    compareFullBuffer("update_bench_triple_tap_invokes");

    // ==== InvokeBench (direct) + throw-in cleanup ====
    swosMemoryInit(true);
    swosBenchInitBenchControls();
    wireTeams(top, bot);
    swosBenchInvokeBench();
    compareFullBuffer("invoke_bench_normal");

    swosMemoryInit(true);
    swosBenchInitBenchControls();
    wireTeams(top, bot);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)top);
    swosTeamDataSetControlledPlayer(true, t1_1);
    swosPlayerSpriteSetPlayerState(1, 5);
    swosBenchInvokeBench();
    compareFullBuffer("invoke_bench_clears_mid_throw_in");

    swosMemoryInit(true);
    swosBenchInitBenchControls();
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameState, 3);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)top);
    swosBenchInvokeBench();
    compareFullBuffer("invoke_bench_keeper_holds_ball_claims");

    // ==== In-bench menu navigation ====
    swosMemoryInit(true);
    swosBenchInitBenchControls();
    wireTeams(top, bot);
    swosWriteWord(ADDR_g_inSubstitutesMenu, 1);
    swosWriteWord(top + TEAMDATA_OFF_TEAM_NUMBER, 1);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_NUMBER, 1);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_COACH_NUMBER, 1);
    swosWriteWord(ADDR_team1NumSubs, 0);
    swosWriteDword(ADDR_ic_pl1Events, IC_EVENT_DOWN);
    swosBenchUpdateBench();
    compareFullBuffer("menu_arrow_navigation_down_then_up");

    swosMemoryInit(true);
    swosBenchInitBenchControls();
    wireTeams(top, bot);
    swosWriteWord(ADDR_g_inSubstitutesMenu, 1);
    swosWriteWord(top + TEAMDATA_OFF_TEAM_NUMBER, 1);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_NUMBER, 1);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_COACH_NUMBER, 1);
    swosWriteDword(ADDR_ic_pl1Events, IC_EVENT_KICK);
    swosBenchUpdateBench();
    compareFullBuffer("menu_fire_on_coach_row_enters_marking");

    swosMemoryInit(true);
    swosBenchInitBenchControls();
    wireTeams(top, bot);
    swosWriteWord(ADDR_g_inSubstitutesMenu, 1);
    swosWriteWord(top + TEAMDATA_OFF_TEAM_NUMBER, 1);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_NUMBER, 1);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_COACH_NUMBER, 1);
    swosWriteByte(K_TOP_TEAM_IN_GAME + 11 * TDL_PLAYER_INFO_SIZE + TDL_OFF_POSITION, 3);
    swosWriteByte(K_TOP_TEAM_IN_GAME + 5 * TDL_PLAYER_INFO_SIZE + TDL_OFF_POSITION, 3);
    swosWriteByte(K_TOP_TEAM_IN_GAME + 5 * TDL_PLAYER_INFO_SIZE + TDL_OFF_CARDS, 0);
    swosWriteDword(ADDR_ic_pl1Events, IC_EVENT_DOWN);
    swosBenchUpdateBench();
    swosWriteDword(ADDR_ic_pl1Events, 0);
    swosBenchUpdateBench();
    swosWriteDword(ADDR_ic_pl1Events, IC_EVENT_KICK);
    swosBenchUpdateBench();
    compareFullBuffer("menu_fire_on_substitute_row_enters_about_to_substitute");

    swosMemoryInit(true);
    swosBenchInitBenchControls();
    wireTeams(top, bot);
    swosWriteWord(ADDR_g_inSubstitutesMenu, 1);
    swosWriteWord(top + TEAMDATA_OFF_TEAM_NUMBER, 1);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_NUMBER, 1);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_COACH_NUMBER, 1);
    swosWriteWord(top + TEAMDATA_OFF_TACTICS, 0);
    swosWriteDword(ADDR_ic_pl1Events, IC_EVENT_KICK);
    swosBenchUpdateBench();
    swosWriteDword(ADDR_ic_pl1Events, 0);
    swosBenchUpdateBench();
    swosWriteDword(ADDR_ic_pl1Events, IC_EVENT_KICK);
    swosBenchUpdateBench();
    swosWriteDword(ADDR_ic_pl1Events, 0);
    swosBenchUpdateBench();
    swosWriteDword(ADDR_ic_pl1Events, IC_EVENT_DOWN);
    swosBenchUpdateBench();
    swosWriteDword(ADDR_ic_pl1Events, 0);
    swosBenchUpdateBench();
    swosWriteDword(ADDR_ic_pl1Events, IC_EVENT_KICK);
    swosBenchUpdateBench();
    compareFullBuffer("formation_menu_navigate_and_change_tactics");

    swosMemoryInit(true);
    swosBenchInitBenchControls();
    wireTeams(top, bot);
    swosWriteWord(ADDR_g_inSubstitutesMenu, 1);
    swosWriteWord(top + TEAMDATA_OFF_TEAM_NUMBER, 1);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_NUMBER, 1);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_COACH_NUMBER, 1);
    swosWriteDword(ADDR_ic_pl1Events, IC_EVENT_LEFT);
    swosBenchUpdateBench();
    compareFullBuffer("leave_bench_via_left_right_motion");

    // ==== InitiateSubstitution / SubstitutePlayer ====
    swosMemoryInit(true);
    swosBenchInitBenchControls();
    wireTeams(top, bot);
    swosWriteWord(ADDR_g_inSubstitutesMenu, 1);
    swosWriteWord(top + TEAMDATA_OFF_TEAM_NUMBER, 1);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_NUMBER, 1);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_COACH_NUMBER, 1);
    swosWriteByte(K_TOP_TEAM_IN_GAME + 11 * TDL_PLAYER_INFO_SIZE + TDL_OFF_POSITION, 3);
    swosWriteByte(K_TOP_TEAM_IN_GAME + 3 * TDL_PLAYER_INFO_SIZE + TDL_OFF_POSITION, 3);
    swosWriteByte(K_TOP_TEAM_IN_GAME + 3 * TDL_PLAYER_INFO_SIZE + TDL_OFF_CARDS, 0);
    swosWriteDword(ADDR_ic_pl1Events, IC_EVENT_DOWN);
    swosBenchUpdateBench();
    swosWriteDword(ADDR_ic_pl1Events, 0);
    swosBenchUpdateBench();
    swosWriteDword(ADDR_ic_pl1Events, IC_EVENT_KICK);
    swosBenchUpdateBench();
    swosWriteDword(ADDR_ic_pl1Events, 0);
    swosBenchUpdateBench();
    swosWriteDword(ADDR_ic_pl1Events, IC_EVENT_KICK);
    swosBenchUpdateBench();
    compareFullBuffer("initiate_substitution_via_menu");

    swosMemoryInit(true);
    swosBenchInitBenchControls();
    wireTeams(top, bot);
    swosWriteByte(K_TOP_TEAM_IN_GAME + 11 * TDL_PLAYER_INFO_SIZE + TDL_OFF_POSITION, 3);
    swosWriteByte(K_TOP_TEAM_IN_GAME + 11 * TDL_PLAYER_INFO_SIZE + TDL_OFF_FACE, 1);
    swosWriteByte(K_TOP_TEAM_IN_GAME + 3 * TDL_PLAYER_INFO_SIZE + TDL_OFF_POSITION, 3);
    swosWriteByte(K_TOP_TEAM_IN_GAME + 3 * TDL_PLAYER_INFO_SIZE + TDL_OFF_FACE, 2);
    swosPlayerSpriteSetPlayerOrdinal(4, 4);
    swosBenchRequestBench1();
    swosWriteWord(ADDR_gameStatePl, 0);
    swosWriteWord(ADDR_gameState, 0);
    swosWriteWord(top + TEAMDATA_OFF_TEAM_NUMBER, 1);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_NUMBER, 1);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_COACH_NUMBER, 1);
    swosBenchUpdateBench();
    swosWriteDword(ADDR_ic_pl1Events, IC_EVENT_DOWN);
    swosBenchUpdateBench();
    swosWriteDword(ADDR_ic_pl1Events, 0);
    swosBenchUpdateBench();
    swosWriteDword(ADDR_ic_pl1Events, IC_EVENT_KICK);
    swosBenchUpdateBench();
    swosWriteDword(ADDR_ic_pl1Events, 0);
    swosBenchUpdateBench();
    swosWriteDword(ADDR_ic_pl1Events, IC_EVENT_KICK);
    swosBenchUpdateBench();
    {
        int32_t outgoingSprite = swosReadSignedDword(ADDR_substitutedPlSprite);
        for (int i = 0; i < 4 && swosReadSignedWord(ADDR_g_substituteInProgress) != 0; i++) {
            int16_t destX = swosReadSignedWord(outgoingSprite + PLSPR_OFF_DEST_X);
            int16_t destY = swosReadSignedWord(outgoingSprite + PLSPR_OFF_DEST_Y);
            swosWriteWord(outgoingSprite + PLSPR_OFF_X + 2, (uint16_t)destX);
            swosWriteWord(outgoingSprite + PLSPR_OFF_Y + 2, (uint16_t)destY);
            swosWriteDword(outgoingSprite + PLSPR_OFF_DELTA_X, 0);
            swosWriteDword(outgoingSprite + PLSPR_OFF_DELTA_Y, 0);
            swosBenchUpdateBench();
        }
    }
    compareFullBuffer("substitute_player_swaps_records_and_sprites");

    // ==== UpdateSubstitutedPlayerWalk -- direct FSM steps ====
    swosMemoryInit(true);
    swosBenchInitBenchControls();
    wireTeams(top, bot);
    swosWriteWord(ADDR_g_substituteInProgress, 1);
    swosWriteDword(ADDR_substitutedPlSprite, (uint32_t)t1_1);
    swosWriteDword(ADDR_teamThatSubstitutes, (uint32_t)top);
    swosPlayerSpriteSetX(1, 200 << 16); swosPlayerSpriteSetY(1, 300 << 16);
    swosBenchUpdateBench();
    compareFullBuffer("walk_fsm_still_travelling_refreshes_dest");

    swosMemoryInit(true);
    swosBenchInitBenchControls();
    wireTeams(top, bot);
    swosWriteWord(ADDR_g_substituteInProgress, 1);
    swosWriteDword(ADDR_substitutedPlSprite, (uint32_t)t1_1);
    swosWriteDword(ADDR_teamThatSubstitutes, (uint32_t)top);
    swosWriteWord(t1_1 + PLSPR_OFF_INJURY_LEVEL, (uint16_t)-2);
    swosBenchUpdateBench();
    compareFullBuffer("walk_fsm_injured_stretchered_shortcuts_to_swap");

    swosMemoryInit(true);
    swosBenchInitBenchControls();
    wireTeams(top, bot);
    swosWriteWord(ADDR_g_substituteInProgress, (uint16_t)-1);
    swosWriteDword(ADDR_substitutedPlSprite, (uint32_t)t1_1);
    swosWriteDword(ADDR_teamThatSubstitutes, (uint32_t)top);
    swosPlayerSpriteSetDeltaX(1, 0); swosPlayerSpriteSetDeltaY(1, 0);
    swosBenchUpdateBench();
    compareFullBuffer("walk_fsm_settle_completes");

    // ==== CheckIfGoalkeeperClaimedTheBall -- both branches ====
    swosMemoryInit(true);
    swosBenchInitBenchControls();
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameState, 3);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)top);
    swosBenchCheckIfGoalkeeperClaimedTheBall();
    compareFullBuffer("check_goalkeeper_claimed_keeper_holds_branch");

    swosMemoryInit(true);
    swosBenchInitBenchControls();
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameState, 0);
    swosWriteWord(top + TEAMDATA_OFF_GOALKEEPER_PLAYING, 1);
    swosWriteWord(bot + TEAMDATA_OFF_GOALKEEPER_PLAYING, 1);
    swosBenchCheckIfGoalkeeperClaimedTheBall();
    compareFullBuffer("check_goalkeeper_claimed_stop_play_branch_top_bug_preserved");

    printf("\nchecked %d scenarios\n", g_checked);
    if (g_failures == 0) {
        printf("all %d checks passed\n", g_checked);
        return 0;
    }
    printf("%d of %d checks FAILED\n", g_failures, g_checked);
    return 1;
}
