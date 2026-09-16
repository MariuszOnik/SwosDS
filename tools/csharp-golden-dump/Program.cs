// Runs OpenSWOS's real Memory.Init(pcMode) and dumps the full memory buffer
// to a binary file, for byte-exact comparison against the C port's output.
// See golden-dump.csproj for why this harness exists.
using OpenSwos.SwosVm;

// Phase 1 lockstep modes (see Step12IntegrationGolden.cs's RunLockstepLog/
// DumpFullAtTick and ../../README.md "Status: Phase 1"). Kept as extra
// dispatch branches in this same entry point rather than a second project
// so both share the exact same OpenSwos source references.
if (args.Length >= 1 && args[0] == "--lockstep-log")
{
    if (args.Length != 4)
    {
        Console.Error.WriteLine("usage: golden-dump --lockstep-log <seed> <maxTicks> <outPath>");
        return 1;
    }
    Step12IntegrationGolden.RunLockstepLog(args[3], int.Parse(args[1]), int.Parse(args[2]));
    return 0;
}
if (args.Length >= 1 && args[0] == "--lockstep-dump")
{
    if (args.Length != 4)
    {
        Console.Error.WriteLine("usage: golden-dump --lockstep-dump <seed> <tick> <outPath>");
        return 1;
    }
    Step12IntegrationGolden.DumpFullAtTick(args[3], int.Parse(args[1]), int.Parse(args[2]));
    return 0;
}

if (args.Length != 1)
{
    Console.Error.WriteLine("usage: golden-dump <output-directory>");
    Console.Error.WriteLine("       golden-dump --lockstep-log <seed> <maxTicks> <outPath>");
    Console.Error.WriteLine("       golden-dump --lockstep-dump <seed> <tick> <outPath>");
    return 1;
}

string outDir = args[0];
Directory.CreateDirectory(outDir);

const int memSize = 0x60000; // matches Memory's private kMemSize / our SWOS_MEM_SIZE

Memory.Init(pcMode: true);
byte[] pcDump = Memory.View(0, memSize).ToArray();
string pcPath = Path.Combine(outDir, "golden_pc.bin");
File.WriteAllBytes(pcPath, pcDump);
Console.WriteLine($"wrote {pcDump.Length} bytes to {pcPath}");

Memory.Init(pcMode: false);
byte[] amigaDump = Memory.View(0, memSize).ToArray();
string amigaPath = Path.Combine(outDir, "golden_amiga.bin");
File.WriteAllBytes(amigaPath, amigaDump);
Console.WriteLine($"wrote {amigaDump.Length} bytes to {amigaPath}");

SpriteUpdateGolden.Run(outDir);
BallUpdateGolden.Run(outDir);
PlayerActionsGolden.Run(outDir);
PlayerUpdateGolden.Run(outDir);
PlayerControlledGolden.Run(outDir);
Step7AGolden.Run(outDir);
Step7BGolden.Run(outDir);
Step8Golden.Run(outDir);
Step9Golden.Run(outDir);
Step10Golden.Run(outDir);
Step11AGolden.Run(outDir);
Step11BenchGolden.Run(outDir);
Step11GameLoopGolden.Run(outDir);
Step12IntegrationGolden.Run(outDir);

return 0;
