// Generates the step-11 (full Bench.cs port) differential-test golden
// dumps. Bench.cs (1868 lines) is almost entirely private surface reached
// through two public entry points (UpdateBench/BenchCheckControls) plus
// ~25 small public accessors -- same shape as step 7B's UpdatePlayers.cs,
// so most scenarios drive the FSM through those entry points across one or
// more ticks rather than calling private helpers directly.
//
// Static-state discipline: every s_* field in Bench.cs is a C#-side static
// (not Memory-backed -- see the file's own header comment), so it persists
// across every scenario in this one-process harness. Every scenario calls
// Bench.InitBenchControls() first to reset that state, matching the same
// discipline established for GameTime/Result in step 10.
using OpenSwos.Sim.Port;
using OpenSwos.SwosVm;

public static class Step11BenchGolden
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
            Bench.InitBenchControls();
            setup();
            act();
            byte[] dump = Memory.View(0, kMemSize).ToArray();
            File.WriteAllBytes(Path.Combine(outDir, $"s11bench_{name}.bin"), dump);
            count++;
        }

        int top = TeamData.TopBase, bot = TeamData.BottomBase;
        int t1_1 = PlayerSprite.Base(1);
        int t2_1 = PlayerSprite.Base(12);

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
        // Simple accessors
        // ==================================================================
        Scenario("accessors_basic", () =>
        {
            WireTeams();
        }, () =>
        {
            _ = Bench.InBench();
            _ = Bench.GetBenchState();
            _ = Bench.InBenchMenus();
            _ = Bench.GetBenchY();
            _ = Bench.GetOpponentBenchY();
            _ = Bench.TrainingTopTeam();
            Bench.SetTrainingTopTeam(true);
            _ = Bench.GetBenchPlayerIndex();
            _ = Bench.GetBenchMenuSelectedPlayer();
            _ = Bench.GetSelectedFormationEntry();
            _ = Bench.PlayerToEnterGameIndex();
            _ = Bench.PlayerToBeSubstitutedIndex();
            _ = Bench.PlayerToBeSubstitutedPos();
            _ = Bench.GetBenchPlayerShirtNumber(true, 3);
            _ = Bench.InBenchOrGoingTo();
            _ = Bench.GoingToBenchDelay();
            _ = Bench.SubstituteInProgress();
            _ = Bench.NewPlayerAboutToGoIn();
            _ = Bench.GetBenchTeamBase();
            _ = Bench.GetBenchTeamGameBase();
            _ = Bench.BenchTeamIsTop();
            _ = Bench.GetBenchPlayerInfoAddr(2);
            _ = Bench.GetBenchPlayerPosition(2);
        });

        Scenario("set_substitute_in_progress", () =>
        {
        }, () => Bench.SetSubstituteInProgress());

        Scenario("get_bench_team_base_resyncs_on_team_number_mismatch", () =>
        {
            WireTeams();
            // InitBenchControls (teamPlayingUp default 0 -> falls to else
            // branch -> bottom) already set s_teamBase; now desync teamNumber
            // at that base to force the resync branch.
            Memory.WriteWord(Memory.Addr.teamPlayingUp, 1); // -> s_teamBase = TopBase after re-init
        }, () =>
        {
            Bench.InitBenchControls();
            Memory.WriteWord(top + TeamData.OffTeamNumber, 9); // mismatch vs s_teamNumber cache
            _ = Bench.GetBenchTeamBase();
        });

        // ==================================================================
        // InitBenchBeforeMatch / InitBenchControls
        // ==================================================================
        Scenario("init_bench_before_match_top_starts", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.teamPlayingUp, 1);
        }, () => Bench.InitBenchBeforeMatch());

        Scenario("init_bench_before_match_bottom_starts_training", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.teamPlayingUp, 2);
            Memory.WriteWord(Memory.Addr.g_trainingGame, 1);
        }, () => Bench.InitBenchBeforeMatch());

        // ==================================================================
        // UpdateBench / BenchCheckControls -- out-of-bench polling
        // ==================================================================
        Scenario("update_bench_blocked_by_wait_timer_ticks_down", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.g_waitForPlayerToGoInTimer, 5);
        }, () => Bench.UpdateBench());

        Scenario("update_bench_blocked_by_referee_active", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.refState, 2); // kRefWaitingPlayer -> RefereeActive() true
        }, () => Bench.UpdateBench());

        Scenario("update_bench_unavailable_game_in_progress", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
        }, () => Bench.UpdateBench());

        Scenario("update_bench_unavailable_ceremony_gamestate", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 0);
            Memory.WriteWord(Memory.Addr.gameState, 25); // ST_RESULT_ON_HALFTIME
        }, () => Bench.UpdateBench());

        Scenario("update_bench_cpu_team_never_invokes", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 0);
            Memory.WriteWord(Memory.Addr.gameState, 0);
            Memory.WriteWord(top + TeamData.OffPlayerNumber, 0);
            Memory.WriteWord(top + TeamData.OffPlayerCoachNumber, 0); // pure AI
            Memory.WriteWord(bot + TeamData.OffPlayerNumber, 0);
            Memory.WriteWord(bot + TeamData.OffPlayerCoachNumber, 0);
        }, () => Bench.UpdateBench());

        Scenario("update_bench_invoked_by_secondary_fire", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 0);
            Memory.WriteWord(Memory.Addr.gameState, 0);
            Bench.RequestBench1();
        }, () => Bench.UpdateBench());

        Scenario("update_bench_single_tap_does_not_invoke", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 0);
            Memory.WriteWord(Memory.Addr.gameState, 0);
            Memory.WriteWord(top + TeamData.OffPlayerNumber, 1);
            Memory.WriteWord(top + TeamData.OffDirection, 2); // one direction press
        }, () => Bench.UpdateBench());

        Scenario("update_bench_triple_tap_invokes", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameStatePl, 0);
            Memory.WriteWord(Memory.Addr.gameState, 0);
            Memory.WriteWord(top + TeamData.OffPlayerNumber, 1);
        }, () =>
        {
            // Alternate top/bottom polling means only every-other UpdateBench
            // call actually samples this team once neither bench{1,2}Called
            // is set -- force it by calling RequestBench-free but polling
            // enough ticks for the alternator to land on top and the tap
            // chain to arm+count three presses with releases between.
            for (int i = 0; i < 2; i++)
            {
                Memory.WriteWord(top + TeamData.OffDirection, 2);
                Bench.UpdateBench();
                Memory.WriteWord(top + TeamData.OffDirection, -1);
                Bench.UpdateBench();
            }
            Memory.WriteWord(top + TeamData.OffDirection, 2);
            Bench.UpdateBench();
        });

        // ==================================================================
        // InvokeBench (direct) + throw-in cleanup
        // ==================================================================
        Scenario("invoke_bench_normal", () =>
        {
            WireTeams();
        }, () => Bench.InvokeBench());

        Scenario("invoke_bench_clears_mid_throw_in", () =>
        {
            WireTeams();
            Memory.WriteDword(Memory.Addr.lastTeamPlayedBeforeBreak, top);
            TeamData.SetControlledPlayer(true, t1_1);
            PlayerSprite.SetPlayerState(1, 5); // kThrowIn
        }, () => Bench.InvokeBench());

        Scenario("invoke_bench_keeper_holds_ball_claims", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameState, 3); // ST_KEEPER_HOLDS_BALL
            Memory.WriteDword(Memory.Addr.lastTeamPlayedBeforeBreak, top);
        }, () => Bench.InvokeBench());

        // ==================================================================
        // In-bench menu navigation
        // ==================================================================
        Scenario("menu_arrow_navigation_down_then_up", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.g_inSubstitutesMenu, 1);
            Memory.WriteWord(top + TeamData.OffTeamNumber, 1);
            Memory.WriteWord(top + TeamData.OffPlayerNumber, 1);
            Memory.WriteWord(top + TeamData.OffPlayerCoachNumber, 1);
            Memory.WriteWord(Memory.Addr.team1NumSubs, 0);
        }, () =>
        {
            Memory.WriteDword(Memory.Addr.ic_pl1Events, (int)InputControls.GameControlEvents.kGameEventDown);
            Bench.UpdateBench();
        });

        Scenario("menu_fire_on_coach_row_enters_marking", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.g_inSubstitutesMenu, 1);
            Memory.WriteWord(top + TeamData.OffTeamNumber, 1);
            Memory.WriteWord(top + TeamData.OffPlayerNumber, 1);
            Memory.WriteWord(top + TeamData.OffPlayerCoachNumber, 1);
        }, () =>
        {
            Memory.WriteDword(Memory.Addr.ic_pl1Events, (int)InputControls.GameControlEvents.kGameEventKick);
            Bench.UpdateBench();
        });

        Scenario("menu_fire_on_substitute_row_enters_about_to_substitute", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.g_inSubstitutesMenu, 1);
            Memory.WriteWord(top + TeamData.OffTeamNumber, 1);
            Memory.WriteWord(top + TeamData.OffPlayerNumber, 1);
            Memory.WriteWord(top + TeamData.OffPlayerCoachNumber, 1);
            // Substitute at bench row 1 (players[11]) not already substituted.
            Memory.WriteByte(kTopTeamInGame + 11 * TeamDataLoader.PlayerInfoSize + TeamDataLoader.OffPosition, 3);
            Memory.WriteByte(kTopTeamInGame + 5 * TeamDataLoader.PlayerInfoSize + TeamDataLoader.OffPosition, 3);
            Memory.WriteByte(kTopTeamInGame + 5 * TeamDataLoader.PlayerInfoSize + TeamDataLoader.OffCards, 0);
        }, () =>
        {
            // First tick: navigate arrow to row 1.
            Memory.WriteDword(Memory.Addr.ic_pl1Events, (int)InputControls.GameControlEvents.kGameEventDown);
            Bench.UpdateBench();
            Memory.WriteDword(Memory.Addr.ic_pl1Events, 0);
            Bench.UpdateBench();
            // Fire to select the substitute.
            Memory.WriteDword(Memory.Addr.ic_pl1Events, (int)InputControls.GameControlEvents.kGameEventKick);
            Bench.UpdateBench();
        });

        Scenario("formation_menu_navigate_and_change_tactics", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.g_inSubstitutesMenu, 1);
            Memory.WriteWord(top + TeamData.OffTeamNumber, 1);
            Memory.WriteWord(top + TeamData.OffPlayerNumber, 1);
            Memory.WriteWord(top + TeamData.OffPlayerCoachNumber, 1);
            Memory.WriteWord(top + TeamData.OffTactics, 0);
        }, () =>
        {
            // Enter marking-players via coach-row fire, then go to formation
            // menu (fire while playerToBeSubstitutedOrd < 0).
            Memory.WriteDword(Memory.Addr.ic_pl1Events, (int)InputControls.GameControlEvents.kGameEventKick);
            Bench.UpdateBench();
            Memory.WriteDword(Memory.Addr.ic_pl1Events, 0);
            Bench.UpdateBench();
            Memory.WriteDword(Memory.Addr.ic_pl1Events, (int)InputControls.GameControlEvents.kGameEventKick);
            Bench.UpdateBench();
            Memory.WriteDword(Memory.Addr.ic_pl1Events, 0);
            Bench.UpdateBench();
            // Now in formation menu: move down then fire to change tactics.
            Memory.WriteDword(Memory.Addr.ic_pl1Events, (int)InputControls.GameControlEvents.kGameEventDown);
            Bench.UpdateBench();
            Memory.WriteDword(Memory.Addr.ic_pl1Events, 0);
            Bench.UpdateBench();
            Memory.WriteDword(Memory.Addr.ic_pl1Events, (int)InputControls.GameControlEvents.kGameEventKick);
            Bench.UpdateBench();
        });

        Scenario("leave_bench_via_left_right_motion", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.g_inSubstitutesMenu, 1);
            Memory.WriteWord(top + TeamData.OffTeamNumber, 1);
            Memory.WriteWord(top + TeamData.OffPlayerNumber, 1);
            Memory.WriteWord(top + TeamData.OffPlayerCoachNumber, 1);
        }, () =>
        {
            Memory.WriteDword(Memory.Addr.ic_pl1Events, (int)InputControls.GameControlEvents.kGameEventLeft);
            Bench.UpdateBench();
        });

        // ==================================================================
        // InitiateSubstitution / SubstitutePlayer -- direct through the FSM
        // ==================================================================
        Scenario("initiate_substitution_via_menu", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.g_inSubstitutesMenu, 1);
            Memory.WriteWord(top + TeamData.OffTeamNumber, 1);
            Memory.WriteWord(top + TeamData.OffPlayerNumber, 1);
            Memory.WriteWord(top + TeamData.OffPlayerCoachNumber, 1);
            Memory.WriteByte(kTopTeamInGame + 11 * TeamDataLoader.PlayerInfoSize + TeamDataLoader.OffPosition, 3);
            Memory.WriteByte(kTopTeamInGame + 3 * TeamDataLoader.PlayerInfoSize + TeamDataLoader.OffPosition, 3);
            Memory.WriteByte(kTopTeamInGame + 3 * TeamDataLoader.PlayerInfoSize + TeamDataLoader.OffCards, 0);
        }, () =>
        {
            // Arrow to row 1, fire to select substitute (enters
            // kAboutToSubstitute with FindInitialPlayerToBeSubstituted's
            // exact-match landing on ord=3's position).
            Memory.WriteDword(Memory.Addr.ic_pl1Events, (int)InputControls.GameControlEvents.kGameEventDown);
            Bench.UpdateBench();
            Memory.WriteDword(Memory.Addr.ic_pl1Events, 0);
            Bench.UpdateBench();
            Memory.WriteDword(Memory.Addr.ic_pl1Events, (int)InputControls.GameControlEvents.kGameEventKick);
            Bench.UpdateBench();
            Memory.WriteDword(Memory.Addr.ic_pl1Events, 0);
            Bench.UpdateBench();
            // Fire again: SelectPlayerToSubstituteMenuHandler -> InitiateSubstitution.
            Memory.WriteDword(Memory.Addr.ic_pl1Events, (int)InputControls.GameControlEvents.kGameEventKick);
            Bench.UpdateBench();
        });

        Scenario("substitute_player_swaps_records_and_sprites", () =>
        {
            WireTeams();
            Memory.WriteByte(kTopTeamInGame + 11 * TeamDataLoader.PlayerInfoSize + TeamDataLoader.OffPosition, 3);
            Memory.WriteByte(kTopTeamInGame + 11 * TeamDataLoader.PlayerInfoSize + TeamDataLoader.OffFace, 1);
            Memory.WriteByte(kTopTeamInGame + 3 * TeamDataLoader.PlayerInfoSize + TeamDataLoader.OffPosition, 3);
            Memory.WriteByte(kTopTeamInGame + 3 * TeamDataLoader.PlayerInfoSize + TeamDataLoader.OffFace, 2);
            PlayerSprite.SetPlayerOrdinal(4, 4); // slot 3 = ordinal 4 (top outfielder #3)
        }, () =>
        {
            // Directly poke the "about to substitute" internal state via the
            // real API surface: RequestBench1 + arrow to row1 + selects via
            // the same menu path as above, then wait out the walk FSM
            // (sip transitions 1 -> 2 -> -1 -> 0) before the actual swap.
            Bench.RequestBench1();
            Memory.WriteWord(Memory.Addr.gameStatePl, 0);
            Memory.WriteWord(Memory.Addr.gameState, 0);
            Memory.WriteWord(top + TeamData.OffTeamNumber, 1);
            Memory.WriteWord(top + TeamData.OffPlayerNumber, 1);
            Memory.WriteWord(top + TeamData.OffPlayerCoachNumber, 1);
            Bench.UpdateBench(); // invoke

            Memory.WriteDword(Memory.Addr.ic_pl1Events, (int)InputControls.GameControlEvents.kGameEventDown);
            Bench.UpdateBench();
            Memory.WriteDword(Memory.Addr.ic_pl1Events, 0);
            Bench.UpdateBench();
            Memory.WriteDword(Memory.Addr.ic_pl1Events, (int)InputControls.GameControlEvents.kGameEventKick);
            Bench.UpdateBench();
            Memory.WriteDword(Memory.Addr.ic_pl1Events, 0);
            Bench.UpdateBench();
            Memory.WriteDword(Memory.Addr.ic_pl1Events, (int)InputControls.GameControlEvents.kGameEventKick);
            Bench.UpdateBench(); // InitiateSubstitution fires here

            // Fast-forward the walk FSM: snap the outgoing sprite onto its
            // recorded destination and re-tick until the swap fires.
            int outgoingSprite = Memory.ReadSignedDword(Memory.Addr.substitutedPlSprite);
            for (int i = 0; i < 4 && Memory.ReadSignedWord(Memory.Addr.g_substituteInProgress) != 0; i++)
            {
                short destX = Memory.ReadSignedWord(outgoingSprite + PlayerSprite.OffDestX);
                short destY = Memory.ReadSignedWord(outgoingSprite + PlayerSprite.OffDestY);
                Memory.WriteWord(outgoingSprite + PlayerSprite.OffX + 2, destX);
                Memory.WriteWord(outgoingSprite + PlayerSprite.OffY + 2, destY);
                Memory.WriteDword(outgoingSprite + PlayerSprite.OffDeltaX, 0);
                Memory.WriteDword(outgoingSprite + PlayerSprite.OffDeltaY, 0);
                Bench.UpdateBench();
            }
        });

        // ==================================================================
        // UpdateSubstitutedPlayerWalk -- direct FSM steps
        // ==================================================================
        Scenario("walk_fsm_still_travelling_refreshes_dest", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.g_substituteInProgress, 1);
            Memory.WriteDword(Memory.Addr.substitutedPlSprite, t1_1);
            Memory.WriteDword(Memory.Addr.teamThatSubstitutes, top);
            PlayerSprite.SetX(1, 200 << 16); PlayerSprite.SetY(1, 300 << 16);
        }, () => Bench.UpdateBench());

        Scenario("walk_fsm_injured_stretchered_shortcuts_to_swap", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.g_substituteInProgress, 1);
            Memory.WriteDword(Memory.Addr.substitutedPlSprite, t1_1);
            Memory.WriteDword(Memory.Addr.teamThatSubstitutes, top);
            Memory.WriteWord(t1_1 + PlayerSprite.OffInjuryLevel, unchecked((short)-2));
        }, () => Bench.UpdateBench());

        Scenario("walk_fsm_settle_completes", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.g_substituteInProgress, unchecked((short)-1));
            Memory.WriteDword(Memory.Addr.substitutedPlSprite, t1_1);
            Memory.WriteDword(Memory.Addr.teamThatSubstitutes, top);
            PlayerSprite.SetDeltaX(1, 0); PlayerSprite.SetDeltaY(1, 0);
        }, () => Bench.UpdateBench());

        // ==================================================================
        // CheckIfGoalkeeperClaimedTheBall -- both branches
        // ==================================================================
        Scenario("check_goalkeeper_claimed_keeper_holds_branch", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameState, 3);
            Memory.WriteDword(Memory.Addr.lastTeamPlayedBeforeBreak, top);
        }, () => Bench.CheckIfGoalkeeperClaimedTheBall());

        Scenario("check_goalkeeper_claimed_stop_play_branch_top_bug_preserved", () =>
        {
            WireTeams();
            Memory.WriteWord(Memory.Addr.gameState, 0);
            Memory.WriteWord(top + TeamData.OffGoalkeeperPlaying, 1);
            Memory.WriteWord(bot + TeamData.OffGoalkeeperPlaying, 1);
        }, () => Bench.CheckIfGoalkeeperClaimedTheBall());

        Console.WriteLine($"wrote {count} Step11Bench scenario dumps ({kMemSize} bytes each) to {outDir}");
    }
}
