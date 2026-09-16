// Generates the step-3 differential-test golden file for SpriteUpdate.cs:
// runs the real CalculateDeltaXAndY/MoveSprite/UpdateSpriteAnimation against
// representative sprite states and dumps inputs+outputs as plain text lines.
// tests/test_sprite_update_golden.c parses this file, recomputes the same
// scenarios via the C port, and asserts exact equality -- this is the
// "positive/negative deltas, stopping, directions" coverage requested in
// review, generalizing the byte-exact golden-dump technique from
// Memory.Init() to individual function outputs.
//
// CalculateDeltaXAndY has no PC/Amiga branch to test (see its own comment:
// "We're hard-locked to PC mode in OpenSWOS" -- the 41/64 damping applies
// unconditionally, no pcMode parameter exists on this function at all).
using OpenSwos.Sim.Port;
using OpenSwos.SwosVm;

public static class SpriteUpdateGolden
{
    public static void Run(string outDir)
    {
        Directory.CreateDirectory(outDir);
        string path = Path.Combine(outDir, "sprite_update_golden.txt");
        using var w = new StreamWriter(path);

        // ---- CalculateDeltaXAndY: directions (all 8 octants) + edge cases ----
        (int speed, int x, int y, int destX, int destY)[] calcCases = {
            (2048, 0, 0, 100, 0),        // pure +X (East)
            (2048, 0, 0, -100, 0),       // pure -X (West)
            (2048, 0, 0, 0, 100),        // pure +Y (South, Y grows downward)
            (2048, 0, 0, 0, -100),       // pure -Y (North)
            (2048, 0, 0, 100, 100),      // +X+Y (SE)
            (2048, 0, 0, -100, 100),     // -X+Y (SW)
            (2048, 0, 0, 100, -100),     // +X-Y (NE)
            (2048, 0, 0, -100, -100),    // -X-Y (NW)
            (2048, 50, 50, 50, 50),      // no movement -> direction = -1
            (0, 0, 0, 100, 100),         // speed = 0 -> deltas zero, direction still computed
            (1024, 0, 0, 1000, 500),     // large deltas, multiple halvings before table lookup
            (1024, 0, 0, 10, 5),         // small deltas, already < 32
            (1500, 200, 200, 50, 50),    // sprite ahead of dest on both axes (both negative)
            (2688, 0, 0, 1, 0),          // kHighKickBallSpeed, minimal delta
        };
        Memory.Init(pcMode: true);
        foreach (var (speed, x, y, destX, destY) in calcCases)
        {
            var r = SpriteUpdate.CalculateDeltaXAndY(speed, x, y, destX, destY);
            w.WriteLine($"CALC {speed} {x} {y} {destX} {destY} => {r.DeltaX} {r.DeltaY} {r.Direction}");
        }

        // ---- MoveSprite: positive/negative deltas, overshoot vs not, stationary ----
        (int xRaw, int destX, int deltaXRaw, int yRaw, int destY, int deltaYRaw)[] moveCases = {
            (0, 100, 0x00010000, 0, 50, 0x00008000),                 // neither axis reaches yet
            (99 << 16, 100, 0x00020000, 49 << 16, 50, 0x00020000),   // both axes overshoot -> snap to dest
            (100 << 16, 0, -0x00010000, 10 << 16, 0, -0x00010000),   // negative, not reaching yet
            (1 << 16, 0, -0x00020000, 1 << 16, 0, -0x00020000),      // negative overshoot -> snap to 0
            (42 << 16, 42, 0, 7 << 16, 99, 0),                       // delta already 0 -> no-op even though far from dest on Y
        };
        foreach (var (xRaw, destX, deltaXRaw, yRaw, destY, deltaYRaw) in moveCases)
        {
            Memory.Init(pcMode: true);
            BallSprite.X = xRaw;
            BallSprite.DestX = (short)destX;
            BallSprite.DeltaX = deltaXRaw;
            BallSprite.Y = yRaw;
            BallSprite.DestY = (short)destY;
            BallSprite.DeltaY = deltaYRaw;

            SpriteUpdate.MoveSprite(BallSprite.Base);

            w.WriteLine($"MOVE {xRaw} {destX} {deltaXRaw} {yRaw} {destY} {deltaYRaw} => "
                + $"{BallSprite.X} {BallSprite.DeltaX} {BallSprite.Y} {BallSprite.DeltaY}");
        }

        // ---- UpdateSpriteAnimation: opcode coverage via synthetic streams ----
        // Fixtures are plain literal test fixtures (not OpenSWOS data), mirrored
        // identically in tests/test_sprite_update_golden.c -- duplicating a
        // handful of small integers between languages doesn't risk drift the
        // way duplicating an ALGORITHM would; the algorithm under test is
        // UpdateSpriteAnimation itself, exercised identically here and in C.
        const int kScratchStreamAddr = 0x20000; // free region between the anim-table arena and the sprite pool
        short[][] fixtures = {
            new short[] { 10, 20, -999 },         // plain loop: 10, 20, 10, 20, ...
            new short[] { 30, -101 },              // hold-last: settles on frame 30 forever
            new short[] { 40, -50, 50, -999 },     // variable-pause opcode (-50 = pause 50 ticks) then continues
            new short[] { 100, 110, 120, -102, -999 }, // negative relative jump back (-102 -> offset -2)
        };
        foreach (var stream in fixtures)
        {
            Memory.Init(pcMode: true);
            for (int i = 0; i < stream.Length; i++)
                Memory.WriteWord(kScratchStreamAddr + i * 2, stream[i]);

            // Sprite.onScreen=1, frameIndicesTable=scratch, frameIndex=-1,
            // frameDelay=1, cycleFramesTimer=1 (fires on the very first tick).
            Memory.WriteWord(BallSprite.Base + 84, 1);          // OffOnScreen (shared Sprite struct layout)
            BallSprite.FrameIndicesTable = kScratchStreamAddr;
            BallSprite.FrameIndex = -1;
            BallSprite.FrameDelay = 1;
            BallSprite.CycleFramesTimer = 1;
            Memory.WriteWord(BallSprite.Base + 28, -1);          // OffFrameSwitchCounter (shared layout, no BallSprite property)

            const int kTicks = 8;
            for (int t = 0; t < kTicks; t++)
                SpriteUpdate.UpdateSpriteAnimation(BallSprite.Base);

            short fsCounter = Memory.ReadSignedWord(BallSprite.Base + 28);
            string streamStr = string.Join(",", stream);
            w.WriteLine($"ANIM [{streamStr}] {kTicks} => "
                + $"{BallSprite.ImageIndex} {BallSprite.FrameIndex} {BallSprite.FrameDelay} "
                + $"{BallSprite.CycleFramesTimer} {fsCounter}");
        }

        // ---- UpdateSpriteDirectionAndDeltas: movement + no-movement, full
        // ---- 0..255 direction AND the 0..7 quantised direction together ----
        (int x, int y, int destX, int destY, int speed)[] sprDirCases = {
            (0, 0, 100, 0, 2048),      // movement, east
            (0, 0, 0, 0, 2048),        // no movement -> fullDirection=-1, direction still 0 (unconditional formula)
            (50, 50, -50, -50, 1500),  // NW-ish
            (100, 100, 100, 200, 1024),// south
            (0, 0, -30, 40, 3000),     // mixed signs, high speed
        };
        foreach (var (x, y, destX, destY, speed) in sprDirCases)
        {
            Memory.Init(pcMode: true);
            BallSprite.XPixels = (short)x;
            BallSprite.YPixels = (short)y;
            BallSprite.DestX = (short)destX;
            BallSprite.DestY = (short)destY;
            BallSprite.Speed = (short)speed;

            SpriteUpdate.UpdateSpriteDirectionAndDeltas(BallSprite.Base);

            w.WriteLine($"SPRDIR {x} {y} {destX} {destY} {speed} => "
                + $"{BallSprite.DeltaX} {BallSprite.DeltaY} {BallSprite.FullDirection} {BallSprite.Direction}");
        }

        // ---- PlayerActions.SetPlayerAnimationTable: team1/team2/goalkeeper/
        // ---- null-frame-pointer (via the stub, a verbatim copy -- see
        // ---- PlayerActionsStub.cs for why) ----
        (int slot, int animTable, int direction)[] setAnimCases = {
            (1, Memory.Addr.kPlayerRunningAnimTableAddr, 2),  // team1 outfielder, direction E
            (12, Memory.Addr.kPlayerRunningAnimTableAddr, 6), // team2 outfielder, direction W
            (0, Memory.Addr.kPlayerRunningAnimTableAddr, 0),  // team1 goalkeeper, direction N
            (0, Memory.Addr.kPlTacklingAnimTableAddr, 0),     // goalkeeper + outfielder-only table -> null frame pointer
        };
        foreach (var (slot, animTable, direction) in setAnimCases)
        {
            Memory.Init(pcMode: true);
            int b = PlayerSprite.Base(slot);
            Memory.WriteWord(b + PlayerSprite.OffDirection, direction);
            // Seed a known "before" state so the null-pointer early-return
            // path's PRESERVED fields are verifiable, not coincidentally 0.
            Memory.WriteWord(b + PlayerSprite.OffFrameIndex, 77);
            Memory.WriteWord(b + PlayerSprite.OffCycleFramesTimer, 77);
            Memory.WriteWord(b + PlayerSprite.OffFrameSwitchCounter, 77);
            Memory.WriteWord(b + PlayerSprite.OffStartingDirection, 77);

            PlayerActions.SetPlayerAnimationTable(b, animTable);

            int animTablePtr = Memory.ReadSignedDword(b + PlayerSprite.OffAnimTablePtr);
            short frameDelay = Memory.ReadSignedWord(b + PlayerSprite.OffFrameDelay);
            int fitPtr = Memory.ReadSignedDword(b + PlayerSprite.OffFrameIndicesTable);
            short fsCounter = Memory.ReadSignedWord(b + PlayerSprite.OffFrameSwitchCounter);
            short frameIndex = Memory.ReadSignedWord(b + PlayerSprite.OffFrameIndex);
            short cycleTimer = Memory.ReadSignedWord(b + PlayerSprite.OffCycleFramesTimer);
            short startDir = Memory.ReadSignedWord(b + PlayerSprite.OffStartingDirection);

            w.WriteLine($"SETANIM {slot} {animTable} {direction} => "
                + $"{animTablePtr} {frameDelay} {fitPtr} {fsCounter} {frameIndex} {cycleTimer} {startDir}");
        }

        // ---- SetNextPlayerFrame: normal tick, direction-change rebind,
        // ---- rebind suppression (goalie dive / injured), and the
        // ---- goal-cheer overlay (scorer / teammate / keeper-excluded) ----
        void RunNextFrameCase(string label, int slot, int installDir, int animTable,
            int? afterDir, int playerState, bool goalScored, int lastTeamScored,
            int lastPlayerScoredPtr, int tick)
        {
            Memory.Init(pcMode: true);
            int b = PlayerSprite.Base(slot);
            Memory.WriteWord(b + PlayerSprite.OffDirection, installDir);
            PlayerActions.SetPlayerAnimationTable(b, animTable); // startingDirection = installDir

            if (afterDir.HasValue)
                Memory.WriteWord(b + PlayerSprite.OffDirection, afterDir.Value);
            Memory.WriteByte(b + PlayerSprite.OffPlayerState, playerState);

            Memory.WriteWord(Memory.Addr.goalScored, goalScored ? 1 : 0);
            Memory.WriteWord(Memory.Addr.lastTeamScoredNumber, lastTeamScored);
            Memory.WriteDword(Memory.Addr.lastPlayerScored, lastPlayerScoredPtr);
            Memory.WriteWord(Memory.Addr.currentGameTick, tick);

            SpriteUpdate.SetNextPlayerFrame(b);

            short imageIndex = Memory.ReadSignedWord(b + PlayerSprite.OffImageIndex);
            short frameIndex = Memory.ReadSignedWord(b + PlayerSprite.OffFrameIndex);
            short cycleTimer = Memory.ReadSignedWord(b + PlayerSprite.OffCycleFramesTimer);
            short frameDelay = Memory.ReadSignedWord(b + PlayerSprite.OffFrameDelay);
            short fsCounter = Memory.ReadSignedWord(b + PlayerSprite.OffFrameSwitchCounter);
            int animTablePtr = Memory.ReadSignedDword(b + PlayerSprite.OffAnimTablePtr);
            int fitPtr = Memory.ReadSignedDword(b + PlayerSprite.OffFrameIndicesTable);
            short startDir = Memory.ReadSignedWord(b + PlayerSprite.OffStartingDirection);

            w.WriteLine($"NEXTFRAME {label} {slot} => "
                + $"{imageIndex} {frameIndex} {cycleTimer} {frameDelay} {fsCounter} {animTablePtr} {fitPtr} {startDir}");
        }

        // slot 1 = team1 outfielder (ordinal 2, teamNumber 1); slot 0 = team1
        // goalkeeper (ordinal 1, teamNumber 1) -- both from PlayerSprite.Init()'s
        // defaults after a fresh Memory.Init(true).
        RunNextFrameCase("normal_no_rebind", 1, 2, Memory.Addr.kPlayerRunningAnimTableAddr,
            afterDir: null, playerState: 0, goalScored: false, lastTeamScored: 0, lastPlayerScoredPtr: 0, tick: 0);
        RunNextFrameCase("direction_change_rebinds", 1, 2, Memory.Addr.kPlayerRunningAnimTableAddr,
            afterDir: 6, playerState: 0, goalScored: false, lastTeamScored: 0, lastPlayerScoredPtr: 0, tick: 0);
        RunNextFrameCase("goalie_diving_suppresses_rebind", 0, 0, Memory.Addr.kPlayerRunningAnimTableAddr,
            afterDir: 2, playerState: 6, goalScored: false, lastTeamScored: 0, lastPlayerScoredPtr: 0, tick: 0);
        RunNextFrameCase("injured_suppresses_rebind", 0, 0, Memory.Addr.kPlayerRunningAnimTableAddr,
            afterDir: 2, playerState: 13, goalScored: false, lastTeamScored: 0, lastPlayerScoredPtr: 0, tick: 0);
        RunNextFrameCase("scorer_cheers", 1, 0, Memory.Addr.kPlayerRunningAnimTableAddr,
            afterDir: null, playerState: 0, goalScored: true, lastTeamScored: 1,
            lastPlayerScoredPtr: PlayerSprite.Base(1), tick: 0); // tick&0x7F<=100 -> cheer
        RunNextFrameCase("teammate_cheers", 1, 0, Memory.Addr.kPlayerRunningAnimTableAddr,
            afterDir: null, playerState: 0, goalScored: true, lastTeamScored: 1,
            lastPlayerScoredPtr: PlayerSprite.Base(2), tick: 0); // (ord<<2+tick)&0x3F<=31 -> cheer
        RunNextFrameCase("keeper_never_cheers", 0, 0, Memory.Addr.kPlayerRunningAnimTableAddr,
            afterDir: null, playerState: 0, goalScored: true, lastTeamScored: 1,
            lastPlayerScoredPtr: PlayerSprite.Base(0), tick: 0);

        // ---- MoveAllPlayers: full 22 x 128-byte sprite pool, one tick,
        // ---- byte-exact (catches cross-slot interference, not just the
        // ---- already-tested per-function math) ----
        Memory.Init(pcMode: true);
        // slot 0: positive delta on both axes, Y reaches/overshoots this tick, X does not.
        PlayerSprite.SetX(0, 10 << 16);
        PlayerSprite.SetY(0, 20 << 16);
        PlayerSprite.SetDestX(0, 15);
        PlayerSprite.SetDestY(0, 20);
        PlayerSprite.SetDeltaX(0, 0x00010000);
        PlayerSprite.SetDeltaY(0, 0x00010000);
        // slot 1: X overshoots destination this tick.
        PlayerSprite.SetX(1, 14 << 16);
        PlayerSprite.SetDestX(1, 15);
        PlayerSprite.SetDeltaX(1, 0x00020000);
        // slot 11: negative delta, not reaching yet.
        PlayerSprite.SetX(11, 100 << 16);
        PlayerSprite.SetDestX(11, 90);
        PlayerSprite.SetDeltaX(11, -0x00010000);
        // slot 12: left stationary (delta 0, PlayerSprite.Init()'s default) --
        // a same-tick baseline amid the other three moving slots.

        SpriteUpdate.MoveAllPlayers();

        byte[] pool = Memory.View(PlayerSprite.SpritePoolBase, PlayerSprite.TotalSlots * PlayerSprite.SlotStride).ToArray();
        string poolPath = Path.Combine(outDir, "sprite_pool_after_moveall.bin");
        File.WriteAllBytes(poolPath, pool);

        Console.WriteLine($"wrote {calcCases.Length} CALC + {moveCases.Length} MOVE + {fixtures.Length} ANIM + "
            + $"{sprDirCases.Length} SPRDIR + {setAnimCases.Length} SETANIM + 7 NEXTFRAME scenarios to {path}, "
            + $"and {pool.Length} bytes to {poolPath}");
    }
}
