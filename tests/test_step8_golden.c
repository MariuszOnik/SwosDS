// Step-8 differential test: compares FULL VM/Memory state against golden
// dumps produced by the REAL C# (tools/csharp-golden-dump/Step8Golden.cs).
// Covers InputControls.cs (gameControls.cpp, 332 LOC) through its public
// entry points; UpdateControlledPlayer/UpdatePlayerBeingPassedTo(Stopped)
// (private in the C#) are exercised indirectly through
// swosUpdateTeamControls(top), same technique as test_step7b_golden.c.
//
// Regenerate build/golden/s8_*.bin with:
//   cd tools/csharp-golden-dump && dotnet run -c Release -- ../../build/golden
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "swos_addr.h"
#include "swos_ball_sprite.h"
#include "swos_input_controls.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"
#include "swos_player_state.h"
#include "swos_team_data.h"

#define GOLDEN_DIR "build/golden"

static int g_failures = 0;
static int g_checked = 0;

static void compareFullBuffer(const char *label) {
    char path[256];
    snprintf(path, sizeof(path), "%s/s8_%s.bin", GOLDEN_DIR, label);

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
    int out1_1 = swosPlayerSpriteBase(1);
    int out1_2 = swosPlayerSpriteBase(2);

    // ---- swosResetGameControls ----
    swosMemoryInit(true);
    swosWriteDword(ADDR_teamSwitchCounter, 7);
    swosWriteByte(ADDR_ic_pl1LastFired, 1);
    swosWriteByte(ADDR_ic_pl2LastFired, 1);
    swosWriteDword(ADDR_ic_pl1FireCounter, (uint32_t)-3);
    swosWriteDword(ADDR_ic_pl2FireCounter, 5);
    swosWriteDword(ADDR_ic_oldPl1Events, 9);
    swosWriteDword(ADDR_ic_oldPl2Events, 3);
    swosWriteDword(ADDR_ic_pl1LastVertical, 1);
    swosWriteDword(ADDR_ic_pl1LastHorizontal, 4);
    swosWriteDword(ADDR_ic_pl2LastVertical, 2);
    swosWriteDword(ADDR_ic_pl2LastHorizontal, 8);
    swosResetGameControls();
    compareFullBuffer("reset_game_controls");

    // ---- swosUpdateFireBlocked ----
    swosMemoryInit(true);
    swosWriteWord(ADDR_fireBlocked, 1);
    swosWriteDword(ADDR_ic_pl1Events, 0);
    swosWriteDword(ADDR_ic_pl2Events, 0);
    swosUpdateFireBlocked();
    compareFullBuffer("fire_blocked_clears_when_released");

    swosMemoryInit(true);
    swosWriteWord(ADDR_fireBlocked, 1);
    swosWriteDword(ADDR_ic_pl1Events, IC_EVENT_KICK);
    swosUpdateFireBlocked();
    compareFullBuffer("fire_blocked_stays_when_firing");

    swosMemoryInit(true);
    swosWriteWord(ADDR_fireBlocked, 0);
    swosUpdateFireBlocked();
    compareFullBuffer("fire_blocked_not_set");

    // ---- swosSelectTeamForUpdate ----
    swosMemoryInit(true);
    swosSelectTeamForUpdate();
    compareFullBuffer("select_team_for_update_first_call");

    swosMemoryInit(true);
    swosSelectTeamForUpdate();
    swosSelectTeamForUpdate();
    compareFullBuffer("select_team_for_update_second_call");

    // ---- swosGetPlayerEvents / filterOverlappedEvents ----
    swosMemoryInit(true);
    swosWriteDword(ADDR_ic_pl1Events, IC_EVENT_UP | IC_EVENT_RIGHT);
    swosGetPlayerEvents(IC_PLAYER1);
    compareFullBuffer("get_player_events_no_conflict");

    swosMemoryInit(true);
    swosWriteDword(ADDR_ic_oldPl1Events, IC_EVENT_UP);
    swosWriteDword(ADDR_ic_pl1Events, IC_EVENT_UP | IC_EVENT_DOWN);
    swosGetPlayerEvents(IC_PLAYER1);
    compareFullBuffer("get_player_events_updown_conflict_latches_down");

    swosMemoryInit(true);
    swosWriteDword(ADDR_ic_oldPl2Events, IC_EVENT_LEFT);
    swosWriteDword(ADDR_ic_pl2Events, IC_EVENT_LEFT | IC_EVENT_RIGHT);
    swosGetPlayerEvents(IC_PLAYER2);
    compareFullBuffer("get_player_events_leftright_conflict_latches_right");

    // ---- swosIsPlayerFiring / swosIsAnyPlayerFiring ----
    swosMemoryInit(true);
    swosWriteDword(ADDR_ic_pl1Events, IC_EVENT_KICK);
    swosIsPlayerFiring(IC_PLAYER1);
    compareFullBuffer("is_player_firing_true");

    swosMemoryInit(true);
    swosWriteDword(ADDR_ic_pl1Events, 0);
    swosIsPlayerFiring(IC_PLAYER1);
    compareFullBuffer("is_player_firing_false");

    swosMemoryInit(true);
    swosWriteDword(ADDR_ic_pl2Events, IC_EVENT_KICK);
    swosIsAnyPlayerFiring();
    compareFullBuffer("is_any_player_firing_true");

    swosMemoryInit(true);
    swosWriteDword(ADDR_ic_pl1Events, 0);
    swosWriteDword(ADDR_ic_pl2Events, 0);
    swosIsAnyPlayerFiring();
    compareFullBuffer("is_any_player_firing_false");

    // ---- swosGetFireStartedAndBumpFireCounter (press -> hold -> release) ----
    swosMemoryInit(true);
    swosGetFireStartedAndBumpFireCounter(true, IC_PLAYER1);
    swosGetFireStartedAndBumpFireCounter(true, IC_PLAYER1);
    swosGetFireStartedAndBumpFireCounter(false, IC_PLAYER1);
    compareFullBuffer("fire_sequence_press_hold_release");

    // ---- swosInputControlsSetJoystickState ----
    swosMemoryInit(true);
    swosInputControlsSetJoystickState(IC_PLAYER1, IC_FACING_TOP_RIGHT, true, false);
    compareFullBuffer("set_joystick_state_up_right_with_fire");

    swosMemoryInit(true);
    swosInputControlsSetJoystickState(IC_PLAYER2, IC_NO_DIRECTION, false, false);
    compareFullBuffer("set_joystick_state_center_no_fire");

    // ---- swosPostUpdateTeamControls ----
    swosMemoryInit(true);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_HEADER_OR_TACKLE, 1);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PLAYER_NUMBER, 1);
    swosWriteDword(ADDR_ic_pl1FireCounter, (uint32_t)-3);
    swosPostUpdateTeamControls(true);
    compareFullBuffer("post_update_team_controls_clears_header_or_tackle");

    // ---- swosUpdateTeamControls ----
    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PLAYER_NUMBER, 1);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_BALL_OUT_OF_PLAY, 1);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_GOALIE_PLAYING_OR_OUT, 1);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_BALL_IN_PLAY, 1);
    swosBallSpriteSetXPixels(300); swosBallSpriteSetYPixels(400);
    swosPlayerSpriteSetX(1, 305 << 16); swosPlayerSpriteSetY(1, 404 << 16);
    swosPlayerSpriteSetX(2, 500 << 16); swosPlayerSpriteSetY(2, 700 << 16);
    swosWriteDword(ADDR_ic_pl1Events, IC_EVENT_UP | IC_EVENT_RIGHT | IC_EVENT_KICK);
    swosUpdateTeamControls(true);
    compareFullBuffer("update_team_controls_human_top_normal_play");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PLAYER_NUMBER, 0);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_BALL_OUT_OF_PLAY, 1);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_GOALIE_PLAYING_OR_OUT, 1);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_BALL_IN_PLAY, 1);
    swosBallSpriteSetXPixels(300); swosBallSpriteSetYPixels(400);
    swosPlayerSpriteSetX(1, 305 << 16); swosPlayerSpriteSetY(1, 404 << 16);
    swosUpdateTeamControls(true);
    compareFullBuffer("update_team_controls_ai_team_skips_input");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PLAYER_NUMBER, 1);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_BALL_OUT_OF_PLAY, 0);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_GOALIE_PLAYING_OR_OUT, 1);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_BALL_IN_PLAY, 1);
    swosTeamDataSetControlledPlayer(true, out1_2);
    swosBallSpriteSetXPixels(300); swosBallSpriteSetYPixels(400);
    swosPlayerSpriteSetX(1, 305 << 16); swosPlayerSpriteSetY(1, 404 << 16);
    swosUpdateTeamControls(true);
    compareFullBuffer("update_team_controls_ball_dead_no_promotion");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PLAYER_NUMBER, 1);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_BALL_OUT_OF_PLAY, 1);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_GOALIE_PLAYING_OR_OUT, 1);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_BALL_IN_PLAY, 1);
    swosBallSpriteSetXPixels(300); swosBallSpriteSetYPixels(400);
    swosPlayerSpriteSetX(1, 301 << 16); swosPlayerSpriteSetY(1, 401 << 16);
    swosWriteWord(out1_1 + PLSPR_OFF_SENT_AWAY, 1);
    swosPlayerSpriteSetX(2, 310 << 16); swosPlayerSpriteSetY(2, 410 << 16);
    swosPlayerSpriteSetPlayerState(2, PLSTATE_TACKLING);
    swosPlayerSpriteSetX(3, 340 << 16); swosPlayerSpriteSetY(3, 440 << 16);
    swosUpdateTeamControls(true);
    compareFullBuffer("update_team_controls_disqualified_sentaway_and_tackling");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 0);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PLAYER_NUMBER, 1);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_BALL_IN_PLAY, 1);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_BALL_OUT_OF_PLAY_OR_KEEPER, 1);
    swosBallSpriteSetXPixels(300); swosBallSpriteSetYPixels(400);
    swosPlayerSpriteSetX(1, 305 << 16); swosPlayerSpriteSetY(1, 404 << 16);
    swosWriteDword(out1_1 + PLSPR_OFF_BALL_DISTANCE, 50);
    swosPlayerSpriteSetX(2, 500 << 16); swosPlayerSpriteSetY(2, 700 << 16);
    swosWriteDword(out1_2 + PLSPR_OFF_BALL_DISTANCE, 90000);
    swosUpdateTeamControls(true);
    compareFullBuffer("update_team_controls_stoppage_pass_to_player");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PLAYER_NUMBER, 0);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_BALL_OUT_OF_PLAY, 0);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_RESET_CONTROLS, 0);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, 3);
    swosWriteByte(TEAMDATA_TOP_BASE + TEAMDATA_OFF_QUICK_FIRE, 1);
    swosWriteByte(TEAMDATA_TOP_BASE + TEAMDATA_OFF_NORMAL_FIRE, 1);
    swosWriteByte(TEAMDATA_TOP_BASE + TEAMDATA_OFF_FIRE_PRESSED, 1);
    swosWriteByte(TEAMDATA_TOP_BASE + TEAMDATA_OFF_FIRE_THIS_FRAME, 1);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_FIRE_COUNTER, 7);
    swosWriteWord(ADDR_g_inSubstitutesMenu, 1);
    swosUpdateTeamControls(true);
    compareFullBuffer("update_team_controls_bench_reset");

    printf("\nchecked %d scenarios\n", g_checked);
    if (g_failures == 0) {
        printf("all %d checks passed\n", g_checked);
        return 0;
    }
    printf("%d of %d checks FAILED\n", g_failures, g_checked);
    return 1;
}
