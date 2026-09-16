// Step-6A full-Memory differential fixtures for the real PlayerControlled.cs.
// All scenarios use human teams, so the intentional AiBrain/AiHelpers boundary
// is never crossed; CPU behavior is completed with step 9.
using OpenSwos.Sim.Port;
using OpenSwos.SwosVm;

public static class PlayerControlledGolden
{
    private const int MemSize = 0x60000;

    public static void Run(string outDir)
    {
        int count = 0;
        int player = PlayerSprite.Base(1);
        int receiver = PlayerSprite.Base(2);
        int keeper = PlayerSprite.Base(PlayerSprite.SlotGoalie1);

        void Scenario(string name, Action setup, Action act)
        {
            Memory.Init(true);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffPlayerNumber, 1);
            setup();
            act();
            File.WriteAllBytes(Path.Combine(outDir, $"pc_{name}.bin"),
                Memory.View(0, MemSize).ToArray());
            count++;
        }

        Scenario("controlled_penalties_early", () => {
            Memory.WriteWord(Memory.Addr.playingPenalties, 1);
            Memory.WriteWord(Memory.Addr.gameState, 3);
        }, () => PlayerControlled.RunControlledBranch(player, true));

        Scenario("controlled_break_wrong_team", () => {
            Memory.WriteWord(Memory.Addr.gameStatePl, 7);
            Memory.WriteDword(Memory.Addr.lastTeamPlayedBeforeBreak, TeamData.BottomBase);
        }, () => PlayerControlled.RunControlledBranch(player, true));

        Scenario("controlled_stopped_no_direction", () => {
            Memory.WriteWord(Memory.Addr.gameStatePl, 7);
            Memory.WriteDword(Memory.Addr.lastTeamPlayedBeforeBreak, TeamData.TopBase);
            Memory.WriteWord(player + PlayerSprite.OffDirection, 3);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffCurrentAllowedDirection, -1);
        }, () => PlayerControlled.RunControlledBranch(player, true));

        Scenario("controlled_has_ball_pin", () => {
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            Memory.WriteDword(TeamData.TopBase + TeamData.OffControlledPlayer, player);
            Memory.WriteWord(TeamData.TopBase + 40, 1);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffCurrentAllowedDirection, 2);
            Memory.WriteWord(player + PlayerSprite.OffPlayerOrdinal, 2);
            PlayerSprite.SetX(1, 300 << 16); PlayerSprite.SetY(1, 400 << 16);
        }, () => PlayerControlled.RunControlledBranch(player, true));

        Scenario("receipt_null_noop", () => { },
            () => PlayerControlled.RunPassReceiptTrigger(player, true));

        Scenario("receipt_wrong_sprite_noop", () => {
            Memory.WriteDword(TeamData.TopBase + TeamData.OffPassToPlayerPtr, receiver);
        }, () => PlayerControlled.RunPassReceiptTrigger(player, true));

        Scenario("receipt_outfielder_commit", () => {
            Memory.WriteDword(TeamData.TopBase + TeamData.OffPassToPlayerPtr, receiver);
            Memory.WriteByte(TeamData.TopBase + 64, 1);
            Memory.WriteByte(TeamData.TopBase + TeamData.OffPlVeryCloseToBall, 1);
            BallSprite.Speed = 900;
            PlayerSprite.SetX(2, 330 << 16); PlayerSprite.SetY(2, 440 << 16);
            Memory.WriteWord(receiver + PlayerSprite.OffPlayerOrdinal, 3);
        }, () => PlayerControlled.RunPassReceiptTrigger(receiver, true));

        Scenario("receipt_keeper_backpass", () => {
            Memory.WriteDword(TeamData.TopBase + TeamData.OffPassToPlayerPtr, keeper);
            Memory.WriteByte(TeamData.TopBase + 64, 1);
            Memory.WriteByte(TeamData.TopBase + TeamData.OffPlCloseToBall, 1);
            BallSprite.Speed = 1800;
            Memory.WriteDword(Memory.Addr.lastTeamPlayed, TeamData.TopBase);
            Memory.WriteWord(Memory.Addr.playerHadBall, 0);
            PlayerSprite.SetX(PlayerSprite.SlotGoalie1, 336 << 16);
            PlayerSprite.SetY(PlayerSprite.SlotGoalie1, 160 << 16);
        }, () => PlayerControlled.RunPassReceiptTrigger(keeper, true));

        Scenario("expecting_outside_pitch", () => {
            PlayerSprite.SetX(2, 70 << 16); PlayerSprite.SetY(2, 400 << 16);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffPassingToPlayer, 1);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffPassingBall, 1);
        }, () => PlayerControlled.RunPassExpectingBranch(receiver, true));

        Scenario("expecting_plain_chase", () => {
            PlayerSprite.SetX(2, 300 << 16); PlayerSprite.SetY(2, 400 << 16);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffBallX, 360);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffBallY, 450);
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
        }, () => PlayerControlled.RunPassExpectingBranch(receiver, true));

        Scenario("expecting_long_spin", () => {
            PlayerSprite.SetX(2, 300 << 16); PlayerSprite.SetY(2, 400 << 16);
            PlayerSprite.SetFullDirection(2, 0);
            BallSprite.Speed = 900; BallSprite.Direction = 2; BallSprite.FullDirection = 32;
            Memory.WriteDword(TeamData.TopBase + TeamData.OffLongPass, 1);
            Memory.WriteWord(TeamData.TopBase + TeamData.OffPassingToPlayer, 1);
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
        }, () => PlayerControlled.RunPassExpectingBranch(receiver, true));

        Scenario("expecting_receipt_commit", () => {
            PlayerSprite.SetX(2, 330 << 16); PlayerSprite.SetY(2, 440 << 16);
            Memory.WriteDword(TeamData.TopBase + TeamData.OffPassToPlayerPtr, receiver);
            Memory.WriteByte(TeamData.TopBase + 64, 1);
            Memory.WriteByte(TeamData.TopBase + TeamData.OffPlVeryCloseToBall, 1);
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            BallSprite.Speed = 900;
        }, () => PlayerControlled.RunPassExpectingBranch(receiver, true));

        Console.WriteLine($"wrote {count} PlayerControlled golden scenarios");
    }
}
