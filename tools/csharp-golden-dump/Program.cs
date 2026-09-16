// Runs OpenSWOS's real Memory.Init(pcMode) and dumps the full memory buffer
// to a binary file, for byte-exact comparison against the C port's output.
// See golden-dump.csproj for why this harness exists.
using OpenSwos.SwosVm;

if (args.Length != 1)
{
    Console.Error.WriteLine("usage: golden-dump <output-directory>");
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

return 0;
