// Generates the step-4 differential-test golden dumps for BallUpdate.cs +
// its forward-pulled dependencies (PlayerUpdate.UpdateBallWithControllingGoalkeeper,
// BallOutOfPlay.CheckIfBallOutOfPlay, UpdateGoals.BumpTeamGoals/GoalScored).
// Per review request, this compares FULL VM/Memory state (not just a
// function's return value) -- for each named scenario, sets up ball/team
// state after a fresh Memory.Init(true), runs ONE tick (or ApplyBallAfterTouch),
// and dumps the entire 0x60000-byte buffer. tests/test_ball_update_golden.c
// replays the same setup through the C port and byte-compares the result.
//
// Coverage requested by review: bounces, friction, heights, ground/post
// contact, extreme values -- plus goal-scoring (the headline feature of
// this module) and ApplyBallAfterTouch's spin/kick-boost paths.
//
// RegisterScorer/MatchAudio are deliberately out of scope for both sides of
// this comparison -- see UpdateGoalsStub.cs/MatchAudioStub.cs.
using OpenSwos.Sim.Port;
using OpenSwos.SwosVm;

public static class BallUpdateGolden
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
            File.WriteAllBytes(Path.Combine(outDir, $"ball_{name}.bin"), dump);
            count++;
        }

        // ---- Friction: ground / air / free-ball pitch-factor / clamp-to-zero ----
        Scenario("friction_ground_with_possession", () =>
        {
            TeamData.SetPlayerHasBall(true, 1); // possession -> no pitch factor
            BallSprite.Speed = 100;
            BallSprite.DestX = 400; BallSprite.DestY = 400;
            BallSprite.XPixels = 336; BallSprite.YPixels = 449;
        }, BallUpdate.Tick);

        Scenario("friction_free_ball_pitch_factor", () =>
        {
            Memory.WriteWord(Memory.Addr.pitchBallSpeedFactor, 5); // muddy pitch
            BallSprite.Speed = 100;
            BallSprite.DestX = 400; BallSprite.DestY = 400;
            BallSprite.XPixels = 336; BallSprite.YPixels = 449;
        }, BallUpdate.Tick);

        Scenario("friction_air", () =>
        {
            BallSprite.Speed = 500;
            BallSprite.Z = 20 << 16; // in the air
            BallSprite.DestX = 400; BallSprite.DestY = 400;
            BallSprite.XPixels = 336; BallSprite.YPixels = 449;
        }, BallUpdate.Tick);

        Scenario("friction_clamps_to_zero", () =>
        {
            BallSprite.Speed = 2; // less than kBallGroundConstant (13) -> clamps to 0
            BallSprite.DestX = 336; BallSprite.DestY = 449;
            BallSprite.XPixels = 336; BallSprite.YPixels = 449;
        }, BallUpdate.Tick);

        // ---- Heights / bounce: ground bounce small, ground bounce loud, extreme Z ----
        Scenario("bounce_small_settles", () =>
        {
            BallSprite.XPixels = 336; BallSprite.YPixels = 449;
            BallSprite.Z = 1 << 16; // just above ground
            BallSprite.DeltaZ = -0x00030000; // small downward delta -> small bounce
            BallSprite.Speed = 300;
        }, BallUpdate.Tick);

        Scenario("bounce_loud", () =>
        {
            BallSprite.XPixels = 336; BallSprite.YPixels = 449;
            BallSprite.Z = 2 << 16;
            BallSprite.DeltaZ = -0x00300000; // large downward delta -> loud bounce
            BallSprite.Speed = 2000;
        }, BallUpdate.Tick);

        Scenario("extreme_high_z_gravity_only", () =>
        {
            BallSprite.XPixels = 336; BallSprite.YPixels = 449;
            BallSprite.Z = 200 << 16; // extreme height, still airborne after gravity
            BallSprite.DeltaZ = 0x00100000;
            BallSprite.Speed = 2688; // kHighKickBallSpeed, near max realistic speed
        }, BallUpdate.Tick);

        // ---- Keeper holds ball: z==5 settle / z>5 falling / z<5 rising ----
        Scenario("keeper_holds_ball_at_hand_height", () =>
        {
            Memory.WriteWord(Memory.Addr.gameState, 3); // ST_KEEPER_HOLDS_BALL
            BallSprite.ZPixels = 5;
            BallSprite.Speed = 0;
        }, BallUpdate.Tick);

        Scenario("keeper_holds_ball_above_hand", () =>
        {
            Memory.WriteWord(Memory.Addr.gameState, 3);
            BallSprite.ZPixels = 20;
            BallSprite.Speed = 0;
        }, BallUpdate.Tick);

        Scenario("keeper_holds_ball_below_hand_with_controller", () =>
        {
            Memory.WriteWord(Memory.Addr.gameState, 3);
            BallSprite.ZPixels = 2;
            BallSprite.Speed = 50; // nonzero -> exercises UpdateBallWithControllingGoalkeeper
            Memory.WriteDword(Memory.Addr.lastTeamPlayedBeforeBreak, TeamData.TopBase);
            int keeperBase = PlayerSprite.Base(PlayerSprite.SlotGoalie1);
            TeamData.SetControlledPlayer(true, keeperBase);
            PlayerSprite.SetDirection(PlayerSprite.SlotGoalie1, 2);
            PlayerSprite.SetX(PlayerSprite.SlotGoalie1, 300 << 16);
            PlayerSprite.SetY(PlayerSprite.SlotGoalie1, 150 << 16);
        }, BallUpdate.Tick);

        // ---- Ground/post contact: X barrier, Y barrier (non-game-in-progress) ----
        Scenario("x_barrier_bounce_left", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 0); // NOT ST_GAME_IN_PROGRESS
            BallSprite.XPixels = 40; // < 53
            BallSprite.YPixels = 400;
            BallSprite.DestX = 10; BallSprite.DestY = 400;
            BallSprite.Speed = 800;
        }, BallUpdate.Tick);

        Scenario("y_barrier_bounce_bottom", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 0);
            BallSprite.XPixels = 300;
            BallSprite.YPixels = 820; // > 799
            BallSprite.DestX = 300; BallSprite.DestY = 850;
            BallSprite.Speed = 800;
        }, BallUpdate.Tick);

        // ---- Goal scoring: upper goal, lower goal, own goal, penalty bar deflect ----
        Scenario("goal_scored_lower_net", () =>
        {
            // Ball deep in the lower net -> l_ball_in_net -> team1_scored path
            // (bottomTeamData.teamNumber default is 2 from a fresh Init, so
            // upper-goal would score for team2; lower net scores for team1).
            BallSprite.XPixels = 336; // within [303, 373) after the -1 bias
            BallSprite.YPixels = 780; // within (778, 785)
            BallSprite.ZPixels = 10;  // <= 19
            BallSprite.DeltaY = 30000; // outside the |dy|<0x5000 bar-deflect band
            Memory.WriteDword(Memory.Addr.lastPlayerPlayed, PlayerSprite.Base(1));
        }, BallUpdate.Tick);

        Scenario("goal_scored_upper_net", () =>
        {
            BallSprite.XPixels = 336;
            BallSprite.YPixels = 115; // within (112, 119)
            BallSprite.ZPixels = 5;
            BallSprite.DeltaY = -30000;
            Memory.WriteDword(Memory.Addr.lastPlayerPlayed, PlayerSprite.Base(12));
        }, BallUpdate.Tick);

        Scenario("penalty_bar_deflect", () =>
        {
            // Ball crosses the goal-line band above the bar height (D3>15) with
            // a SMALL deltaY -> l_penalty_goal -> l_reverse_delta_z (deflect,
            // not a real goal).
            BallSprite.XPixels = 336;
            BallSprite.YPixels = 128; // <=128, not in either near-net band -> falls to cseg_7C7F0's D2<=128 upper-cap check
            BallSprite.ZPixels = 18;  // >15 -> l_penalty_goal
            BallSprite.DeltaY = 1000; // within |dy|<0x5000 -> deflect, not goal
            BallSprite.DeltaZ = 5000;
        }, BallUpdate.Tick);

        // ---- Out of play: corner, throw-in (both halves), goal-out ----
        Scenario("corner_left_upper", () =>
        {
            BallSprite.XPixels = 90; // < 81 -> out of play, out left
            BallSprite.YPixels = 120; // < 129 -> upper corner band
            Memory.WriteDword(Memory.Addr.lastTeamPlayed, TeamData.TopBase);
        }, BallUpdate.Tick);

        Scenario("throw_in_left_half", () =>
        {
            BallSprite.XPixels = 75; // < 81 -> out left
            BallSprite.YPixels = 400; // mid-pitch -> throw-in, not corner/goal-out
            Memory.WriteDword(Memory.Addr.lastTeamPlayed, TeamData.BottomBase);
        }, BallUpdate.Tick);

        Scenario("throw_in_right_half", () =>
        {
            BallSprite.XPixels = 600; // > 590 -> out right
            BallSprite.YPixels = 400;
            Memory.WriteDword(Memory.Addr.lastTeamPlayed, TeamData.TopBase);
        }, BallUpdate.Tick);

        Scenario("goal_out_upper", () =>
        {
            BallSprite.XPixels = 336;
            BallSprite.YPixels = 100; // < 129 -> out top, outside goal-mouth X range
        }, BallUpdate.Tick);

        // ---- ApplyBallAfterTouch: spin, high-kick boost, speed adjustment, long pass ----
        Scenario("aftertouch_left_spin", () =>
        {
            TeamData.SetSpinTimer(true, 1);
            TeamData.SetCurrentAllowedDirection(true, 0);
            TeamData.SetControlledPlDirection(true, 2); // delta=2 -> left spin (table offset 0)
            BallSprite.DestX = 300; BallSprite.DestY = 400;
        }, () => BallUpdate.ApplyBallAfterTouch(true));

        Scenario("aftertouch_right_spin", () =>
        {
            TeamData.SetSpinTimer(true, 1);
            TeamData.SetCurrentAllowedDirection(true, 0);
            TeamData.SetControlledPlDirection(true, 6); // delta=6 -> right spin (table offset 4)
            BallSprite.DestX = 300; BallSprite.DestY = 400;
        }, () => BallUpdate.ApplyBallAfterTouch(true));

        Scenario("aftertouch_tick4_high_kick", () =>
        {
            TeamData.SetSpinTimer(true, 4);
            TeamData.SetCurrentAllowedDirection(true, 0);
            TeamData.SetControlledPlDirection(true, 4); // delta=4 -> HIGH kick boost
            BallSprite.DestX = 300; BallSprite.DestY = 400;
        }, () => BallUpdate.ApplyBallAfterTouch(true));

        Scenario("aftertouch_tick4_normal_kick", () =>
        {
            TeamData.SetSpinTimer(true, 4);
            TeamData.SetCurrentAllowedDirection(true, 0);
            TeamData.SetControlledPlDirection(true, 2); // delta=2 -> NORMAL kick boost
            BallSprite.DestX = 300; BallSprite.DestY = 400;
        }, () => BallUpdate.ApplyBallAfterTouch(true));

        Scenario("aftertouch_pass_long_pass_boost", () =>
        {
            TeamData.SetPassInProgress(true, 1);
            TeamData.SetSpinTimer(true, 1);
            TeamData.SetCurrentAllowedDirection(true, 0);
            TeamData.SetControlledPlDirection(true, 2); // perpendicular -> longPass boost
            BallSprite.Direction = 0;
            BallSprite.Speed = 1000;
            BallSprite.DestX = 300; BallSprite.DestY = 400;
        }, () => BallUpdate.ApplyBallAfterTouch(true));

        Scenario("aftertouch_spin_timer_expires", () =>
        {
            TeamData.SetSpinTimer(true, 9); // -> increments to 10 -> resets to -1
            TeamData.SetCurrentAllowedDirection(true, -1); // no spin, aligned kick
            BallSprite.DestX = 300; BallSprite.DestY = 400;
        }, () => BallUpdate.ApplyBallAfterTouch(true));

        Console.WriteLine($"wrote {count} BallUpdate scenario dumps ({kMemSize} bytes each) to {outDir}");
    }
}
