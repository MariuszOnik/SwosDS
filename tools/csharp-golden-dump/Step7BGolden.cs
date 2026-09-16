// Generates the step-7B differential-test golden dumps: UpdatePlayers.cs
// itself, exercised through its ONE public entry point, Update(teamIndex)
// (every other member is `private static`, so per-function golden dumps
// like Step7AGolden.cs's are not possible here -- each scenario instead
// seeds full match state so the 11-player loop routes ONE targeted sprite
// through the specific state handler being tested, while the other 10
// sprites take the safe default (PL_NORMAL, not controlled, not pass-
// target, human team) so they don't introduce confounding hook calls).
// Same full-Memory-buffer pattern as every other *Golden.cs file.
// tests/test_step7b_golden.c replays the same setup through the C port
// and byte-compares the result.
//
// Every scenario sets BOTH teams' playerNumber to a human value (1/2).
// This is deliberate, not a coverage gap: it is what keeps every scenario
// clear of the two deferred hook boundaries this step establishes --
// AiBrain.SetControlsDirection/AiHelpers.AI_Kick (step 9, gated on
// playerNumber == 0 at every call site -- see UpdatePlayers.cs's own CPU-
// TEAM GATE comment) and SetPieces (step 10, gated on gameState/playerState
// reaching a throw-in, which none of these scenarios enter). Both hooks are
// real, executed, and left as loud assert-backed stubs -- see
// swos_player_controlled.h / swos_set_pieces.h -- exercising the CPU-AI
// branch itself is out of scope until steps 9/10 land, exactly like the
// 6A/6B boundary before it.
using OpenSwos.Sim.Port;
using OpenSwos.SwosVm;

public static class Step7BGolden
{
    private const int kMemSize = 0x60000;

    public static void Run(string outDir)
    {
        Directory.CreateDirectory(outDir);
        int count = 0;

        void Scenario(string name, Action setup, Action act)
        {
            Memory.Init(pcMode: true);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffPlayerNumber, 1);
            Memory.WriteWord(TeamData.BottomBase + TeamData.OffPlayerNumber, 2);
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            Memory.WriteWord(Memory.Addr.gameState, 100);
            setup();
            act();
            byte[] dump = Memory.View(0, kMemSize).ToArray();
            File.WriteAllBytes(Path.Combine(outDir, $"s7b_{name}.bin"), dump);
            count++;
        }

        // ---- Full orchestration: normal in-progress tick, both teams ----
        Scenario("update_normal_in_progress_top", () =>
        {
            TeamData.SetControlledPlayer(true, PlayerSprite.Base(1));
        }, () => UpdatePlayers.Update(0));

        Scenario("update_normal_in_progress_bottom", () =>
        {
            TeamData.SetControlledPlayer(false, PlayerSprite.Base(12));
        }, () => UpdatePlayers.Update(1));

        // ---- Stoppage tick: SetPlayerPositionsForGameBreak, both tables ----
        Scenario("update_stoppage_kickoff_top", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 0);
            Memory.WriteWord(Memory.Addr.gameState, 0);
            Memory.WriteDword(Memory.Addr.lastTeamPlayedBeforeBreak, TeamData.TopBase);
        }, () => UpdatePlayers.Update(0));

        Scenario("update_stoppage_kickoff_bottom", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 0);
            Memory.WriteWord(Memory.Addr.gameState, 0);
            Memory.WriteDword(Memory.Addr.lastTeamPlayedBeforeBreak, TeamData.TopBase);
        }, () => UpdatePlayers.Update(1));

        // ---- TickTackledPlayer ----
        Scenario("update_tackled_player", () =>
        {
            PlayerSprite.SetPlayerState(2, (byte)PortPlayerState.kTackled);
            Memory.WriteByte(PlayerSprite.Base(2) + PlayerSprite.OffPlayerDownTimer, 10);
            Memory.WriteWord(PlayerSprite.Base(2) + PlayerSprite.OffSpeed, 200);
            PlayerSprite.SetX(2, 336 << 16); PlayerSprite.SetY(2, 400 << 16);
        }, () => UpdatePlayers.Update(0));

        // ---- TickTacklingPlayer ----
        Scenario("update_tackling_player", () =>
        {
            PlayerSprite.SetPlayerState(2, (byte)PortPlayerState.kTackling);
            Memory.WriteByte(PlayerSprite.Base(2) + PlayerSprite.OffPlayerDownTimer, 10);
            Memory.WriteWord(PlayerSprite.Base(2) + PlayerSprite.OffTacklingTimer, 5);
            Memory.WriteWord(PlayerSprite.Base(2) + PlayerSprite.OffSpeed, 300);
            PlayerSprite.SetX(2, 336 << 16); PlayerSprite.SetY(2, 400 << 16);
        }, () => UpdatePlayers.Update(0));

        // ---- TickInjuredRollingPlayer ----
        Scenario("update_injured_rolling_player", () =>
        {
            PlayerSprite.SetPlayerState(2, (byte)PortPlayerState.kInjured);
            Memory.WriteByte(PlayerSprite.Base(2) + PlayerSprite.OffPlayerDownTimer, 10);
        }, () => UpdatePlayers.Update(0));

        // ---- TickJumpHeader ----
        Scenario("update_jump_header_player", () =>
        {
            PlayerSprite.SetPlayerState(2, (byte)PortPlayerState.kJumpHeader);
            Memory.WriteByte(PlayerSprite.Base(2) + PlayerSprite.OffPlayerDownTimer, 50);
            Memory.WriteWord(PlayerSprite.Base(2) + PlayerSprite.OffSpeed, 0);
            PlayerSprite.SetX(2, 336 << 16); PlayerSprite.SetY(2, 400 << 16);
        }, () => UpdatePlayers.Update(0));

        // ---- TickStaticHeader ----
        Scenario("update_static_header_player", () =>
        {
            PlayerSprite.SetPlayerState(2, (byte)PortPlayerState.kStaticHeader);
            Memory.WriteByte(PlayerSprite.Base(2) + PlayerSprite.OffPlayerDownTimer, 50);
            Memory.WriteWord(PlayerSprite.Base(2) + PlayerSprite.OffSpeed, 0);
            PlayerSprite.SetX(2, 336 << 16); PlayerSprite.SetY(2, 400 << 16);
        }, () => UpdatePlayers.Update(0));

        // ---- TickGoalieDiving (rise path) ----
        Scenario("update_goalie_diving_rise", () =>
        {
            PlayerSprite.SetPlayerState(PlayerSprite.SlotGoalie1, (byte)PortPlayerState.kGoalieDivingHigh);
            Memory.WriteByte(PlayerSprite.Base(PlayerSprite.SlotGoalie1) + PlayerSprite.OffPlayerDownTimer, 70);
        }, () => UpdatePlayers.Update(0));

        // ---- CheckIfThisPlayerGettingBooked (via TickAiControlled's
        // stoppage tail) ----
        Scenario("update_booked_player_walk_to_referee", () =>
        {
            int sa = PlayerSprite.Base(2);
            Memory.WriteDword(Memory.Addr.bookedPlayer, sa);
            Memory.WriteWord(Memory.Addr.foulXCoordinate, 300);
            Memory.WriteWord(Memory.Addr.foulYCoordinate, 400);
            Memory.WriteWord(Memory.Addr.refState, 2); // kRefWaitingPlayer
            PlayerSprite.SetX(2, 321 << 16); PlayerSprite.SetY(2, 400 << 16); // == foulX+21, foulY
            Memory.WriteWord(Memory.Addr.gameStatePl, 0);
            Memory.WriteWord(Memory.Addr.gameState, 0);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffUpdatePlayerIndex, 2); // round-robin -> slot 2's turn
        }, () => UpdatePlayers.Update(0));

        Console.WriteLine($"wrote {count} Step7B scenario dumps ({kMemSize} bytes each) to {outDir}");
    }
}
