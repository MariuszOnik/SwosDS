// Minimal stand-in for OpenSwos.Sim.Port.Bench, needed so InputControls.cs
// (step 8), and now Camera.cs/SpinningLogo.cs (step 11), can compile here
// without pulling in the rest of Bench.cs (1868 lines, the substitutes-menu
// UI/state machine -- a different layer, not called from anything ported
// so far -- own future step). InBench()/InBenchMenus()/GetBenchState() are
// the only members called (comment-filtered-grep verified) -- copied
// VERBATIM from the real openswos/game/scripts/Sim/Port/Bench.cs:254-255,
// 264-266, 271-274 (the same source swos_bench.c independently ports to C).
namespace OpenSwos.Sim.Port;

using OpenSwos.SwosVm;

public static class Bench
{
    public static bool InBench()
        => Memory.ReadSignedWord(Memory.Addr.g_inSubstitutesMenu) != 0;

    public const int kBenchStateInitial = 0;

    public static int GetBenchState()
        => Memory.ReadSignedWord(Memory.Addr.m_benchState);

    public static bool InBenchMenus()
        => InBench() && GetBenchState() == kBenchStateInitial;
}
