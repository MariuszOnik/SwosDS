// ETAP 0 audit (2026-09-16): every prior golden-dump file exercises one
// step's functions against a hand-set Memory state -- none run the full
// cold-start pipeline steps 7B/10/11/12 all depend on together for the
// first time: Memory.Init() -> a real match setup -> Kickoff.
// PrepareForInitialKick() -> live GameLoop.Tick() calls. This file closes
// that gap: it mirrors nds-app/source/match_bootstrap.c's dsBootstrapMatch()
// (same PlayerInfo/sprite/TeamData seeding, same formation placeholder
// coordinates) using the real, unmodified OpenSWOS C# (PlayerSprite,
// TeamData, Kickoff, Camera, GameLoop -- all already ported and covered by
// their own per-function golden tests), then dumps the full 0x60000-byte
// buffer after setup, after one tick, and after ten ticks.
//
// Deliberately does NOT reproduce the DS-adapter bug this test was written
// to catch (forcing gameStatePl/breakCameraMode past Kickoff.
// PrepareForInitialKick()'s own state) -- that was fixed directly in
// match_bootstrap.c once this test's real C# behavior confirmed the fix.
// tests/test_step12_integration_golden.c replays the identical setup
// through match_bootstrap.c's actual dsBootstrapMatch() (linked directly,
// not copied) and swosGameLoopTick(), and byte-compares against these dumps.
// Scope: parity of this synthetic setup through tick 10. It does not prove
// equivalence to OpenSWOS Main.cs or a complete kickoff-to-live sequence.
using OpenSwos.Sim.Port;
using OpenSwos.SwosVm;

public static class Step12IntegrationGolden
{
    private const int kMemSize = 0x60000;

    // Same layout as match_bootstrap.c's PLAYERINFO_TOP_BASE/PLAYERINFO_BOTTOM_BASE.
    private const int PlayerInfoTopBase = 0x51000;
    private const int PlayerInfoBottomBase = 0x51400;
    private const int PlayerInfoSize = 61; // TeamDataLoader.PlayerInfoSize

    // Same hand-picked placeholder formation as match_bootstrap.c (NOT
    // OpenSWOS's own starting-position tables -- see that file's header for
    // why). Index 0 = goalkeeper (ordinal 1), 1..10 = outfielders (ordinal 2..11).
    private static readonly int[] TopFormationX = { 336, 150, 280, 392, 522, 150, 280, 392, 522, 250, 422 };
    private static readonly int[] TopFormationY = { 40, 160, 160, 160, 160, 280, 280, 280, 280, 380, 380 };
    private static readonly int[] BottomFormationX = { 336, 150, 280, 392, 522, 150, 280, 392, 522, 250, 422 };
    private static readonly int[] BottomFormationY = { 808, 688, 688, 688, 688, 568, 568, 568, 568, 468, 468 };

    // Same byte offsets as swos_team_data_loader.h's TDL_OFF_* constants.
    private const int OffSubstituted = 0, OffPosition = 4, OffFace = 5, OffCards = 10;
    private const int OffPassing = 27, OffShooting = 28, OffHeading = 29, OffTackling = 30;
    private const int OffBallControl = 31, OffSpeed = 32, OffFinishing = 33, OffGoalieSkill = 34;

    private static void SeedPlayerInfo(int baseAddr)
    {
        for (int i = 0; i < 11; i++)
        {
            int addr = baseAddr + i * PlayerInfoSize;
            Memory.WriteByte(addr + OffSubstituted, 0);
            Memory.WriteByte(addr + OffCards, 0);
            Memory.WriteByte(addr + OffFace, 0);
            Memory.WriteByte(addr + OffPosition, i == 0 ? 0 : 1);
            Memory.WriteByte(addr + OffPassing, 4);
            Memory.WriteByte(addr + OffShooting, 4);
            Memory.WriteByte(addr + OffHeading, 4);
            Memory.WriteByte(addr + OffTackling, 4);
            Memory.WriteByte(addr + OffBallControl, 4);
            Memory.WriteByte(addr + OffSpeed, 4);
            Memory.WriteByte(addr + OffFinishing, 4);
            Memory.WriteByte(addr + OffGoalieSkill, 4);
        }
    }

    private static void SeedTeamSprites(bool top, int[] fx, int[] fy)
    {
        int firstSlot = PlayerSprite.FirstSlotForTeam(top);
        int initialDir = top ? 4 : 0;

        for (int i = 0; i < 11; i++)
        {
            int slot = firstSlot + i;
            PlayerSprite.SetTeamNumber(slot, top ? 1 : 2);
            PlayerSprite.SetPlayerOrdinal(slot, i + 1);
            PlayerSprite.SetX(slot, fx[i] << 16);
            PlayerSprite.SetY(slot, fy[i] << 16);
            PlayerSprite.SetDirection(slot, initialDir);
            PlayerSprite.SetPlayerState(slot, 0); // PLSTATE_NORMAL
            PlayerSprite.SetImageIndex(slot, 0);
        }
    }

    private static void SeedTeamData(bool top, int playerInfoBase)
    {
        int teamBase = TeamData.Base(top);
        Memory.WriteDword(teamBase + TeamData.OffInGameTeamPtr, playerInfoBase);
        // playerNumber=0 on both teams -> both AI-controlled, matching
        // match_bootstrap.c's first AI-vs-AI milestone.
        Memory.WriteWord(teamBase + TeamData.OffPlayerNumber, 0);

        if (top)
            Memory.WriteDword(Memory.Addr.topTeamInGame, playerInfoBase);
        else
            Memory.WriteDword(Memory.Addr.bottomTeamInGame, playerInfoBase);
    }

    // Mirrors match_bootstrap.c's dsBootstrapMatch() MINUS the bug that
    // function had (forcing gameStatePl/breakCameraMode past
    // PrepareForInitialKick()'s own real state) -- this is what
    // dsBootstrapMatch() does now, after the ETAP 0 fix.
    private static void Bootstrap()
    {
        Memory.Init(pcMode: true);

        SeedPlayerInfo(PlayerInfoTopBase);
        SeedPlayerInfo(PlayerInfoBottomBase);

        SeedTeamSprites(true, TopFormationX, TopFormationY);
        SeedTeamSprites(false, BottomFormationX, BottomFormationY);

        SeedTeamData(true, PlayerInfoTopBase);
        SeedTeamData(false, PlayerInfoBottomBase);

        Kickoff.PrepareForInitialKick();
        Camera.SetCameraToInitialPosition();
    }

    public static void Run(string outDir)
    {
        Directory.CreateDirectory(outDir);

        void Dump(string name)
        {
            byte[] dump = Memory.View(0, kMemSize).ToArray();
            File.WriteAllBytes(Path.Combine(outDir, $"s12_{name}.bin"), dump);
        }

        Bootstrap();
        Dump("after_setup");

        GameLoop.Tick();
        Dump("after_tick1");

        for (int i = 0; i < 9; i++)
            GameLoop.Tick();
        Dump("after_tick10");
    }
}
