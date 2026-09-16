// Minimal stand-in for OpenSwos.Sim.Port.TeamDataLoader, needed so
// PlayerActions.cs (which references TeamDataLoader.PlayerInfoSize and its
// Off* skill offsets) can compile here without pulling in the real
// TeamDataLoader.WritePlayerInfos/WireTeamFields -- those pull in
// OpenSwos.Assets.TeamRecord/PlayerRecord (team-file parsing),
// SkillScaling.cs, TeamPort.cs, and a Godot.GD.Print call, none of which
// this headless harness has (or needs -- see
// swos-vm-c/include/swos_team_data_loader.h for the full "why" this is a
// deliberate, documented scope boundary, not an approximation of anything
// PlayerActions.cs itself does).
//
// Every constant below is copied VERBATIM from the real
// openswos/game/scripts/Sim/Port/TeamDataLoader.cs:1-40.
namespace OpenSwos.Sim.Port;

public static class TeamDataLoader
{
    public const int PlayerInfoSize = 61;

    public const int OffPassing      = 27;
    public const int OffShooting     = 28;
    public const int OffHeading      = 29;
    public const int OffTackling     = 30;
    public const int OffBallControl  = 31;
    public const int OffSpeed        = 32;
    public const int OffFinishing    = 33;
}
