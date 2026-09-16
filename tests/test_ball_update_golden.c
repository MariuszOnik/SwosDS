// Step-4 differential test, per review request: compares FULL VM/Memory
// state (not just a function's return value) against golden dumps produced
// by the REAL C# (tools/csharp-golden-dump/BallUpdateGolden.cs). Covers
// BallUpdate.cs + its forward-pulled dependencies (PlayerUpdate.
// UpdateBallWithControllingGoalkeeper, BallOutOfPlay.CheckIfBallOutOfPlay,
// UpdateGoals.BumpTeamGoals/GoalScored) -- friction, bounces, heights,
// ground/post contact, keeper-holds-ball Z handling, goal scoring, out-of-
// play dispatch, and ApplyBallAfterTouch's spin/kick-boost/long-pass paths.
//
// Regenerate build/golden/ball_*.bin with:
//   cd tools/csharp-golden-dump && dotnet run -c Release -- ../../build/golden
//
// RegisterScorer/MatchAudio are deliberately out of scope for both sides of
// this comparison -- see swos_update_goals.h's PORT_PENDING note and
// swos_ball_update.h's MatchAudio-omission note. Each scenario here mirrors
// BallUpdateGolden.cs's setup exactly, by name.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "swos_addr.h"
#include "swos_ball_sprite.h"
#include "swos_ball_update.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"
#include "swos_team_data.h"

#define GOLDEN_DIR "build/golden"

static int g_failures = 0;
static int g_checked = 0;

static void compareFullBuffer(const char *label) {
    char path[256];
    snprintf(path, sizeof(path), "%s/ball_%s.bin", GOLDEN_DIR, label);

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

int main(void) {
    // ---- Friction ----
    swosMemoryInit(true);
    swosTeamDataSetPlayerHasBall(true, 1);
    swosBallSpriteSetSpeed(100);
    swosBallSpriteSetDestX(400); swosBallSpriteSetDestY(400);
    swosBallSpriteSetXPixels(336); swosBallSpriteSetYPixels(449);
    swosBallUpdateTick();
    compareFullBuffer("friction_ground_with_possession");

    swosMemoryInit(true);
    swosWriteWord(ADDR_pitchBallSpeedFactor, 5);
    swosBallSpriteSetSpeed(100);
    swosBallSpriteSetDestX(400); swosBallSpriteSetDestY(400);
    swosBallSpriteSetXPixels(336); swosBallSpriteSetYPixels(449);
    swosBallUpdateTick();
    compareFullBuffer("friction_free_ball_pitch_factor");

    swosMemoryInit(true);
    swosBallSpriteSetSpeed(500);
    swosBallSpriteSetZ(20 << 16);
    swosBallSpriteSetDestX(400); swosBallSpriteSetDestY(400);
    swosBallSpriteSetXPixels(336); swosBallSpriteSetYPixels(449);
    swosBallUpdateTick();
    compareFullBuffer("friction_air");

    swosMemoryInit(true);
    swosBallSpriteSetSpeed(2);
    swosBallSpriteSetDestX(336); swosBallSpriteSetDestY(449);
    swosBallSpriteSetXPixels(336); swosBallSpriteSetYPixels(449);
    swosBallUpdateTick();
    compareFullBuffer("friction_clamps_to_zero");

    // ---- Heights / bounce ----
    swosMemoryInit(true);
    swosBallSpriteSetXPixels(336); swosBallSpriteSetYPixels(449);
    swosBallSpriteSetZ(1 << 16);
    swosBallSpriteSetDeltaZ(-0x00030000);
    swosBallSpriteSetSpeed(300);
    swosBallUpdateTick();
    compareFullBuffer("bounce_small_settles");

    swosMemoryInit(true);
    swosBallSpriteSetXPixels(336); swosBallSpriteSetYPixels(449);
    swosBallSpriteSetZ(2 << 16);
    swosBallSpriteSetDeltaZ(-0x00300000);
    swosBallSpriteSetSpeed(2000);
    swosBallUpdateTick();
    compareFullBuffer("bounce_loud");

    swosMemoryInit(true);
    swosBallSpriteSetXPixels(336); swosBallSpriteSetYPixels(449);
    swosBallSpriteSetZ(200 << 16);
    swosBallSpriteSetDeltaZ(0x00100000);
    swosBallSpriteSetSpeed(2688);
    swosBallUpdateTick();
    compareFullBuffer("extreme_high_z_gravity_only");

    // ---- Keeper holds ball ----
    swosMemoryInit(true);
    swosWriteWord(ADDR_gameState, 3);
    swosBallSpriteSetZPixels(5);
    swosBallSpriteSetSpeed(0);
    swosBallUpdateTick();
    compareFullBuffer("keeper_holds_ball_at_hand_height");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameState, 3);
    swosBallSpriteSetZPixels(20);
    swosBallSpriteSetSpeed(0);
    swosBallUpdateTick();
    compareFullBuffer("keeper_holds_ball_above_hand");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameState, 3);
    swosBallSpriteSetZPixels(2);
    swosBallSpriteSetSpeed(50);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)TEAMDATA_TOP_BASE);
    {
        int keeperBase = swosPlayerSpriteBase(PLSPR_SLOT_GOALIE1);
        swosTeamDataSetControlledPlayer(true, keeperBase);
        swosPlayerSpriteSetDirection(PLSPR_SLOT_GOALIE1, 2);
        swosPlayerSpriteSetX(PLSPR_SLOT_GOALIE1, 300 << 16);
        swosPlayerSpriteSetY(PLSPR_SLOT_GOALIE1, 150 << 16);
    }
    swosBallUpdateTick();
    compareFullBuffer("keeper_holds_ball_below_hand_with_controller");

    // ---- Ground/post contact: X/Y barriers ----
    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 0);
    swosBallSpriteSetXPixels(40);
    swosBallSpriteSetYPixels(400);
    swosBallSpriteSetDestX(10); swosBallSpriteSetDestY(400);
    swosBallSpriteSetSpeed(800);
    swosBallUpdateTick();
    compareFullBuffer("x_barrier_bounce_left");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 0);
    swosBallSpriteSetXPixels(300);
    swosBallSpriteSetYPixels(820);
    swosBallSpriteSetDestX(300); swosBallSpriteSetDestY(850);
    swosBallSpriteSetSpeed(800);
    swosBallUpdateTick();
    compareFullBuffer("y_barrier_bounce_bottom");

    // ---- Goal scoring ----
    swosMemoryInit(true);
    swosBallSpriteSetXPixels(336);
    swosBallSpriteSetYPixels(780);
    swosBallSpriteSetZPixels(10);
    swosBallSpriteSetDeltaY(30000);
    swosWriteDword(ADDR_lastPlayerPlayed, (uint32_t)swosPlayerSpriteBase(1));
    swosBallUpdateTick();
    compareFullBuffer("goal_scored_lower_net");

    swosMemoryInit(true);
    swosBallSpriteSetXPixels(336);
    swosBallSpriteSetYPixels(115);
    swosBallSpriteSetZPixels(5);
    swosBallSpriteSetDeltaY(-30000);
    swosWriteDword(ADDR_lastPlayerPlayed, (uint32_t)swosPlayerSpriteBase(12));
    swosBallUpdateTick();
    compareFullBuffer("goal_scored_upper_net");

    swosMemoryInit(true);
    swosBallSpriteSetXPixels(336);
    swosBallSpriteSetYPixels(128);
    swosBallSpriteSetZPixels(18);
    swosBallSpriteSetDeltaY(1000);
    swosBallSpriteSetDeltaZ(5000);
    swosBallUpdateTick();
    compareFullBuffer("penalty_bar_deflect");

    // ---- Out of play ----
    swosMemoryInit(true);
    swosBallSpriteSetXPixels(90);
    swosBallSpriteSetYPixels(120);
    swosWriteDword(ADDR_lastTeamPlayed, (uint32_t)TEAMDATA_TOP_BASE);
    swosBallUpdateTick();
    compareFullBuffer("corner_left_upper");

    swosMemoryInit(true);
    swosBallSpriteSetXPixels(75);
    swosBallSpriteSetYPixels(400);
    swosWriteDword(ADDR_lastTeamPlayed, (uint32_t)TEAMDATA_BOTTOM_BASE);
    swosBallUpdateTick();
    compareFullBuffer("throw_in_left_half");

    swosMemoryInit(true);
    swosBallSpriteSetXPixels(600);
    swosBallSpriteSetYPixels(400);
    swosWriteDword(ADDR_lastTeamPlayed, (uint32_t)TEAMDATA_TOP_BASE);
    swosBallUpdateTick();
    compareFullBuffer("throw_in_right_half");

    swosMemoryInit(true);
    swosBallSpriteSetXPixels(336);
    swosBallSpriteSetYPixels(100);
    swosBallUpdateTick();
    compareFullBuffer("goal_out_upper");

    // ---- ApplyBallAfterTouch ----
    swosMemoryInit(true);
    swosTeamDataSetSpinTimer(true, 1);
    swosTeamDataSetCurrentAllowedDirection(true, 0);
    swosTeamDataSetControlledPlDirection(true, 2);
    swosBallSpriteSetDestX(300); swosBallSpriteSetDestY(400);
    swosApplyBallAfterTouch(true);
    compareFullBuffer("aftertouch_left_spin");

    swosMemoryInit(true);
    swosTeamDataSetSpinTimer(true, 1);
    swosTeamDataSetCurrentAllowedDirection(true, 0);
    swosTeamDataSetControlledPlDirection(true, 6);
    swosBallSpriteSetDestX(300); swosBallSpriteSetDestY(400);
    swosApplyBallAfterTouch(true);
    compareFullBuffer("aftertouch_right_spin");

    swosMemoryInit(true);
    swosTeamDataSetSpinTimer(true, 4);
    swosTeamDataSetCurrentAllowedDirection(true, 0);
    swosTeamDataSetControlledPlDirection(true, 4);
    swosBallSpriteSetDestX(300); swosBallSpriteSetDestY(400);
    swosApplyBallAfterTouch(true);
    compareFullBuffer("aftertouch_tick4_high_kick");

    swosMemoryInit(true);
    swosTeamDataSetSpinTimer(true, 4);
    swosTeamDataSetCurrentAllowedDirection(true, 0);
    swosTeamDataSetControlledPlDirection(true, 2);
    swosBallSpriteSetDestX(300); swosBallSpriteSetDestY(400);
    swosApplyBallAfterTouch(true);
    compareFullBuffer("aftertouch_tick4_normal_kick");

    swosMemoryInit(true);
    swosTeamDataSetPassInProgress(true, 1);
    swosTeamDataSetSpinTimer(true, 1);
    swosTeamDataSetCurrentAllowedDirection(true, 0);
    swosTeamDataSetControlledPlDirection(true, 2);
    swosBallSpriteSetDirection(0);
    swosBallSpriteSetSpeed(1000);
    swosBallSpriteSetDestX(300); swosBallSpriteSetDestY(400);
    swosApplyBallAfterTouch(true);
    compareFullBuffer("aftertouch_pass_long_pass_boost");

    swosMemoryInit(true);
    swosTeamDataSetSpinTimer(true, 9);
    swosTeamDataSetCurrentAllowedDirection(true, -1);
    swosBallSpriteSetDestX(300); swosBallSpriteSetDestY(400);
    swosApplyBallAfterTouch(true);
    compareFullBuffer("aftertouch_spin_timer_expires");

    printf("\nchecked %d scenarios\n", g_checked);
    if (g_failures) {
        printf("%d check(s) FAILED\n", g_failures);
        return 1;
    }
    printf("all %d checks passed\n", g_checked);
    return 0;
}
