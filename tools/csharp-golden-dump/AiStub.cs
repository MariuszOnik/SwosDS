// AiBrain/AiHelpers throw-stubs, needed so PlayerControlled.cs (step 6A)
// could compile here without pulling in AiBrain.cs/AiHelpers.cs before step
// 9 landed. All of this file's step-6A scenarios are human-team paths, so
// these were never actually invoked even while wired in.
//
// (Formerly PlayerControlledDepsStub.cs, which also carried PlayerHeader/
// PlayerTackle stand-ins for step 6A -- both files are now ported in full
// (step 7A) and referenced for real in golden-dump.csproj instead.)
//
// SUPERSEDED as of step 9: AiHelpers.cs/AiBrain.cs are now compiled in for
// real (golden-dump.csproj), which define the real AiBrain/AiHelpers
// classes -- this file is excluded from compilation there
// (`<Compile Remove="AiStub.cs" />`) to avoid a duplicate-definition error.
// Left in place (not deleted) as the historical record of the 6A/6B
// boundary this step closed.
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
