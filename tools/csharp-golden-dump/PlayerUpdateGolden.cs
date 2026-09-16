// Generates the step-5.5 differential-test golden dumps for the rest of
// PlayerUpdate.cs (UpdateBallWithControllingGoalkeeper was already covered
// in step 4's BallUpdateGolden.cs). Same full-Memory-buffer pattern: for
// each named scenario, set up state after a fresh Memory.Init(true), run
// ONE call into PlayerUpdate, and dump the entire 0x60000-byte buffer.
// tests/test_player_update_golden.c replays the same setup through the C
// port and byte-compares the result.
//
// MatchAudio/telemetry (keeper-dive counters)/the Godot debug print are
// deliberately out of scope for both sides -- see swos_player_update.h.
//
// RNG: swosShouldGoalkeeperDive's penalty-save-distance pick is the only
// RNG draw in this file's scope (Rng.NextByte()). The one scenario that
// reaches it explicitly reseeds immediately before acting.
//
// PlayerInfo / shotChanceTable: like PlayerActionsGolden.cs, most scenarios
// leave these unwired (0) -- the honest current state of this port. Two
// scenarios additionally wire a synthetic PlayerInfo/shotChanceTable pair
// into the small unused gap between the per-team sprite pointer tables
// (ending 0x4FE58) and the sprite pool (starting 0x50000), reusing/
// extending PlayerActionsGolden's scratch layout: PlayerInfo at
// 0x4FE60/0x4FEA0 (top/bottom, 61 bytes each, ending 0x4FEDD), plus a new
// 64-byte shotChanceTable scratch pair at 0x4FEE0/0x4FF20 (well clear of
// both the PlayerInfo blocks and the 0x50000 sprite pool).
using OpenSwos.Sim.Port;
using OpenSwos.SwosVm;

public static class PlayerUpdateGolden
{
    private const int kMemSize = 0x60000;

    private const int kTopPlayerInfo = 0x4FE60;
    private const int kBottomPlayerInfo = 0x4FEA0;
    private const int kTopShotChance = 0x4FEE0;
    private const int kBottomShotChance = 0x4FF20;

    private static void WirePlayerInfo(bool top, int passing, int shooting, int heading,
        int tackling, int ballControl, int speed, int finishing, int goalieSkill)
    {
        int teamBase = top ? TeamData.TopBase : TeamData.BottomBase;
        int piBase = top ? kTopPlayerInfo : kBottomPlayerInfo;
        Memory.WriteDword(teamBase + TeamData.OffInGameTeamPtr, piBase);
        Memory.WriteByte(piBase + TeamDataLoader.OffPassing, passing);
        Memory.WriteByte(piBase + TeamDataLoader.OffShooting, shooting);
        Memory.WriteByte(piBase + TeamDataLoader.OffHeading, heading);
        Memory.WriteByte(piBase + TeamDataLoader.OffTackling, tackling);
        Memory.WriteByte(piBase + TeamDataLoader.OffBallControl, ballControl);
        Memory.WriteByte(piBase + TeamDataLoader.OffSpeed, speed);
        Memory.WriteByte(piBase + TeamDataLoader.OffFinishing, finishing);
        Memory.WriteByte(piBase + 34 /* goalieSkill */, goalieSkill);
    }

    private static void WireShotChanceTable(bool top, short entry10, short entry48,
        short thresh52, short thresh54)
    {
        int teamBase = top ? TeamData.TopBase : TeamData.BottomBase;
        int scBase = top ? kTopShotChance : kBottomShotChance;
        Memory.WriteDword(teamBase + TeamData.OffShotChanceTable, scBase);
        Memory.WriteWord(scBase + 10, entry10);
        Memory.WriteWord(scBase + 48, entry48);
        Memory.WriteWord(scBase + 52, thresh52);
        Memory.WriteWord(scBase + 54, thresh54);
    }

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
            File.WriteAllBytes(Path.Combine(outDir, $"pu_{name}.bin"), dump);
            count++;
        }

        int keeper1 = PlayerSprite.Base(PlayerSprite.SlotGoalie1);
        int keeper2 = PlayerSprite.Base(PlayerSprite.SlotGoalie2);

        // ---- GoalkeeperClaimedTheBall ----
        Scenario("claimed_normal_top", () =>
        {
            BallSprite.XPixels = 336; BallSprite.YPixels = 780;
            PlayerSprite.SetDirection(PlayerSprite.SlotGoalie1, 2);
        }, () => PlayerUpdate.GoalkeeperClaimedTheBall(keeper1, true));

        Scenario("claimed_mid_dive_early_out", () =>
        {
            TeamData.SetGoalkeeperDivingRight(true, 1);
            BallSprite.XPixels = 336; BallSprite.YPixels = 780;
        }, () => PlayerUpdate.GoalkeeperClaimedTheBall(keeper1, true));

        Scenario("claimed_cpu_clears_ew_flags", () =>
        {
            Memory.WriteWord(TeamData.BottomBase + TeamData.OffPlayerNumber, 0); // CPU
            BallSprite.XPixels = 336; BallSprite.YPixels = 129;
        }, () => PlayerUpdate.GoalkeeperClaimedTheBall(keeper2, false));

        // ---- TickGoalieDivingClaimCompletion ----
        Scenario("dive_claim_not_diving_state", () =>
        {
            PlayerSprite.SetPlayerState(PlayerSprite.SlotGoalie1, 0); // Normal
        }, () => PlayerUpdate.TickGoalieDivingClaimCompletion(keeper1, true));

        Scenario("dive_claim_flag_not_set", () =>
        {
            PlayerSprite.SetPlayerState(PlayerSprite.SlotGoalie1, 6); // diving high
            TeamData.SetGoalkeeperDivingRight(true, 0);
        }, () => PlayerUpdate.TickGoalieDivingClaimCompletion(keeper1, true));

        Scenario("dive_claim_too_early", () =>
        {
            PlayerSprite.SetPlayerState(PlayerSprite.SlotGoalie1, 7); // diving low
            TeamData.SetGoalkeeperDivingRight(true, 1);
            PlayerSprite.SetPlayerDownTimer(PlayerSprite.SlotGoalie1, 50); // ->49 >42
        }, () => PlayerUpdate.TickGoalieDivingClaimCompletion(keeper1, true));

        Scenario("dive_claim_rise_path", () =>
        {
            PlayerSprite.SetPlayerState(PlayerSprite.SlotGoalie1, 6);
            TeamData.SetGoalkeeperDivingRight(true, 1);
            PlayerSprite.SetPlayerDownTimer(PlayerSprite.SlotGoalie1, 1); // ->0
        }, () => PlayerUpdate.TickGoalieDivingClaimCompletion(keeper1, true));

        Scenario("dive_claim_completes", () =>
        {
            PlayerSprite.SetPlayerState(PlayerSprite.SlotGoalie1, 6);
            TeamData.SetGoalkeeperDivingRight(true, 1);
            PlayerSprite.SetPlayerDownTimer(PlayerSprite.SlotGoalie1, 20); // ->19, in (0,42]
            PlayerSprite.SetX(PlayerSprite.SlotGoalie1, 336 << 16);
            PlayerSprite.SetY(PlayerSprite.SlotGoalie1, 780 << 16);
        }, () => PlayerUpdate.TickGoalieDivingClaimCompletion(keeper1, true));

        // ---- GoalkeeperCaughtTheBall ----
        Scenario("caught_clamp_low", () =>
        {
            Memory.WriteWord(Memory.Addr.ballNextGroundX, 300);
            Memory.WriteWord(Memory.Addr.ballNextGroundY, 100); // < 137
        }, () => PlayerUpdate.GoalkeeperCaughtTheBall(keeper1, true));

        Scenario("caught_clamp_high", () =>
        {
            Memory.WriteWord(Memory.Addr.ballNextGroundX, 300);
            Memory.WriteWord(Memory.Addr.ballNextGroundY, 800); // > 761
        }, () => PlayerUpdate.GoalkeeperCaughtTheBall(keeper2, false));

        Scenario("caught_no_clamp", () =>
        {
            Memory.WriteWord(Memory.Addr.ballNextGroundX, 300);
            Memory.WriteWord(Memory.Addr.ballNextGroundY, 400);
        }, () => PlayerUpdate.GoalkeeperCaughtTheBall(keeper1, true));

        // ---- TickGoalieCatchingBall ----
        Scenario("catching_still_diving_left_skip", () =>
        {
            PlayerSprite.SetPlayerDownTimer(PlayerSprite.SlotGoalie1, 10); // ->9 != 0
            TeamData.SetGoalkeeperDivingLeft(true, 1);
        }, () => PlayerUpdate.TickGoalieCatchingBall(keeper1, true));

        Scenario("catching_not_close_skip", () =>
        {
            PlayerSprite.SetPlayerDownTimer(PlayerSprite.SlotGoalie1, 10);
            TeamData.SetGoalkeeperDivingLeft(true, 0);
            Memory.WriteByte(TeamData.TopBase + 61, 0); // plVeryCloseToBall
        }, () => PlayerUpdate.TickGoalieCatchingBall(keeper1, true));

        Scenario("catching_close_catch", () =>
        {
            PlayerSprite.SetPlayerDownTimer(PlayerSprite.SlotGoalie1, 10);
            TeamData.SetGoalkeeperDivingLeft(true, 0);
            Memory.WriteByte(TeamData.TopBase + 61, 1); // plVeryCloseToBall
            Memory.WriteWord(Memory.Addr.currentGameTick, 0); // d1Rnd=0, shotChance48 default 0 -> diff=0, not <0... use table
            WireShotChanceTable(true, entry10: 0, entry48: 5, thresh52: 0, thresh54: 0);
        }, () => PlayerUpdate.TickGoalieCatchingBall(keeper1, true));

        Scenario("catching_close_deflect", () =>
        {
            PlayerSprite.SetPlayerDownTimer(PlayerSprite.SlotGoalie1, 10);
            TeamData.SetGoalkeeperDivingLeft(true, 0);
            Memory.WriteByte(TeamData.TopBase + 61, 1);
            Memory.WriteWord(Memory.Addr.currentGameTick, 0xF0); // d1Rnd=15
            WireShotChanceTable(true, entry10: 0, entry48: -5, thresh52: 0, thresh54: 0);
        }, () => PlayerUpdate.TickGoalieCatchingBall(keeper1, true));

        Scenario("catching_timer_zero_no_diving", () =>
        {
            PlayerSprite.SetPlayerDownTimer(PlayerSprite.SlotGoalie1, 1); // ->0
            TeamData.SetGoalkeeperDivingLeft(true, 0);
        }, () => PlayerUpdate.TickGoalieCatchingBall(keeper1, true));

        Scenario("catching_timer_zero_with_diving", () =>
        {
            PlayerSprite.SetPlayerDownTimer(PlayerSprite.SlotGoalie1, 1);
            TeamData.SetGoalkeeperDivingLeft(true, 1);
            BallSprite.XPixels = 336; BallSprite.YPixels = 780;
        }, () => PlayerUpdate.TickGoalieCatchingBall(keeper1, true));

        // ---- TickGoalieClaimed ----
        Scenario("claimed_state_still", () =>
        {
            PlayerSprite.SetPlayerDownTimer(PlayerSprite.SlotGoalie1, 5); // ->4 != 0
        }, () => PlayerUpdate.TickGoalieClaimed(keeper1));

        Scenario("claimed_state_finishes", () =>
        {
            PlayerSprite.SetPlayerDownTimer(PlayerSprite.SlotGoalie1, 1); // ->0
        }, () => PlayerUpdate.TickGoalieClaimed(keeper1));

        // ---- TickGoalkeeperHoldAutoRelease ----
        Scenario("hold_release_not_gs3", () =>
        {
            Memory.WriteWord(Memory.Addr.gameState, 5);
        }, () => PlayerUpdate.TickGoalkeeperHoldAutoRelease(keeper1, true));

        Scenario("hold_release_wrong_team", () =>
        {
            Memory.WriteWord(Memory.Addr.gameState, 3);
            Memory.WriteDword(Memory.Addr.lastTeamPlayedBeforeBreak, TeamData.BottomBase);
        }, () => PlayerUpdate.TickGoalkeeperHoldAutoRelease(keeper1, true));

        Scenario("hold_release_human_never", () =>
        {
            Memory.WriteWord(Memory.Addr.gameState, 3);
            Memory.WriteDword(Memory.Addr.lastTeamPlayedBeforeBreak, TeamData.TopBase);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffPlayerNumber, 1); // human
            Memory.WriteWord(Memory.Addr.stoppageTimerActive, 400);
        }, () => PlayerUpdate.TickGoalkeeperHoldAutoRelease(keeper1, true));

        Scenario("hold_release_cpu_not_yet", () =>
        {
            Memory.WriteWord(Memory.Addr.gameState, 3);
            Memory.WriteDword(Memory.Addr.lastTeamPlayedBeforeBreak, TeamData.TopBase);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffPlayerNumber, 0);
            Memory.WriteWord(Memory.Addr.stoppageTimerActive, 100); // < 300
        }, () => PlayerUpdate.TickGoalkeeperHoldAutoRelease(keeper1, true));

        Scenario("hold_release_cpu_fires", () =>
        {
            Memory.WriteWord(Memory.Addr.gameState, 3);
            Memory.WriteDword(Memory.Addr.lastTeamPlayedBeforeBreak, TeamData.TopBase);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffPlayerNumber, 0);
            Memory.WriteWord(Memory.Addr.stoppageTimerActive, 350); // >= 300
            TeamData.SetControlledPlayer(true, keeper1);
            PlayerSprite.SetDirection(PlayerSprite.SlotGoalie1, 3);
            BallSprite.XPixels = 336; BallSprite.YPixels = 780;
        }, () => PlayerUpdate.TickGoalkeeperHoldAutoRelease(keeper1, true));

        Scenario("hold_release_dive_completion_delegate", () =>
        {
            // TickGoalieDivingClaimCompletion fires first and takes priority.
            PlayerSprite.SetPlayerState(PlayerSprite.SlotGoalie1, 6);
            TeamData.SetGoalkeeperDivingRight(true, 1);
            PlayerSprite.SetPlayerDownTimer(PlayerSprite.SlotGoalie1, 20);
            PlayerSprite.SetX(PlayerSprite.SlotGoalie1, 336 << 16);
            PlayerSprite.SetY(PlayerSprite.SlotGoalie1, 780 << 16);
        }, () => PlayerUpdate.TickGoalkeeperHoldAutoRelease(keeper1, true));

        // ---- ShouldGoalkeeperDive ----
        // TOP team flips the sign: d0w = -(ball.y - keeper.y). "Behind" means
        // d0w < 0, i.e. ball.y > keeper.y for the top team.
        Scenario("should_dive_behind_too_far", () =>
        {
            BallSprite.YPixels = 300;
            PlayerSprite.SetYPixels(PlayerSprite.SlotGoalie1, 100); // raw=200 -> d0w=-200 < -10
        }, () => PlayerUpdate.ShouldGoalkeeperDive(keeper1, BallSprite.Base, TeamData.TopBase));

        Scenario("should_dive_behind_close_try", () =>
        {
            BallSprite.YPixels = 105;
            PlayerSprite.SetYPixels(PlayerSprite.SlotGoalie1, 100); // raw=5 -> d0w=-5, in [-10,0)
        }, () => PlayerUpdate.ShouldGoalkeeperDive(keeper1, BallSprite.Base, TeamData.TopBase));

        Scenario("should_dive_front_too_far", () =>
        {
            Memory.WriteWord(Memory.Addr.kKeeperSaveDistance, 50);
            BallSprite.YPixels = 100;
            PlayerSprite.SetYPixels(PlayerSprite.SlotGoalie1, 300); // raw=-200 -> d0w=200, front, far beyond save distance
        }, () => PlayerUpdate.ShouldGoalkeeperDive(keeper1, BallSprite.Base, TeamData.TopBase));

        Scenario("should_dive_front_succeeds", () =>
        {
            BallSprite.YPixels = 90;
            PlayerSprite.SetYPixels(PlayerSprite.SlotGoalie1, 100); // raw=-10 -> d0w=10, front, close
            BallSprite.DeltaY = -2000;
            PlayerSprite.SetX(PlayerSprite.SlotGoalie1, 300 << 16);
            Memory.WriteWord(Memory.Addr.ballDefensiveX, 320);
            PlayerSprite.SetDeltaX(PlayerSprite.SlotGoalie1, 3000);
            WireShotChanceTable(true, entry10: 3, entry48: 0, thresh52: 0, thresh54: 0);
        }, () => PlayerUpdate.ShouldGoalkeeperDive(keeper1, BallSprite.Base, TeamData.TopBase));

        Scenario("should_dive_penalty", () =>
        {
            Rng.Reseed(3);
            Memory.WriteWord(Memory.Addr.penalty, 1);
            BallSprite.YPixels = 90;
            PlayerSprite.SetYPixels(PlayerSprite.SlotGoalie1, 100); // raw=-10 -> d0w=10, front
        }, () => PlayerUpdate.ShouldGoalkeeperDive(keeper1, BallSprite.Base, TeamData.TopBase));

        // ---- GoalkeeperJumping ----
        Scenario("jumping_near_low", () =>
        {
            PlayerSprite.SetBallDistance(PlayerSprite.SlotGoalie1, 50); // <= 128 -> near
            Memory.WriteWord(Memory.Addr.ballNotHighZ, 2); // <= 5 -> diving low
            PlayerSprite.SetX(PlayerSprite.SlotGoalie1, 300 << 16);
            PlayerSprite.SetY(PlayerSprite.SlotGoalie1, 400 << 16);
        }, () => PlayerUpdate.GoalkeeperJumping(2, 0, 2, keeper1, BallSprite.Base, TeamData.TopBase));

        Scenario("jumping_far_bottom_high", () =>
        {
            PlayerSprite.SetBallDistance(PlayerSprite.SlotGoalie2, 500); // > 128 -> far
            Memory.WriteWord(Memory.Addr.ballNotHighZ, 10); // > 5 -> diving high
            PlayerSprite.SetX(PlayerSprite.SlotGoalie2, 300 << 16);
            PlayerSprite.SetY(PlayerSprite.SlotGoalie2, 400 << 16);
        }, () => PlayerUpdate.GoalkeeperJumping(6, 0, 6, keeper2, BallSprite.Base, TeamData.BottomBase));

        Scenario("jumping_far_slower", () =>
        {
            PlayerSprite.SetBallDistance(PlayerSprite.SlotGoalie1, 500);
            Memory.WriteWord(Memory.Addr.ballNotHighZ, 2);
            PlayerSprite.SetX(PlayerSprite.SlotGoalie1, 300 << 16);
            PlayerSprite.SetY(PlayerSprite.SlotGoalie1, 400 << 16);
        }, () => PlayerUpdate.GoalkeeperJumping(6, 1, 6, keeper1, BallSprite.Base, TeamData.TopBase));

        Scenario("jumping_far_random", () =>
        {
            PlayerSprite.SetBallDistance(PlayerSprite.SlotGoalie1, 500);
            Memory.WriteWord(Memory.Addr.ballNotHighZ, 2);
            Memory.WriteWord(Memory.Addr.currentGameTick, 123);
            PlayerSprite.SetX(PlayerSprite.SlotGoalie1, 300 << 16);
            PlayerSprite.SetY(PlayerSprite.SlotGoalie1, 400 << 16);
        }, () => PlayerUpdate.GoalkeeperJumping(6, 2, 6, keeper1, BallSprite.Base, TeamData.TopBase));

        // ---- GoalkeeperDeflectedBall ----
        Scenario("deflect_default_weak", () =>
        {
            BallSprite.XPixels = 336; BallSprite.YPixels = 780;
            Memory.WriteWord(Memory.Addr.currentGameTick, 40);
        }, () => PlayerUpdate.GoalkeeperDeflectedBall(BallSprite.Base, TeamData.TopBase));

        Scenario("deflect_strong_with_table", () =>
        {
            BallSprite.XPixels = 336; BallSprite.YPixels = 780;
            Memory.WriteWord(Memory.Addr.currentGameTick, 40);
            WireShotChanceTable(true, entry10: 0, entry48: 0, thresh52: -100, thresh54: -100);
        }, () => PlayerUpdate.GoalkeeperDeflectedBall(BallSprite.Base, TeamData.TopBase));

        // ---- RunShotTripWire ----
        Scenario("trip_wire_opponent_has_ball", () =>
        {
            TeamData.SetPlayerHasBall(false, 1); // opponent (bottom, relative to top team) has ball
        }, () => PlayerUpdate.RunShotTripWire(keeper1, BallSprite.Base, TeamData.TopBase, true));

        Scenario("trip_wire_shot_at_goal_fast_ball", () =>
        {
            Memory.WriteWord(Memory.Addr.kShotAtGoalMinumumSpeed, 100);
            BallSprite.Speed = 500;
        }, () => PlayerUpdate.RunShotTripWire(keeper1, BallSprite.Base, TeamData.TopBase, true));

        Scenario("trip_wire_ball_far_c7fc01", () =>
        {
            Memory.WriteWord(Memory.Addr.kShotAtGoalMinumumSpeed, 900);
            BallSprite.Speed = 100;
            PlayerSprite.SetBallDistance(PlayerSprite.SlotGoalie1, 9000); // > 5000
        }, () => PlayerUpdate.RunShotTripWire(keeper1, BallSprite.Base, TeamData.TopBase, true));

        Scenario("trip_wire_deltaZ_eq_0x8000", () =>
        {
            Memory.WriteWord(Memory.Addr.kShotAtGoalMinumumSpeed, 900);
            BallSprite.Speed = 100;
            PlayerSprite.SetBallDistance(PlayerSprite.SlotGoalie1, 100);
            BallSprite.ZPixels = 5;
            BallSprite.DeltaZ = 0x8000;
        }, () => PlayerUpdate.RunShotTripWire(keeper1, BallSprite.Base, TeamData.TopBase, true));

        // ---- RunShotAtGoal ----
        Scenario("shot_at_goal_too_high", () =>
        {
            BallSprite.ZPixels = 20; // > 16
        }, () => PlayerUpdate.RunShotAtGoal(keeper1, BallSprite.Base, TeamData.TopBase, true));

        Scenario("shot_at_goal_forced_saved_far_distance", () =>
        {
            BallSprite.ZPixels = 5;
            PlayerSprite.SetBallDistance(PlayerSprite.SlotGoalie1, 9000); // > 128 -> forced saved path
            BallSprite.YPixels = 300;
            PlayerSprite.SetYPixels(PlayerSprite.SlotGoalie1, 400); // no dive (too far behind)
        }, () => PlayerUpdate.RunShotAtGoal(keeper1, BallSprite.Base, TeamData.TopBase, true));

        Scenario("shot_at_goal_goal_scored", () =>
        {
            BallSprite.ZPixels = 5;
            PlayerSprite.SetBallDistance(PlayerSprite.SlotGoalie1, 9000);
            Memory.WriteByte(TeamData.TopBase + TeamData.OffBallAbove17, 0);
            Memory.WriteByte(TeamData.BottomBase + TeamData.OffFirePressed, 1);
            TeamData.SetPlayerHasBall(false, 1);
            PlayerSprite.SetPlayerOrdinal(0, 1); // keeper1 ordinal 1
            WirePlayerInfo(true, 4, 4, 4, 4, 4, 4, 4, goalieSkill: 0); // weak keeper
            Memory.WriteWord(Memory.Addr.currentGameTick, 0); // d0Sample = 0 -> beats any positive threshold
            BallSprite.XPixels = 300;
            PlayerSprite.SetX(PlayerSprite.SlotGoalie1, 320 << 16);
            PlayerSprite.SetY(PlayerSprite.SlotGoalie1, 780 << 16);
        }, () => PlayerUpdate.RunShotAtGoal(keeper1, BallSprite.Base, TeamData.TopBase, true));

        Scenario("shot_at_goal_saved_no_dive", () =>
        {
            BallSprite.ZPixels = 5;
            PlayerSprite.SetBallDistance(PlayerSprite.SlotGoalie1, 9000);
            Memory.WriteByte(TeamData.TopBase + TeamData.OffBallAbove17, 0);
            Memory.WriteByte(TeamData.BottomBase + TeamData.OffFirePressed, 1);
            TeamData.SetPlayerHasBall(false, 1);
            PlayerSprite.SetPlayerOrdinal(0, 1);
            WirePlayerInfo(true, 4, 4, 4, 4, 4, 4, 4, goalieSkill: 7); // strong keeper
            Memory.WriteWord(Memory.Addr.currentGameTick, 30); // large sample, unlikely to beat threshold
            BallSprite.XPixels = 300; BallSprite.YPixels = 300;
            PlayerSprite.SetX(PlayerSprite.SlotGoalie1, 320 << 16);
            PlayerSprite.SetY(PlayerSprite.SlotGoalie1, 400 << 16); // far behind -> ShouldGoalkeeperDive vetoes
        }, () => PlayerUpdate.RunShotAtGoal(keeper1, BallSprite.Base, TeamData.TopBase, true));

        Scenario("shot_at_goal_saved_dive_committed", () =>
        {
            BallSprite.ZPixels = 5;
            PlayerSprite.SetBallDistance(PlayerSprite.SlotGoalie1, 9000);
            Memory.WriteByte(TeamData.TopBase + TeamData.OffBallAbove17, 0);
            Memory.WriteByte(TeamData.BottomBase + TeamData.OffFirePressed, 1);
            TeamData.SetPlayerHasBall(false, 1);
            PlayerSprite.SetPlayerOrdinal(0, 1);
            WirePlayerInfo(true, 4, 4, 4, 4, 4, 4, 4, goalieSkill: 7);
            Memory.WriteWord(Memory.Addr.currentGameTick, 30);
            BallSprite.XPixels = 300; BallSprite.YPixels = 405;
            PlayerSprite.SetX(PlayerSprite.SlotGoalie1, 320 << 16);
            PlayerSprite.SetY(PlayerSprite.SlotGoalie1, 400 << 16); // close -> ShouldGoalkeeperDive may commit
            BallSprite.DeltaY = -3000;
            PlayerSprite.SetDeltaX(PlayerSprite.SlotGoalie1, 3000);
            Memory.WriteWord(Memory.Addr.ballDefensiveX, 250);
            WireShotChanceTable(true, entry10: 3, entry48: 0, thresh52: 0, thresh54: 0);
        }, () => PlayerUpdate.RunShotAtGoal(keeper1, BallSprite.Base, TeamData.TopBase, true));

        Console.WriteLine($"wrote {count} PlayerUpdate scenario dumps ({kMemSize} bytes each) to {outDir}");
    }
}
