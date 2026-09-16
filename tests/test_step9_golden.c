// Step-9 differential test: compares FULL VM/Memory state against golden
// dumps produced by the REAL C# (tools/csharp-golden-dump/Step9Golden.cs).
// Covers AiHelpers.cs, AiBrain.SetControlsDirection, and the CPU-team
// branches in swosRunControlledBranch/swosUpdatePlayersUpdate that were
// previously gated behind assert-backed hooks and are now real.
//
// RNG: AiBrain.SetControlsDirection draws exactly one Rng byte per call
// (AI_rand). Every scenario below re-seeds with the SAME seed the C# side
// used (see Step9Golden.cs's header comment for why: Rng state is separate
// from Memory and Memory.Init()'s own internal reseed, while deterministic,
// isn't scenario-controlled) immediately after swosMemoryInit(true).
//
// Regenerate build/golden/s9_*.bin with:
//   cd tools/csharp-golden-dump && dotnet run -c Release -- ../../build/golden
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "swos_addr.h"
#include "swos_ai_brain.h"
#include "swos_ai_helpers.h"
#include "swos_ball_sprite.h"
#include "swos_memory.h"
#include "swos_player_controlled.h"
#include "swos_player_sprite.h"
#include "swos_rng.h"
#include "swos_team_data.h"
#include "swos_update_players.h"

#define GOLDEN_DIR "build/golden"

static int g_failures = 0;
static int g_checked = 0;

static void compareFullBuffer(const char *label) {
    char path[256];
    snprintf(path, sizeof(path), "%s/s9_%s.bin", GOLDEN_DIR, label);

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

// Common two-controlled-player baseline several scenarios build on.
static void baseSetup(int t1_1, int t2_1) {
    swosTeamDataSetControlledPlayer(true, t1_1);
    swosTeamDataSetControlledPlayer(false, t2_1);
    swosPlayerSpriteSetX(1, 300 << 16); swosPlayerSpriteSetY(1, 400 << 16);
    swosPlayerSpriteSetX(12, 350 << 16); swosPlayerSpriteSetY(12, 450 << 16);
    swosBallSpriteSetXPixels(320); swosBallSpriteSetYPixels(420);
}

int main(void) {
    int top = TEAMDATA_TOP_BASE, bot = TEAMDATA_BOTTOM_BASE;
    int t1_1 = swosPlayerSpriteBase(1);
    int t1_2 = swosPlayerSpriteBase(2);
    int t2_1 = swosPlayerSpriteBase(12);

    // ---- top-level gates ----
    swosMemoryInit(true);
    swosWriteWord(top + TEAMDATA_OFF_RESET_CONTROLS, 1);
    swosAiBrainSetControlsDirection(top);
    compareFullBuffer("reset_controls_gate");

    swosMemoryInit(true);
    swosWriteWord(ADDR_g_inSubstitutesMenu, 1);
    swosAiBrainSetControlsDirection(top);
    compareFullBuffer("subs_menu_gate");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 0);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)bot);
    swosAiBrainSetControlsDirection(top);
    compareFullBuffer("not_in_progress_not_last_team");

    // ---- game-over result-showing auto-fire ----
    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 0);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)top);
    swosWriteWord(ADDR_gameState, 25);
    swosWriteWord(ADDR_team1Computer, 1);
    swosWriteWord(ADDR_team2Computer, 1);
    swosWriteWord(ADDR_stoppageTimerTotal, 500);
    swosWriteWord(ADDR_m_clearResultInterval, 100);
    swosAiBrainSetControlsDirection(top);
    compareFullBuffer("game_over_result_halftime");

    // ---- game-not-over: stoppage set-piece dispatch ----
    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 0);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)top);
    swosWriteWord(ADDR_gameState, 3);
    swosWriteWord(ADDR_stoppageTimerActive, 0);
    baseSetup(t1_1, t2_1);
    swosRngReseed(20);
    swosAiBrainSetControlsDirection(top);
    compareFullBuffer("game_not_over_keeper_holds_ball");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 0);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)top);
    swosWriteWord(ADDR_gameState, 0);
    swosWriteWord(ADDR_stoppageTimerActive, 200);
    baseSetup(t1_1, t2_1);
    swosAiBrainSetControlsDirection(top);
    compareFullBuffer("game_not_over_goal_scored");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 0);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)top);
    swosWriteWord(ADDR_gameState, 15);
    swosWriteWord(ADDR_stoppageTimerActive, 0);
    baseSetup(t1_1, t2_1);
    swosRngReseed(20);
    swosAiBrainSetControlsDirection(top);
    compareFullBuffer("game_not_over_throw_in");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 0);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)top);
    swosWriteWord(ADDR_gameState, 6);
    swosWriteWord(ADDR_stoppageTimerActive, 0);
    baseSetup(t1_1, t2_1);
    swosRngReseed(24);
    swosAiBrainSetControlsDirection(top);
    compareFullBuffer("game_not_over_free_kick");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 0);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)top);
    swosWriteWord(ADDR_playingPenalties, 1);
    swosWriteWord(ADDR_stoppageTimerActive, 0);
    baseSetup(t1_1, t2_1);
    swosRngReseed(20);
    swosAiBrainSetControlsDirection(top);
    compareFullBuffer("game_not_over_penalties");

    // ---- game-in-progress: penalty/spin-timer fast path ----
    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosWriteWord(ADDR_penalty, 1);
    swosTeamDataSetSpinTimer(true, 3);
    baseSetup(t1_1, t2_1);
    swosRngReseed(20);
    swosAiBrainSetControlsDirection(top);
    compareFullBuffer("game_in_progress_penalty_spin_timer");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosBallSpriteSetXPixels(320); swosBallSpriteSetYPixels(420);
    swosRngReseed(20);
    swosAiBrainSetControlsDirection(top);
    compareFullBuffer("game_in_progress_no_controlled_player");

    // ---- game-in-progress: player-near fire decision ----
    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 100);
    baseSetup(t1_1, t2_1);
    swosWriteByte(top + TEAMDATA_OFF_PL_VERY_CLOSE_TO_BALL, 1);
    swosPlayerSpriteSetDirection(1, 4);
    swosPlayerSpriteSetBallDistance(1, 100);
    swosBallSpriteSetDeltaZ(1000);
    swosBallSpriteSetZPixels(10);
    swosRngReseed(20);
    swosAiBrainSetControlsDirection(top);
    compareFullBuffer("game_in_progress_player_near_fires");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 100);
    baseSetup(t1_1, t2_1);
    swosWriteByte(top + TEAMDATA_OFF_PL_VERY_CLOSE_TO_BALL, 1);
    swosPlayerSpriteSetDirection(1, 0);
    swosRngReseed(20);
    swosAiBrainSetControlsDirection(top);
    compareFullBuffer("game_in_progress_player_near_no_fire_chase");

    // ---- chase success ----
    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 100);
    baseSetup(t1_1, t2_1);
    swosWriteByte(top + TEAMDATA_OFF_PL_CLOSE_TO_BALL, 1);
    swosPlayerSpriteSetDirection(1, 2);
    swosWriteWord(top + TEAMDATA_OFF_PASS_KICK_TIMER, 5);
    swosWriteWord(ADDR_AI_resumePlayTimer, 0);
    swosRngReseed(20);
    swosAiBrainSetControlsDirection(top);
    compareFullBuffer("our_player_closest_chase_success");

    // ---- no-one near ----
    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosTeamDataSetControlledPlayer(true, t1_1);
    swosWriteDword(top + TEAMDATA_OFF_PASS_TO_PLAYER_PTR, (uint32_t)t1_2);
    swosPlayerSpriteSetX(1, 300 << 16); swosPlayerSpriteSetY(1, 400 << 16);
    swosPlayerSpriteSetBallDistance(1, 5000);
    swosPlayerSpriteSetX(2, 305 << 16); swosPlayerSpriteSetY(2, 405 << 16);
    swosPlayerSpriteSetBallDistance(2, 100);
    swosBallSpriteSetXPixels(305); swosBallSpriteSetYPixels(405);
    swosRngReseed(20);
    swosAiBrainSetControlsDirection(top);
    compareFullBuffer("game_in_progress_noone_near_reassign");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosTeamDataSetControlledPlayer(true, t1_1);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_NUMBER, 0);
    swosWriteWord(bot + TEAMDATA_OFF_PLAYER_NUMBER, 0);
    swosPlayerSpriteSetX(1, 300 << 16); swosPlayerSpriteSetY(1, 400 << 16);
    swosBallSpriteSetXPixels(320); swosBallSpriteSetYPixels(420);
    swosRngReseed(20);
    swosAiBrainSetControlsDirection(top);
    compareFullBuffer("game_in_progress_noone_near_random_flip");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosTeamDataSetControlledPlayer(true, t1_1);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_NUMBER, 1);
    swosPlayerSpriteSetX(1, 300 << 16); swosPlayerSpriteSetY(1, 400 << 16);
    swosPlayerSpriteSetDirection(1, 3);
    swosBallSpriteSetXPixels(320); swosBallSpriteSetYPixels(420);
    swosRngReseed(20);
    swosAiBrainSetControlsDirection(top);
    compareFullBuffer("game_in_progress_noone_near_use_current_direction");

    // ---- l_ball_after_touch_allowed ----
    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 100);
    baseSetup(t1_1, t2_1);
    swosTeamDataSetSpinTimer(true, 3);
    swosWriteWord(top + TEAMDATA_OFF_AI_BALL_SPIN_DIRECTION, (uint16_t)(int16_t)-1);
    swosWriteWord(top + TEAMDATA_OFF_CONTROLLED_PL_DIRECTION, 3);
    swosRngReseed(20);
    swosAiBrainSetControlsDirection(top);
    compareFullBuffer("ball_after_touch_left_spin");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 100);
    baseSetup(t1_1, t2_1);
    swosTeamDataSetSpinTimer(true, 3);
    swosWriteWord(top + TEAMDATA_OFF_AI_BALL_SPIN_DIRECTION, 1);
    swosWriteWord(top + TEAMDATA_OFF_CONTROLLED_PL_DIRECTION, 3);
    swosRngReseed(20);
    swosAiBrainSetControlsDirection(top);
    compareFullBuffer("ball_after_touch_right_spin");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 100);
    baseSetup(t1_1, t2_1);
    swosTeamDataSetSpinTimer(true, 3);
    swosWriteWord(top + TEAMDATA_OFF_AI_BALL_SPIN_DIRECTION, 0);
    swosWriteWord(top + TEAMDATA_OFF_AI_AFTER_TOUCH_STRENGTH, 1);
    swosWriteWord(top + TEAMDATA_OFF_CONTROLLED_PL_DIRECTION, 3);
    swosRngReseed(24);
    swosAiBrainSetControlsDirection(top);
    compareFullBuffer("ball_after_touch_no_spin_medium_strength");

    // ---- AiHelpers standalone ----
    swosMemoryInit(true);
    swosWriteDword(top + TEAMDATA_OFF_OPPONENTS_TEAM, (uint32_t)bot);
    swosWriteWord(bot + TEAMDATA_OFF_PLAYER_HAS_BALL, 1);
    swosWriteDword(bot + TEAMDATA_OFF_CONTROLLED_PLAYER, (uint32_t)t2_1);
    swosPlayerSpriteSetBallDistance(1, 50);
    swosPlayerSpriteSetDirection(1, 0);
    swosWriteWord(t2_1 + TEAMDATA_OFF_ALLOWED_DIRECTIONS, 0);
    swosAiHelpersAiKick(t1_1, top);
    compareFullBuffer("ai_helpers_kick_direct");

    swosMemoryInit(true);
    swosWriteWord(ADDR_AI_counter, 5);
    swosWriteWord(ADDR_AI_attackHalf, 2);
    swosBallSpriteSetXPixels(200);
    swosAiHelpersSetDirectionTowardOpponentsGoal(top);
    compareFullBuffer("ai_helpers_set_direction_toward_goal");

    swosMemoryInit(true);
    swosPlayerSpriteSetPlayerOrdinal(1, 4);
    swosPlayerSpriteSetBallDistance(1, 100);
    swosBallSpriteSetDeltaZ(1000); swosBallSpriteSetZPixels(10);
    swosAiHelpersDecideWhetherToTriggerFire(4, t1_1, top);
    compareFullBuffer("ai_helpers_decide_fire_true");

    swosMemoryInit(true);
    swosPlayerSpriteSetPlayerOrdinal(1, 4);
    swosPlayerSpriteSetBallDistance(1, 5000);
    swosAiHelpersDecideWhetherToTriggerFire(4, t1_1, top);
    compareFullBuffer("ai_helpers_decide_fire_false");

    // ---- CPU-team integration ----
    swosMemoryInit(true);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_NUMBER, 0);
    swosTeamDataSetControlledPlayer(true, t1_1);
    baseSetup(t1_1, t2_1);
    swosRngReseed(20);
    swosRunControlledBranch(t1_1, true);
    compareFullBuffer("player_controlled_cpu_team_uses_real_ai");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosWriteWord(ADDR_gameState, 100);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_NUMBER, 0);
    swosWriteWord(bot + TEAMDATA_OFF_PLAYER_NUMBER, 0);
    swosBallSpriteSetXPixels(320); swosBallSpriteSetYPixels(420);
    swosRngReseed(20);
    swosUpdatePlayersUpdate(0);
    compareFullBuffer("update_players_cpu_off_ball_real_ai");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 0);
    swosWriteWord(ADDR_gameState, 0);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_NUMBER, 0);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)top);
    swosWriteDword(top + TEAMDATA_OFF_PASS_TO_PLAYER_PTR, (uint32_t)t1_2);
    swosPlayerSpriteSetX(2, 305 << 16); swosPlayerSpriteSetY(2, 405 << 16);
    swosBallSpriteSetXPixels(305); swosBallSpriteSetYPixels(405);
    swosWriteWord(top + TEAMDATA_OFF_UPDATE_PLAYER_INDEX, 1);
    swosRngReseed(20);
    swosUpdatePlayersUpdate(0);
    compareFullBuffer("update_players_cpu_stoppage_ai_kick");

    printf("\nchecked %d scenarios\n", g_checked);
    if (g_failures == 0) {
        printf("all %d checks passed\n", g_checked);
        return 0;
    }
    printf("%d of %d checks FAILED\n", g_failures, g_checked);
    return 1;
}
