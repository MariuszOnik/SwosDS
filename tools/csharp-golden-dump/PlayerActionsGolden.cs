// Generates the step-5 differential-test golden dumps for the rest of
// PlayerActions.cs (SetPlayerAnimationTable was already covered in step 3's
// SpriteUpdateGolden.cs). Same full-Memory-buffer pattern as
// BallUpdateGolden.cs/SpriteUpdateGolden.cs: for each named scenario, set up
// state after a fresh Memory.Init(true), run ONE call into PlayerActions,
// and dump the entire 0x60000-byte buffer. tests/test_player_actions_golden.c
// replays the same setup through the C port and byte-compares the result.
//
// MatchAudio/telemetry/PlayerControlled counters are deliberately out of
// scope for both sides of this comparison -- see
// swos-vm-c/include/swos_player_actions.h and MatchAudioStub.cs/
// PlayerControlledStub.cs.
//
// RNG: CalculateIfPlayerWinsBall is the only function here that draws from
// the shared Rng stream (Rng.NextByte(), for the 50/50 duel roll). Each
// scenario that reaches it explicitly reseeds immediately before acting, so
// results don't depend on how many Rng draws happened in earlier scenarios
// (in this file or SpriteUpdateGolden/BallUpdateGolden, which run first in
// Program.cs).
//
// PlayerInfo (skill bytes): most scenarios leave TeamData.OffInGameTeamPtr
// unwired (0), which is the honest current state of this port (TeamDataLoader.
// WritePlayerInfos is deliberately not ported -- see swos_team_data_loader.h)
// -- GetPlayerInfoForSprite returns 0 and every skill lookup falls back to
// the documented default skill=4. One scenario per skill-consuming function
// family additionally wires a synthetic single-record PlayerInfo block into
// the small unused gap between the per-team sprite pointer tables (ending
// 0x4FE58) and the sprite pool (starting 0x50000), to exercise the real
// skill-byte-read branch too.
using OpenSwos.Sim.Port;
using OpenSwos.SwosVm;

public static class PlayerActionsGolden
{
    private const int kMemSize = 0x60000;

    // Scratch PlayerInfo blocks -- unused gap, see file header.
    private const int kTopPlayerInfo = 0x4FE60;
    private const int kBottomPlayerInfo = 0x4FEA0;

    private static void WirePlayerInfo(bool top, int passing, int shooting, int heading,
        int tackling, int ballControl, int speed, int finishing)
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
            File.WriteAllBytes(Path.Combine(outDir, $"pa_{name}.bin"), dump);
            count++;
        }

        int keeper1 = PlayerSprite.Base(PlayerSprite.SlotGoalie1);
        int out1_1 = PlayerSprite.Base(1);   // team1 outfielder ordinal 2
        int keeper2 = PlayerSprite.Base(PlayerSprite.SlotGoalie2);
        int out2_1 = PlayerSprite.Base(12);  // team2 outfielder ordinal 2

        // ---- UpdatePlayerSpeedAndFrameDelay / RecomputeSpriteDeltas ----
        Scenario("speed_normal_outfielder", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            PlayerSprite.SetPlayerState(1, 0);
            PlayerSprite.SetX(1, 300 << 16);
            PlayerSprite.SetY(1, 400 << 16);
            PlayerSprite.SetDestX(1, 320);
            PlayerSprite.SetDestY(1, 420);
            PlayerSprite.SetDirection(1, 3);
        }, () => PlayerActions.UpdatePlayerSpeedAndFrameDelay(TeamData.TopBase, out1_1));

        Scenario("speed_with_wired_playerinfo", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            PlayerSprite.SetPlayerOrdinal(1, 1); // matches kTopPlayerInfo record at ordinal 1
            PlayerSprite.SetPlayerState(1, 0);
            PlayerSprite.SetX(1, 300 << 16);
            PlayerSprite.SetY(1, 400 << 16);
            PlayerSprite.SetDestX(1, 320);
            PlayerSprite.SetDestY(1, 420);
            WirePlayerInfo(true, passing: 5, shooting: 6, heading: 3, tackling: 2, ballControl: 7, speed: 6, finishing: 4);
        }, () => PlayerActions.UpdatePlayerSpeedAndFrameDelay(TeamData.TopBase, out1_1));

        Scenario("speed_keeper_early_out", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            PlayerSprite.SetPlayerState(PlayerSprite.SlotGoalie1, 0);
            TeamData.SetControlledPlayer(true, out1_1); // keeper is NOT the controlled player
            PlayerSprite.SetX(PlayerSprite.SlotGoalie1, 300 << 16);
            PlayerSprite.SetY(PlayerSprite.SlotGoalie1, 100 << 16);
            PlayerSprite.SetDestX(PlayerSprite.SlotGoalie1, 300);
            PlayerSprite.SetDestY(PlayerSprite.SlotGoalie1, 110);
        }, () => PlayerActions.UpdatePlayerSpeedAndFrameDelay(TeamData.TopBase, keeper1));

        Scenario("speed_injured_human_team", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffPlayerNumber, 1); // human-controlled
            PlayerSprite.SetPlayerState(1, 0);
            Memory.WriteWord(out1_1 + PlayerSprite.OffInjuryLevel, 96); // bucket 3
            PlayerSprite.SetX(1, 300 << 16);
            PlayerSprite.SetY(1, 400 << 16);
            PlayerSprite.SetDestX(1, 300);
            PlayerSprite.SetDestY(1, 400);
        }, () => PlayerActions.UpdatePlayerSpeedAndFrameDelay(TeamData.TopBase, out1_1));

        Scenario("speed_ball_carrier_slowdown", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            TeamData.SetControlledPlayer(true, out1_1);
            TeamData.SetPlayerHasBall(true, 1);
            PlayerSprite.SetPlayerState(1, 0);
            PlayerSprite.SetX(1, 300 << 16);
            PlayerSprite.SetY(1, 400 << 16);
            PlayerSprite.SetDestX(1, 300);
            PlayerSprite.SetDestY(1, 400);
        }, () => PlayerActions.UpdatePlayerSpeedAndFrameDelay(TeamData.TopBase, out1_1));

        Scenario("speed_pass_overlap_boost", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffPlayerNumber, 1);
            Memory.WriteDword(TeamData.TopBase + TeamData.OffPassToPlayerPtr, out1_1);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffPassingToPlayer, 1);
            TeamData.SetLongPass(true, 1);
            BallSprite.Speed = 500;
            BallSprite.FullDirection = 10;
            PlayerSprite.SetFullDirection(1, 8);
            PlayerSprite.SetPlayerState(1, 0);
            PlayerSprite.SetX(1, 300 << 16);
            PlayerSprite.SetY(1, 400 << 16);
            PlayerSprite.SetDestX(1, 300);
            PlayerSprite.SetDestY(1, 400);
        }, () => PlayerActions.UpdatePlayerSpeedAndFrameDelay(TeamData.TopBase, out1_1));

        Scenario("speed_stoppage_slowdown", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 0); // not in progress
            Memory.WriteWord(Memory.Addr.gameState, 29); // kFirstHalfEnded
            Memory.WriteWord(Memory.Addr.stoppageTimerTotal, 3);
            PlayerSprite.SetPlayerState(1, 0);
            PlayerSprite.SetX(1, 300 << 16);
            PlayerSprite.SetY(1, 400 << 16);
            PlayerSprite.SetDestX(1, 300);
            PlayerSprite.SetDestY(1, 400);
        }, () => PlayerActions.UpdatePlayerSpeedAndFrameDelay(TeamData.TopBase, out1_1));

        // ---- UpdatePlayerWithBall / UpdateControllingPlayer ----
        Scenario("update_player_with_ball", () =>
        {
            BallSprite.XPixels = 300; BallSprite.YPixels = 400;
            PlayerSprite.SetDirection(1, 2);
        }, () => PlayerActions.UpdatePlayerWithBall(out1_1));

        Scenario("update_controlling_player", () =>
        {
            PlayerSprite.SetX(1, 300 << 16); PlayerSprite.SetY(1, 400 << 16);
            PlayerSprite.SetDirection(1, 5);
            BallSprite.DeltaZ = -400;
        }, () => PlayerActions.UpdateControllingPlayer(out1_1));

        // ---- CalculateIfPlayerWinsBall ----
        Scenario("wins_ball_duel_no_opponent_controlled", () =>
        {
            Rng.Reseed(1);
            // opp.controlledPlayer stays 0 -> l_no_opponent_controlled_player -> l_set_team_direction.
            BallSprite.XPixels = 300; BallSprite.YPixels = 400;
            PlayerSprite.SetX(1, 300 << 16); PlayerSprite.SetY(1, 400 << 16);
        }, () => PlayerActions.CalculateIfPlayerWinsBall(2, TeamData.TopBase, out1_1));

        Scenario("wins_ball_duel_resolved", () =>
        {
            Rng.Reseed(7);
            TeamData.SetPlayerHasBall(false, 1); // opponent has the ball
            TeamData.SetCurrentAllowedDirection(false, 3);
            TeamData.SetControlledPlayer(false, out2_1);
        }, () => PlayerActions.CalculateIfPlayerWinsBall(2, TeamData.TopBase, out1_1));

        Scenario("wins_ball_duel_with_skills", () =>
        {
            Rng.Reseed(42);
            TeamData.SetPlayerHasBall(false, 1);
            TeamData.SetCurrentAllowedDirection(false, 3);
            TeamData.SetControlledPlayer(false, out2_1);
            PlayerSprite.SetPlayerOrdinal(1, 1);
            PlayerSprite.SetPlayerOrdinal(12, 1);
            WirePlayerInfo(true, passing: 4, shooting: 4, heading: 4, tackling: 6, ballControl: 6, speed: 4, finishing: 4);
            WirePlayerInfo(false, passing: 4, shooting: 4, heading: 4, tackling: 2, ballControl: 2, speed: 4, finishing: 4);
        }, () => PlayerActions.CalculateIfPlayerWinsBall(2, TeamData.TopBase, out1_1));

        // ---- PlayerKickingBall ----
        Scenario("kick_no_shot", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            PlayerSprite.SetDirection(1, 3);
            BallSprite.XPixels = 300; BallSprite.YPixels = 400;
        }, () => PlayerActions.PlayerKickingBall(TeamData.TopBase, out1_1));

        Scenario("kick_finishing_shot_bottom_team", () =>
        {
            // BOTTOM team: only a possible shot when ball.y <= 342
            // (player.cpp:858-870), then ctrlDir in {0,1,7}, then y<204 -> finishing.
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            TeamData.SetControlledPlDirection(false, 0);
            PlayerSprite.SetDirection(12, 4);
            BallSprite.XPixels = 300; BallSprite.YPixels = 150;
        }, () => PlayerActions.PlayerKickingBall(TeamData.BottomBase, out2_1));

        Scenario("kick_long_shot_top_team", () =>
        {
            // TOP team: only a possible shot when ball.y >= 556
            // (player.cpp:910-922), then ctrlDir in {3,4,5}; x<241 -> long
            // regardless of y (player.cpp:964-977).
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            TeamData.SetControlledPlDirection(true, 4);
            PlayerSprite.SetDirection(1, 1);
            BallSprite.XPixels = 200; BallSprite.YPixels = 600;
        }, () => PlayerActions.PlayerKickingBall(TeamData.TopBase, out1_1));

        Scenario("kick_finishing_with_wired_skill_and_fatigue", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            TeamData.SetControlledPlDirection(false, 0);
            PlayerSprite.SetDirection(12, 4);
            PlayerSprite.SetPlayerOrdinal(12, 1);
            WirePlayerInfo(false, passing: 4, shooting: 4, heading: 4, tackling: 4, ballControl: 4, speed: 4, finishing: 6);
            PlayerEnergy.EffectEnabled = true;
            Memory.WriteWord(out2_1 + PlayerSprite.OffEnergy, PlayerEnergy.Max / 20); // <10% -> ShotPenalty=1
            BallSprite.XPixels = 300; BallSprite.YPixels = 150;
        }, () => PlayerActions.PlayerKickingBall(TeamData.BottomBase, out2_1));

        // ---- PlayerHittingStaticHeader ----
        Scenario("static_header", () =>
        {
            TeamData.SetCurrentAllowedDirection(true, 5);
            PlayerSprite.SetDirection(1, 2);
            BallSprite.XPixels = 300; BallSprite.YPixels = 400; BallSprite.DeltaZ = -500;
        }, () => PlayerActions.PlayerHittingStaticHeader(TeamData.TopBase, out1_1));

        // ---- PlayerHittingJumpHeader (flying / lob / static branches) ----
        Scenario("jump_header_flying", () =>
        {
            TeamData.SetCurrentAllowedDirection(true, 2);
            PlayerSprite.SetDirection(1, 4); // diff&7 == 2 -> l_left_held -> DoFlyingHeader
            PlayerSprite.SetSpeed(1, 400);
            BallSprite.XPixels = 300; BallSprite.YPixels = 400;
        }, () => PlayerActions.PlayerHittingJumpHeader(TeamData.TopBase, out1_1));

        Scenario("jump_header_lob", () =>
        {
            TeamData.SetCurrentAllowedDirection(true, 0);
            PlayerSprite.SetDirection(1, 4); // diff&7 == 4 -> l_lob_header -> DoLobHeader
            PlayerSprite.SetSpeed(1, 400);
            BallSprite.XPixels = 300; BallSprite.YPixels = 400;
        }, () => PlayerActions.PlayerHittingJumpHeader(TeamData.TopBase, out1_1));

        Scenario("jump_header_static_path", () =>
        {
            TeamData.SetCurrentAllowedDirection(true, -1); // < 0 -> l_do_static_header
            PlayerSprite.SetDirection(1, 3);
            PlayerSprite.SetSpeed(1, 400);
            BallSprite.XPixels = 300; BallSprite.YPixels = 400;
        }, () => PlayerActions.PlayerHittingJumpHeader(TeamData.TopBase, out1_1));

        // ---- PlayerTackledTheBallStrong / Weak ----
        Scenario("tackle_strong_cpu_good_tackle", () =>
        {
            Memory.WriteWord(TeamData.TopBase + TeamData.OffPlayerNumber, 0); // CPU
            TeamData.SetCurrentAllowedDirection(true, 6);
            PlayerSprite.SetDirection(1, 2);
            PlayerSprite.SetSpeed(1, 600);
            TeamData.SetControlledPlayer(false, out2_1);
            PlayerSprite.SetBallDistance(12, 20); // >= 9
            PlayerSprite.SetX(1, 100 << 16); PlayerSprite.SetY(1, 100 << 16);
            PlayerSprite.SetX(12, 200 << 16); PlayerSprite.SetY(12, 200 << 16); // far apart -> distSq > 32
        }, () => PlayerActions.PlayerTackledTheBallStrong(TeamData.TopBase, out1_1));

        Scenario("tackle_strong_human", () =>
        {
            Memory.WriteWord(TeamData.TopBase + TeamData.OffPlayerNumber, 1); // human
            TeamData.SetCurrentAllowedDirection(true, 6);
            PlayerSprite.SetDirection(1, 2);
            PlayerSprite.SetSpeed(1, 600);
        }, () => PlayerActions.PlayerTackledTheBallStrong(TeamData.TopBase, out1_1));

        Scenario("tackle_weak_good_tackle", () =>
        {
            TeamData.SetCurrentAllowedDirection(true, 6);
            PlayerSprite.SetDirection(1, 2);
            PlayerSprite.SetSpeed(1, 600);
            TeamData.SetControlledPlayer(false, out2_1);
            PlayerSprite.SetBallDistance(12, 20);
            PlayerSprite.SetX(1, 100 << 16); PlayerSprite.SetY(1, 100 << 16);
            PlayerSprite.SetX(12, 200 << 16); PlayerSprite.SetY(12, 200 << 16);
        }, () => PlayerActions.PlayerTackledTheBallWeak(TeamData.TopBase, out1_1));

        // ---- DoPass ----
        Scenario("pass_found_closest_ai", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffPlayerNumber, 0); // AI
            PlayerSprite.SetDirection(1, 3);
            BallSprite.XPixels = 300; BallSprite.YPixels = 400;
            // Team-mate at slot 2 (outfielder ordinal 3), close to the pass direction.
            PlayerSprite.SetPlayerState(2, 0);
            PlayerSprite.SetFullDirection(2, 3 * 32);
            PlayerSprite.SetBallDistance(2, 1000);
            Memory.WriteWord(Memory.Addr.currentGameTick, 0);
        }, () => PlayerActions.DoPass(TeamData.TopBase, out1_1));

        Scenario("pass_found_closest_human_far", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffPlayerNumber, 1); // human -> skip AI failed-pass check
            PlayerSprite.SetDirection(1, 3);
            BallSprite.XPixels = 300; BallSprite.YPixels = 400;
            PlayerSprite.SetPlayerState(2, 0);
            PlayerSprite.SetFullDirection(2, 3 * 32);
            PlayerSprite.SetBallDistance(2, 90000); // far -> higher passing-speed bracket
            PlayerSprite.SetX(2, 500 << 16); PlayerSprite.SetY(2, 700 << 16);
        }, () => PlayerActions.DoPass(TeamData.TopBase, out1_1));

        Scenario("pass_no_closest_player", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 0); // not in progress -> l_play_kick_and_pass_samples path via <15/<=20 checks
            Memory.WriteWord(Memory.Addr.gameState, 10);
            PlayerSprite.SetDirection(1, 3);
            BallSprite.XPixels = 300; BallSprite.YPixels = 400;
            // No other team-mate is PL_NORMAL with a matching direction -> a0Closest == -1.
            for (int slot = 2; slot <= 10; slot++) PlayerSprite.SetPlayerState(slot, 4);
        }, () => PlayerActions.DoPass(TeamData.TopBase, out1_1));

        // ---- SetPlayerDowntimeAfterTackle / SetJumpHeaderHitAnimTable ----
        Scenario("downtime_after_tackle_cpu", () =>
        {
            PlayerSprite.SetTacklingTimer(1, -1); // CPU table
        }, () => PlayerActions.SetPlayerDowntimeAfterTackle(TeamData.TopBase, out1_1));

        Scenario("downtime_after_tackle_human", () =>
        {
            PlayerSprite.SetTacklingTimer(1, 5); // human table
        }, () => PlayerActions.SetPlayerDowntimeAfterTackle(TeamData.TopBase, out1_1));

        Scenario("set_jump_header_hit_anim_table_all_gates_pass", () =>
        {
            PlayerSprite.SetPlayerDownTimer(1, 40);
            Memory.WriteWord(out1_1 + 98 /* heading */, 0);
            Memory.WriteWord(out1_1 + PlayerSprite.OffFrameSwitchCounter, 1);
            Memory.WriteWord(Memory.Addr.currentGameTick, 2); // (tick+1)=3, bit1 set
            PlayerSprite.SetSpeed(1, 600);
        }, () => PlayerActions.SetJumpHeaderHitAnimTable(out1_1));

        // ---- PlayStopGoodPassSampleIfNeeded / StopGoodPassSample / EnqueuePlayingGoodPassSample ----
        Scenario("good_pass_sample_enqueue_on_5th", () =>
        {
            Memory.WriteWord(Memory.Addr.goodPassSampleCommand, -1);
            Memory.WriteWord(Memory.Addr.goodPassTimer, 4);
        }, () => PlayerActions.PlayStopGoodPassSampleIfNeeded());

        Scenario("good_pass_sample_stop", () =>
        {
            Memory.WriteWord(Memory.Addr.goodPassSampleCommand, -2);
        }, () => PlayerActions.PlayStopGoodPassSampleIfNeeded());

        Console.WriteLine($"wrote {count} PlayerActions scenario dumps ({kMemSize} bytes each) to {outDir}");
    }
}
