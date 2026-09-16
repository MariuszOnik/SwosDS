// Step-7A differential test: compares FULL VM/Memory state against golden
// dumps produced by the REAL C# (tools/csharp-golden-dump/Step7AGolden.cs).
// Covers the real local dependencies UpdatePlayers.cs pulls in:
// BallVariables.cs (full file), TeamPort.UpdatePlayerShotChanceTable,
// PlayerEnergy.DrainSlot/DrainOnTackle/InjuryRiskDoubled,
// PlayerHeader.SetStaticHeaderDirection/SetPlayerWithNoBallDestination,
// and PlayerTackle.cs's PlayerTacklingTestFoul/PlayersTackledTheBallStrong
// plus their whole executed call chain (TestFoulForPenaltyAndFreeKick,
// TryBookingThePlayer, TrySendingOffThePlayer, PlayerTackled,
// Referee.ActivateReferee).
//
// Regenerate build/golden/s7a_*.bin with:
//   cd tools/csharp-golden-dump && dotnet run -c Release -- ../../build/golden
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "swos_addr.h"
#include "swos_ball_sprite.h"
#include "swos_ball_variables.h"
#include "swos_memory.h"
#include "swos_player_energy.h"
#include "swos_player_header.h"
#include "swos_player_sprite.h"
#include "swos_player_tackle.h"
#include "swos_referee.h"
#include "swos_rng.h"
#include "swos_team_data.h"
#include "swos_team_port.h"

#define GOLDEN_DIR "build/golden"

static int g_failures = 0;
static int g_checked = 0;

static void compareFullBuffer(const char *label) {
    char path[256];
    snprintf(path, sizeof(path), "%s/s7a_%s.bin", GOLDEN_DIR, label);

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
    int keeper1 = swosPlayerSpriteBase(PLSPR_SLOT_GOALIE1);
    int out1_1 = swosPlayerSpriteBase(1);
    int out1_2 = swosPlayerSpriteBase(2);
    int out2_1 = swosPlayerSpriteBase(12);
    (void)out1_2;

    // ---- BallVariables.UpdateBallVariables ----
    swosMemoryInit(true);
    swosBallSpriteSetY(400 << 16);
    swosBallSpriteSetDeltaY(-80000);
    swosBallSpriteSetDeltaZ(2000);
    swosPlayerSpriteSetY(1, 100 << 16);
    swosUpdateBallVariables(out1_1, BALLSPR_BASE, TEAMDATA_TOP_BASE);
    compareFullBuffer("ball_vars_going_up_top_team");

    swosMemoryInit(true);
    swosBallSpriteSetY(400 << 16);
    swosBallSpriteSetDeltaY(80000);
    swosBallSpriteSetDeltaZ(2000);
    swosPlayerSpriteSetY(12, 700 << 16);
    swosUpdateBallVariables(out2_1, BALLSPR_BASE, TEAMDATA_BOTTOM_BASE);
    compareFullBuffer("ball_vars_going_down_bottom_team");

    swosMemoryInit(true);
    swosBallSpriteSetX(400 << 16);
    swosBallSpriteSetDeltaX(-80000);
    swosPlayerSpriteSetX(1, 300 << 16);
    swosUpdateBallVariables(out1_1, BALLSPR_BASE, TEAMDATA_TOP_BASE);
    compareFullBuffer("ball_vars_going_left");

    swosMemoryInit(true);
    swosBallSpriteSetX(300 << 16);
    swosBallSpriteSetDeltaX(80000);
    swosPlayerSpriteSetX(1, 400 << 16);
    swosUpdateBallVariables(out1_1, BALLSPR_BASE, TEAMDATA_TOP_BASE);
    compareFullBuffer("ball_vars_going_right");

    swosMemoryInit(true);
    swosBallSpriteSetXPixels(300); swosBallSpriteSetYPixels(400); swosBallSpriteSetZPixels(5);
    swosBallSpriteSetDeltaX(0); swosBallSpriteSetDeltaY(0);
    swosUpdateBallVariables(out1_1, BALLSPR_BASE, TEAMDATA_TOP_BASE);
    compareFullBuffer("ball_vars_not_moving");

    // ---- BallVariables.CalculateBallNextGroundXYPositions ----
    swosMemoryInit(true);
    swosBallSpriteSetX(300 << 16); swosBallSpriteSetY(400 << 16); swosBallSpriteSetZ(40 << 16);
    swosBallSpriteSetDeltaX(30000); swosBallSpriteSetDeltaY(-20000); swosBallSpriteSetDeltaZ(-1000);
    swosCalculateBallNextGroundXYPositions(BALLSPR_BASE);
    compareFullBuffer("ball_next_ground_moving");

    swosMemoryInit(true);
    swosBallSpriteSetDeltaX(0); swosBallSpriteSetDeltaY(0);
    swosCalculateBallNextGroundXYPositions(BALLSPR_BASE);
    compareFullBuffer("ball_next_ground_standing");

    // ---- TeamPort.UpdatePlayerShotChanceTable ----
    swosMemoryInit(true);
    swosWriteDword(TEAMDATA_TOP_BASE + TEAMDATA_OFF_SHOT_CHANCE_TABLE, 0x4FEE0);
    swosWriteByte(0x4FE60 + 4, 0);
    swosWriteByte(0x4FE60 + 34, 3);
    swosTeamPortUpdatePlayerShotChanceTable(true, 0x4FE60);
    compareFullBuffer("shot_chance_goalkeeper");

    swosMemoryInit(true);
    swosWriteDword(TEAMDATA_TOP_BASE + TEAMDATA_OFF_SHOT_CHANCE_TABLE, 0x4FEE0);
    swosWriteByte(0x4FE60 + 4, 4);
    swosTeamPortUpdatePlayerShotChanceTable(true, 0x4FE60);
    compareFullBuffer("shot_chance_outfielder");

    swosMemoryInit(true);
    swosWriteByte(0x4FE60 + 4, 0);
    swosWriteByte(0x4FE60 + 34, 5);
    swosTeamPortUpdatePlayerShotChanceTable(true, 0x4FE60);
    compareFullBuffer("shot_chance_no_buffer");

    // ---- PlayerEnergy.DrainSlot ----
    swosMemoryInit(true);
    g_swosPlayerEnergyEffectEnabled = true;
    swosWriteByte(out1_1 + PLSPR_OFF_IS_MOVING, 0xFF);
    swosWriteByte(out1_1 + PLSPR_OFF_STAMINA, 4);
    swosWriteWord(out1_1 + PLSPR_OFF_ENERGY, 2000);
    swosWriteWord(out1_1 + PLSPR_OFF_ENERGY_ACC, 90);
    swosPlayerEnergyDrainSlot(out1_1);
    g_swosPlayerEnergyEffectEnabled = false;
    compareFullBuffer("energy_drain_slot_moving");

    swosMemoryInit(true);
    swosWriteByte(out1_1 + PLSPR_OFF_IS_MOVING, 0);
    swosWriteWord(out1_1 + PLSPR_OFF_ENERGY, 2000);
    swosPlayerEnergyDrainSlot(out1_1);
    compareFullBuffer("energy_drain_slot_not_moving");

    // ---- PlayerHeader.SetStaticHeaderDirection ----
    swosMemoryInit(true);
    swosTeamDataSetCurrentAllowedDirection(true, 3);
    swosPlayerSpriteSetDirection(1, 5);
    swosWriteByte(out1_1 + PLSPR_OFF_PLAYER_DOWN_TIMER, 5);
    swosWriteDword(out1_1 + PLSPR_OFF_ANIM_TABLE_PTR, (uint32_t)ADDR_kStaticHeaderAttemptAnimTableAddr);
    swosSetStaticHeaderDirection(out1_1, TEAMDATA_TOP_BASE);
    compareFullBuffer("static_header_direction_turn");

    // ---- PlayerHeader.SetPlayerWithNoBallDestination ----
    swosMemoryInit(true);
    swosPlayerSpriteSetPlayerOrdinal(1, 4);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_TACTICS, 0);
    swosWriteWord(ADDR_gameState, 100);
    swosWriteWord(ADDR_ballQuadrantIndex, 2);
    swosSetPlayerWithNoBallDestination(out1_1, TEAMDATA_TOP_BASE, 300, 400);
    compareFullBuffer("no_ball_dest_outfielder_top");

    swosMemoryInit(true);
    swosPlayerSpriteSetPlayerOrdinal(12, 4);
    swosWriteWord(TEAMDATA_BOTTOM_BASE + TEAMDATA_OFF_TACTICS, 0);
    swosWriteWord(ADDR_gameState, 100);
    swosWriteWord(ADDR_ballQuadrantIndex, 5);
    swosSetPlayerWithNoBallDestination(out2_1, TEAMDATA_BOTTOM_BASE, 300, 400);
    compareFullBuffer("no_ball_dest_outfielder_bottom");

    swosMemoryInit(true);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_TACTICS, 0);
    swosWriteWord(ADDR_gameState, 100);
    swosSetPlayerWithNoBallDestination(keeper1, TEAMDATA_TOP_BASE, 300, 400);
    compareFullBuffer("no_ball_dest_goalkeeper_top");

    // ---- Referee.ActivateReferee (direct) ----
    swosMemoryInit(true);
    swosRngReseed(11);
    swosWriteWord(ADDR_foulXCoordinate, 300);
    swosWriteWord(ADDR_foulYCoordinate, 400);
    swosRefereeActivate();
    compareFullBuffer("referee_activate_direct");

    // ---- PlayerTackle.PlayerTacklingTestFoul ----
    swosMemoryInit(true);
    swosTeamDataSetControlledPlayer(false, out2_1);
    swosPlayerSpriteSetX(1, 100 << 16); swosPlayerSpriteSetY(1, 100 << 16);
    swosPlayerSpriteSetX(12, 500 << 16); swosPlayerSpriteSetY(12, 700 << 16);
    swosPlayerTacklingTestFoul(out1_1, TEAMDATA_TOP_BASE);
    compareFullBuffer("test_foul_too_far");

    swosMemoryInit(true);
    swosTeamDataSetControlledPlayer(false, swosPlayerSpriteBase(PLSPR_SLOT_GOALIE2));
    swosPlayerSpriteSetX(1, 300 << 16); swosPlayerSpriteSetY(1, 400 << 16);
    swosPlayerSpriteSetX(PLSPR_SLOT_GOALIE2, 300 << 16);
    swosPlayerSpriteSetY(PLSPR_SLOT_GOALIE2, 400 << 16);
    swosPlayerTacklingTestFoul(out1_1, TEAMDATA_TOP_BASE);
    compareFullBuffer("test_foul_close_goalkeeper");

    swosMemoryInit(true);
    swosRngReseed(5);
    swosTeamDataSetControlledPlayer(false, out2_1);
    swosPlayerSpriteSetPlayerOrdinal(12, 3);
    swosPlayerSpriteSetX(1, 336 << 16); swosPlayerSpriteSetY(1, 200 << 16);
    swosPlayerSpriteSetX(12, 336 << 16); swosPlayerSpriteSetY(12, 200 << 16);
    swosPlayerSpriteSetTackleState(1, 0);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PLAYER_NUMBER, 1);
    swosWriteDword(ADDR_dseg_17E3EE, 0x4FEE0);
    swosWriteDword(ADDR_dseg_17E3F3, 0x4FEE0);
    swosPlayerTacklingTestFoul(out1_1, TEAMDATA_TOP_BASE);
    compareFullBuffer("test_foul_close_yellow_card");

    swosMemoryInit(true);
    swosRngReseed(1);
    swosWriteWord(ADDR_cardsDisallowed, 1);
    swosTeamDataSetControlledPlayer(false, out2_1);
    swosPlayerSpriteSetPlayerOrdinal(12, 3);
    swosPlayerSpriteSetX(1, 336 << 16); swosPlayerSpriteSetY(1, 200 << 16);
    swosPlayerSpriteSetX(12, 336 << 16); swosPlayerSpriteSetY(12, 200 << 16);
    swosPlayerSpriteSetTackleState(1, 0);
    swosPlayerTacklingTestFoul(out1_1, TEAMDATA_TOP_BASE);
    compareFullBuffer("test_foul_close_no_cards");

    // ---- PlayerTackle.PlayersTackledTheBallStrong ----
    swosMemoryInit(true);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PLAYER_NUMBER, 0);
    swosTeamDataSetCurrentAllowedDirection(true, 3);
    swosPlayerSpriteSetDirection(1, 3);
    swosPlayerSpriteSetSpeed(1, 500);
    swosBallSpriteSetXPixels(300); swosBallSpriteSetYPixels(400);
    swosPlayersTackledTheBallStrong(out1_1, TEAMDATA_TOP_BASE);
    compareFullBuffer("tackled_ball_strong_cpu");

    swosMemoryInit(true);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PLAYER_NUMBER, 1);
    swosTeamDataSetCurrentAllowedDirection(true, 3);
    swosPlayerSpriteSetDirection(1, 3);
    swosPlayerSpriteSetSpeed(1, 500);
    swosBallSpriteSetXPixels(300); swosBallSpriteSetYPixels(400);
    swosTeamDataSetControlledPlayer(false, out2_1);
    swosPlayerSpriteSetBallDistance(12, 50);
    swosPlayerSpriteSetX(1, 100 << 16); swosPlayerSpriteSetY(1, 100 << 16);
    swosPlayerSpriteSetX(12, 300 << 16); swosPlayerSpriteSetY(12, 300 << 16);
    swosPlayersTackledTheBallStrong(out1_1, TEAMDATA_TOP_BASE);
    compareFullBuffer("tackled_ball_strong_human_good_tackle");

    // ---- PlayerTackle.PlayerTackled (indirect, via a close foul) ----
    swosMemoryInit(true);
    swosRngReseed(2);
    swosTeamDataSetControlledPlayer(false, out2_1);
    swosPlayerSpriteSetPlayerOrdinal(1, 2);
    swosPlayerSpriteSetPlayerOrdinal(12, 3);
    swosPlayerSpriteSetX(1, 300 << 16); swosPlayerSpriteSetY(1, 400 << 16);
    swosPlayerSpriteSetX(12, 300 << 16); swosPlayerSpriteSetY(12, 400 << 16);
    swosPlayerSpriteSetTackleState(1, 1);
    swosPlayerSpriteSetDirection(1, 2);
    swosPlayerSpriteSetDirection(12, 2);
    swosWriteWord(ADDR_g_trainingGame, 0);
    swosWriteWord(ADDR_team2NumAllowedInjuries, 3);
    swosWriteWord(ADDR_gameLengthInGame, 0);
    swosPlayerTacklingTestFoul(out1_1, TEAMDATA_TOP_BASE);
    compareFullBuffer("test_foul_triggers_player_tackled_injury");

    printf("\nchecked %d scenarios\n", g_checked);
    if (g_failures) {
        printf("%d check(s) FAILED\n", g_failures);
        return 1;
    }
    printf("all %d checks passed\n", g_checked);
    return 0;
}
