// Generates the step-8 differential-test golden dumps: InputControls.cs
// (gameControls.cpp, 332 LOC). Unlike UpdatePlayers.cs (step 7B), every
// function here except the four file-static helpers is public, so most
// scenarios call the specific function under test directly -- only
// UpdateControlledPlayer/UpdatePlayerBeingPassedTo/
// UpdatePlayerBeingPassedToStopped (all `private static`) are exercised
// indirectly through UpdateTeamControls(top), same technique as
// Step7BGolden.cs.
//
// EventsToDirection/DirectionToEvents are pure functions with NO Memory
// side effects of their own -- a full-buffer diff can't observe their
// return value directly. They're exercised indirectly instead: DirectionToEvents
// via SetJoystickState (writes the resulting events bitmask to Memory) and
// EventsToDirection via UpdateTeamControls's updateTeamControlsInternal
// (writes the resulting direction to TeamData.OffCurrentAllowedDirection/
// OffDirection) -- both with varied direction/event combinations.
using OpenSwos.Sim.Port;
using OpenSwos.SwosVm;

public static class Step8Golden
{
    private const int kMemSize = 0x60000;

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
            File.WriteAllBytes(Path.Combine(outDir, $"s8_{name}.bin"), dump);
            count++;
        }

        int out1_1 = PlayerSprite.Base(1);
        int out1_2 = PlayerSprite.Base(2);
        int out2_1 = PlayerSprite.Base(12);

        // ---- ResetGameControls ----
        Scenario("reset_game_controls", () =>
        {
            Memory.WriteDword(Memory.Addr.teamSwitchCounter, 7);
            Memory.WriteByte(Memory.Addr.ic_pl1LastFired, 1);
            Memory.WriteByte(Memory.Addr.ic_pl2LastFired, 1);
            Memory.WriteDword(Memory.Addr.ic_pl1FireCounter, -3);
            Memory.WriteDword(Memory.Addr.ic_pl2FireCounter, 5);
            Memory.WriteDword(Memory.Addr.ic_oldPl1Events, 9);
            Memory.WriteDword(Memory.Addr.ic_oldPl2Events, 3);
            Memory.WriteDword(Memory.Addr.ic_pl1LastVertical, 1);
            Memory.WriteDword(Memory.Addr.ic_pl1LastHorizontal, 4);
            Memory.WriteDword(Memory.Addr.ic_pl2LastVertical, 2);
            Memory.WriteDword(Memory.Addr.ic_pl2LastHorizontal, 8);
        }, () => InputControls.ResetGameControls());

        // ---- UpdateFireBlocked ----
        Scenario("fire_blocked_clears_when_released", () =>
        {
            Memory.WriteWord(Memory.Addr.fireBlocked, 1);
            Memory.WriteDword(Memory.Addr.ic_pl1Events, 0);
            Memory.WriteDword(Memory.Addr.ic_pl2Events, 0);
        }, () => InputControls.UpdateFireBlocked());

        Scenario("fire_blocked_stays_when_firing", () =>
        {
            Memory.WriteWord(Memory.Addr.fireBlocked, 1);
            Memory.WriteDword(Memory.Addr.ic_pl1Events, (int)InputControls.GameControlEvents.kGameEventKick);
        }, () => InputControls.UpdateFireBlocked());

        Scenario("fire_blocked_not_set", () =>
        {
            Memory.WriteWord(Memory.Addr.fireBlocked, 0);
        }, () => InputControls.UpdateFireBlocked());

        // ---- SelectTeamForUpdate ----
        Scenario("select_team_for_update_first_call", () => { },
            () => InputControls.SelectTeamForUpdate());

        Scenario("select_team_for_update_second_call", () => { },
            () => { InputControls.SelectTeamForUpdate(); InputControls.SelectTeamForUpdate(); });

        // ---- GetPlayerEvents / filterOverlappedEvents ----
        Scenario("get_player_events_no_conflict", () =>
        {
            Memory.WriteDword(Memory.Addr.ic_pl1Events,
                (int)(InputControls.GameControlEvents.kGameEventUp | InputControls.GameControlEvents.kGameEventRight));
        }, () => InputControls.GetPlayerEvents(InputControls.kPlayer1));

        Scenario("get_player_events_updown_conflict_latches_down", () =>
        {
            Memory.WriteDword(Memory.Addr.ic_oldPl1Events, (int)InputControls.GameControlEvents.kGameEventUp);
            Memory.WriteDword(Memory.Addr.ic_pl1Events,
                (int)(InputControls.GameControlEvents.kGameEventUp | InputControls.GameControlEvents.kGameEventDown));
        }, () => InputControls.GetPlayerEvents(InputControls.kPlayer1));

        Scenario("get_player_events_leftright_conflict_latches_right", () =>
        {
            Memory.WriteDword(Memory.Addr.ic_oldPl2Events, (int)InputControls.GameControlEvents.kGameEventLeft);
            Memory.WriteDword(Memory.Addr.ic_pl2Events,
                (int)(InputControls.GameControlEvents.kGameEventLeft | InputControls.GameControlEvents.kGameEventRight));
        }, () => InputControls.GetPlayerEvents(InputControls.kPlayer2));

        // ---- IsPlayerFiring / IsAnyPlayerFiring ----
        Scenario("is_player_firing_true", () =>
        {
            Memory.WriteDword(Memory.Addr.ic_pl1Events, (int)InputControls.GameControlEvents.kGameEventKick);
        }, () => InputControls.IsPlayerFiring(InputControls.kPlayer1));

        Scenario("is_player_firing_false", () =>
        {
            Memory.WriteDword(Memory.Addr.ic_pl1Events, 0);
        }, () => InputControls.IsPlayerFiring(InputControls.kPlayer1));

        Scenario("is_any_player_firing_true", () =>
        {
            Memory.WriteDword(Memory.Addr.ic_pl2Events, (int)InputControls.GameControlEvents.kGameEventKick);
        }, () => InputControls.IsAnyPlayerFiring());

        Scenario("is_any_player_firing_false", () =>
        {
            Memory.WriteDword(Memory.Addr.ic_pl1Events, 0);
            Memory.WriteDword(Memory.Addr.ic_pl2Events, 0);
        }, () => InputControls.IsAnyPlayerFiring());

        // ---- GetFireStartedAndBumpFireCounter (press -> hold -> release) ----
        Scenario("fire_sequence_press_hold_release", () => { }, () =>
        {
            InputControls.GetFireStartedAndBumpFireCounter(true, InputControls.kPlayer1);  // press
            InputControls.GetFireStartedAndBumpFireCounter(true, InputControls.kPlayer1);  // hold
            InputControls.GetFireStartedAndBumpFireCounter(false, InputControls.kPlayer1); // release
        });

        // ---- SetJoystickState (exercises DirectionToEvents indirectly) ----
        Scenario("set_joystick_state_up_right_with_fire", () => { },
            () => InputControls.SetJoystickState(InputControls.kPlayer1, InputControls.kFacingTopRight, true, false));

        Scenario("set_joystick_state_center_no_fire", () => { },
            () => InputControls.SetJoystickState(InputControls.kPlayer2, InputControls.kNoDirection, false, false));

        // ---- PostUpdateTeamControls ----
        Scenario("post_update_team_controls_clears_header_or_tackle", () =>
        {
            Memory.WriteWord(TeamData.TopBase + TeamData.OffHeaderOrTackle, 1);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffPlayerNumber, 1);
            Memory.WriteDword(Memory.Addr.ic_pl1FireCounter, -3);
        }, () => InputControls.PostUpdateTeamControls(true));

        // ---- UpdateTeamControls (covers UpdateControlledPlayer +
        // UpdatePlayerBeingPassedTo(Stopped) + updateGameControls +
        // updateTeamControlsInternal) ----
        Scenario("update_team_controls_human_top_normal_play", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffPlayerNumber, 1);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffBallOutOfPlay, 1);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffGoaliePlayingOrOut, 1);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffBallInPlay, 1);
            BallSprite.XPixels = 300; BallSprite.YPixels = 400;
            PlayerSprite.SetX(1, 305 << 16); PlayerSprite.SetY(1, 404 << 16); // closest
            PlayerSprite.SetX(2, 500 << 16); PlayerSprite.SetY(2, 700 << 16); // far
            Memory.WriteDword(Memory.Addr.ic_pl1Events,
                (int)(InputControls.GameControlEvents.kGameEventUp | InputControls.GameControlEvents.kGameEventRight
                      | InputControls.GameControlEvents.kGameEventKick));
        }, () => InputControls.UpdateTeamControls(true));

        Scenario("update_team_controls_ai_team_skips_input", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffPlayerNumber, 0); // CPU
            Memory.WriteWord(TeamData.TopBase + TeamData.OffBallOutOfPlay, 1);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffGoaliePlayingOrOut, 1);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffBallInPlay, 1);
            BallSprite.XPixels = 300; BallSprite.YPixels = 400;
            PlayerSprite.SetX(1, 305 << 16); PlayerSprite.SetY(1, 404 << 16);
        }, () => InputControls.UpdateTeamControls(true));

        Scenario("update_team_controls_ball_dead_no_promotion", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffPlayerNumber, 1);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffBallOutOfPlay, 0); // dead ball -> no promotion
            Memory.WriteWord(TeamData.TopBase + TeamData.OffGoaliePlayingOrOut, 1);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffBallInPlay, 1);
            TeamData.SetControlledPlayer(true, out1_2);
            BallSprite.XPixels = 300; BallSprite.YPixels = 400;
            PlayerSprite.SetX(1, 305 << 16); PlayerSprite.SetY(1, 404 << 16);
        }, () => InputControls.UpdateTeamControls(true));

        Scenario("update_team_controls_disqualified_sentaway_and_tackling", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffPlayerNumber, 1);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffBallOutOfPlay, 1);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffGoaliePlayingOrOut, 1);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffBallInPlay, 1);
            BallSprite.XPixels = 300; BallSprite.YPixels = 400;
            // slot 1: closest by distance but sent away -> disqualified.
            PlayerSprite.SetX(1, 301 << 16); PlayerSprite.SetY(1, 401 << 16);
            Memory.WriteWord(out1_1 + PlayerSprite.OffSentAway, 1);
            // slot 2: next closest but tackling -> disqualified.
            PlayerSprite.SetX(2, 310 << 16); PlayerSprite.SetY(2, 410 << 16);
            PlayerSprite.SetPlayerState(2, 1); // PL_TACKLING
            // slot 3: farther but eligible -> promoted.
            PlayerSprite.SetX(3, 340 << 16); PlayerSprite.SetY(3, 440 << 16);
        }, () => InputControls.UpdateTeamControls(true));

        Scenario("update_team_controls_stoppage_pass_to_player", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 0); // stopped
            Memory.WriteWord(TeamData.TopBase + TeamData.OffPlayerNumber, 1);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffBallInPlay, 1);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffBallOutOfPlayOrKeeper, 1);
            BallSprite.XPixels = 300; BallSprite.YPixels = 400;
            PlayerSprite.SetX(1, 305 << 16); PlayerSprite.SetY(1, 404 << 16);
            Memory.WriteDword(out1_1 + PlayerSprite.OffBallDistance, 50);
            PlayerSprite.SetX(2, 500 << 16); PlayerSprite.SetY(2, 700 << 16);
            Memory.WriteDword(out1_2 + PlayerSprite.OffBallDistance, 90000);
        }, () => InputControls.UpdateTeamControls(true));

        Scenario("update_team_controls_bench_reset", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffPlayerNumber, 0); // avoid input branch
            Memory.WriteWord(TeamData.TopBase + TeamData.OffBallOutOfPlay, 0);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffResetControls, 0);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffCurrentAllowedDirection, 3);
            Memory.WriteByte(TeamData.TopBase + TeamData.OffQuickFire, 1);
            Memory.WriteByte(TeamData.TopBase + TeamData.OffNormalFire, 1);
            Memory.WriteByte(TeamData.TopBase + TeamData.OffFirePressed, 1);
            Memory.WriteByte(TeamData.TopBase + TeamData.OffFireThisFrame, 1);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffFireCounter, 7);
            Memory.WriteWord(Memory.Addr.g_inSubstitutesMenu, 1);
        }, () => InputControls.UpdateTeamControls(true));

        Console.WriteLine($"wrote {count} Step8 scenario dumps ({kMemSize} bytes each) to {outDir}");
    }
}
