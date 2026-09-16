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
using OpenSwos.Assets;
using OpenSwos.Sim.Port;
using OpenSwos.SwosVm;

public static class Step12IntegrationGolden
{
    private const int kMemSize = 0x60000;

    // PHASE 1 BOOTSTRAP-COMPLETENESS FOLLOW-UP (2026-09-16, see README.md
    // "Status: Phase 1"): Bootstrap() now runs the REAL production sequence
    // -- Main.cs's InitSwosVmFromMatchSetup, read in full and matched call
    // for call (SaveTeams/InitPlayerCardChance/DetermineStartingTeamAndTeam
    // PlayingUp/Pitch.SetPitchTypeAndNumber/InitPitchBallFactors/
    // TimeDeltaOverride/InitGameVariables/WritePlayerInfos/WireTeamFields/
    // PlayerEnergy.EffectEnabled+SetMatchLength/Result.ResetResult/
    // TacticsLoader.LoadAllTactics/team{1,2}Computer/Kickoff.StartingMatch/
    // playGame/Bench.InitBenchBeforeMatch/Camera.SetCameraToInitialPosition)
    // -- rather than a hand-poked stand-in for it. The ONLY input this file
    // still synthesizes is the two TeamRecord rosters themselves (real
    // team-FILE parsing is not ported anywhere in this repo); everything
    // downstream of that is the real, unmodified production code path.
    // This SUPERSEDES the old SeedPlayerInfo/SeedTeamSprites/SeedTeamData
    // hand-poking (removed) -- PlayerSprite.Init() (already called by
    // Memory.Init) already assigns every sprite's team number/ordinal, and
    // Kickoff.StartingMatch()'s InitPlayersBeforeEnteringPitch() now
    // positions all 22 sprites at the real entry line, so no manual sprite
    // seeding is needed at all.

    // BuildTeamRecord's synthetic roster: flat mid-range skills (4/7) and a
    // simple back-four/midfield/attack position shape, same spirit as the
    // OLD hand-poked bootstrap's flat skill=4 values -- NOT a claim of
    // realism, just enough shape (valid position enum per slot, a keeper)
    // for the real WritePlayerInfos/SkillScaling/GoalieSkillFromPrice
    // pipeline to have something sensible to scale.
    private static readonly string[] SyntheticPositions =
        { "G", "RB", "D", "D", "LB", "RW", "M", "M", "LW", "A", "A" };

    private static TeamRecord BuildTeamRecord(string name)
    {
        var players = new List<PlayerRecord>();
        for (int i = 0; i < 11; i++)
        {
            players.Add(new PlayerRecord
            {
                ShirtNumber = (byte)(i + 1),
                Name = $"P{i + 1}",
                Position = SyntheticPositions[i],
                Passing = 4, Shooting = 4, Heading = 4, Tackling = 4,
                Control = 4, Speed = 4, Finishing = 4,
                ValueCode = 20,
                Stamina = 7,
                FatigueCarry = 0,
                InjurySeverity = 0,
            });
        }
        return new TeamRecord { Name = name, Players = players };
    }

    // seed: applied via Rng.Reseed(seed) IMMEDIATELY after Memory.Init(),
    // i.e. BEFORE any of the real production calls below -- every one of
    // GameTime.DetermineStartingTeamAndTeamPlayingUp/Pitch.SetPitchType
    // AndNumber/WritePlayerInfos(via SkillScaling)/Kickoff.StartingMatch
    // draws real Rng bytes, so reseeding first lets the chosen seed
    // genuinely govern the WHOLE match. Memory.Init(true) already ends with
    // an internal Rng.Reseed(ReadWord(Addr.currentGameTick)) (Memory.cs:2268),
    // i.e. Rng.Reseed(0) on a fresh Init -- calling Rng.Reseed(0) again
    // right after is a no-op for seed=0, while Rng.Reseed(seed) for any
    // other seed cleanly overrides it. BuildTeamRecord draws no Rng bytes,
    // so the two rosters themselves stay BIT-IDENTICAL across seeds --
    // only the seed changes, exactly as the Phase 1 plan requires.
    private static void Bootstrap(int seed)
    {
        Memory.Init(pcMode: true);
        Rng.Reseed(seed);

        // Explicit reset of C#-side statics outside Memory this file's
        // dependency chain touches, per the Phase 1 plan's explicit "jawny
        // reset statyków poza Memory" step -- each process invocation of
        // this tool only ever calls Bootstrap() once (one seed per
        // process), so these are defensive, not currently load-bearing.
        GameTime.ResetGameTime();
        UpdatePlayers.ResetFallbackCounters();

        // ---- Main.cs InitSwosVmFromMatchSetup, in its real order --------
        GameTime.SaveTeams();
        GameTime.InitPlayerCardChance();
        GameTime.DetermineStartingTeamAndTeamPlayingUp();
        Pitch.SetPitchTypeAndNumber();
        GameTime.InitPitchBallFactors();

        // Main.cs: TimeDeltaOverride = clamp(2700 / max(1, SecondsPerHalf), 1, 70),
        // with the default match-length preset (180s total, 90s/half):
        // clamp(2700/90, 1, 70) = 30.
        GameTime.TimeDeltaOverride = 30;
        GameTime.InitGameVariables();

        var topTeam = BuildTeamRecord("TOP");
        var bottomTeam = BuildTeamRecord("BOTTOM");
        const bool topIsHuman = false, bottomIsHuman = false; // AI-vs-AI, matches match_bootstrap.c's milestone

        TeamDataLoader.WritePlayerInfos(Memory.Addr.team1InGameTeamPlayers, topTeam, topIsHuman);
        TeamDataLoader.WritePlayerInfos(Memory.Addr.team2InGameTeamPlayers, bottomTeam, bottomIsHuman);
        TeamDataLoader.WireTeamFields(top: true, team: topTeam,
            playersBaseAddr: Memory.Addr.team1InGameTeamPlayers,
            shotChanceTableAddr: Memory.Addr.team1ShotChanceTable,
            nameStorageAddr: Memory.Addr.team1NameStorage,
            isHumanControlled: topIsHuman);
        TeamDataLoader.WireTeamFields(top: false, team: bottomTeam,
            playersBaseAddr: Memory.Addr.team2InGameTeamPlayers,
            shotChanceTableAddr: Memory.Addr.team2ShotChanceTable,
            nameStorageAddr: Memory.Addr.team2NameStorage,
            isHumanControlled: bottomIsHuman);

        // Main.cs: PlayerEnergy.EffectEnabled = _fatigueSim || _competitionMatchPending;
        // both default false for a non-career, non-fatigue-sim match.
        PlayerEnergy.EffectEnabled = false;
        PlayerEnergy.SetMatchLength(180); // TotalMatchSeconds, default preset

        Result.ResetResult(topTeam.Name, bottomTeam.Name);

        TacticsLoader.LoadAllTactics();

        // Main.cs: team1Computer/team2Computer = topHuman/bottomHuman ? 0 : -1;
        // both AI here.
        Memory.WriteWord(Memory.Addr.team1Computer, topIsHuman ? 0 : -1);
        Memory.WriteWord(Memory.Addr.team2Computer, bottomIsHuman ? 0 : -1);

        Kickoff.StartingMatch();
        Memory.WriteWord(Memory.Addr.playGame, 1);

        Bench.InitBenchBeforeMatch();
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

    // PHASE 1 BOOTSTRAP-COMPLETENESS FOLLOW-UP (2026-09-16): was 300 (matching
    // sdl-debug's own stall convention), which turned out to be a FALSE
    // POSITIVE here -- ball/player positions legitimately freeze during
    // ST_RESULT_ON_HALFTIME (gameState 25) for up to
    // Memory.Addr.m_clearResultInterval ticks (660 in this synthetic
    // bootstrap) before AiBrain.SetControlsDirection's CPU auto-continue
    // path fires and the match proceeds into the second half -- confirmed
    // by probing a dump past the old 300-tick cutoff (see README.md
    // "Status: Phase 1 follow-up, continued" for the full trace). 2000
    // gives comfortable margin above any single known dwell interval
    // (clearResultInterval/clearResultHalftimeInterval) without being so
    // large that a GENUINE stall takes forever to confirm.
    private const int kStallTicks = 2000;

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
