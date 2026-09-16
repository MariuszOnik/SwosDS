// Generates the step-11A differential-test golden dumps: the real local
// dependencies of GameLoop.cs discovered by its comment-filtered dependency
// scan -- Kickoff.cs (minimal slice: PrepareForInitialKick +
// ReseatTeamsForNewHalf only), Camera.cs (full), GameSprites.cs (full),
// SpinningLogo.cs (full), PlayerNameDisplay.cs (full), Stats.cs (full), and
// the InBenchMenus/GetBenchState extension to the existing Bench.cs
// minimal-slice stub. GameLoop.cs itself (2045 lines) and the full
// dedicated port of Bench.cs (1868 lines, UpdateBench/
// CheckIfGoalkeeperClaimedTheBall) are their own later steps -- see
// swos-vm-c/README.md's step-11 status entry.
//
// RNG discipline: Camera.SetCameraToInitialPosition draws exactly one Rng
// byte (top/bottom start-Y coin flip). Every scenario exercising it
// explicitly reseeds with a chosen seed, same pattern as every other step.
using OpenSwos.Sim.Port;
using OpenSwos.SwosVm;

public static class Step11AGolden
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
            File.WriteAllBytes(Path.Combine(outDir, $"s11a_{name}.bin"), dump);
            count++;
        }

        int top = TeamData.TopBase, bot = TeamData.BottomBase;
        int t1_1 = PlayerSprite.Base(1);
        int t1_2 = PlayerSprite.Base(2);
        int t2_1 = PlayerSprite.Base(12);

        // ==================================================================
        // Kickoff.PrepareForInitialKick / ReseatTeamsForNewHalf
        // ==================================================================
        Scenario("kickoff_prepare_top_starts", () =>
        {
            Memory.WriteWord(Memory.Addr.teamStarting, 1);
            Memory.WriteWord(Memory.Addr.teamPlayingUp, 1);
        }, () => Kickoff.PrepareForInitialKick());

        Scenario("kickoff_prepare_bottom_starts", () =>
        {
            Memory.WriteWord(Memory.Addr.teamStarting, 2);
            Memory.WriteWord(Memory.Addr.teamPlayingUp, 1);
        }, () => Kickoff.PrepareForInitialKick());

        Scenario("kickoff_reseat_teams_for_new_half", () =>
        {
            Memory.WriteDword(top + TeamData.OffInGameTeamPtr, kTopTeamInGame);
            Memory.WriteDword(bot + TeamData.OffInGameTeamPtr, kBotTeamInGame);
            Memory.WriteWord(top + TeamData.OffPlayerNumber, 1);
            Memory.WriteWord(bot + TeamData.OffPlayerNumber, 0);
            Memory.WriteWord(top + TeamData.OffTeamNumber, 1);
            Memory.WriteWord(bot + TeamData.OffTeamNumber, 2);
        }, () => Kickoff.ReseatTeamsForNewHalf());

        // ==================================================================
        // Camera -- accessors + MoveCamera mode dispatch
        // ==================================================================
        Scenario("camera_set_and_get_xy", () =>
        {
        }, () =>
        {
            Camera.SetCameraX(100 << 16);
            Camera.SetCameraY(200 << 16);
            _ = Camera.GetCameraX();
            _ = Camera.GetCameraY();
            _ = Camera.GetCameraXWhole();
            _ = Camera.GetCameraYWhole();
        });

        Scenario("camera_move_fans_counter_early_out", () =>
        {
            Camera.SetCameraX(50 << 16);
            Camera.SetCameraY(50 << 16);
            Memory.WriteWord(Memory.Addr.showFansCounter, 5);
        }, () => Camera.MoveCamera());

        Scenario("camera_move_self_heal_zero_zero", () =>
        {
            Rng.Reseed(3);
        }, () => Camera.MoveCamera());

        Scenario("camera_move_booking_mode", () =>
        {
            Camera.SetCameraX(176 << 16); Camera.SetCameraY(16 << 16);
            Memory.WriteWord(Memory.Addr.whichCard, 1);
            Memory.WriteDword(Memory.Addr.bookedPlayer, t1_1);
            PlayerSprite.SetX(1, 300 << 16); PlayerSprite.SetY(1, 400 << 16);
        }, () => Camera.MoveCamera());

        Scenario("camera_move_penalty_shootout_mode", () =>
        {
            Camera.SetCameraX(176 << 16); Camera.SetCameraY(16 << 16);
            Memory.WriteWord(Memory.Addr.playingPenalties, 1);
        }, () => Camera.MoveCamera());

        Scenario("camera_move_bench_mode_substituting", () =>
        {
            Camera.SetCameraX(176 << 16); Camera.SetCameraY(16 << 16);
            Memory.WriteWord(Memory.Addr.g_waitForPlayerToGoInTimer, 5);
        }, () => Camera.MoveCamera());

        Scenario("camera_move_leaving_bench_mode", () =>
        {
            Camera.SetCameraX(30 << 16); Camera.SetCameraY(16 << 16);
            Memory.WriteWord(Memory.Addr.leavingBenchMode, 1);
        }, () => Camera.MoveCamera());

        Scenario("camera_move_standard_in_progress_follow_ball", () =>
        {
            Camera.SetCameraX(176 << 16); Camera.SetCameraY(16 << 16);
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            Memory.WriteWord(Memory.Addr.gameState, 100);
            BallSprite.XPixels = 300; BallSprite.YPixels = 400;
            BallSprite.DeltaX = 5 << 16; BallSprite.DeltaY = -3 << 16;
        }, () => Camera.MoveCamera());

        Scenario("camera_move_standard_stopped_waiting_for_players", () =>
        {
            Camera.SetCameraX(176 << 16); Camera.SetCameraY(16 << 16);
            Memory.WriteWord(Memory.Addr.gameStatePl, 0);
            Memory.WriteWord(Memory.Addr.gameState, 21); // ST_STARTING_GAME
        }, () => Camera.MoveCamera());

        Scenario("camera_move_standard_result_after_game", () =>
        {
            Camera.SetCameraX(176 << 16); Camera.SetCameraY(16 << 16);
            Memory.WriteWord(Memory.Addr.gameStatePl, 0);
            Memory.WriteWord(Memory.Addr.gameState, 26); // ST_RESULT_AFTER_THE_GAME
        }, () => Camera.MoveCamera());

        Scenario("camera_move_standard_game_ended_penalties_unresolved", () =>
        {
            Camera.SetCameraX(176 << 16); Camera.SetCameraY(16 << 16);
            Memory.WriteWord(Memory.Addr.gameStatePl, 0);
            Memory.WriteWord(Memory.Addr.gameState, 30); // ST_GAME_ENDED
            Memory.WriteWord(Memory.Addr.penaltiesState, -1);
        }, () => Camera.MoveCamera());

        Scenario("camera_set_to_initial_position_bottom", () =>
        {
            Rng.Reseed(3); // & 1 != 0 -> bottom
        }, () => Camera.SetCameraToInitialPosition());

        Scenario("camera_switch_to_leaving_bench_mode", () =>
        {
        }, () => Camera.SwitchCameraToLeavingBenchMode());

        // ==================================================================
        // GameSprites
        // ==================================================================
        Scenario("game_sprites_update_corner_flags", () =>
        {
            Memory.WriteDword(Memory.Addr.frameCounter, 40);
        }, () => GameSprites.UpdateCornerFlags());

        Scenario("game_sprites_controlled_numbers_gate_fails", () =>
        {
            Memory.WriteWord(top + TeamData.OffPlayerNumber, 0);
            Memory.WriteWord(top + TeamData.OffIsPlCoach, 0);
            Memory.WriteWord(Memory.Addr.g_trainingGame, 0);
        }, () => GameSprites.UpdateControlledPlayerNumbers());

        Scenario("game_sprites_controlled_numbers_shows_digit", () =>
        {
            Memory.WriteWord(top + TeamData.OffPlayerNumber, 1);
            TeamData.SetControlledPlayer(true, t1_2);
            PlayerSprite.SetPlayerOrdinal(2, 3);
            PlayerSprite.SetX(2, 250 << 16); PlayerSprite.SetY(2, 350 << 16); PlayerSprite.SetZ(2, 0);
            Memory.WriteDword(top + TeamData.OffInGameTeamPtr, kTopTeamInGame);
            Memory.WriteWord(kTopTeamInGame - 22, unchecked((short)-1)); // markedPlayer != ordinal-1
            Memory.WriteByte(kTopTeamInGame + 2 * 61 + 3, 9); // shirt number for slot 2 (ordinal 3 - 1)
            Memory.WriteWord(Memory.Addr.currentGameTick, 0); // top team blink phase (tick&0x10==0)
        }, () => GameSprites.UpdateControlledPlayerNumbers());

        Scenario("game_sprites_controlled_numbers_marked_player_hidden", () =>
        {
            Memory.WriteWord(bot + TeamData.OffPlayerNumber, 1);
            TeamData.SetControlledPlayer(false, t2_1);
            PlayerSprite.SetPlayerOrdinal(12, 1);
            Memory.WriteDword(bot + TeamData.OffInGameTeamPtr, kBotTeamInGame);
            Memory.WriteWord(kBotTeamInGame - 22, 0); // markedPlayer == ordinal-1 (0)
            Memory.WriteWord(Memory.Addr.currentGameTick, 0x10); // bottom-team mark phase
        }, () => GameSprites.UpdateControlledPlayerNumbers());

        Scenario("game_sprites_face_offset_helpers", () =>
        {
        }, () =>
        {
            _ = GameSprites.GetPlayerSpriteOffsetFromFace(2);
            _ = GameSprites.GetPlayerSpriteOffsetFromFace(-1);
            _ = GameSprites.GetPlayerSpriteOffsetFromFace(9);
            _ = GameSprites.GetGoalkeeperSpriteOffset(true, 3);
        });

        // ==================================================================
        // SpinningLogo
        // ==================================================================
        Scenario("spinning_logo_disabled", () =>
        {
            SpinningLogo.EnableSpinningLogo(false);
        }, () => SpinningLogo.UpdateSpinningLogo());

        Scenario("spinning_logo_enabled_spinning_advances", () =>
        {
            SpinningLogo.EnableSpinningLogo(true);
            Memory.WriteWord(Memory.Addr.currentGameTick, 2); // bit1 set -> advance
        }, () => SpinningLogo.UpdateSpinningLogo());

        Scenario("spinning_logo_enabled_but_bench_menus_no_advance", () =>
        {
            SpinningLogo.EnableSpinningLogo(true);
            Memory.WriteWord(Memory.Addr.currentGameTick, 2);
            Memory.WriteWord(Memory.Addr.g_inSubstitutesMenu, 1);
            Memory.WriteWord(Memory.Addr.m_benchState, 0); // kInitial
        }, () => SpinningLogo.UpdateSpinningLogo());

        // ==================================================================
        // PlayerNameDisplay
        // ==================================================================
        Scenario("player_name_display_scorer_blinking_shown", () =>
        {
            Memory.WriteDword(Memory.Addr.currentScorer, t1_1);
            Memory.WriteDword(Memory.Addr.lastTeamScored, top);
            Memory.WriteDword(Memory.Addr.topTeamInGame, kTopTeamInGame);
            Memory.WriteDword(top + TeamData.OffInGameTeamPtr, kTopTeamInGame);
            PlayerSprite.SetPlayerOrdinal(1, 4);
            Memory.WriteWord(Memory.Addr.currentGameTick, 8); // bit3 set -> show
        }, () => PlayerNameDisplay.UpdateCurrentPlayerName());

        Scenario("player_name_display_card_blinking_hidden", () =>
        {
            Memory.WriteWord(Memory.Addr.whichCard, 1);
            Memory.WriteDword(Memory.Addr.bookedPlayer, t1_1);
            Memory.WriteDword(Memory.Addr.lastTeamBooked, top);
            Memory.WriteWord(Memory.Addr.currentGameTick, 0); // bit3 clear -> hide
        }, () => PlayerNameDisplay.UpdateCurrentPlayerName());

        Scenario("player_name_display_prolong_last_before_goalkeeper", () =>
        {
            Memory.WriteDword(Memory.Addr.lastPlayerBeforeGoalkeeper, t1_1);
            Memory.WriteDword(Memory.Addr.lastTeamScored, top);
            Memory.WriteDword(Memory.Addr.topTeamInGame, kTopTeamInGame);
            Memory.WriteDword(top + TeamData.OffInGameTeamPtr, kTopTeamInGame);
            PlayerSprite.SetPlayerOrdinal(1, 2);
            Memory.WriteWord(Memory.Addr.nobodysBallTimer, 10);
        }, () => PlayerNameDisplay.UpdateCurrentPlayerName());

        Scenario("player_name_display_in_progress_resets_and_shows", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            Memory.WriteDword(Memory.Addr.lastPlayerPlayed, t1_1);
            Memory.WriteDword(Memory.Addr.lastTeamPlayed, top);
            Memory.WriteWord(top + TeamData.OffPlayerHasBall, 1);
            Memory.WriteDword(Memory.Addr.topTeamInGame, kTopTeamInGame);
            Memory.WriteDword(top + TeamData.OffInGameTeamPtr, kTopTeamInGame);
            PlayerSprite.SetPlayerOrdinal(1, 3);
            Memory.WriteWord(Memory.Addr.nobodysBallTimer, 0);
        }, () => PlayerNameDisplay.UpdateCurrentPlayerName());

        Scenario("player_name_display_in_progress_no_player_hides", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            Memory.WriteDword(Memory.Addr.lastPlayerPlayed, 0);
        }, () => PlayerNameDisplay.UpdateCurrentPlayerName());

        Scenario("player_name_display_stopped_hides_first_frame", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 0);
            Memory.WriteDword(Memory.Addr.lastPlayerPlayed, t1_1);
            Memory.WriteDword(Memory.Addr.lastTeamPlayed, top);
            TeamData.SetControlledPlayer(true, t1_1);
            Memory.WriteWord(Memory.Addr.pnd_nobodysBallLastFrame, 3);
        }, () => PlayerNameDisplay.UpdateCurrentPlayerName());

        Scenario("player_name_display_stopped_prolongs", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 0);
            Memory.WriteDword(Memory.Addr.lastPlayerPlayed, t1_1);
            Memory.WriteDword(Memory.Addr.lastTeamPlayed, top);
            TeamData.SetControlledPlayer(true, t1_1);
            Memory.WriteWord(Memory.Addr.pnd_nobodysBallLastFrame, 0);
            Memory.WriteDword(Memory.Addr.topTeamInGame, kTopTeamInGame);
            Memory.WriteDword(top + TeamData.OffInGameTeamPtr, kTopTeamInGame);
            PlayerSprite.SetPlayerOrdinal(1, 5);
        }, () => PlayerNameDisplay.UpdateCurrentPlayerName());

        // ==================================================================
        // Stats
        // ==================================================================
        Scenario("stats_init", () =>
        {
            Memory.WriteWord(Memory.Addr.st_isGoalAttempt, 1);
            Memory.WriteWord(Memory.Addr.st_showStats, 1);
        }, () => Stats.InitStats());

        Scenario("stats_toggle_shows_from_hidden", () =>
        {
            Memory.WriteWord(Memory.Addr.st_showingUserRequestedStats, 0);
            Memory.WriteDword(Memory.Addr.resultTimer, 500);
        }, () => Stats.ToggleStats());

        Scenario("stats_toggle_hides_when_user_requested", () =>
        {
            Memory.WriteWord(Memory.Addr.st_showingUserRequestedStats, 1);
        }, () => Stats.ToggleStats());

        Scenario("stats_update_bumps_possession", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            Memory.WriteDword(Memory.Addr.lastTeamPlayed, top);
            BallSprite.YPixels = 300;
        }, () => Stats.UpdateStatistics());

        Scenario("stats_update_registers_goal_attempt", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            Memory.WriteDword(Memory.Addr.lastTeamPlayed, top);
            Memory.WriteWord(Memory.Addr.ballInGoalkeeperArea, 1);
            BallSprite.Y = 200 << 16; // <= center -> bottom keeper area
            BallSprite.DeltaY = 5 << 16;
            Memory.WriteWord(bot + TeamData.OffPlayerHasBall, 0);
            Memory.WriteWord(Memory.Addr.strikeDestX, 330); // within goal + attempt window
        }, () => Stats.UpdateStatistics());

        Scenario("stats_update_penalties_skip_bump", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            Memory.WriteWord(Memory.Addr.playingPenalties, 1);
            Memory.WriteDword(Memory.Addr.lastTeamPlayed, top);
        }, () => Stats.UpdateStatistics());

        Scenario("stats_check_timer_auto_hides", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 0);
            Memory.WriteWord(Memory.Addr.statsTimer, unchecked((short)-1));
            Memory.WriteWord(Memory.Addr.st_showStats, 1);
        }, () => Stats.UpdateStatistics());

        // ==================================================================
        // Bench.InBenchMenus extension
        // ==================================================================
        Scenario("bench_in_bench_menus_true", () =>
        {
            Memory.WriteWord(Memory.Addr.g_inSubstitutesMenu, 1);
            Memory.WriteWord(Memory.Addr.m_benchState, 0); // kBenchStateInitial
        }, () => { _ = Bench.InBenchMenus(); });

        Scenario("bench_in_bench_menus_false_wrong_state", () =>
        {
            Memory.WriteWord(Memory.Addr.g_inSubstitutesMenu, 1);
            Memory.WriteWord(Memory.Addr.m_benchState, 2); // kBenchStateFormationMenu
        }, () => { _ = Bench.InBenchMenus(); });

        Console.WriteLine($"wrote {count} Step11A scenario dumps ({kMemSize} bytes each) to {outDir}");
    }
}
