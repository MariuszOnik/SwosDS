// Step-5.5 differential test: compares FULL VM/Memory state against golden
// dumps produced by the REAL C# (tools/csharp-golden-dump/
// PlayerUpdateGolden.cs). Covers the rest of PlayerUpdate.cs
// (UpdateBallWithControllingGoalkeeper was already covered in step 4's
// test_ball_update_golden.c). Each scenario here mirrors
// PlayerUpdateGolden.cs's setup exactly, by name.
//
// Regenerate build/golden/pu_*.bin with:
//   cd tools/csharp-golden-dump && dotnet run -c Release -- ../../build/golden
//
// MatchAudio/telemetry (keeper-dive counters)/the Godot debug print are
// deliberately out of scope for both sides -- see swos_player_update.h.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "swos_addr.h"
#include "swos_ball_sprite.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"
#include "swos_player_update.h"
#include "swos_rng.h"
#include "swos_team_data.h"
#include "swos_team_data_loader.h"

#define GOLDEN_DIR "build/golden"

// Same scratch addresses as PlayerUpdateGolden.cs.
#define TOP_PLAYER_INFO      0x4FE60
#define BOTTOM_PLAYER_INFO   0x4FEA0
#define TOP_SHOT_CHANCE      0x4FEE0
#define BOTTOM_SHOT_CHANCE   0x4FF20

static int g_failures = 0;
static int g_checked = 0;

static void compareFullBuffer(const char *label) {
    char path[256];
    snprintf(path, sizeof(path), "%s/pu_%s.bin", GOLDEN_DIR, label);

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

static void wirePlayerInfo(bool top, int passing, int shooting, int heading,
                            int tackling, int ballControl, int speed, int finishing,
                            int goalieSkill) {
    int teamBase = top ? TEAMDATA_TOP_BASE : TEAMDATA_BOTTOM_BASE;
    int piBase = top ? TOP_PLAYER_INFO : BOTTOM_PLAYER_INFO;
    swosWriteDword(teamBase + TEAMDATA_OFF_IN_GAME_TEAM_PTR, (uint32_t)piBase);
    swosWriteByte(piBase + TDL_OFF_PASSING, passing);
    swosWriteByte(piBase + TDL_OFF_SHOOTING, shooting);
    swosWriteByte(piBase + TDL_OFF_HEADING, heading);
    swosWriteByte(piBase + TDL_OFF_TACKLING, tackling);
    swosWriteByte(piBase + TDL_OFF_BALL_CONTROL, ballControl);
    swosWriteByte(piBase + TDL_OFF_SPEED, speed);
    swosWriteByte(piBase + TDL_OFF_FINISHING, finishing);
    swosWriteByte(piBase + TDL_OFF_GOALIE_SKILL, goalieSkill);
}

static void wireShotChanceTable(bool top, int16_t entry10, int16_t entry48,
                                 int16_t thresh52, int16_t thresh54) {
    int teamBase = top ? TEAMDATA_TOP_BASE : TEAMDATA_BOTTOM_BASE;
    int scBase = top ? TOP_SHOT_CHANCE : BOTTOM_SHOT_CHANCE;
    swosWriteDword(teamBase + TEAMDATA_OFF_SHOT_CHANCE_TABLE, (uint32_t)scBase);
    swosWriteWord(scBase + 10, (uint16_t)entry10);
    swosWriteWord(scBase + 48, (uint16_t)entry48);
    swosWriteWord(scBase + 52, (uint16_t)thresh52);
    swosWriteWord(scBase + 54, (uint16_t)thresh54);
}

int main(void) {
    int keeper1 = swosPlayerSpriteBase(PLSPR_SLOT_GOALIE1);
    int keeper2 = swosPlayerSpriteBase(PLSPR_SLOT_GOALIE2);

    // ---- GoalkeeperClaimedTheBall ----
    swosMemoryInit(true);
    swosBallSpriteSetXPixels(336); swosBallSpriteSetYPixels(780);
    swosPlayerSpriteSetDirection(PLSPR_SLOT_GOALIE1, 2);
    swosGoalkeeperClaimedTheBall(keeper1, true);
    compareFullBuffer("claimed_normal_top");

    swosMemoryInit(true);
    swosTeamDataSetGoalkeeperDivingRight(true, 1);
    swosBallSpriteSetXPixels(336); swosBallSpriteSetYPixels(780);
    swosGoalkeeperClaimedTheBall(keeper1, true);
    compareFullBuffer("claimed_mid_dive_early_out");

    swosMemoryInit(true);
    swosWriteWord(TEAMDATA_BOTTOM_BASE + TEAMDATA_OFF_PLAYER_NUMBER, 0);
    swosBallSpriteSetXPixels(336); swosBallSpriteSetYPixels(129);
    swosGoalkeeperClaimedTheBall(keeper2, false);
    compareFullBuffer("claimed_cpu_clears_ew_flags");

    // ---- TickGoalieDivingClaimCompletion ----
    swosMemoryInit(true);
    swosPlayerSpriteSetPlayerState(PLSPR_SLOT_GOALIE1, 0);
    swosTickGoalieDivingClaimCompletion(keeper1, true);
    compareFullBuffer("dive_claim_not_diving_state");

    swosMemoryInit(true);
    swosPlayerSpriteSetPlayerState(PLSPR_SLOT_GOALIE1, 6);
    swosTeamDataSetGoalkeeperDivingRight(true, 0);
    swosTickGoalieDivingClaimCompletion(keeper1, true);
    compareFullBuffer("dive_claim_flag_not_set");

    swosMemoryInit(true);
    swosPlayerSpriteSetPlayerState(PLSPR_SLOT_GOALIE1, 7);
    swosTeamDataSetGoalkeeperDivingRight(true, 1);
    swosPlayerSpriteSetPlayerDownTimer(PLSPR_SLOT_GOALIE1, 50);
    swosTickGoalieDivingClaimCompletion(keeper1, true);
    compareFullBuffer("dive_claim_too_early");

    swosMemoryInit(true);
    swosPlayerSpriteSetPlayerState(PLSPR_SLOT_GOALIE1, 6);
    swosTeamDataSetGoalkeeperDivingRight(true, 1);
    swosPlayerSpriteSetPlayerDownTimer(PLSPR_SLOT_GOALIE1, 1);
    swosTickGoalieDivingClaimCompletion(keeper1, true);
    compareFullBuffer("dive_claim_rise_path");

    swosMemoryInit(true);
    swosPlayerSpriteSetPlayerState(PLSPR_SLOT_GOALIE1, 6);
    swosTeamDataSetGoalkeeperDivingRight(true, 1);
    swosPlayerSpriteSetPlayerDownTimer(PLSPR_SLOT_GOALIE1, 20);
    swosPlayerSpriteSetX(PLSPR_SLOT_GOALIE1, 336 << 16);
    swosPlayerSpriteSetY(PLSPR_SLOT_GOALIE1, 780 << 16);
    swosTickGoalieDivingClaimCompletion(keeper1, true);
    compareFullBuffer("dive_claim_completes");

    // ---- GoalkeeperCaughtTheBall ----
    swosMemoryInit(true);
    swosWriteWord(ADDR_ballNextGroundX, 300);
    swosWriteWord(ADDR_ballNextGroundY, 100);
    swosGoalkeeperCaughtTheBall(keeper1, true);
    compareFullBuffer("caught_clamp_low");

    swosMemoryInit(true);
    swosWriteWord(ADDR_ballNextGroundX, 300);
    swosWriteWord(ADDR_ballNextGroundY, 800);
    swosGoalkeeperCaughtTheBall(keeper2, false);
    compareFullBuffer("caught_clamp_high");

    swosMemoryInit(true);
    swosWriteWord(ADDR_ballNextGroundX, 300);
    swosWriteWord(ADDR_ballNextGroundY, 400);
    swosGoalkeeperCaughtTheBall(keeper1, true);
    compareFullBuffer("caught_no_clamp");

    // ---- TickGoalieCatchingBall ----
    swosMemoryInit(true);
    swosPlayerSpriteSetPlayerDownTimer(PLSPR_SLOT_GOALIE1, 10);
    swosTeamDataSetGoalkeeperDivingLeft(true, 1);
    swosTickGoalieCatchingBall(keeper1, true);
    compareFullBuffer("catching_still_diving_left_skip");

    swosMemoryInit(true);
    swosPlayerSpriteSetPlayerDownTimer(PLSPR_SLOT_GOALIE1, 10);
    swosTeamDataSetGoalkeeperDivingLeft(true, 0);
    swosWriteByte(TEAMDATA_TOP_BASE + 61, 0);
    swosTickGoalieCatchingBall(keeper1, true);
    compareFullBuffer("catching_not_close_skip");

    swosMemoryInit(true);
    swosPlayerSpriteSetPlayerDownTimer(PLSPR_SLOT_GOALIE1, 10);
    swosTeamDataSetGoalkeeperDivingLeft(true, 0);
    swosWriteByte(TEAMDATA_TOP_BASE + 61, 1);
    swosWriteWord(ADDR_currentGameTick, 0);
    wireShotChanceTable(true, 0, 5, 0, 0);
    swosTickGoalieCatchingBall(keeper1, true);
    compareFullBuffer("catching_close_catch");

    swosMemoryInit(true);
    swosPlayerSpriteSetPlayerDownTimer(PLSPR_SLOT_GOALIE1, 10);
    swosTeamDataSetGoalkeeperDivingLeft(true, 0);
    swosWriteByte(TEAMDATA_TOP_BASE + 61, 1);
    swosWriteWord(ADDR_currentGameTick, 0xF0);
    wireShotChanceTable(true, 0, -5, 0, 0);
    swosTickGoalieCatchingBall(keeper1, true);
    compareFullBuffer("catching_close_deflect");

    swosMemoryInit(true);
    swosPlayerSpriteSetPlayerDownTimer(PLSPR_SLOT_GOALIE1, 1);
    swosTeamDataSetGoalkeeperDivingLeft(true, 0);
    swosTickGoalieCatchingBall(keeper1, true);
    compareFullBuffer("catching_timer_zero_no_diving");

    swosMemoryInit(true);
    swosPlayerSpriteSetPlayerDownTimer(PLSPR_SLOT_GOALIE1, 1);
    swosTeamDataSetGoalkeeperDivingLeft(true, 1);
    swosBallSpriteSetXPixels(336); swosBallSpriteSetYPixels(780);
    swosTickGoalieCatchingBall(keeper1, true);
    compareFullBuffer("catching_timer_zero_with_diving");

    // ---- TickGoalieClaimed ----
    swosMemoryInit(true);
    swosPlayerSpriteSetPlayerDownTimer(PLSPR_SLOT_GOALIE1, 5);
    swosTickGoalieClaimed(keeper1);
    compareFullBuffer("claimed_state_still");

    swosMemoryInit(true);
    swosPlayerSpriteSetPlayerDownTimer(PLSPR_SLOT_GOALIE1, 1);
    swosTickGoalieClaimed(keeper1);
    compareFullBuffer("claimed_state_finishes");

    // ---- TickGoalkeeperHoldAutoRelease ----
    swosMemoryInit(true);
    swosWriteWord(ADDR_gameState, 5);
    swosTickGoalkeeperHoldAutoRelease(keeper1, true);
    compareFullBuffer("hold_release_not_gs3");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameState, 3);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)TEAMDATA_BOTTOM_BASE);
    swosTickGoalkeeperHoldAutoRelease(keeper1, true);
    compareFullBuffer("hold_release_wrong_team");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameState, 3);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)TEAMDATA_TOP_BASE);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PLAYER_NUMBER, 1);
    swosWriteWord(ADDR_stoppageTimerActive, 400);
    swosTickGoalkeeperHoldAutoRelease(keeper1, true);
    compareFullBuffer("hold_release_human_never");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameState, 3);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)TEAMDATA_TOP_BASE);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PLAYER_NUMBER, 0);
    swosWriteWord(ADDR_stoppageTimerActive, 100);
    swosTickGoalkeeperHoldAutoRelease(keeper1, true);
    compareFullBuffer("hold_release_cpu_not_yet");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameState, 3);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)TEAMDATA_TOP_BASE);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PLAYER_NUMBER, 0);
    swosWriteWord(ADDR_stoppageTimerActive, 350);
    swosTeamDataSetControlledPlayer(true, keeper1);
    swosPlayerSpriteSetDirection(PLSPR_SLOT_GOALIE1, 3);
    swosBallSpriteSetXPixels(336); swosBallSpriteSetYPixels(780);
    swosTickGoalkeeperHoldAutoRelease(keeper1, true);
    compareFullBuffer("hold_release_cpu_fires");

    swosMemoryInit(true);
    swosPlayerSpriteSetPlayerState(PLSPR_SLOT_GOALIE1, 6);
    swosTeamDataSetGoalkeeperDivingRight(true, 1);
    swosPlayerSpriteSetPlayerDownTimer(PLSPR_SLOT_GOALIE1, 20);
    swosPlayerSpriteSetX(PLSPR_SLOT_GOALIE1, 336 << 16);
    swosPlayerSpriteSetY(PLSPR_SLOT_GOALIE1, 780 << 16);
    swosTickGoalkeeperHoldAutoRelease(keeper1, true);
    compareFullBuffer("hold_release_dive_completion_delegate");

    // ---- ShouldGoalkeeperDive ----
    swosMemoryInit(true);
    swosBallSpriteSetYPixels(300);
    swosPlayerSpriteSetYPixels(PLSPR_SLOT_GOALIE1, 100);
    swosShouldGoalkeeperDive(keeper1, BALLSPR_BASE, TEAMDATA_TOP_BASE);
    compareFullBuffer("should_dive_behind_too_far");

    swosMemoryInit(true);
    swosBallSpriteSetYPixels(105);
    swosPlayerSpriteSetYPixels(PLSPR_SLOT_GOALIE1, 100);
    swosShouldGoalkeeperDive(keeper1, BALLSPR_BASE, TEAMDATA_TOP_BASE);
    compareFullBuffer("should_dive_behind_close_try");

    swosMemoryInit(true);
    swosWriteWord(ADDR_kKeeperSaveDistance, 50);
    swosBallSpriteSetYPixels(100);
    swosPlayerSpriteSetYPixels(PLSPR_SLOT_GOALIE1, 300);
    swosShouldGoalkeeperDive(keeper1, BALLSPR_BASE, TEAMDATA_TOP_BASE);
    compareFullBuffer("should_dive_front_too_far");

    swosMemoryInit(true);
    swosBallSpriteSetYPixels(90);
    swosPlayerSpriteSetYPixels(PLSPR_SLOT_GOALIE1, 100);
    swosBallSpriteSetDeltaY(-2000);
    swosPlayerSpriteSetX(PLSPR_SLOT_GOALIE1, 300 << 16);
    swosWriteWord(ADDR_ballDefensiveX, 320);
    swosPlayerSpriteSetDeltaX(PLSPR_SLOT_GOALIE1, 3000);
    wireShotChanceTable(true, 3, 0, 0, 0);
    swosShouldGoalkeeperDive(keeper1, BALLSPR_BASE, TEAMDATA_TOP_BASE);
    compareFullBuffer("should_dive_front_succeeds");

    swosMemoryInit(true);
    swosRngReseed(3);
    swosWriteWord(ADDR_penalty, 1);
    swosBallSpriteSetYPixels(90);
    swosPlayerSpriteSetYPixels(PLSPR_SLOT_GOALIE1, 100);
    swosShouldGoalkeeperDive(keeper1, BALLSPR_BASE, TEAMDATA_TOP_BASE);
    compareFullBuffer("should_dive_penalty");

    // ---- GoalkeeperJumping ----
    swosMemoryInit(true);
    swosPlayerSpriteSetBallDistance(PLSPR_SLOT_GOALIE1, 50);
    swosWriteWord(ADDR_ballNotHighZ, 2);
    swosPlayerSpriteSetX(PLSPR_SLOT_GOALIE1, 300 << 16);
    swosPlayerSpriteSetY(PLSPR_SLOT_GOALIE1, 400 << 16);
    swosGoalkeeperJumping(2, 0, 2, keeper1, BALLSPR_BASE, TEAMDATA_TOP_BASE);
    compareFullBuffer("jumping_near_low");

    swosMemoryInit(true);
    swosPlayerSpriteSetBallDistance(PLSPR_SLOT_GOALIE2, 500);
    swosWriteWord(ADDR_ballNotHighZ, 10);
    swosPlayerSpriteSetX(PLSPR_SLOT_GOALIE2, 300 << 16);
    swosPlayerSpriteSetY(PLSPR_SLOT_GOALIE2, 400 << 16);
    swosGoalkeeperJumping(6, 0, 6, keeper2, BALLSPR_BASE, TEAMDATA_BOTTOM_BASE);
    compareFullBuffer("jumping_far_bottom_high");

    swosMemoryInit(true);
    swosPlayerSpriteSetBallDistance(PLSPR_SLOT_GOALIE1, 500);
    swosWriteWord(ADDR_ballNotHighZ, 2);
    swosPlayerSpriteSetX(PLSPR_SLOT_GOALIE1, 300 << 16);
    swosPlayerSpriteSetY(PLSPR_SLOT_GOALIE1, 400 << 16);
    swosGoalkeeperJumping(6, 1, 6, keeper1, BALLSPR_BASE, TEAMDATA_TOP_BASE);
    compareFullBuffer("jumping_far_slower");

    swosMemoryInit(true);
    swosPlayerSpriteSetBallDistance(PLSPR_SLOT_GOALIE1, 500);
    swosWriteWord(ADDR_ballNotHighZ, 2);
    swosWriteWord(ADDR_currentGameTick, 123);
    swosPlayerSpriteSetX(PLSPR_SLOT_GOALIE1, 300 << 16);
    swosPlayerSpriteSetY(PLSPR_SLOT_GOALIE1, 400 << 16);
    swosGoalkeeperJumping(6, 2, 6, keeper1, BALLSPR_BASE, TEAMDATA_TOP_BASE);
    compareFullBuffer("jumping_far_random");

    // ---- GoalkeeperDeflectedBall ----
    swosMemoryInit(true);
    swosBallSpriteSetXPixels(336); swosBallSpriteSetYPixels(780);
    swosWriteWord(ADDR_currentGameTick, 40);
    swosGoalkeeperDeflectedBall(BALLSPR_BASE, TEAMDATA_TOP_BASE);
    compareFullBuffer("deflect_default_weak");

    swosMemoryInit(true);
    swosBallSpriteSetXPixels(336); swosBallSpriteSetYPixels(780);
    swosWriteWord(ADDR_currentGameTick, 40);
    wireShotChanceTable(true, 0, 0, -100, -100);
    swosGoalkeeperDeflectedBall(BALLSPR_BASE, TEAMDATA_TOP_BASE);
    compareFullBuffer("deflect_strong_with_table");

    // ---- RunShotTripWire ----
    swosMemoryInit(true);
    swosTeamDataSetPlayerHasBall(false, 1);
    swosRunShotTripWire(keeper1, BALLSPR_BASE, TEAMDATA_TOP_BASE, true);
    compareFullBuffer("trip_wire_opponent_has_ball");

    swosMemoryInit(true);
    swosWriteWord(ADDR_kShotAtGoalMinumumSpeed, 100);
    swosBallSpriteSetSpeed(500);
    swosRunShotTripWire(keeper1, BALLSPR_BASE, TEAMDATA_TOP_BASE, true);
    compareFullBuffer("trip_wire_shot_at_goal_fast_ball");

    swosMemoryInit(true);
    swosWriteWord(ADDR_kShotAtGoalMinumumSpeed, 900);
    swosBallSpriteSetSpeed(100);
    swosPlayerSpriteSetBallDistance(PLSPR_SLOT_GOALIE1, 9000);
    swosRunShotTripWire(keeper1, BALLSPR_BASE, TEAMDATA_TOP_BASE, true);
    compareFullBuffer("trip_wire_ball_far_c7fc01");

    swosMemoryInit(true);
    swosWriteWord(ADDR_kShotAtGoalMinumumSpeed, 900);
    swosBallSpriteSetSpeed(100);
    swosPlayerSpriteSetBallDistance(PLSPR_SLOT_GOALIE1, 100);
    swosBallSpriteSetZPixels(5);
    swosBallSpriteSetDeltaZ(0x8000);
    swosRunShotTripWire(keeper1, BALLSPR_BASE, TEAMDATA_TOP_BASE, true);
    compareFullBuffer("trip_wire_deltaZ_eq_0x8000");

    // ---- RunShotAtGoal ----
    swosMemoryInit(true);
    swosBallSpriteSetZPixels(20);
    swosRunShotAtGoal(keeper1, BALLSPR_BASE, TEAMDATA_TOP_BASE, true);
    compareFullBuffer("shot_at_goal_too_high");

    swosMemoryInit(true);
    swosBallSpriteSetZPixels(5);
    swosPlayerSpriteSetBallDistance(PLSPR_SLOT_GOALIE1, 9000);
    swosBallSpriteSetYPixels(300);
    swosPlayerSpriteSetYPixels(PLSPR_SLOT_GOALIE1, 400);
    swosRunShotAtGoal(keeper1, BALLSPR_BASE, TEAMDATA_TOP_BASE, true);
    compareFullBuffer("shot_at_goal_forced_saved_far_distance");

    swosMemoryInit(true);
    swosBallSpriteSetZPixels(5);
    swosPlayerSpriteSetBallDistance(PLSPR_SLOT_GOALIE1, 9000);
    swosWriteByte(TEAMDATA_TOP_BASE + TEAMDATA_OFF_BALL_ABOVE_17, 0);
    swosWriteByte(TEAMDATA_BOTTOM_BASE + TEAMDATA_OFF_FIRE_PRESSED, 1);
    swosTeamDataSetPlayerHasBall(false, 1);
    swosPlayerSpriteSetPlayerOrdinal(0, 1);
    wirePlayerInfo(true, 4, 4, 4, 4, 4, 4, 4, 0);
    swosWriteWord(ADDR_currentGameTick, 0);
    swosBallSpriteSetXPixels(300);
    swosPlayerSpriteSetX(PLSPR_SLOT_GOALIE1, 320 << 16);
    swosPlayerSpriteSetY(PLSPR_SLOT_GOALIE1, 780 << 16);
    swosRunShotAtGoal(keeper1, BALLSPR_BASE, TEAMDATA_TOP_BASE, true);
    compareFullBuffer("shot_at_goal_goal_scored");

    swosMemoryInit(true);
    swosBallSpriteSetZPixels(5);
    swosPlayerSpriteSetBallDistance(PLSPR_SLOT_GOALIE1, 9000);
    swosWriteByte(TEAMDATA_TOP_BASE + TEAMDATA_OFF_BALL_ABOVE_17, 0);
    swosWriteByte(TEAMDATA_BOTTOM_BASE + TEAMDATA_OFF_FIRE_PRESSED, 1);
    swosTeamDataSetPlayerHasBall(false, 1);
    swosPlayerSpriteSetPlayerOrdinal(0, 1);
    wirePlayerInfo(true, 4, 4, 4, 4, 4, 4, 4, 7);
    swosWriteWord(ADDR_currentGameTick, 30);
    swosBallSpriteSetXPixels(300); swosBallSpriteSetYPixels(300);
    swosPlayerSpriteSetX(PLSPR_SLOT_GOALIE1, 320 << 16);
    swosPlayerSpriteSetY(PLSPR_SLOT_GOALIE1, 400 << 16);
    swosRunShotAtGoal(keeper1, BALLSPR_BASE, TEAMDATA_TOP_BASE, true);
    compareFullBuffer("shot_at_goal_saved_no_dive");

    swosMemoryInit(true);
    swosBallSpriteSetZPixels(5);
    swosPlayerSpriteSetBallDistance(PLSPR_SLOT_GOALIE1, 9000);
    swosWriteByte(TEAMDATA_TOP_BASE + TEAMDATA_OFF_BALL_ABOVE_17, 0);
    swosWriteByte(TEAMDATA_BOTTOM_BASE + TEAMDATA_OFF_FIRE_PRESSED, 1);
    swosTeamDataSetPlayerHasBall(false, 1);
    swosPlayerSpriteSetPlayerOrdinal(0, 1);
    wirePlayerInfo(true, 4, 4, 4, 4, 4, 4, 4, 7);
    swosWriteWord(ADDR_currentGameTick, 30);
    swosBallSpriteSetXPixels(300); swosBallSpriteSetYPixels(405);
    swosPlayerSpriteSetX(PLSPR_SLOT_GOALIE1, 320 << 16);
    swosPlayerSpriteSetY(PLSPR_SLOT_GOALIE1, 400 << 16);
    swosBallSpriteSetDeltaY(-3000);
    swosPlayerSpriteSetDeltaX(PLSPR_SLOT_GOALIE1, 3000);
    swosWriteWord(ADDR_ballDefensiveX, 250);
    wireShotChanceTable(true, 3, 0, 0, 0);
    swosRunShotAtGoal(keeper1, BALLSPR_BASE, TEAMDATA_TOP_BASE, true);
    compareFullBuffer("shot_at_goal_saved_dive_committed");

    printf("\nchecked %d scenarios\n", g_checked);
    if (g_failures) {
        printf("%d check(s) FAILED\n", g_failures);
        return 1;
    }
    printf("all %d checks passed\n", g_checked);
    return 0;
}
