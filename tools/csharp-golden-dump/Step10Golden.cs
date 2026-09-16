// Generates the step-10 differential-test golden dumps: SetPieces.cs
// (throw-in/corner/goal-kick/free-kick/penalty set-piece handling),
// GameTime.cs (match clock, half/full-time/extra-time/penalties
// transitions), Referee.cs (the per-tick referee movement/card-handing
// state machine, joining step 7A's ActivateReferee), and Result.cs
// (scorer list + result-display timer, closing the step-4
// swosRegisterScorerHook PORT_PENDING boundary).
//
// RNG discipline (same pattern as Step9Golden.cs): several functions here
// draw Rng bytes (TickPenalty's direction pick, StartFirstExtraTime/
// StartPenalties' coin tosses, MarkPlayersHappyOrSad's per-player roll,
// PutRefereeToLeavingState/ActivateReferee's jitter). Every scenario that
// depends on a specific RNG outcome explicitly reseeds right after
// Memory.Init() with a seed picked by probing Rng.Reseed(N); Rng.NextByte()
// for the needed bit pattern -- never relies on Init()'s own internal
// reseed.
//
// Static-state discipline: GameTime.s_stoppageRealTicks and Result.cs's
// m_team{1,2}Scorers/m_team{1,2}ScorerLines are C#-side statics, NOT reset
// by Memory.Init() (same "one process runs every *Golden.Run()" gotcha
// noted in project memory/README). Every scenario that touches either
// calls GameTime.ResetGameTime() / Result.ResetResult("", "") first so
// state from an earlier scenario in this same process can't leak in.
using OpenSwos.Sim.Port;
using OpenSwos.SwosVm;

public static class Step10Golden
{
    private const int kMemSize = 0x60000;

    // Scratch "TeamGame.players[]" base addresses -- same convention as
    // Step7AGolden.cs/PlayerActionsGolden.cs's 0x4FE60 top-team PlayerInfo
    // scratch (61 bytes/record); 0x4FEA0 is the analogous bottom-team slot
    // (PlayerUpdateGolden.cs's own comment: "0x4FE60/0x4FEA0 (top/bottom,
    // 61 bytes each, ending 0x4FEDD)"). Reused here as topTeamInGame/
    // bottomTeamInGame -- the exact realism of what's beyond the first
    // couple of PlayerInfo records doesn't matter for a differential test:
    // both sides read the same zeroed-then-written Memory bytes and must
    // still agree byte for byte.
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
            File.WriteAllBytes(Path.Combine(outDir, $"s10_{name}.bin"), dump);
            count++;
        }

        int top = TeamData.TopBase, bot = TeamData.BottomBase;
        int t1_1 = PlayerSprite.Base(1);
        int t1_2 = PlayerSprite.Base(2);
        int t2_1 = PlayerSprite.Base(12);

        // ==================================================================
        // SetPieces.SetThrowInPlayerDestinationCoordinates
        // ==================================================================
        Scenario("set_throw_in_dest_right_half", () =>
        {
            BallSprite.XPixels = 400; BallSprite.YPixels = 300;
        }, () => SetPieces.SetThrowInPlayerDestinationCoordinates(t1_1));

        Scenario("set_throw_in_dest_left_half", () =>
        {
            BallSprite.XPixels = 100; BallSprite.YPixels = 500;
        }, () => SetPieces.SetThrowInPlayerDestinationCoordinates(t1_1));

        // ==================================================================
        // SetPieces.TickThrowIn -- the big state machine
        // ==================================================================
        Scenario("tick_throw_in_ai_thrower_normal_fire_kick", () =>
        {
            Memory.WriteWord(top + TeamData.OffPlayerNumber, 0); // AI thrower
            Memory.WriteByte(Memory.Addr.playerTurnFlags, 0xFF); // every direction allowed
            PlayerSprite.SetDirection(1, 2);
            Memory.WriteWord(Memory.Addr.gameState, 15); // ST_THROW_IN_FORWARD_RIGHT
            Memory.WriteWord(Memory.Addr.stoppageTimerActive, 0);
            Memory.WriteByte(t1_1 + PlayerSprite.OffPlayerDownTimer, 0);
            Memory.WriteWord(top + TeamData.OffCurrentAllowedDirection, -1);
            Memory.WriteByte(top + TeamData.OffNormalFire, 1);
            Rng.Reseed(20); // one Rng byte drawn inside AiBrain.SetControlsDirection
        }, () => SetPieces.TickThrowIn(t1_1, BallSprite.Base, top));

        Scenario("tick_throw_in_human_quick_fire_pass", () =>
        {
            Memory.WriteWord(top + TeamData.OffPlayerNumber, 1); // human thrower
            Memory.WriteByte(Memory.Addr.playerTurnFlags, 0xFF);
            PlayerSprite.SetDirection(1, 2);
            Memory.WriteWord(Memory.Addr.gameState, 15);
            Memory.WriteWord(top + TeamData.OffCurrentAllowedDirection, 2);
            Memory.WriteByte(top + TeamData.OffQuickFire, 1);
            TeamData.SetControlledPlayer(true, t1_1);
            PlayerSprite.SetX(2, 310 << 16); PlayerSprite.SetY(2, 400 << 16);
            PlayerSprite.SetX(1, 300 << 16); PlayerSprite.SetY(1, 400 << 16);
        }, () => SetPieces.TickThrowIn(t1_1, BallSprite.Base, top));

        Scenario("tick_throw_in_countdown_not_ready", () =>
        {
            Memory.WriteWord(top + TeamData.OffPlayerNumber, 1);
            Memory.WriteByte(Memory.Addr.playerTurnFlags, 0xFF);
            PlayerSprite.SetDirection(1, 2);
            Memory.WriteWord(Memory.Addr.gameState, 15);
            Memory.WriteByte(t1_1 + PlayerSprite.OffPlayerDownTimer, 5);
        }, () => SetPieces.TickThrowIn(t1_1, BallSprite.Base, top));

        Scenario("tick_throw_in_abort_wrong_game_state", () =>
        {
            Memory.WriteWord(top + TeamData.OffPlayerNumber, 1);
            Memory.WriteByte(Memory.Addr.playerTurnFlags, 0xFF);
            PlayerSprite.SetDirection(1, 2);
            Memory.WriteWord(Memory.Addr.gameState, 100); // ST_GAME_IN_PROGRESS -- not a throw-in state
            Memory.WriteByte(t1_1 + PlayerSprite.OffPlayerDownTimer, 0);
        }, () => SetPieces.TickThrowIn(t1_1, BallSprite.Base, top));

        // ==================================================================
        // SetPieces.DispatchByGameState
        // ==================================================================
        Scenario("dispatch_throw_in", () =>
        {
            Memory.WriteWord(Memory.Addr.gameState, 16); // ST_THROW_IN_CENTER_RIGHT
            Memory.WriteWord(top + TeamData.OffPlayerNumber, 1);
            Memory.WriteByte(Memory.Addr.playerTurnFlags, 0xFF);
            PlayerSprite.SetDirection(1, 2);
            Memory.WriteByte(t1_1 + PlayerSprite.OffPlayerDownTimer, 0);
        }, () => SetPieces.DispatchByGameState(t1_1, BallSprite.Base, 0, top));

        Scenario("dispatch_foul", () =>
        {
            Memory.WriteWord(Memory.Addr.gameState, 13); // ST_FOUL
            Memory.WriteWord(Memory.Addr.resultTimer, 0);
            Memory.WriteWord(top + TeamData.OffControlledPlDirection, 3);
        }, () => SetPieces.DispatchByGameState(t1_1, BallSprite.Base, 0, top));

        Scenario("dispatch_penalty", () =>
        {
            Memory.WriteWord(Memory.Addr.gameState, 14); // ST_PENALTY
            Memory.WriteWord(Memory.Addr.resultTimer, 0);
            Memory.WriteWord(Memory.Addr.AI_rand, 3);
            Memory.WriteByte(Memory.Addr.playerTurnFlags, 0xFF);
        }, () => SetPieces.DispatchByGameState(t1_1, BallSprite.Base, t1_1, top));

        // ==================================================================
        // SetPieces.TickSetPieces -- auto-resolvers
        // ==================================================================
        Scenario("tick_set_pieces_corner", () =>
        {
            Memory.WriteWord(Memory.Addr.gameState, 4); // ST_CORNER_LEFT
            Memory.WriteWord(Memory.Addr.resultTimer, 0);
            Memory.WriteWord(Memory.Addr.foulXCoordinate, 86);
            Memory.WriteWord(Memory.Addr.foulYCoordinate, 134);
            Memory.WriteDword(Memory.Addr.lastTeamPlayedBeforeBreak, top);
            Memory.WriteWord(Memory.Addr.cameraDirection, 2);
            PlayerSprite.SetX(2, 90 << 16); PlayerSprite.SetY(2, 140 << 16);
        }, () => SetPieces.TickSetPieces());

        Scenario("tick_set_pieces_goal_kick", () =>
        {
            Memory.WriteWord(Memory.Addr.gameState, 1); // ST_GOAL_OUT_LEFT
            Memory.WriteWord(Memory.Addr.resultTimer, 0);
            Memory.WriteWord(Memory.Addr.foulXCoordinate, 396);
            Memory.WriteWord(Memory.Addr.foulYCoordinate, 744);
            Memory.WriteDword(Memory.Addr.lastTeamPlayedBeforeBreak, bot);
            Memory.WriteWord(Memory.Addr.cameraDirection, 0);
        }, () => SetPieces.TickSetPieces());

        Scenario("tick_set_pieces_throw_in", () =>
        {
            Memory.WriteWord(Memory.Addr.gameState, 15); // ST_THROW_IN_FORWARD_RIGHT
            Memory.WriteWord(Memory.Addr.resultTimer, 0);
            Memory.WriteWord(Memory.Addr.foulXCoordinate, 590);
            Memory.WriteWord(Memory.Addr.foulYCoordinate, 300);
            Memory.WriteDword(Memory.Addr.lastTeamPlayedBeforeBreak, top);
            PlayerSprite.SetX(2, 585 << 16); PlayerSprite.SetY(2, 305 << 16);
        }, () => SetPieces.TickSetPieces());

        Scenario("tick_set_pieces_free_kick", () =>
        {
            Memory.WriteWord(Memory.Addr.gameState, 13); // ST_FOUL
            Memory.WriteWord(Memory.Addr.resultTimer, 0);
            Memory.WriteWord(Memory.Addr.foulXCoordinate, 250);
            Memory.WriteWord(Memory.Addr.foulYCoordinate, 400);
            Memory.WriteDword(Memory.Addr.lastTeamPlayedBeforeBreak, bot);
            Memory.WriteWord(Memory.Addr.cameraDirection, 4);
            PlayerSprite.SetX(12, 255 << 16); PlayerSprite.SetY(12, 405 << 16);
        }, () => SetPieces.TickSetPieces());

        Scenario("tick_set_pieces_penalty_shootout", () =>
        {
            Memory.WriteWord(Memory.Addr.gameState, 31); // ST_PENALTIES
            Memory.WriteWord(Memory.Addr.gameStatePl, 101); // ST_STOPPED
            Memory.WriteWord(Memory.Addr.foulXCoordinate, 336);
            Memory.WriteWord(Memory.Addr.foulYCoordinate, 187);
            Memory.WriteDword(Memory.Addr.penaltyShooterSprite, t2_1);
            Memory.WriteWord(Memory.Addr.cameraDirection, 0);
        }, () => SetPieces.TickSetPieces());

        Scenario("tick_set_pieces_result_timer_blocks", () =>
        {
            Memory.WriteWord(Memory.Addr.gameState, 4); // ST_CORNER_LEFT
            Memory.WriteWord(Memory.Addr.resultTimer, 50); // showing -- set-piece input suppressed
        }, () => SetPieces.TickSetPieces());

        // ==================================================================
        // SetPieces.TickFreeKick / TickPenalty
        // ==================================================================
        Scenario("tick_free_kick_direction", () =>
        {
            Memory.WriteWord(Memory.Addr.resultTimer, 0);
            Memory.WriteWord(top + TeamData.OffControlledPlDirection, 5);
        }, () => SetPieces.TickFreeKick(top));

        Scenario("tick_penalty_random_direction_allowed", () =>
        {
            Memory.WriteWord(Memory.Addr.resultTimer, 0);
            Memory.WriteWord(Memory.Addr.gameState, 14); // ST_PENALTY -- not shootout
            Memory.WriteWord(Memory.Addr.AI_rand, 3);
            Memory.WriteByte(Memory.Addr.playerTurnFlags, 0xFF);
        }, () => SetPieces.TickPenalty(t1_1, top));

        Scenario("tick_penalty_direction_disallowed_zero_dir_returns", () =>
        {
            Memory.WriteWord(Memory.Addr.resultTimer, 0);
            Memory.WriteWord(Memory.Addr.gameState, 14);
            Memory.WriteWord(Memory.Addr.AI_rand, 3);
            Memory.WriteByte(Memory.Addr.playerTurnFlags, 0x00); // nothing allowed
            PlayerSprite.SetDirection(1, 0);
        }, () => SetPieces.TickPenalty(t1_1, top));

        // ==================================================================
        // SetPieces.AdvancePenaltiesTimer
        // ==================================================================
        Scenario("advance_penalties_timer_increment_only", () =>
        {
            Memory.WriteWord(Memory.Addr.playingPenalties, 1);
            Memory.WriteWord(Memory.Addr.gameState, 100); // not ST_PENALTIES
            Memory.WriteWord(Memory.Addr.penaltiesTimer, 5);
            Memory.WriteWord(Memory.Addr.m_penaltiesInterval, 110);
        }, () => SetPieces.AdvancePenaltiesTimer());

        Scenario("advance_penalties_timer_triggers_next_penalty", () =>
        {
            Memory.WriteWord(Memory.Addr.playingPenalties, 1);
            Memory.WriteWord(Memory.Addr.gameState, 100);
            Memory.WriteWord(Memory.Addr.penaltiesTimer, 109);
            Memory.WriteWord(Memory.Addr.m_penaltiesInterval, 110);
            Memory.WriteWord(Memory.Addr.team1PenaltyAttempts, 1);
            Memory.WriteWord(Memory.Addr.team2PenaltyAttempts, 1);
            Memory.WriteWord(Memory.Addr.team1PenaltyGoals, 1);
            Memory.WriteWord(Memory.Addr.team2PenaltyGoals, 0);
            Memory.WriteWord(bot + TeamData.OffTeamNumber, 2);
            Memory.WriteDword(bot + TeamData.OffPlayers, 0x4FE2C);
        }, () => SetPieces.AdvancePenaltiesTimer());

        // ==================================================================
        // GameTime -- clock lifecycle
        // ==================================================================
        Scenario("game_time_reset_and_showing", () =>
        {
            Memory.WriteWord(Memory.Addr.gameLengthInGame, 1);
        }, () =>
        {
            GameTime.ResetGameTime();
        });

        Scenario("game_time_update_normal_tick_no_rollover", () =>
        {
            GameTime.ResetGameTime();
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteDword(Memory.Addr.gt_timeDelta, 30);
            Memory.WriteDword(Memory.Addr.gt_secondsSwitchAccumulator, 40);
        }, () => GameTime.UpdateGameTime());

        Scenario("game_time_update_minute_rollover", () =>
        {
            GameTime.ResetGameTime();
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteDword(Memory.Addr.gt_timeDelta, 30);
            Memory.WriteDword(Memory.Addr.gt_secondsSwitchAccumulator, 0);
            Memory.WriteDword(Memory.Addr.gt_gameSeconds, 59);
            Memory.WriteDword(Memory.Addr.gt_gameTimeInMinutes, 10);
        }, () => GameTime.UpdateGameTime());

        Scenario("game_time_update_prolong_pin_then_refresh", () =>
        {
            GameTime.ResetGameTime();
            Memory.WriteWord(Memory.Addr.gameStatePl, 0); // not game-in-progress -> prolong
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteDword(Memory.Addr.gt_gameSeconds, unchecked((int)0xFFFFFFFF)); // -1
            Memory.WriteDword(Memory.Addr.gt_endGameCounter, 5);
        }, () => GameTime.UpdateGameTime());

        Scenario("game_time_update_period_end_fires_end_first_half", () =>
        {
            GameTime.ResetGameTime();
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteDword(Memory.Addr.gt_gameSeconds, unchecked((int)0xFFFFFFFF));
            Memory.WriteDword(Memory.Addr.gt_endGameCounter, 0);
            Memory.WriteDword(Memory.Addr.gt_gameTimeInMinutes, 45);
            Memory.WriteWord(Memory.Addr.goalCounter, 0);
        }, () => GameTime.UpdateGameTime());

        Scenario("game_time_update_period_end_waits_for_goal_celebration", () =>
        {
            GameTime.ResetGameTime();
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteDword(Memory.Addr.gt_gameSeconds, unchecked((int)0xFFFFFFFF));
            Memory.WriteDword(Memory.Addr.gt_endGameCounter, 0);
            Memory.WriteDword(Memory.Addr.gt_gameTimeInMinutes, 45);
            Memory.WriteWord(Memory.Addr.goalCounter, 3); // celebration still draining -> retry next tick
        }, () => GameTime.UpdateGameTime());

        Scenario("game_time_accessors", () =>
        {
            GameTime.ResetGameTime();
            Memory.WriteDword(Memory.Addr.gt_gameTimeInMinutes, 67);
            Memory.WriteDword(Memory.Addr.gt_gameTime + 1 * 4, 0);
            Memory.WriteDword(Memory.Addr.gt_gameTime + 2 * 4, 6);
            Memory.WriteDword(Memory.Addr.gt_gameTime + 3 * 4, 7);
        }, () =>
        {
            _ = GameTime.GameTimeInMinutes();
            _ = GameTime.GameTimeAsBcd();
            _ = GameTime.GameAtZeroMinute();
            _ = GameTime.GameTimeShowing();
        });

        // ==================================================================
        // GameTime.MarkPlayersHappyOrSad
        // ==================================================================
        Scenario("mark_players_happy_or_sad_top_wins", () =>
        {
            Memory.WriteWord(Memory.Addr.gameState, 30); // ST_GAME_ENDED
            Memory.WriteDword(Memory.Addr.topTeamInGame, kTopTeamInGame);
            Memory.WriteDword(Memory.Addr.bottomTeamInGame, kBotTeamInGame);
            Memory.WriteDword(Memory.Addr.winningTeamPtr, kTopTeamInGame);
            // Every outfielder standing still, PL_NORMAL (already the sprite
            // pool's init default) -- deltaX/Y default to 0.
            Rng.Reseed(1); // first NextByte() <= 64 -> at least the first roll reacts
        }, () => GameTime.MarkPlayersHappyOrSad());

        Scenario("mark_players_happy_or_sad_tie_no_poses", () =>
        {
            Memory.WriteWord(Memory.Addr.gameState, 30);
            Memory.WriteDword(Memory.Addr.topTeamInGame, kTopTeamInGame);
            Memory.WriteDword(Memory.Addr.bottomTeamInGame, kBotTeamInGame);
            Memory.WriteDword(Memory.Addr.winningTeamPtr, 0); // tie
        }, () => GameTime.MarkPlayersHappyOrSad());

        // ==================================================================
        // GameTime.NextPenalty (also reachable via AdvancePenaltiesTimer
        // above, tested directly here for both outcomes)
        // ==================================================================
        Scenario("next_penalty_continues_shootout", () =>
        {
            Memory.WriteWord(Memory.Addr.team1PenaltyAttempts, 2);
            Memory.WriteWord(Memory.Addr.team2PenaltyAttempts, 2);
            Memory.WriteWord(Memory.Addr.team1PenaltyGoals, 1);
            Memory.WriteWord(Memory.Addr.team2PenaltyGoals, 1);
            Memory.WriteWord(bot + TeamData.OffTeamNumber, 2);
            Memory.WriteDword(bot + TeamData.OffPlayers, 0x4FE2C);
            Memory.WriteWord(Memory.Addr.team2PenaltyShooterIndex, 5);
        }, () => GameTime.NextPenalty());

        Scenario("next_penalty_finishes_shootout", () =>
        {
            // total >= 10, attempts equal, goals differ -> finish.
            Memory.WriteWord(Memory.Addr.team1PenaltyAttempts, 5);
            Memory.WriteWord(Memory.Addr.team2PenaltyAttempts, 5);
            Memory.WriteWord(Memory.Addr.team1PenaltyGoals, 4);
            Memory.WriteWord(Memory.Addr.team2PenaltyGoals, 3);
            Memory.WriteWord(Memory.Addr.savedTeam1Goals, 2);
            Memory.WriteWord(Memory.Addr.savedTeam2Goals, 1);
        }, () => GameTime.NextPenalty());

        // ==================================================================
        // GameTime -- initMatch()-adjacent helpers
        // ==================================================================
        Scenario("init_player_card_chance", () =>
        {
            Memory.WriteWord(Memory.Addr.gameLengthInGame, 2);
            Rng.Reseed(7);
        }, () => GameTime.InitPlayerCardChance());

        Scenario("determine_starting_team_and_playing_up", () =>
        {
            Rng.Reseed(3);
        }, () => GameTime.DetermineStartingTeamAndTeamPlayingUp());

        Scenario("init_pitch_ball_factors_normal", () =>
        {
            OpenSwos.Sim.BallSim.CurrentPitchType = 4;
        }, () => GameTime.InitPitchBallFactors());

        Scenario("init_pitch_ball_factors_frozen_clamped_low", () =>
        {
            OpenSwos.Sim.BallSim.CurrentPitchType = -3; // clamps to 0
        }, () =>
        {
            GameTime.InitPitchBallFactors();
            OpenSwos.Sim.BallSim.CurrentPitchType = 4; // restore the default for later scenarios
        });

        Scenario("save_and_restore_teams", () =>
        {
            Memory.WriteByte(Memory.Addr.team1InGameTeamHeader, 0xAB);
            Memory.WriteByte(Memory.Addr.team2InGameTeamHeader, 0xCD);
        }, () =>
        {
            GameTime.SaveTeams();
            Memory.WriteByte(Memory.Addr.team1InGameTeamHeader, 0x00);
            Memory.WriteByte(Memory.Addr.team2InGameTeamHeader, 0x00);
            GameTime.RestoreTeams();
        });

        Scenario("init_game_variables", () =>
        {
            Memory.WriteWord(Memory.Addr.secondLeg, 0);
            Rng.Reseed(9);
        }, () => GameTime.InitGameVariables());

        Scenario("init_game_variables_second_leg", () =>
        {
            Memory.WriteWord(Memory.Addr.secondLeg, 1);
            Memory.WriteWord(Memory.Addr.team1GoalsFirstLeg, 2);
            Memory.WriteWord(Memory.Addr.team2GoalsFirstLeg, 1);
            Rng.Reseed(9);
        }, () => GameTime.InitGameVariables());

        // ==================================================================
        // Referee -- the per-tick state machine (joining step 7A's
        // ActivateReferee)
        // ==================================================================
        Scenario("referee_incoming_walk_tick", () =>
        {
            Memory.WriteWord(Memory.Addr.foulXCoordinate, 300);
            Memory.WriteWord(Memory.Addr.foulYCoordinate, 400);
            Referee.ActivateReferee();
        }, () => Referee.UpdateReferee());

        Scenario("referee_offscreen_game_in_progress_moves_toward_pitch", () =>
        {
            Memory.WriteWord(Memory.Addr.refState, 1); // kRefIncoming
            Memory.WriteWord(RefereeSprite.Base + RefereeSprite.OffVisible, 1);
            Memory.WriteWord(RefereeSprite.Base + RefereeSprite.OffOnScreen, 0);
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            Memory.WriteWord(RefereeSprite.Base + RefereeSprite.OffX + 2, 1900); // far off-screen
            Memory.WriteWord(RefereeSprite.Base + RefereeSprite.OffY + 2, 400);
        }, () => Referee.UpdateReferee());

        Scenario("referee_inactive_noop", () =>
        {
            Memory.WriteWord(Memory.Addr.refState, 0); // kRefOffScreen
        }, () => Referee.UpdateReferee());

        Scenario("referee_about_to_give_yellow_card_transitions_to_booking", () =>
        {
            Memory.WriteWord(Memory.Addr.refState, 3); // kRefAboutToGiveCard
            Memory.WriteWord(RefereeSprite.Base + RefereeSprite.OffVisible, 1);
            Memory.WriteWord(RefereeSprite.Base + RefereeSprite.OffOnScreen, 1);
            Memory.WriteWord(Memory.Addr.whichCard, 1); // kYellowCard
        }, () => Referee.UpdateReferee());

        Scenario("referee_about_to_give_red_card_transitions_to_booking", () =>
        {
            Memory.WriteWord(Memory.Addr.refState, 3);
            Memory.WriteWord(RefereeSprite.Base + RefereeSprite.OffVisible, 1);
            Memory.WriteWord(RefereeSprite.Base + RefereeSprite.OffOnScreen, 1);
            Memory.WriteWord(Memory.Addr.whichCard, 2); // kRedCard
        }, () => Referee.UpdateReferee());

        Scenario("referee_leaving_transitions_offscreen", () =>
        {
            Memory.WriteWord(Memory.Addr.refState, 5); // kRefLeaving
            Memory.WriteWord(RefereeSprite.Base + RefereeSprite.OffVisible, 1);
            Memory.WriteWord(RefereeSprite.Base + RefereeSprite.OffOnScreen, 0);
            Memory.WriteWord(Memory.Addr.gameStatePl, 101); // stopped -> state-machine-only path
        }, () => Referee.UpdateReferee());

        Scenario("referee_update_booked_player_number_sprite_blink_on", () =>
        {
            Memory.WriteWord(Memory.Addr.whichCard, 1); // kYellowCard
            Memory.WriteWord(Memory.Addr.refState, 4); // kRefBooking
            Memory.WriteDword(Memory.Addr.bookedPlayer, t1_2);
            Memory.WriteByte(t1_2 + PlayerSprite.OffPlayerState, 12); // kBooked
            Memory.WriteWord(Memory.Addr.refTimer, 0);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteDword(Memory.Addr.lastTeamBooked, top);
            Memory.WriteDword(top + TeamData.OffInGameTeamPtr, kTopTeamInGame);
            PlayerSprite.SetPlayerOrdinal(2, 5);
            Memory.WriteByte(kTopTeamInGame + 42 + 4 * 61 + 3, 9); // shirt number for slot 4
        }, () => Referee.UpdateBookedPlayerNumberSprite());

        Scenario("referee_update_booked_player_number_sprite_sentinel_sends_off", () =>
        {
            Memory.WriteWord(Memory.Addr.whichCard, 2); // kRedCard
            Memory.WriteWord(Memory.Addr.refState, 4);
            Memory.WriteDword(Memory.Addr.bookedPlayer, t1_2);
            Memory.WriteByte(t1_2 + PlayerSprite.OffPlayerState, 12);
            Memory.WriteWord(Memory.Addr.refTimer, 231); // index (231>>3)=28 -> next tick hits 29 (sentinel)
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteDword(Memory.Addr.lastTeamBooked, top);
            Memory.WriteDword(top + TeamData.OffInGameTeamPtr, kTopTeamInGame);
            PlayerSprite.SetPlayerOrdinal(2, 3);
        }, () => Referee.UpdateBookedPlayerNumberSprite());

        Scenario("referee_remove_referee", () =>
        {
            Memory.WriteWord(Memory.Addr.refState, 5);
            Memory.WriteWord(RefereeSprite.Base + RefereeSprite.OffVisible, 1);
        }, () => Referee.RemoveReferee());

        Scenario("referee_active_and_card_handing_accessors", () =>
        {
            Memory.WriteWord(Memory.Addr.refState, 2); // kRefWaitingPlayer
            Memory.WriteWord(Memory.Addr.whichCard, 1);
        }, () =>
        {
            _ = Referee.RefereeActive();
            _ = Referee.CardHandingInProgress();
        });

        // ==================================================================
        // Result -- scorer list + result-display timer
        // ==================================================================
        Scenario("result_reset_and_show_lifecycle", () =>
        {
        }, () =>
        {
            Result.ResetResult("Reds", "Blues");
            Memory.WriteDword(Memory.Addr.resultTimer, Result.kEndOfHalfResult);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Result.UpdateResult();
        });

        Scenario("result_countdown_hides_at_zero_halftime", () =>
        {
            Result.ResetResult("", "");
            Memory.WriteDword(Memory.Addr.resultTimer, 1);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 5);
            Memory.WriteWord(Memory.Addr.gameState, 25); // ST_RESULT_ON_HALFTIME
        }, () => Result.UpdateResult());

        Scenario("result_negative_timer_hides_immediately", () =>
        {
            Result.ResetResult("", "");
            Memory.WriteDword(Memory.Addr.resultTimer, -1);
        }, () => Result.UpdateResult());

        Scenario("result_hide_and_should_draw", () =>
        {
            Result.ResetResult("", "");
            Memory.WriteWord(Memory.Addr.res_showResult, 1);
        }, () =>
        {
            _ = Result.ShouldDrawResult();
            Result.HideResult();
            _ = Result.ShouldDrawResult();
        });

        Scenario("result_register_scorer_regular_goal", () =>
        {
            Result.ResetResult("", "");
            Memory.WriteDword(Memory.Addr.topTeamInGame, kTopTeamInGame);
            Memory.WriteDword(Memory.Addr.bottomTeamInGame, kBotTeamInGame);
            PlayerSprite.SetPlayerOrdinal(1, 1); // slot 1 -> playerInfo[0]
            Memory.WriteByte(kTopTeamInGame + 3, 7); // shirt number 7
            Memory.WriteDword(Memory.Addr.gt_gameTime + 1 * 4, 0);
            Memory.WriteDword(Memory.Addr.gt_gameTime + 2 * 4, 3);
            Memory.WriteDword(Memory.Addr.gt_gameTime + 3 * 4, 4);
        }, () => Result.RegisterScorer(t1_1, 1, 0)); // kRegular

        Scenario("result_register_scorer_own_goal", () =>
        {
            Result.ResetResult("", "");
            Memory.WriteDword(Memory.Addr.topTeamInGame, kTopTeamInGame);
            Memory.WriteDword(Memory.Addr.bottomTeamInGame, kBotTeamInGame);
            PlayerSprite.SetPlayerOrdinal(1, 1);
            Memory.WriteByte(kTopTeamInGame + 3, 5);
        }, () => Result.RegisterScorer(t1_1, 1, 2)); // kOwnGoal

        Scenario("result_register_scorer_second_goal_same_scorer", () =>
        {
            Result.ResetResult("", "");
            Memory.WriteDword(Memory.Addr.topTeamInGame, kTopTeamInGame);
            Memory.WriteDword(Memory.Addr.bottomTeamInGame, kBotTeamInGame);
            PlayerSprite.SetPlayerOrdinal(1, 1);
            Memory.WriteByte(kTopTeamInGame + 3, 9);
        }, () =>
        {
            Result.RegisterScorer(t1_1, 1, 0);
            Memory.WriteDword(Memory.Addr.gt_gameTime + 3 * 4, 55);
            Result.RegisterScorer(t1_1, 1, 1); // kPenalty, same shirt -> appends to same slot
        });

        // ==================================================================
        // Integration scenarios -- throw-in, corner/free-kick, card, referee
        // state change, time passing, half/match end (per review request)
        // ==================================================================
        Scenario("integration_throw_in_full_cycle", () =>
        {
            Memory.WriteWord(Memory.Addr.gameState, 15);
            Memory.WriteWord(Memory.Addr.resultTimer, 0);
            Memory.WriteWord(Memory.Addr.foulXCoordinate, 590);
            Memory.WriteWord(Memory.Addr.foulYCoordinate, 300);
            Memory.WriteDword(Memory.Addr.lastTeamPlayedBeforeBreak, top);
            PlayerSprite.SetX(2, 585 << 16); PlayerSprite.SetY(2, 305 << 16);
        }, () =>
        {
            // Spawn (ResolveThrowIn via TickSetPieces), then the per-player
            // dispatch that runs once the thrower is parked in PL_THROW_IN.
            SetPieces.TickSetPieces();
            Memory.WriteByte(top + TeamData.OffNormalFire, 1);
            Memory.WriteByte(Memory.Addr.playerTurnFlags, 0xFF);
        });

        Scenario("integration_corner_then_free_kick_direction_update", () =>
        {
            Memory.WriteWord(Memory.Addr.gameState, 5); // ST_CORNER_RIGHT
            Memory.WriteWord(Memory.Addr.resultTimer, 0);
            Memory.WriteWord(Memory.Addr.foulXCoordinate, 585);
            Memory.WriteWord(Memory.Addr.foulYCoordinate, 764);
            Memory.WriteDword(Memory.Addr.lastTeamPlayedBeforeBreak, top);
            Memory.WriteWord(Memory.Addr.cameraDirection, 6);
        }, () =>
        {
            SetPieces.TickSetPieces();
            Memory.WriteWord(Memory.Addr.gameState, 13); // ST_FOUL
            SetPieces.TickFreeKick(top);
        });

        Scenario("integration_card_activates_referee_and_walks_in", () =>
        {
            Memory.WriteWord(Memory.Addr.foulXCoordinate, 250);
            Memory.WriteWord(Memory.Addr.foulYCoordinate, 600);
            Memory.WriteWord(Memory.Addr.whichCard, 1); // kYellowCard
        }, () =>
        {
            Referee.ActivateReferee(); // referee-state change: off-screen -> incoming
            Referee.UpdateReferee();
        });

        Scenario("integration_time_passes_to_halftime_and_result_shows", () =>
        {
            GameTime.ResetGameTime();
            Result.ResetResult("", "");
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            Memory.WriteWord(Memory.Addr.lastFrameTicks, 1);
            Memory.WriteDword(Memory.Addr.gt_gameSeconds, unchecked((int)0xFFFFFFFF));
            Memory.WriteDword(Memory.Addr.gt_endGameCounter, 0);
            Memory.WriteDword(Memory.Addr.gt_gameTimeInMinutes, 45);
            Memory.WriteWord(Memory.Addr.goalCounter, 0);
        }, () =>
        {
            GameTime.UpdateGameTime(); // fires EndFirstHalf -> gameState=29, stoppageEventTimer=100
            Memory.WriteDword(Memory.Addr.resultTimer, Result.kEndOfHalfResult);
            Result.UpdateResult();
        });

        Console.WriteLine($"wrote {count} Step10 scenario dumps ({kMemSize} bytes each) to {outDir}");
    }
}
