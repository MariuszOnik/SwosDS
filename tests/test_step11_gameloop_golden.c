// Step-11B (GameLoop.cs itself) differential test: compares FULL VM/Memory
// state against golden dumps produced by the REAL C# (tools/csharp-golden-
// dump/Step11GameLoopGolden.cs).
//
// Regenerate build/golden/s11gl_*.bin with:
//   cd tools/csharp-golden-dump && dotnet run -c Release -- ../../build/golden
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "swos_addr.h"
#include "swos_ball_sprite.h"
#include "swos_game_loop.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"
#include "swos_team_data.h"

#define GOLDEN_DIR "build/golden"

static int g_failures = 0;
static int g_checked = 0;

static void compareFullBuffer(const char *label) {
    char path[256];
    snprintf(path, sizeof(path), "%s/s11gl_%s.bin", GOLDEN_DIR, label);

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

    // ==== Tick / UpdateTimers / CoreGameUpdate ====
    swosMemoryInit(true);
    swosWriteWord(ADDR_currentGameTick, 100);
    swosWriteWord(ADDR_lastGameTick, 90);
    swosWriteWord(ADDR_spaceReplayTimer, 5);
    swosGameLoopUpdateTimers();
    compareFullBuffer("update_timers_normal_tick");

    swosMemoryInit(true);
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosWriteWord(ADDR_gameState, 100);
    swosGameLoopTick();
    compareFullBuffer("tick_full_entry_runs_core_update");

    swosMemoryInit(true);
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosWriteWord(ADDR_gameState, 100);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosGameLoopCoreGameUpdate();
    compareFullBuffer("core_game_update_in_progress_minimal");

    // ==== UpdateFireBlocked / SelectTeamForUpdate ====
    swosMemoryInit(true);
    swosWriteWord(ADDR_fireBlocked, 1);
    (void)swosGameLoopUpdateFireBlocked();
    compareFullBuffer("fire_blocked_clears_when_released");

    swosMemoryInit(true);
    (void)swosGameLoopSelectTeamForUpdate();
    (void)swosGameLoopSelectTeamForUpdate();
    compareFullBuffer("select_team_alternates");

    // ==== UpdateGameTimersAndCameraBreakMode -- top-level branches ====
    swosMemoryInit(true);
    swosWriteWord(ADDR_m_goalCameraInterval, 50);
    swosWriteWord(ADDR_m_allowPlayerControlCameraInterval, 75);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("interval_seed_correction_applied");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosWriteWord(ADDR_lastFrameTicks, 3);
    swosWriteWord(ADDR_inGameCounter, 10);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("in_progress_bumps_in_game_counter");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 0);
    swosWriteWord(ADDR_gameState, 100);
    swosWriteWord(ADDR_lastFrameTicks, 2);
    swosWriteWord(ADDR_stoppageEventTimer, 500);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("not_in_progress_bumps_stoppage_totals");

    swosMemoryInit(true);
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 102);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)top);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_NUMBER, 1);
    swosWriteWord(ADDR_m_initalKickInterval, 825);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("waiting_on_player_human_returns_early");

    swosMemoryInit(true);
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 102);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)top);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_NUMBER, 0);
    swosWriteWord(ADDR_stoppageTimerActive, 100);
    swosWriteWord(ADDR_m_initalKickInterval, 825);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("waiting_on_player_cpu_interval_not_reached");

    swosMemoryInit(true);
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 102);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)top);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_NUMBER, 0);
    swosWriteWord(ADDR_stoppageTimerActive, 824);
    swosWriteWord(ADDR_m_initalKickInterval, 825);
    swosWriteWord(ADDR_teamStarting, 1);
    swosWriteWord(ADDR_teamPlayingUp, 1);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("waiting_on_player_cpu_interval_reached_kicks_off");

    swosMemoryInit(true);
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 102);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)top);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_NUMBER, 0);
    swosWriteWord(ADDR_stoppageTimerActive, 1650);
    swosWriteWord(ADDR_m_initalKickInterval, 825);
    swosTeamDataSetControlledPlayer(true, t1_1);
    swosPlayerSpriteSetX(1, 300 << 16); swosPlayerSpriteSetY(1, 400 << 16);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("waiting_on_player_safety_net_force_fires");

    swosMemoryInit(true);
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 0);
    swosWriteWord(ADDR_gameState, 25);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteByte(top + TEAMDATA_OFF_FIRE_PRESSED, 1);
    swosWriteWord(ADDR_teamStarting, 1);
    swosWriteWord(ADDR_teamPlayingUp, 1);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("fire_fast_forward_halftime_result");

    swosMemoryInit(true);
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 0);
    swosWriteWord(ADDR_gameState, 26);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteByte(top + TEAMDATA_OFF_FIRE_PRESSED, 1);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("fire_fast_forward_fulltime_result");

    swosMemoryInit(true);
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 0);
    swosWriteWord(ADDR_gameState, 21);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteByte(top + TEAMDATA_OFF_FIRE_PRESSED, 1);
    swosWriteWord(ADDR_teamStarting, 1);
    swosWriteWord(ADDR_teamPlayingUp, 2);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("fire_fast_forward_starting_game");

    swosMemoryInit(true);
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 0);
    swosWriteWord(ADDR_gameState, 22);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_COACH_NUMBER, 2);
    swosWriteWord(ADDR_ic_pl2Fire, 1);
    swosWriteWord(ADDR_teamStarting, 2);
    swosWriteWord(ADDR_teamPlayingUp, 2);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("fire_fast_forward_via_coach_pl2");

    swosMemoryInit(true);
    swosWriteWord(ADDR_gameStatePl, 0);
    swosWriteWord(ADDR_gameState, 100);
    swosWriteWord(ADDR_lastFrameTicks, 3);
    swosWriteWord(ADDR_stoppageEventTimer, 50);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("stoppage_event_timer_counts_down_no_dispatch");

    // ==== DispatchStoppageEventTriggered -- every gameState arm ====
    swosMemoryInit(true);
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 101);
    swosWriteWord(ADDR_gameState, 25);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteWord(ADDR_stoppageEventTimer, 1);
    swosWriteWord(ADDR_teamPlayingUp, 1);
    swosWriteWord(ADDR_teamStarting, 1);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("dispatch_stoppage_halftime_result_gone");

    swosMemoryInit(true);
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 101);
    swosWriteWord(ADDR_gameState, 26);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteWord(ADDR_stoppageEventTimer, 1);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("dispatch_stoppage_fulltime_result_gone");

    swosMemoryInit(true);
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 101);
    swosWriteWord(ADDR_gameState, 21);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteWord(ADDR_stoppageEventTimer, 1);
    swosWriteWord(ADDR_teamStarting, 1);
    swosWriteWord(ADDR_teamPlayingUp, 1);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("dispatch_stoppage_starting_game");

    swosMemoryInit(true);
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 101);
    swosWriteWord(ADDR_gameState, 22);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteWord(ADDR_stoppageEventTimer, 1);
    swosWriteWord(ADDR_teamStarting, 2);
    swosWriteWord(ADDR_teamPlayingUp, 1);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("dispatch_stoppage_camera_going_to_showers");

    swosMemoryInit(true);
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 101);
    swosWriteWord(ADDR_gameState, 29);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteWord(ADDR_stoppageEventTimer, 1);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("dispatch_stoppage_first_half_ended");

    swosMemoryInit(true);
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 101);
    swosWriteWord(ADDR_gameState, 23);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteWord(ADDR_stoppageEventTimer, 1);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("dispatch_stoppage_going_to_halftime");

    swosMemoryInit(true);
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 101);
    swosWriteWord(ADDR_gameState, 30);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteWord(ADDR_stoppageEventTimer, 1);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("dispatch_stoppage_game_ended");

    swosMemoryInit(true);
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 101);
    swosWriteWord(ADDR_gameState, 24);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteWord(ADDR_stoppageEventTimer, 1);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("dispatch_stoppage_players_going_to_shower");

    swosMemoryInit(true);
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 101);
    swosWriteWord(ADDR_gameState, 27);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteWord(ADDR_stoppageEventTimer, 1);
    swosWriteWord(ADDR_teamStarting, 1);
    swosWriteWord(ADDR_teamPlayingUp, 1);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("dispatch_stoppage_first_extra_starting");

    swosMemoryInit(true);
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 101);
    swosWriteWord(ADDR_gameState, 28);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteWord(ADDR_stoppageEventTimer, 1);
    swosWriteWord(ADDR_teamStarting, 2);
    swosWriteWord(ADDR_teamPlayingUp, 2);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("dispatch_stoppage_first_extra_ended");

    swosMemoryInit(true);
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 101);
    swosWriteWord(ADDR_gameState, 13);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteWord(ADDR_stoppageEventTimer, 1);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("dispatch_stoppage_plain_arms_break_ladder_mode0");

    swosMemoryInit(true);
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 101);
    swosWriteWord(ADDR_gameState, 3);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteWord(ADDR_stoppageEventTimer, 1);
    swosWriteWord(ADDR_g_inSubstitutesMenu, 0);
    swosWriteWord(ADDR_g_cameraLeavingSubsTimer, 0);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("dispatch_stoppage_plain_keeper_holds_arms_clock_panel");

    // ==== DispatchBreakCameraMode -- guards + all nine modes ====
    swosMemoryInit(true);
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 101);
    swosWriteWord(ADDR_gameState, 13);
    swosWriteWord(ADDR_stoppageEventTimer, 0);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteWord(ADDR_breakCameraMode, 0);
    swosWriteWord(ADDR_g_inSubstitutesMenu, 1);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("break_mode_guard_subs_menu_blocks");

    swosMemoryInit(true);
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 101);
    swosWriteWord(ADDR_gameState, 13);
    swosWriteWord(ADDR_stoppageEventTimer, 0);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteWord(ADDR_breakCameraMode, 0);
    swosBallSpriteSetDeltaX(5 << 16);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("mode0_ball_moving_no_transition");

    swosMemoryInit(true);
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 101);
    swosWriteWord(ADDR_gameState, 13);
    swosWriteWord(ADDR_stoppageEventTimer, 0);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteWord(ADDR_breakCameraMode, 0);
    swosWriteWord(ADDR_m_goalCameraInterval, 55);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("mode0_ball_stopped_transitions_to_mode1");

    swosMemoryInit(true);
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 101);
    swosWriteWord(ADDR_gameState, 13);
    swosWriteWord(ADDR_stoppageEventTimer, 0);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteWord(ADDR_breakCameraMode, 1);
    swosWriteWord(ADDR_goalCameraMode, 0);
    swosWriteWord(ADDR_foulXCoordinate, 250);
    swosWriteWord(ADDR_foulYCoordinate, 400);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("mode1_no_goal_camera_places_ball_at_foul_spot");

    swosMemoryInit(true);
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 101);
    swosWriteWord(ADDR_gameState, 13);
    swosWriteWord(ADDR_stoppageEventTimer, 0);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteWord(ADDR_breakCameraMode, 2);
    swosWriteWord(ADDR_whichCard, 0);
    swosWriteWord(ADDR_cameraCoordinatesValid, 1);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("mode2_writes_dest_reached_and_advances");

    swosMemoryInit(true);
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 101);
    swosWriteWord(ADDR_gameState, 13);
    swosWriteWord(ADDR_stoppageEventTimer, 0);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteWord(ADDR_breakCameraMode, 3);
    swosWriteWord(ADDR_refState, 0);
    swosWriteWord(ADDR_injuriesForever, 0);
    for (int slot = 0; slot < PLSPR_TOTAL_SLOTS; slot++)
        swosWriteWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_DEST_REACHED_STATE, 3);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("mode3_waits_until_all_players_arrived");

    swosMemoryInit(true);
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 101);
    swosWriteWord(ADDR_gameState, 13);
    swosWriteWord(ADDR_stoppageEventTimer, 0);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteWord(ADDR_breakCameraMode, 4);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("mode4_subs_menu_blocks_transition");

    swosMemoryInit(true);
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 101);
    swosWriteWord(ADDR_gameState, 13);
    swosWriteWord(ADDR_stoppageEventTimer, 0);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteWord(ADDR_breakCameraMode, 5);
    swosWriteWord(ADDR_whichCard, 1);
    swosWriteDword(ADDR_bookedPlayer, (uint32_t)t1_1);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("mode5_clears_card_state");

    swosMemoryInit(true);
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 101);
    swosWriteWord(ADDR_gameState, 13);
    swosWriteWord(ADDR_stoppageEventTimer, 0);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteWord(ADDR_breakCameraMode, 6);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("mode6_non_keeper_arms_clock_panel");

    swosMemoryInit(true);
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 101);
    swosWriteWord(ADDR_gameState, 13);
    swosWriteWord(ADDR_stoppageEventTimer, 0);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteWord(ADDR_breakCameraMode, 7);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)top);
    swosWriteWord(ADDR_ballOutOfGameTimer, 10);
    swosWriteWord(ADDR_m_allowPlayerControlCameraInterval, 550);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("mode7_waits_for_controlled_player");

    swosMemoryInit(true);
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 101);
    swosWriteWord(ADDR_gameState, 13);
    swosWriteWord(ADDR_stoppageEventTimer, 0);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteWord(ADDR_breakCameraMode, 7);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)top);
    swosTeamDataSetControlledPlayer(true, t1_1);
    swosWriteWord(ADDR_ballOutOfGameTimer, 10);
    swosWriteWord(ADDR_m_allowPlayerControlCameraInterval, 550);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("mode7_controlled_player_present_arms_result_panel");

    swosMemoryInit(true);
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 101);
    swosWriteWord(ADDR_gameState, 3);
    swosWriteWord(ADDR_stoppageEventTimer, 0);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteWord(ADDR_breakCameraMode, 7);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)top);
    swosWriteWord(ADDR_ballOutOfGameTimer, 600);
    swosWriteWord(ADDR_m_allowPlayerControlCameraInterval, 550);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("mode7_timeout_falls_back_to_keeper_claim_check");

    swosMemoryInit(true);
    wireTeams(top, bot);
    swosWriteWord(ADDR_gameStatePl, 101);
    swosWriteWord(ADDR_gameState, 13);
    swosWriteWord(ADDR_stoppageEventTimer, 0);
    swosWriteWord(ADDR_lastFrameTicks, 1);
    swosWriteWord(ADDR_breakCameraMode, 8);
    swosGameLoopUpdateGameTimersAndCameraBreakMode();
    compareFullBuffer("mode8_terminal_parks_waiting_on_player");

    // ==== Half-end / ET-end / shower / game-over transitions ====
    swosMemoryInit(true);
    wireTeams(top, bot);
    swosWriteWord(ADDR_teamPlayingUp, 1);
    swosWriteWord(ADDR_teamStarting, 1);
    swosWriteWord(top + TEAMDATA_OFF_PLAYER_NUMBER, 1);
    swosWriteWord(bot + TEAMDATA_OFF_PLAYER_NUMBER, 0);
    swosGameLoopSetCameraMovingToShowerState();
    compareFullBuffer("set_camera_moving_to_shower_state");

    swosMemoryInit(true);
    for (int slot = 0; slot < PLSPR_TOTAL_SLOTS; slot++)
        swosWriteWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_ENERGY, 1000);
    swosGameLoopFirstHalfJustEnded();
    compareFullBuffer("first_half_just_ended_recovers_energy");

    swosMemoryInit(true);
    swosGameLoopGoToHalftime();
    compareFullBuffer("go_to_halftime");

    swosMemoryInit(true);
    swosGameLoopGameOver();
    compareFullBuffer("game_over_sets_result_after_the_game");

    // ==== IsMatchRunning / SetMatchRunning / interval setters ====
    swosMemoryInit(true);
    swosGameLoopSetMatchRunning(true);
    (void)swosGameLoopIsMatchRunning();
    swosGameLoopSetMatchRunning(false);
    (void)swosGameLoopIsMatchRunning();
    compareFullBuffer("match_running_flag_round_trip");

    swosMemoryInit(true);
    swosGameLoopSetPenaltiesInterval(120);
    swosGameLoopSetInitalKickInterval(900);
    swosGameLoopSetGoalCameraInterval(60);
    swosGameLoopSetAllowPlayerControlCameraInterval(500);
    compareFullBuffer("interval_setters");

    printf("\nchecked %d scenarios\n", g_checked);
    if (g_failures == 0) {
        printf("all %d checks passed\n", g_checked);
        return 0;
    }
    printf("%d of %d checks FAILED\n", g_failures, g_checked);
    return 1;
}
