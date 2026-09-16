// Generates the step-7A differential-test golden dumps: the real local
// dependencies UpdatePlayers.cs pulls in (BallVariables.cs full file,
// TeamPort.UpdatePlayerShotChanceTable, PlayerEnergy.DrainSlot/
// DrainOnTackle/InjuryRiskDoubled, PlayerHeader.SetStaticHeaderDirection/
// SetPlayerWithNoBallDestination, and PlayerTackle.cs's
// PlayerTacklingTestFoul/PlayersTackledTheBallStrong plus their whole
// executed call chain, including Referee.ActivateReferee). Same
// full-Memory-buffer pattern as every other *Golden.cs file: for each
// named scenario, set up state after a fresh Memory.Init(true), run ONE
// call, and dump the entire 0x60000-byte buffer.
// tests/test_step7a_golden.c replays the same setup through the C port
// and byte-compares the result.
//
// MatchAudio/telemetry (Referee's Dbg* counters, PlayerTackle's
// PlayerControlled-adjacent counters -- none apply here) are deliberately
// out of scope for both sides -- see swos_player_tackle.h/swos_referee.h.
//
// RNG: PlayerTacklingTestFoul's card roll, PlayerTackled's injury roll, and
// Referee.ActivateReferee's spawn-X jitter all draw from the shared Rng
// stream. Every scenario that reaches any of them explicitly reseeds
// immediately before acting.
using OpenSwos.Sim.Port;
using OpenSwos.SwosVm;

public static class Step7AGolden
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
            File.WriteAllBytes(Path.Combine(outDir, $"s7a_{name}.bin"), dump);
            count++;
        }

        int keeper1 = PlayerSprite.Base(PlayerSprite.SlotGoalie1);
        int out1_1 = PlayerSprite.Base(1);
        int out1_2 = PlayerSprite.Base(2);
        int out2_1 = PlayerSprite.Base(12);

        // ---- BallVariables.UpdateBallVariables ----
        Scenario("ball_vars_going_up_top_team", () =>
        {
            BallSprite.Y = 400 << 16;
            BallSprite.DeltaY = -80000; // < -65536 -> going up
            BallSprite.DeltaZ = 2000;
            PlayerSprite.SetY(1, 100 << 16);
        }, () => BallVariables.UpdateBallVariables(out1_1, BallSprite.Base, TeamData.TopBase));

        Scenario("ball_vars_going_down_bottom_team", () =>
        {
            BallSprite.Y = 400 << 16;
            BallSprite.DeltaY = 80000; // > 65536 -> going down
            BallSprite.DeltaZ = 2000;
            PlayerSprite.SetY(12, 700 << 16);
        }, () => BallVariables.UpdateBallVariables(out2_1, BallSprite.Base, TeamData.BottomBase));

        Scenario("ball_vars_going_left", () =>
        {
            BallSprite.X = 400 << 16;
            BallSprite.DeltaX = -80000;
            PlayerSprite.SetX(1, 300 << 16);
        }, () => BallVariables.UpdateBallVariables(out1_1, BallSprite.Base, TeamData.TopBase));

        Scenario("ball_vars_going_right", () =>
        {
            BallSprite.X = 300 << 16;
            BallSprite.DeltaX = 80000;
            PlayerSprite.SetX(1, 400 << 16);
        }, () => BallVariables.UpdateBallVariables(out1_1, BallSprite.Base, TeamData.TopBase));

        Scenario("ball_vars_not_moving", () =>
        {
            BallSprite.XPixels = 300; BallSprite.YPixels = 400; BallSprite.ZPixels = 5;
            BallSprite.DeltaX = 0; BallSprite.DeltaY = 0;
        }, () => BallVariables.UpdateBallVariables(out1_1, BallSprite.Base, TeamData.TopBase));

        // ---- BallVariables.CalculateBallNextGroundXYPositions ----
        Scenario("ball_next_ground_moving", () =>
        {
            BallSprite.X = 300 << 16; BallSprite.Y = 400 << 16; BallSprite.Z = 40 << 16;
            BallSprite.DeltaX = 30000; BallSprite.DeltaY = -20000; BallSprite.DeltaZ = -1000;
        }, () => BallVariables.CalculateBallNextGroundXYPositions(BallSprite.Base));

        Scenario("ball_next_ground_standing", () =>
        {
            BallSprite.DeltaX = 0; BallSprite.DeltaY = 0;
        }, () => BallVariables.CalculateBallNextGroundXYPositions(BallSprite.Base));

        // ---- TeamPort.UpdatePlayerShotChanceTable ----
        Scenario("shot_chance_goalkeeper", () =>
        {
            Memory.WriteDword(TeamData.TopBase + TeamData.OffShotChanceTable, 0x4FEE0);
            int piBase = 0x4FE60;
            Memory.WriteByte(piBase + 4, 0);  // position = goalkeeper
            Memory.WriteByte(piBase + 34, 3); // goalieSkill
            TeamPort.UpdatePlayerShotChanceTable(true, piBase);
        }, () => { });

        Scenario("shot_chance_outfielder", () =>
        {
            Memory.WriteDword(TeamData.TopBase + TeamData.OffShotChanceTable, 0x4FEE0);
            int piBase = 0x4FE60;
            Memory.WriteByte(piBase + 4, 4); // position = outfielder
            TeamPort.UpdatePlayerShotChanceTable(true, piBase);
        }, () => { });

        Scenario("shot_chance_no_buffer", () =>
        {
            // OffShotChanceTable left at 0 -- fallback path.
            int piBase = 0x4FE60;
            Memory.WriteByte(piBase + 4, 0);
            Memory.WriteByte(piBase + 34, 5);
            TeamPort.UpdatePlayerShotChanceTable(true, piBase);
        }, () => { });

        // ---- PlayerEnergy.DrainSlot ----
        Scenario("energy_drain_slot_moving", () =>
        {
            PlayerEnergy.EffectEnabled = true; // DrainSlot itself isn't gated, but exercise alongside the flag.
            Memory.WriteByte(out1_1 + PlayerSprite.OffIsMoving, 0xFF);
            Memory.WriteByte(out1_1 + PlayerSprite.OffStamina, 4);
            Memory.WriteWord(out1_1 + PlayerSprite.OffEnergy, 2000);
            Memory.WriteWord(out1_1 + PlayerSprite.OffEnergyAcc, 90);
            PlayerEnergy.DrainSlot(out1_1);
            PlayerEnergy.EffectEnabled = false;
        }, () => { });

        Scenario("energy_drain_slot_not_moving", () =>
        {
            Memory.WriteByte(out1_1 + PlayerSprite.OffIsMoving, 0);
            Memory.WriteWord(out1_1 + PlayerSprite.OffEnergy, 2000);
            PlayerEnergy.DrainSlot(out1_1);
        }, () => { });

        // ---- PlayerHeader.SetStaticHeaderDirection ----
        Scenario("static_header_direction_turn", () =>
        {
            TeamData.SetCurrentAllowedDirection(true, 3);
            PlayerSprite.SetDirection(1, 5);
            Memory.WriteByte(out1_1 + PlayerSprite.OffPlayerDownTimer, 5);
            Memory.WriteDword(out1_1 + PlayerSprite.OffAnimTablePtr, Memory.Addr.kStaticHeaderAttemptAnimTableAddr);
        }, () => PlayerHeader.SetStaticHeaderDirection(out1_1, TeamData.TopBase));

        // ---- PlayerHeader.SetPlayerWithNoBallDestination ----
        Scenario("no_ball_dest_outfielder_top", () =>
        {
            PlayerSprite.SetPlayerOrdinal(1, 4); // outfielder
            Memory.WriteWord(TeamData.TopBase + TeamData.OffTactics, 0);
            Memory.WriteWord(Memory.Addr.gameState, 100);
            Memory.WriteWord(Memory.Addr.ballQuadrantIndex, 2);
        }, () => PlayerHeader.SetPlayerWithNoBallDestination(out1_1, TeamData.TopBase, 300, 400));

        Scenario("no_ball_dest_outfielder_bottom", () =>
        {
            PlayerSprite.SetPlayerOrdinal(12, 4);
            Memory.WriteWord(TeamData.BottomBase + TeamData.OffTactics, 0);
            Memory.WriteWord(Memory.Addr.gameState, 100);
            Memory.WriteWord(Memory.Addr.ballQuadrantIndex, 5);
        }, () => PlayerHeader.SetPlayerWithNoBallDestination(out2_1, TeamData.BottomBase, 300, 400));

        Scenario("no_ball_dest_goalkeeper_top", () =>
        {
            Memory.WriteWord(TeamData.TopBase + TeamData.OffTactics, 0);
            Memory.WriteWord(Memory.Addr.gameState, 100);
        }, () => PlayerHeader.SetPlayerWithNoBallDestination(keeper1, TeamData.TopBase, 300, 400));

        // ---- Referee.ActivateReferee (via PlayerTacklingTestFoul's card path) ----
        Scenario("referee_activate_direct", () =>
        {
            Rng.Reseed(11);
            Memory.WriteWord(Memory.Addr.foulXCoordinate, 300);
            Memory.WriteWord(Memory.Addr.foulYCoordinate, 400);
        }, () => Referee.ActivateReferee());

        // ---- PlayerTackle.PlayerTacklingTestFoul ----
        Scenario("test_foul_too_far", () =>
        {
            TeamData.SetControlledPlayer(false, out2_1); // opponent (bottom) has a controlled player
            PlayerSprite.SetX(1, 100 << 16); PlayerSprite.SetY(1, 100 << 16);
            PlayerSprite.SetX(12, 500 << 16); PlayerSprite.SetY(12, 700 << 16); // far apart
        }, () => PlayerTackle.PlayerTacklingTestFoul(out1_1, TeamData.TopBase));

        Scenario("test_foul_close_goalkeeper", () =>
        {
            TeamData.SetControlledPlayer(false, PlayerSprite.Base(PlayerSprite.SlotGoalie2));
            PlayerSprite.SetX(1, 300 << 16); PlayerSprite.SetY(1, 400 << 16);
            PlayerSprite.SetX(PlayerSprite.SlotGoalie2, 300 << 16);
            PlayerSprite.SetY(PlayerSprite.SlotGoalie2, 400 << 16); // same spot -> very close
        }, () => PlayerTackle.PlayerTacklingTestFoul(out1_1, TeamData.TopBase));

        Scenario("test_foul_close_yellow_card", () =>
        {
            Rng.Reseed(5); // small roll -> direct-red gate misses -> yellow path
            TeamData.SetControlledPlayer(false, out2_1);
            PlayerSprite.SetPlayerOrdinal(12, 3); // not goalkeeper
            PlayerSprite.SetX(1, 336 << 16); PlayerSprite.SetY(1, 200 << 16); // inside penalty band
            PlayerSprite.SetX(12, 336 << 16); PlayerSprite.SetY(12, 200 << 16);
            PlayerSprite.SetTackleState(1, 0);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffPlayerNumber, 1);
            Memory.WriteDword(Memory.Addr.dseg_17E3EE, 0x4FEE0);
            Memory.WriteDword(Memory.Addr.dseg_17E3F3, 0x4FEE0);
        }, () => PlayerTackle.PlayerTacklingTestFoul(out1_1, TeamData.TopBase));

        Scenario("test_foul_close_no_cards", () =>
        {
            Rng.Reseed(1);
            Memory.WriteWord(Memory.Addr.cardsDisallowed, 1); // no cards at all
            TeamData.SetControlledPlayer(false, out2_1);
            PlayerSprite.SetPlayerOrdinal(12, 3);
            PlayerSprite.SetX(1, 336 << 16); PlayerSprite.SetY(1, 200 << 16);
            PlayerSprite.SetX(12, 336 << 16); PlayerSprite.SetY(12, 200 << 16);
            PlayerSprite.SetTackleState(1, 0);
        }, () => PlayerTackle.PlayerTacklingTestFoul(out1_1, TeamData.TopBase));

        // ---- PlayerTackle.PlayersTackledTheBallStrong ----
        Scenario("tackled_ball_strong_cpu", () =>
        {
            Memory.WriteWord(TeamData.TopBase + TeamData.OffPlayerNumber, 0); // CPU
            TeamData.SetCurrentAllowedDirection(true, 3);
            PlayerSprite.SetDirection(1, 3);
            PlayerSprite.SetSpeed(1, 500);
            BallSprite.XPixels = 300; BallSprite.YPixels = 400;
        }, () => PlayerTackle.PlayersTackledTheBallStrong(out1_1, TeamData.TopBase));

        Scenario("tackled_ball_strong_human_good_tackle", () =>
        {
            Memory.WriteWord(TeamData.TopBase + TeamData.OffPlayerNumber, 1); // human
            TeamData.SetCurrentAllowedDirection(true, 3);
            PlayerSprite.SetDirection(1, 3);
            PlayerSprite.SetSpeed(1, 500);
            BallSprite.XPixels = 300; BallSprite.YPixels = 400;
            TeamData.SetControlledPlayer(false, out2_1);
            PlayerSprite.SetBallDistance(12, 50); // >= 9
            PlayerSprite.SetX(1, 100 << 16); PlayerSprite.SetY(1, 100 << 16);
            PlayerSprite.SetX(12, 300 << 16); PlayerSprite.SetY(12, 300 << 16); // far -> good tackle
        }, () => PlayerTackle.PlayersTackledTheBallStrong(out1_1, TeamData.TopBase));

        // ---- PlayerTackle.PlayerTackled (indirectly, via a close foul that
        // triggers the "players very close" -> PlayerTackled branch) ----
        Scenario("test_foul_triggers_player_tackled_injury", () =>
        {
            Rng.Reseed(2); // small draw -> likely under injury threshold
            TeamData.SetControlledPlayer(false, out2_1);
            PlayerSprite.SetPlayerOrdinal(1, 2);
            PlayerSprite.SetPlayerOrdinal(12, 3);
            PlayerSprite.SetX(1, 300 << 16); PlayerSprite.SetY(1, 400 << 16);
            PlayerSprite.SetX(12, 300 << 16); PlayerSprite.SetY(12, 400 << 16); // same spot -> very close
            PlayerSprite.SetTackleState(1, 1); // not 0 and not TS_GOOD_TACKLE -> direction gate
            PlayerSprite.SetDirection(1, 2);
            PlayerSprite.SetDirection(12, 2); // same direction -> diff 0 -> foul_conceded
            Memory.WriteWord(Memory.Addr.g_trainingGame, 0);
            Memory.WriteWord(Memory.Addr.team2NumAllowedInjuries, 3);
            Memory.WriteWord(Memory.Addr.gameLengthInGame, 0);
        }, () => PlayerTackle.PlayerTacklingTestFoul(out1_1, TeamData.TopBase));

        Console.WriteLine($"wrote {count} Step7A scenario dumps ({kMemSize} bytes each) to {outDir}");
    }
}
