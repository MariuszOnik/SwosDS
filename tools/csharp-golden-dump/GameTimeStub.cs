// Minimal stand-in for OpenSwos.Sim.Port.GameTime, needed so AiBrain.cs
// (step 9) can compile here without pulling in the rest of GameTime.cs
// (1736 lines, match-clock orchestration -- a different layer, its own
// future step). AiBrain.cs calls exactly one member, GameTime.AmigaModeActive()
// (comment-filtered-grep verified) -- copied VERBATIM from the real
// openswos/game/scripts/Sim/Port/GameTime.cs:770.
namespace OpenSwos.Sim.Port;

public static class GameTime
{
    public static bool AmigaModeActive() => false;
}
