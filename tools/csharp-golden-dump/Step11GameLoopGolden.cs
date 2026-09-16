// Generates the step-11B (GameLoop.cs itself) differential-test golden
// dumps. GameLoop.cs is the per-tick orchestrator: Tick() -> UpdateTimers +
// CoreGameUpdate, and CoreGameUpdate's own UpdateGameTimersAndCameraBreakMode
// is the ~1400-line stoppage/restart state machine (penalty-shootout pause,
// the ST_WAITING_ON_PLAYER accumulator + CPU safety net, the fire-press
// ceremony-skip paths, the stoppageEventTimer countdown, the
// DispatchStoppageEventTriggered gameState dispatch, and the nine-mode
// break-camera-mode ladder).
using OpenSwos.Sim.Port;
using OpenSwos.SwosVm;

public static class Step11GameLoopGolden
{
    private const int kMemSize = 0x60000;
    private const int kTopTeamInGame = 0x4FE60;
    private const int kBotTeamInGame = 0x4FEA0;

    public static void Run(string outDir)
    {
        Directory.CreateDirectory(outDir);
        int count = 0;

        void Scenario(string name, Action setup, Action act)
        {
            Memory.Init(pcMode: true);
            setup();
            act();
            byte[] dump = Memory.View(0, kMemSize).ToArray();
            File.WriteAllBytes(Path.Combine(outDir, $"s11gl_{name}.bin"), dump);
            count++;
        }

        int top = TeamData.TopBase, bot = TeamData.BottomBase;
        int t1_1 = PlayerSprite.Base(1);

        void WireTeams()
        {
            Memory.WriteDword(Memory.Addr.topTeamInGame, kTopTeamInGame);
            Memory.WriteDword(Memory.Addr.bottomTeamInGame, kBotTeamInGame);
            Memory.WriteWord(top + TeamData.OffTeamNumber, 1);
            Memory.WriteWord(bot + TeamData.OffTeamNumber, 2);
            Memory.WriteDword(top + TeamData.OffOpponentsTeam, bot);
            Memory.WriteDword(bot + TeamData.OffOpponentsTeam, top);
        }

        // ==================================================================
        // Tick / UpdateTimers / CoreGameUpdate -- top-level entry points
        // ==================================================================
        Scenario("update_timers_normal_tick", () =>
        {
            Memory.WriteWord(Memory.Addr.currentGameTick, 100);
            Memory.WriteWord(Memory.Addr.lastGameTick, 90);
            Memory.WriteWord(Memory.Addr.spaceReplayTimer, 5);
        }, () => GameLoop.UpdateTimers());

        Scenario("tick_full_entry_runs_core_update", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            Memory.WriteWord(Memory.Addr.gameState, 100);
        }, () => GameLoop.Tick());

        Scenario("core_game_update_in_progress_minimal", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            Memory.WriteWord(Memory.Addr.gameState, 100);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
        }, () => GameLoop.CoreGameUpdate());

        // ==================================================================
        // UpdateFireBlocked / SelectTeamForUpdate
        // ==================================================================
        Scenario("fire_blocked_clears_when_released", () =>
        {
            Memory.WriteWord(Memory.Addr.fireBlocked, 1);
        }, () => { _ = GameLoop.UpdateFireBlocked(); });

        Scenario("select_team_alternates", () =>
        {
        }, () =>
        {
            _ = GameLoop.SelectTeamForUpdate();
            _ = GameLoop.SelectTeamForUpdate();
        });

        // ==================================================================
        // UpdateGameTimersAndCameraBreakMode -- top-level branches
        // ==================================================================
        Scenario("interval_seed_correction_applied", () =>
        {
            Memory.WriteWord(Memory.Addr.m_goalCameraInterval, 50);
            Memory.WriteWord(Memory.Addr.m_allowPlayerControlCameraInterval, 75);
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        Scenario("in_progress_bumps_in_game_counter", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 3);
            Memory.WriteWord(Memory.Addr.inGameCounter, 10);
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        Scenario("not_in_progress_bumps_stoppage_totals", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 0);
            Memory.WriteWord(Memory.Addr.gameState, 100); // not 21..30
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 2);
            Memory.WriteWord(Memory.Addr.stoppageEventTimer, 500);
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        Scenario("waiting_on_player_human_returns_early", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 102);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteDword(Memory.Addr.lastTeamPlayedBeforeBreak, top);
            Memory.WriteWord(top + TeamData.OffPlayerNumber, 1); // human
            Memory.WriteWord(Memory.Addr.m_initalKickInterval, 825);
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        Scenario("waiting_on_player_cpu_interval_not_reached", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 102);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteDword(Memory.Addr.lastTeamPlayedBeforeBreak, top);
            Memory.WriteWord(top + TeamData.OffPlayerNumber, 0); // CPU
            Memory.WriteWord(Memory.Addr.stoppageTimerActive, 100);
            Memory.WriteWord(Memory.Addr.m_initalKickInterval, 825);
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        Scenario("waiting_on_player_cpu_interval_reached_kicks_off", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 102);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteDword(Memory.Addr.lastTeamPlayedBeforeBreak, top);
            Memory.WriteWord(top + TeamData.OffPlayerNumber, 0);
            Memory.WriteWord(Memory.Addr.stoppageTimerActive, 824);
            Memory.WriteWord(Memory.Addr.m_initalKickInterval, 825);
            Memory.WriteWord(Memory.Addr.teamStarting, 1);
            Memory.WriteWord(Memory.Addr.teamPlayingUp, 1);
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        Scenario("waiting_on_player_safety_net_force_fires", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 102);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteDword(Memory.Addr.lastTeamPlayedBeforeBreak, top);
            Memory.WriteWord(top + TeamData.OffPlayerNumber, 0);
            Memory.WriteWord(Memory.Addr.stoppageTimerActive, 1650);
            Memory.WriteWord(Memory.Addr.m_initalKickInterval, 825);
            TeamData.SetControlledPlayer(true, t1_1);
            PlayerSprite.SetX(1, 300 << 16); PlayerSprite.SetY(1, 400 << 16);
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        Scenario("fire_fast_forward_halftime_result", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 0);
            Memory.WriteWord(Memory.Addr.gameState, 25);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteByte(top + TeamData.OffFirePressed, 1);
            Memory.WriteWord(Memory.Addr.teamStarting, 1);
            Memory.WriteWord(Memory.Addr.teamPlayingUp, 1);
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        Scenario("fire_fast_forward_fulltime_result", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 0);
            Memory.WriteWord(Memory.Addr.gameState, 26);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteByte(top + TeamData.OffFirePressed, 1);
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        Scenario("fire_fast_forward_starting_game", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 0);
            Memory.WriteWord(Memory.Addr.gameState, 21);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteByte(top + TeamData.OffFirePressed, 1);
            Memory.WriteWord(Memory.Addr.teamStarting, 1);
            Memory.WriteWord(Memory.Addr.teamPlayingUp, 2);
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        Scenario("fire_fast_forward_via_coach_pl2", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 0);
            Memory.WriteWord(Memory.Addr.gameState, 22);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteWord(top + TeamData.OffPlayerCoachNumber, 2);
            Memory.WriteWord(Memory.Addr.ic_pl2Fire, 1);
            Memory.WriteWord(Memory.Addr.teamStarting, 2);
            Memory.WriteWord(Memory.Addr.teamPlayingUp, 2);
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        Scenario("stoppage_event_timer_counts_down_no_dispatch", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 0);
            Memory.WriteWord(Memory.Addr.gameState, 100);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 3);
            Memory.WriteWord(Memory.Addr.stoppageEventTimer, 50);
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        // ==================================================================
        // DispatchStoppageEventTriggered -- every gameState arm
        // ==================================================================
        Scenario("dispatch_stoppage_halftime_result_gone", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 101);
            Memory.WriteWord(Memory.Addr.gameState, 25);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteWord(Memory.Addr.stoppageEventTimer, 1);
            Memory.WriteWord(Memory.Addr.teamPlayingUp, 1);
            Memory.WriteWord(Memory.Addr.teamStarting, 1);
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        Scenario("dispatch_stoppage_fulltime_result_gone", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 101);
            Memory.WriteWord(Memory.Addr.gameState, 26);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteWord(Memory.Addr.stoppageEventTimer, 1);
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        Scenario("dispatch_stoppage_starting_game", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 101);
            Memory.WriteWord(Memory.Addr.gameState, 21);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteWord(Memory.Addr.stoppageEventTimer, 1);
            Memory.WriteWord(Memory.Addr.teamStarting, 1);
            Memory.WriteWord(Memory.Addr.teamPlayingUp, 1);
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        Scenario("dispatch_stoppage_camera_going_to_showers", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 101);
            Memory.WriteWord(Memory.Addr.gameState, 22);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteWord(Memory.Addr.stoppageEventTimer, 1);
            Memory.WriteWord(Memory.Addr.teamStarting, 2);
            Memory.WriteWord(Memory.Addr.teamPlayingUp, 1);
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        Scenario("dispatch_stoppage_first_half_ended", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 101);
            Memory.WriteWord(Memory.Addr.gameState, 29);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteWord(Memory.Addr.stoppageEventTimer, 1);
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        Scenario("dispatch_stoppage_going_to_halftime", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 101);
            Memory.WriteWord(Memory.Addr.gameState, 23);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteWord(Memory.Addr.stoppageEventTimer, 1);
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        Scenario("dispatch_stoppage_game_ended", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 101);
            Memory.WriteWord(Memory.Addr.gameState, 30);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteWord(Memory.Addr.stoppageEventTimer, 1);
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        Scenario("dispatch_stoppage_players_going_to_shower", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 101);
            Memory.WriteWord(Memory.Addr.gameState, 24);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteWord(Memory.Addr.stoppageEventTimer, 1);
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        Scenario("dispatch_stoppage_first_extra_starting", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 101);
            Memory.WriteWord(Memory.Addr.gameState, 27);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteWord(Memory.Addr.stoppageEventTimer, 1);
            Memory.WriteWord(Memory.Addr.teamStarting, 1);
            Memory.WriteWord(Memory.Addr.teamPlayingUp, 1);
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        Scenario("dispatch_stoppage_first_extra_ended", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 101);
            Memory.WriteWord(Memory.Addr.gameState, 28);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteWord(Memory.Addr.stoppageEventTimer, 1);
            Memory.WriteWord(Memory.Addr.teamStarting, 2);
            Memory.WriteWord(Memory.Addr.teamPlayingUp, 2);
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        Scenario("dispatch_stoppage_plain_arms_break_ladder_mode0", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 101);
            Memory.WriteWord(Memory.Addr.gameState, 13); // ST_FOUL -- plain stoppage
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteWord(Memory.Addr.stoppageEventTimer, 1);
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        Scenario("dispatch_stoppage_plain_keeper_holds_arms_clock_panel", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 101);
            Memory.WriteWord(Memory.Addr.gameState, 3); // ST_KEEPER_HOLDS_BALL
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteWord(Memory.Addr.stoppageEventTimer, 1);
            Memory.WriteWord(Memory.Addr.g_inSubstitutesMenu, 0);
            Memory.WriteWord(Memory.Addr.g_cameraLeavingSubsTimer, 0);
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        // ==================================================================
        // DispatchBreakCameraMode -- guards + all nine modes
        // ==================================================================
        Scenario("break_mode_guard_subs_menu_blocks", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 101);
            Memory.WriteWord(Memory.Addr.gameState, 13);
            Memory.WriteWord(Memory.Addr.stoppageEventTimer, 0);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteWord(Memory.Addr.breakCameraMode, 0);
            Memory.WriteWord(Memory.Addr.g_inSubstitutesMenu, 1);
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        Scenario("mode0_ball_moving_no_transition", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 101);
            Memory.WriteWord(Memory.Addr.gameState, 13);
            Memory.WriteWord(Memory.Addr.stoppageEventTimer, 0);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteWord(Memory.Addr.breakCameraMode, 0);
            BallSprite.DeltaX = 5 << 16;
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        Scenario("mode0_ball_stopped_transitions_to_mode1", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 101);
            Memory.WriteWord(Memory.Addr.gameState, 13);
            Memory.WriteWord(Memory.Addr.stoppageEventTimer, 0);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteWord(Memory.Addr.breakCameraMode, 0);
            Memory.WriteWord(Memory.Addr.m_goalCameraInterval, 55);
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        Scenario("mode1_no_goal_camera_places_ball_at_foul_spot", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 101);
            Memory.WriteWord(Memory.Addr.gameState, 13);
            Memory.WriteWord(Memory.Addr.stoppageEventTimer, 0);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteWord(Memory.Addr.breakCameraMode, 1);
            Memory.WriteWord(Memory.Addr.goalCameraMode, 0);
            Memory.WriteWord(Memory.Addr.foulXCoordinate, 250);
            Memory.WriteWord(Memory.Addr.foulYCoordinate, 400);
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        Scenario("mode2_writes_dest_reached_and_advances", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 101);
            Memory.WriteWord(Memory.Addr.gameState, 13);
            Memory.WriteWord(Memory.Addr.stoppageEventTimer, 0);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteWord(Memory.Addr.breakCameraMode, 2);
            Memory.WriteWord(Memory.Addr.whichCard, 0);
            Memory.WriteWord(Memory.Addr.cameraCoordinatesValid, 1);
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        Scenario("mode3_waits_until_all_players_arrived", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 101);
            Memory.WriteWord(Memory.Addr.gameState, 13);
            Memory.WriteWord(Memory.Addr.stoppageEventTimer, 0);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteWord(Memory.Addr.breakCameraMode, 3);
            Memory.WriteWord(Memory.Addr.refState, 0);
            Memory.WriteWord(Memory.Addr.injuriesForever, 0);
            for (int slot = 0; slot < PlayerSprite.TotalSlots; slot++)
                Memory.WriteWord(PlayerSprite.Base(slot) + PlayerSprite.OffDestReachedState, 3);
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        Scenario("mode4_subs_menu_blocks_transition", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 101);
            Memory.WriteWord(Memory.Addr.gameState, 13);
            Memory.WriteWord(Memory.Addr.stoppageEventTimer, 0);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteWord(Memory.Addr.breakCameraMode, 4);
            // g_inSubstitutesMenu guarded at DispatchBreakCameraMode entry
            // (already covered above); exercise mode4's OWN body directly by
            // starting from a clean (non-subs-menu) state instead so the
            // ladder actually reaches mode4's write.
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        Scenario("mode5_clears_card_state", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 101);
            Memory.WriteWord(Memory.Addr.gameState, 13);
            Memory.WriteWord(Memory.Addr.stoppageEventTimer, 0);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteWord(Memory.Addr.breakCameraMode, 5);
            Memory.WriteWord(Memory.Addr.whichCard, 1);
            Memory.WriteDword(Memory.Addr.bookedPlayer, t1_1);
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        Scenario("mode6_non_keeper_arms_clock_panel", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 101);
            Memory.WriteWord(Memory.Addr.gameState, 13);
            Memory.WriteWord(Memory.Addr.stoppageEventTimer, 0);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteWord(Memory.Addr.breakCameraMode, 6);
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        Scenario("mode7_waits_for_controlled_player", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 101);
            Memory.WriteWord(Memory.Addr.gameState, 13);
            Memory.WriteWord(Memory.Addr.stoppageEventTimer, 0);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteWord(Memory.Addr.breakCameraMode, 7);
            Memory.WriteDword(Memory.Addr.lastTeamPlayedBeforeBreak, top);
            Memory.WriteWord(Memory.Addr.ballOutOfGameTimer, 10);
            Memory.WriteWord(Memory.Addr.m_allowPlayerControlCameraInterval, 550);
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        Scenario("mode7_controlled_player_present_arms_result_panel", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 101);
            Memory.WriteWord(Memory.Addr.gameState, 13);
            Memory.WriteWord(Memory.Addr.stoppageEventTimer, 0);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteWord(Memory.Addr.breakCameraMode, 7);
            Memory.WriteDword(Memory.Addr.lastTeamPlayedBeforeBreak, top);
            TeamData.SetControlledPlayer(true, t1_1);
            Memory.WriteWord(Memory.Addr.ballOutOfGameTimer, 10);
            Memory.WriteWord(Memory.Addr.m_allowPlayerControlCameraInterval, 550);
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        Scenario("mode7_timeout_falls_back_to_keeper_claim_check", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 101);
            Memory.WriteWord(Memory.Addr.gameState, 3);
            Memory.WriteWord(Memory.Addr.stoppageEventTimer, 0);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteWord(Memory.Addr.breakCameraMode, 7);
            Memory.WriteDword(Memory.Addr.lastTeamPlayedBeforeBreak, top);
            Memory.WriteWord(Memory.Addr.ballOutOfGameTimer, 600);
            Memory.WriteWord(Memory.Addr.m_allowPlayerControlCameraInterval, 550);
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        Scenario("mode8_terminal_parks_waiting_on_player", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 101);
            Memory.WriteWord(Memory.Addr.gameState, 13);
            Memory.WriteWord(Memory.Addr.stoppageEventTimer, 0);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteWord(Memory.Addr.breakCameraMode, 8);
        }, () => GameLoop.UpdateGameTimersAndCameraBreakMode());

        // ==================================================================
        // Half-end / ET-end / shower / game-over state transitions
        // ==================================================================
        Scenario("set_camera_moving_to_shower_state", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.teamPlayingUp, 1);
            Memory.WriteWord(Memory.Addr.teamStarting, 1);
            Memory.WriteWord(top + TeamData.OffPlayerNumber, 1);
            Memory.WriteWord(bot + TeamData.OffPlayerNumber, 0);
        }, () => GameLoop.SetCameraMovingToShowerState());

        Scenario("first_half_just_ended_recovers_energy", () =>
        {
            for (int slot = 0; slot < PlayerSprite.TotalSlots; slot++)
                Memory.WriteWord(PlayerSprite.Base(slot) + PlayerSprite.OffEnergy, 1000);
        }, () => GameLoop.FirstHalfJustEnded());

        Scenario("go_to_halftime", () =>
        {
        }, () => GameLoop.GoToHalftime());

        Scenario("game_over_sets_result_after_the_game", () =>
        {
        }, () => GameLoop.GameOver());

        // ==================================================================
        // IsMatchRunning / SetMatchRunning / interval setters
        // ==================================================================
        Scenario("match_running_flag_round_trip", () =>
        {
        }, () =>
        {
            GameLoop.SetMatchRunning(true);
            _ = GameLoop.IsMatchRunning();
            GameLoop.SetMatchRunning(false);
            _ = GameLoop.IsMatchRunning();
        });

        Scenario("interval_setters", () =>
        {
        }, () =>
        {
            GameLoop.SetPenaltiesInterval(120);
            GameLoop.SetInitalKickInterval(900);
            GameLoop.SetGoalCameraInterval(60);
            GameLoop.SetAllowPlayerControlCameraInterval(500);
        });

        Console.WriteLine($"wrote {count} Step11GameLoop scenario dumps ({kMemSize} bytes each) to {outDir}");
    }
}
