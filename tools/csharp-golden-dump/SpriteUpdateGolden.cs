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

        Console.WriteLine($"wrote {calcCases.Length} CALC + {moveCases.Length} MOVE + {fixtures.Length} ANIM scenarios to {path}");
    }
}
