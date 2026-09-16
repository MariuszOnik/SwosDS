// AiBrain/AiHelpers throw-stubs, needed so PlayerControlled.cs (step 6A)
// can compile here without pulling in AiBrain.cs/AiHelpers.cs (step 9, not
// otherwise touched). Deliberately throws rather than no-oping -- matches
// the 6A/6B boundary: entering a CPU-AI path before step 9 is a hard
// failure on both sides of the differential test, not a silently-faked
// gameplay outcome. All of this file's scenarios are human-team paths, so
// these are never actually invoked.
//
// (Formerly PlayerControlledDepsStub.cs, which also carried PlayerHeader/
// PlayerTackle stand-ins for step 6A -- both files are now ported in full
// (step 7A) and referenced for real in golden-dump.csproj instead.)
namespace OpenSwos.Sim.Port;

public static class AiBrain
{
    public static void SetControlsDirection(int teamBase) =>
        throw new InvalidOperationException("AiBrain belongs to step 9");
}

public static class AiHelpers
{
    public static void AI_Kick(int spriteAddr, int teamBase) =>
        throw new InvalidOperationException("AiHelpers belongs to step 9");
}
