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
//
// PHASE 1 (lockstep, added 2026-09-16): extended with a long-running,
// tick-by-tick lockstep log generator (RunLockstepLog) and an on-demand
// full-buffer dump at an exact tick (DumpFullAtTick), both driven by an
// explicit seed -- see tools/lockstep_runner.c (the C-side consumer) and
// this file's own RunLockstepLog/DumpFullAtTick doc comments below for the
// full design. Bootstrap() itself is now parameterized by seed but the
// underlying setup (formation, PlayerInfo, Kickoff/Camera calls) is
// UNCHANGED and seed-independent -- only Rng.Reseed(seed), applied last,
// makes the subsequent GameLoop.Tick() stream diverge per seed. This keeps
// "identical synthetic setup" literally identical across seeds while still
// giving each seed its own RNG-driven match.
using System.Reflection;
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

    // Same default the real production loader uses --
    // TeamDataLoader.WireTeamFields's defaultTacticsIndex parameter defaults
    // to 5 (4-3-3), Main.cs never overrides it. See Bootstrap()'s PHASE 1
    // comment for why this now matters (paired with TacticsLoader.LoadAllTactics()).
    private const int DefaultTacticsIndex = 5;

    private static void SeedTeamData(bool top, int playerInfoBase)
    {
        int teamBase = TeamData.Base(top);
        Memory.WriteDword(teamBase + TeamData.OffInGameTeamPtr, playerInfoBase);
        // playerNumber=0 on both teams -> both AI-controlled, matching
        // match_bootstrap.c's first AI-vs-AI milestone.
        Memory.WriteWord(teamBase + TeamData.OffPlayerNumber, 0);
        // PHASE 1 BOOTSTRAP FIX (2026-09-16): was never set at all
        // (implicitly 0 = tact_4_4_2 via Memory.Init's zero-fill) -- now
        // matches the real production default, mirrored exactly in
        // match_bootstrap.c's seedTeamData().
        Memory.WriteWord(teamBase + TeamData.OffTactics, DefaultTacticsIndex);

        if (top)
            Memory.WriteDword(Memory.Addr.topTeamInGame, playerInfoBase);
        else
            Memory.WriteDword(Memory.Addr.bottomTeamInGame, playerInfoBase);
    }

    // Mirrors match_bootstrap.c's dsBootstrapMatch() MINUS the bug that
    // function had (forcing gameStatePl/breakCameraMode past
    // PrepareForInitialKick()'s own real state) -- this is what
    // dsBootstrapMatch() does now, after the ETAP 0 fix.
    //
    // seed: applied via Rng.Reseed(seed) IMMEDIATELY after Memory.Init(),
    // i.e. BEFORE any of the seeding/Kickoff/Camera calls below -- not
    // after them. Two reasons:
    //   1. Kickoff.PrepareForInitialKick() itself draws real Rng bytes
    //      (teamPlayingUp/teamStarting coin-flip, kickoff-formation jitter
    //      -- Kickoff.cs:130/134/193), so reseeding before it lets the
    //      chosen seed genuinely govern the WHOLE match, kickoff side
    //      included, not just ticks from GameLoop.Tick() onward.
    //   2. Memory.Init(true) already ends with an internal
    //      Rng.Reseed(ReadWord(Addr.currentGameTick)) (Memory.cs:2268),
    //      which is Rng.Reseed(0) on a fresh Init (currentGameTick==0).
    //      Calling Rng.Reseed(0) again right after Init() is therefore a
    //      pure no-op for seed=0 -- byte-identical to every dump this file
    //      produced before Phase 1 -- while Rng.Reseed(seed) for any other
    //      seed cleanly overrides that internal default.
    // SeedPlayerInfo/SeedTeamSprites/SeedTeamData draw no Rng bytes, so the
    // synthetic setup itself (formation, PlayerInfo, TeamData) stays
    // BIT-IDENTICAL across seeds, exactly as the Phase 1 plan requires --
    // only the seed changes.
    private static void Bootstrap(int seed)
    {
        Memory.Init(pcMode: true);
        Rng.Reseed(seed);

        // Explicit reset of C#-side statics outside Memory this file's
        // dependency chain is known to touch (see README "RNG and
        // static-state discipline" notes from steps 9-11B), per the Phase 1
        // plan's explicit "jawny reset statyków poza Memory" step -- each
        // process invocation of this tool only ever calls Bootstrap() once
        // (one seed per process, see Program.cs's --lockstep-log/
        // --lockstep-dump dispatch), so these are defensive, not currently
        // load-bearing. GameTime.ResetGameTime()/UpdatePlayers.
        // ResetFallbackCounters() only touch already-zero Memory slots /
        // already-default C# statics on a freshly-Init'd buffer, so they're
        // safe no-ops here. Result.ResetResult(team1Name, team2Name) is
        // DELIBERATELY NOT called: it writes team-name-DERIVED bytes into
        // Memory (res_team1NameLength etc.) that match_bootstrap.c's C
        // bootstrap has no equivalent for (no team-name concept exists in
        // the synthetic setup) -- calling it here would silently diverge
        // this file's dumps from the C port with no way to match on the C
        // side, exactly the kind of asymmetry Phase 1 must not introduce.
        GameTime.ResetGameTime();
        UpdatePlayers.ResetFallbackCounters();

        SeedPlayerInfo(PlayerInfoTopBase);
        SeedPlayerInfo(PlayerInfoBottomBase);

        SeedTeamSprites(true, TopFormationX, TopFormationY);
        SeedTeamSprites(false, BottomFormationX, BottomFormationY);

        SeedTeamData(true, PlayerInfoTopBase);
        SeedTeamData(false, PlayerInfoBottomBase);

        // PHASE 1 BOOTSTRAP FIX (2026-09-16): mirrors match_bootstrap.c's
        // matching call -- the real InitSwosVmFromMatchSetup (Main.cs) calls
        // TacticsLoader.LoadAllTactics() before Kickoff.StartingMatch();
        // this synthetic bootstrap never did, leaving teamTacticsPool
        // entirely zero (see this file's Phase 1 header comment / README.md
        // "Status: Phase 1" for the audit that found this). Same call point
        // here, before Kickoff.PrepareForInitialKick() below.
        TacticsLoader.LoadAllTactics();

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

        Bootstrap(0);
        Dump("after_setup");

        GameLoop.Tick();
        Dump("after_tick1");

        for (int i = 0; i < 9; i++)
            GameLoop.Tick();
        Dump("after_tick10");
    }

    // ---- Phase 1 lockstep -------------------------------------------------
    //
    // FNV-1a 64-bit, the exact algorithm named in the Phase 1 plan (offset
    // basis 0xcbf29ce484222325, prime 0x100000001b3), over the full
    // 0x60000-byte Memory buffer.
    private static ulong Fnv1a64(byte[] data)
    {
        ulong h = 0xcbf29ce484222325UL;
        for (int i = 0; i < data.Length; i++)
        {
            h ^= data[i];
            h *= 0x100000001b3UL;
        }
        return h;
    }

    // Rng's seed/xorKey/xorIndex state (both streams) is private with no
    // public getter (see swos_rng.h's header comment) -- reflection is the
    // only way to read it from outside Rng.cs without modifying that file,
    // which must stay an unmodified copy of the real OpenSWOS source for
    // every other golden-dump harness in this repo to remain meaningful.
    private static readonly FieldInfo FSeed = typeof(Rng).GetField("m_seed", BindingFlags.NonPublic | BindingFlags.Static)!;
    private static readonly FieldInfo FXorKey = typeof(Rng).GetField("m_xorKey", BindingFlags.NonPublic | BindingFlags.Static)!;
    private static readonly FieldInfo FXorIndex = typeof(Rng).GetField("m_xorIndex", BindingFlags.NonPublic | BindingFlags.Static)!;
    private static readonly FieldInfo FSeed2 = typeof(Rng).GetField("m_seed2", BindingFlags.NonPublic | BindingFlags.Static)!;
    private static readonly FieldInfo FXorKey2 = typeof(Rng).GetField("m_xorKey2", BindingFlags.NonPublic | BindingFlags.Static)!;
    private static readonly FieldInfo FXorIndex2 = typeof(Rng).GetField("m_xorIndex2", BindingFlags.NonPublic | BindingFlags.Static)!;

    private static (byte, byte, byte, byte, byte, byte) ReadRngState() => (
        (byte)FSeed.GetValue(null)!, (byte)FXorKey.GetValue(null)!, (byte)FXorIndex.GetValue(null)!,
        (byte)FSeed2.GetValue(null)!, (byte)FXorKey2.GetValue(null)!, (byte)FXorIndex2.GetValue(null)!);

    // One lockstep tick record, written as a flat sequence of fixed-size
    // little-endian fields via BinaryWriter (.NET's default on all
    // platforms this harness runs on) -- tests/lockstep_runner.c reads the
    // exact same field sequence with plain fwrite()/fread() of stdint.h
    // types, no packed-struct/alignment assumptions on either side. See
    // this file's header comment and lockstep_runner.c's own header for the
    // full field list and why each one is included (Phase 1 plan step 3).
    private static void WriteTickRecord(BinaryWriter w, uint tick)
    {
        byte[] mem = Memory.View(0, kMemSize).ToArray();
        ulong hash = Fnv1a64(mem);
        var (seed, xorKey, xorIndex, seed2, xorKey2, xorIndex2) = ReadRngState();

        w.Write(tick);
        w.Write(hash);
        w.Write(seed); w.Write(xorKey); w.Write(xorIndex);
        w.Write(seed2); w.Write(xorKey2); w.Write(xorIndex2);
        w.Write((short)Memory.ReadSignedWord(Memory.Addr.gameState));
        w.Write((short)Memory.ReadSignedWord(Memory.Addr.gameStatePl));
        w.Write((short)Memory.ReadSignedWord(Memory.Addr.breakCameraMode));
        w.Write(BallSprite.X);
        w.Write(BallSprite.Y);
        w.Write(BallSprite.Z);
        w.Write((short)Memory.ReadSignedWord(Memory.Addr.team1TotalGoals));
        w.Write((short)Memory.ReadSignedWord(Memory.Addr.team2TotalGoals));
        w.Write((ushort)Memory.ReadWord(Memory.Addr.currentGameTick));
        w.Write(Memory.ReadDword(Memory.Addr.gt_gameTimeInMinutes));
        w.Write(TeamData.ControlledPlayer(true));
        w.Write(TeamData.ControlledPlayer(false));
    }

    // Ball + all 22 players' whole positions, for the stall detector below.
    // Matches sdl-debug's own "300 ticks without movement of the ball or
    // any player" stall definition (README's SDL diagnostic frontend
    // section) -- reused here for consistency rather than inventing a
    // second stall heuristic in this repo.
    private static int[] SnapshotPositions()
    {
        var pos = new int[2 + 22 * 2];
        pos[0] = BallSprite.X;
        pos[1] = BallSprite.Y;
        for (int slot = 0; slot < 22; slot++)
        {
            pos[2 + slot * 2] = PlayerSprite.X(slot);
            pos[3 + slot * 2] = PlayerSprite.Y(slot);
        }
        return pos;
    }

    private const int kStallTicks = 300;

    // Runs Bootstrap(seed) then up to maxTicks GameLoop.Tick() calls,
    // writing one WriteTickRecord per tick to outPath. Stops early (per the
    // Phase 1 plan step 6) if ball+all-22-player positions haven't changed
    // for kStallTicks consecutive ticks -- the record at the tick where the
    // stall was first CONFIRMED (i.e. kStallTicks after the last real
    // movement) is the last one written, and a one-line sidecar
    // "<path>.meta.txt" records whether the run ended by stall or by
    // reaching maxTicks, and the exact stall-start tick if applicable.
    public static void RunLockstepLog(string outPath, int seed, int maxTicks)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(outPath))!);
        Bootstrap(seed);

        int[]? lastPos = null;
        int unchangedTicks = 0;
        int stallStartTick = -1;

        using var fs = new FileStream(outPath, FileMode.Create, FileAccess.Write);
        using var w = new BinaryWriter(fs);

        uint written = 0;
        for (uint tick = 1; tick <= (uint)maxTicks; tick++)
        {
            GameLoop.Tick();
            WriteTickRecord(w, tick);
            written = tick;

            int[] pos = SnapshotPositions();
            if (lastPos != null && pos.AsSpan().SequenceEqual(lastPos))
            {
                if (unchangedTicks == 0) stallStartTick = (int)tick - 1;
                unchangedTicks++;
                if (unchangedTicks >= kStallTicks)
                    break;
            }
            else
            {
                unchangedTicks = 0;
                stallStartTick = -1;
            }
            lastPos = pos;
        }

        bool stalled = unchangedTicks >= kStallTicks;
        File.WriteAllText(outPath + ".meta.txt",
            $"seed={seed}\nrequested_max_ticks={maxTicks}\nticks_written={written}\n" +
            $"stalled={(stalled ? 1 : 0)}\nstall_start_tick={(stalled ? stallStartTick : -1)}\n");

        Console.WriteLine($"lockstep seed={seed}: wrote {written} tick record(s) to {outPath}" +
            (stalled ? $" (stalled at tick {stallStartTick}, confirmed after {kStallTicks} unchanged ticks)" : " (reached max ticks)"));
    }

    // On-demand full 0x60000-byte Memory dump at an EXACT tick, for
    // diagnosing a lockstep mismatch (Phase 1 plan step 4: "zapisz pełne
    // dumpy C# i C"). Deterministically replays Bootstrap(seed) + `tick`
    // GameLoop.Tick() calls from scratch -- cheap enough (a handful of
    // ticks to ~100000) that there is no need to keep every tick's full
    // buffer around during RunLockstepLog itself, which would be tens of
    // GB for the long tier.
    public static void DumpFullAtTick(string outPath, int seed, int tick)
    {
        Bootstrap(seed);
        for (int i = 0; i < tick; i++)
            GameLoop.Tick();
        byte[] mem = Memory.View(0, kMemSize).ToArray();
        File.WriteAllBytes(outPath, mem);
        var (rseed, xorKey, xorIndex, seed2, xorKey2, xorIndex2) = ReadRngState();
        File.WriteAllText(outPath + ".rng.txt",
            $"seed={rseed} xorKey={xorKey} xorIndex={xorIndex} seed2={seed2} xorKey2={xorKey2} xorIndex2={xorIndex2}\n");
        Console.WriteLine($"dumped seed={seed} tick={tick} full buffer to {outPath}");
    }
}
