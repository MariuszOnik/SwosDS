// Minimal stand-in for OpenSwos.Sim.Port.Bench, needed so InputControls.cs
// (step 8) can compile here without pulling in the rest of Bench.cs (1868
// lines, the substitutes-menu UI/state machine -- a different layer, not
// called from anything ported so far). InputControls.UpdateTeamControls
// calls exactly one member, Bench.InBench() (comment-filtered-grep
// verified) -- copied VERBATIM from the real
// openswos/game/scripts/Sim/Port/Bench.cs:254-255.
namespace OpenSwos.Sim.Port;

using OpenSwos.SwosVm;

public static class Bench
{
    public static bool InBench()
        => Memory.ReadSignedWord(Memory.Addr.g_inSubstitutesMenu) != 0;
}
