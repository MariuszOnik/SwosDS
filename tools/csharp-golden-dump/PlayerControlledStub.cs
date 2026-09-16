// Minimal stand-in for OpenSwos.Sim.Port.PlayerControlled, needed so
// PlayerActions.cs (which calls PlayerControlled.IncSkillDuelOwnWin/
// IncSkillDuelOppWin) can compile here without pulling in the rest of
// PlayerControlled.cs (1881 lines, step 6 -- not ported/verified yet).
//
// Both methods copied VERBATIM from the real
// openswos/game/scripts/Sim/Port/PlayerControlled.cs:151-152 -- one-line
// static counters with zero Memory effect (confirmed by reading the whole
// file during step 5 scoping), used only for the --swos-smoke dev
// diagnostic. See swos-vm-c/include/swos_player_actions.h for why these
// are omitted (not stubbed-as-approximation) from the C port itself.
namespace OpenSwos.Sim.Port;

public static class PlayerControlled
{
    private static int s_skillDuelOwnWin, s_skillDuelOppWin;

    public static void IncSkillDuelOwnWin() { s_skillDuelOwnWin++; }
    public static void IncSkillDuelOppWin() { s_skillDuelOppWin++; }
}
