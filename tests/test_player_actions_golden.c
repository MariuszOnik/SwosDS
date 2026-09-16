// Step-5 differential test: compares FULL VM/Memory state against golden
// dumps produced by the REAL C# (tools/csharp-golden-dump/
// PlayerActionsGolden.cs). Covers the rest of PlayerActions.cs
// (SetPlayerAnimationTable was already covered in step 3's
// test_sprite_update_golden.c). Each scenario here mirrors
// PlayerActionsGolden.cs's setup exactly, by name.
//
// Regenerate build/golden/pa_*.bin with:
//   cd tools/csharp-golden-dump && dotnet run -c Release -- ../../build/golden
//
// MatchAudio/telemetry/PlayerControlled counters are deliberately out of
// scope for both sides of this comparison -- see swos_player_actions.h.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "swos_addr.h"
#include "swos_ball_sprite.h"
#include "swos_memory.h"
#include "swos_player_actions.h"
#include "swos_player_energy.h"
#include "swos_player_sprite.h"
#include "swos_rng.h"
#include "swos_team_data.h"
#include "swos_team_data_loader.h"

#define GOLDEN_DIR "build/golden"

// Same scratch PlayerInfo addresses as PlayerActionsGolden.cs -- the unused
// gap between the per-team sprite pointer tables (ending 0x4FE58) and the
// sprite pool (starting 0x50000).
#define TOP_PLAYER_INFO    0x4FE60
#define BOTTOM_PLAYER_INFO 0x4FEA0

static int g_failures = 0;
static int g_checked = 0;

static void compareFullBuffer(const char *label) {
    char path[256];
    snprintf(path, sizeof(path), "%s/pa_%s.bin", GOLDEN_DIR, label);

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
                            int tackling, int ballControl, int speed, int finishing) {
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
}

int main(void) {
    int keeper1 = swosPlayerSpriteBase(PLSPR_SLOT_GOALIE1);
    int out1_1 = swosPlayerSpriteBase(1);
    int out2_1 = swosPlayerSpriteBase(12);

    // ---- UpdatePlayerSpeedAndFrameDelay / RecomputeSpriteDeltas ----
    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosPlayerSpriteSetPlayerState(1, 0);
    swosPlayerSpriteSetX(1, 300 << 16);
    swosPlayerSpriteSetY(1, 400 << 16);
    swosPlayerSpriteSetDestX(1, 320);
    swosPlayerSpriteSetDestY(1, 420);
    swosPlayerSpriteSetDirection(1, 3);
    swosUpdatePlayerSpeedAndFrameDelay(TEAMDATA_TOP_BASE, out1_1);
    compareFullBuffer("speed_normal_outfielder");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosPlayerSpriteSetPlayerOrdinal(1, 1);
    swosPlayerSpriteSetPlayerState(1, 0);
    swosPlayerSpriteSetX(1, 300 << 16);
    swosPlayerSpriteSetY(1, 400 << 16);
    swosPlayerSpriteSetDestX(1, 320);
    swosPlayerSpriteSetDestY(1, 420);
    wirePlayerInfo(true, 5, 6, 3, 2, 7, 6, 4);
    swosUpdatePlayerSpeedAndFrameDelay(TEAMDATA_TOP_BASE, out1_1);
    compareFullBuffer("speed_with_wired_playerinfo");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosPlayerSpriteSetPlayerState(PLSPR_SLOT_GOALIE1, 0);
    swosTeamDataSetControlledPlayer(true, out1_1);
    swosPlayerSpriteSetX(PLSPR_SLOT_GOALIE1, 300 << 16);
    swosPlayerSpriteSetY(PLSPR_SLOT_GOALIE1, 100 << 16);
    swosPlayerSpriteSetDestX(PLSPR_SLOT_GOALIE1, 300);
    swosPlayerSpriteSetDestY(PLSPR_SLOT_GOALIE1, 110);
    swosUpdatePlayerSpeedAndFrameDelay(TEAMDATA_TOP_BASE, keeper1);
    compareFullBuffer("speed_keeper_early_out");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PLAYER_NUMBER, 1);
    swosPlayerSpriteSetPlayerState(1, 0);
    swosWriteWord(out1_1 + PLSPR_OFF_INJURY_LEVEL, 96);
    swosPlayerSpriteSetX(1, 300 << 16);
    swosPlayerSpriteSetY(1, 400 << 16);
    swosPlayerSpriteSetDestX(1, 300);
    swosPlayerSpriteSetDestY(1, 400);
    swosUpdatePlayerSpeedAndFrameDelay(TEAMDATA_TOP_BASE, out1_1);
    compareFullBuffer("speed_injured_human_team");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosTeamDataSetControlledPlayer(true, out1_1);
    swosTeamDataSetPlayerHasBall(true, 1);
    swosPlayerSpriteSetPlayerState(1, 0);
    swosPlayerSpriteSetX(1, 300 << 16);
    swosPlayerSpriteSetY(1, 400 << 16);
    swosPlayerSpriteSetDestX(1, 300);
    swosPlayerSpriteSetDestY(1, 400);
    swosUpdatePlayerSpeedAndFrameDelay(TEAMDATA_TOP_BASE, out1_1);
    compareFullBuffer("speed_ball_carrier_slowdown");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PLAYER_NUMBER, 1);
    swosWriteDword(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PASS_TO_PLAYER_PTR, (uint32_t)out1_1);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PASSING_TO_PLAYER, 1);
    swosTeamDataSetLongPass(true, 1);
    swosBallSpriteSetSpeed(500);
    swosBallSpriteSetFullDirection(10);
    swosPlayerSpriteSetFullDirection(1, 8);
    swosPlayerSpriteSetPlayerState(1, 0);
    swosPlayerSpriteSetX(1, 300 << 16);
    swosPlayerSpriteSetY(1, 400 << 16);
    swosPlayerSpriteSetDestX(1, 300);
    swosPlayerSpriteSetDestY(1, 400);
    swosUpdatePlayerSpeedAndFrameDelay(TEAMDATA_TOP_BASE, out1_1);
    compareFullBuffer("speed_pass_overlap_boost");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 0);
    swosWriteWord(ADDR_gameState, 29);
    swosWriteWord(ADDR_stoppageTimerTotal, 3);
    swosPlayerSpriteSetPlayerState(1, 0);
    swosPlayerSpriteSetX(1, 300 << 16);
    swosPlayerSpriteSetY(1, 400 << 16);
    swosPlayerSpriteSetDestX(1, 300);
    swosPlayerSpriteSetDestY(1, 400);
    swosUpdatePlayerSpeedAndFrameDelay(TEAMDATA_TOP_BASE, out1_1);
    compareFullBuffer("speed_stoppage_slowdown");

    // ---- UpdatePlayerWithBall / UpdateControllingPlayer ----
    swosMemoryInit(true);
    swosBallSpriteSetXPixels(300); swosBallSpriteSetYPixels(400);
    swosPlayerSpriteSetDirection(1, 2);
    swosUpdatePlayerWithBall(out1_1);
    compareFullBuffer("update_player_with_ball");

    swosMemoryInit(true);
    swosPlayerSpriteSetX(1, 300 << 16); swosPlayerSpriteSetY(1, 400 << 16);
    swosPlayerSpriteSetDirection(1, 5);
    swosBallSpriteSetDeltaZ(-400);
    swosUpdateControllingPlayer(out1_1);
    compareFullBuffer("update_controlling_player");

    // ---- CalculateIfPlayerWinsBall ----
    swosMemoryInit(true);
    swosRngReseed(1);
    swosBallSpriteSetXPixels(300); swosBallSpriteSetYPixels(400);
    swosPlayerSpriteSetX(1, 300 << 16); swosPlayerSpriteSetY(1, 400 << 16);
    swosCalculateIfPlayerWinsBall(2, TEAMDATA_TOP_BASE, out1_1);
    compareFullBuffer("wins_ball_duel_no_opponent_controlled");

    swosMemoryInit(true);
    swosRngReseed(7);
    swosTeamDataSetPlayerHasBall(false, 1);
    swosTeamDataSetCurrentAllowedDirection(false, 3);
    swosTeamDataSetControlledPlayer(false, out2_1);
    swosCalculateIfPlayerWinsBall(2, TEAMDATA_TOP_BASE, out1_1);
    compareFullBuffer("wins_ball_duel_resolved");

    swosMemoryInit(true);
    swosRngReseed(42);
    swosTeamDataSetPlayerHasBall(false, 1);
    swosTeamDataSetCurrentAllowedDirection(false, 3);
    swosTeamDataSetControlledPlayer(false, out2_1);
    swosPlayerSpriteSetPlayerOrdinal(1, 1);
    swosPlayerSpriteSetPlayerOrdinal(12, 1);
    wirePlayerInfo(true, 4, 4, 4, 6, 6, 4, 4);
    wirePlayerInfo(false, 4, 4, 4, 2, 2, 4, 4);
    swosCalculateIfPlayerWinsBall(2, TEAMDATA_TOP_BASE, out1_1);
    compareFullBuffer("wins_ball_duel_with_skills");

    // ---- PlayerKickingBall ----
    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosPlayerSpriteSetDirection(1, 3);
    swosBallSpriteSetXPixels(300); swosBallSpriteSetYPixels(400);
    swosPlayerKickingBall(TEAMDATA_TOP_BASE, out1_1);
    compareFullBuffer("kick_no_shot");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosTeamDataSetControlledPlDirection(false, 0);
    swosPlayerSpriteSetDirection(12, 4);
    swosBallSpriteSetXPixels(300); swosBallSpriteSetYPixels(150);
    swosPlayerKickingBall(TEAMDATA_BOTTOM_BASE, out2_1);
    compareFullBuffer("kick_finishing_shot_bottom_team");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosTeamDataSetControlledPlDirection(true, 4);
    swosPlayerSpriteSetDirection(1, 1);
    swosBallSpriteSetXPixels(200); swosBallSpriteSetYPixels(600);
    swosPlayerKickingBall(TEAMDATA_TOP_BASE, out1_1);
    compareFullBuffer("kick_long_shot_top_team");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosTeamDataSetControlledPlDirection(false, 0);
    swosPlayerSpriteSetDirection(12, 4);
    swosPlayerSpriteSetPlayerOrdinal(12, 1);
    wirePlayerInfo(false, 4, 4, 4, 4, 4, 4, 6);
    g_swosPlayerEnergyEffectEnabled = true;
    swosWriteWord(out2_1 + PLSPR_OFF_ENERGY, 4096 / 20);
    swosBallSpriteSetXPixels(300); swosBallSpriteSetYPixels(150);
    swosPlayerKickingBall(TEAMDATA_BOTTOM_BASE, out2_1);
    compareFullBuffer("kick_finishing_with_wired_skill_and_fatigue");
    g_swosPlayerEnergyEffectEnabled = false;

    // ---- PlayerHittingStaticHeader ----
    swosMemoryInit(true);
    swosTeamDataSetCurrentAllowedDirection(true, 5);
    swosPlayerSpriteSetDirection(1, 2);
    swosBallSpriteSetXPixels(300); swosBallSpriteSetYPixels(400); swosBallSpriteSetDeltaZ(-500);
    swosPlayerHittingStaticHeader(TEAMDATA_TOP_BASE, out1_1);
    compareFullBuffer("static_header");

    // ---- PlayerHittingJumpHeader (flying / lob / static branches) ----
    swosMemoryInit(true);
    swosTeamDataSetCurrentAllowedDirection(true, 2);
    swosPlayerSpriteSetDirection(1, 4);
    swosPlayerSpriteSetSpeed(1, 400);
    swosBallSpriteSetXPixels(300); swosBallSpriteSetYPixels(400);
    swosPlayerHittingJumpHeader(TEAMDATA_TOP_BASE, out1_1);
    compareFullBuffer("jump_header_flying");

    swosMemoryInit(true);
    swosTeamDataSetCurrentAllowedDirection(true, 0);
    swosPlayerSpriteSetDirection(1, 4);
    swosPlayerSpriteSetSpeed(1, 400);
    swosBallSpriteSetXPixels(300); swosBallSpriteSetYPixels(400);
    swosPlayerHittingJumpHeader(TEAMDATA_TOP_BASE, out1_1);
    compareFullBuffer("jump_header_lob");

    swosMemoryInit(true);
    swosTeamDataSetCurrentAllowedDirection(true, -1);
    swosPlayerSpriteSetDirection(1, 3);
    swosPlayerSpriteSetSpeed(1, 400);
    swosBallSpriteSetXPixels(300); swosBallSpriteSetYPixels(400);
    swosPlayerHittingJumpHeader(TEAMDATA_TOP_BASE, out1_1);
    compareFullBuffer("jump_header_static_path");

    // ---- PlayerTackledTheBallStrong / Weak ----
    swosMemoryInit(true);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PLAYER_NUMBER, 0);
    swosTeamDataSetCurrentAllowedDirection(true, 6);
    swosPlayerSpriteSetDirection(1, 2);
    swosPlayerSpriteSetSpeed(1, 600);
    swosTeamDataSetControlledPlayer(false, out2_1);
    swosPlayerSpriteSetBallDistance(12, 20);
    swosPlayerSpriteSetX(1, 100 << 16); swosPlayerSpriteSetY(1, 100 << 16);
    swosPlayerSpriteSetX(12, 200 << 16); swosPlayerSpriteSetY(12, 200 << 16);
    swosPlayerTackledTheBallStrong(TEAMDATA_TOP_BASE, out1_1);
    compareFullBuffer("tackle_strong_cpu_good_tackle");

    swosMemoryInit(true);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PLAYER_NUMBER, 1);
    swosTeamDataSetCurrentAllowedDirection(true, 6);
    swosPlayerSpriteSetDirection(1, 2);
    swosPlayerSpriteSetSpeed(1, 600);
    swosPlayerTackledTheBallStrong(TEAMDATA_TOP_BASE, out1_1);
    compareFullBuffer("tackle_strong_human");

    swosMemoryInit(true);
    swosTeamDataSetCurrentAllowedDirection(true, 6);
    swosPlayerSpriteSetDirection(1, 2);
    swosPlayerSpriteSetSpeed(1, 600);
    swosTeamDataSetControlledPlayer(false, out2_1);
    swosPlayerSpriteSetBallDistance(12, 20);
    swosPlayerSpriteSetX(1, 100 << 16); swosPlayerSpriteSetY(1, 100 << 16);
    swosPlayerSpriteSetX(12, 200 << 16); swosPlayerSpriteSetY(12, 200 << 16);
    swosPlayerTackledTheBallWeak(TEAMDATA_TOP_BASE, out1_1);
    compareFullBuffer("tackle_weak_good_tackle");

    // ---- DoPass ----
    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PLAYER_NUMBER, 0);
    swosPlayerSpriteSetDirection(1, 3);
    swosBallSpriteSetXPixels(300); swosBallSpriteSetYPixels(400);
    swosPlayerSpriteSetPlayerState(2, 0);
    swosPlayerSpriteSetFullDirection(2, 3 * 32);
    swosPlayerSpriteSetBallDistance(2, 1000);
    swosWriteWord(ADDR_currentGameTick, 0);
    swosDoPass(TEAMDATA_TOP_BASE, out1_1);
    compareFullBuffer("pass_found_closest_ai");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PLAYER_NUMBER, 1);
    swosPlayerSpriteSetDirection(1, 3);
    swosBallSpriteSetXPixels(300); swosBallSpriteSetYPixels(400);
    swosPlayerSpriteSetPlayerState(2, 0);
    swosPlayerSpriteSetFullDirection(2, 3 * 32);
    swosPlayerSpriteSetBallDistance(2, 90000);
    swosPlayerSpriteSetX(2, 500 << 16); swosPlayerSpriteSetY(2, 700 << 16);
    swosDoPass(TEAMDATA_TOP_BASE, out1_1);
    compareFullBuffer("pass_found_closest_human_far");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 0);
    swosWriteWord(ADDR_gameState, 10);
    swosPlayerSpriteSetDirection(1, 3);
    swosBallSpriteSetXPixels(300); swosBallSpriteSetYPixels(400);
    for (int slot = 2; slot <= 10; slot++) swosPlayerSpriteSetPlayerState(slot, 4);
    swosDoPass(TEAMDATA_TOP_BASE, out1_1);
    compareFullBuffer("pass_no_closest_player");

    // ---- SetPlayerDowntimeAfterTackle / SetJumpHeaderHitAnimTable ----
    swosMemoryInit(true);
    swosPlayerSpriteSetTacklingTimer(1, -1);
    swosSetPlayerDowntimeAfterTackle(TEAMDATA_TOP_BASE, out1_1);
    compareFullBuffer("downtime_after_tackle_cpu");

    swosMemoryInit(true);
    swosPlayerSpriteSetTacklingTimer(1, 5);
    swosSetPlayerDowntimeAfterTackle(TEAMDATA_TOP_BASE, out1_1);
    compareFullBuffer("downtime_after_tackle_human");

    swosMemoryInit(true);
    swosPlayerSpriteSetPlayerDownTimer(1, 40);
    swosWriteWord(out1_1 + 98 /* heading */, 0);
    swosWriteWord(out1_1 + PLSPR_OFF_FRAME_SWITCH_COUNTER, 1);
    swosWriteWord(ADDR_currentGameTick, 2);
    swosPlayerSpriteSetSpeed(1, 600);
    swosSetJumpHeaderHitAnimTable(out1_1);
    compareFullBuffer("set_jump_header_hit_anim_table_all_gates_pass");

    // ---- PlayStopGoodPassSampleIfNeeded / StopGoodPassSample / EnqueuePlayingGoodPassSample ----
    swosMemoryInit(true);
    swosWriteWord(ADDR_goodPassSampleCommand, (uint16_t)-1);
    swosWriteWord(ADDR_goodPassTimer, 4);
    swosPlayStopGoodPassSampleIfNeeded();
    compareFullBuffer("good_pass_sample_enqueue_on_5th");

    swosMemoryInit(true);
    swosWriteWord(ADDR_goodPassSampleCommand, (uint16_t)-2);
    swosPlayStopGoodPassSampleIfNeeded();
    compareFullBuffer("good_pass_sample_stop");

    printf("\nchecked %d scenarios\n", g_checked);
    if (g_failures) {
        printf("%d check(s) FAILED\n", g_failures);
        return 1;
    }
    printf("all %d checks passed\n", g_checked);
    return 0;
}
