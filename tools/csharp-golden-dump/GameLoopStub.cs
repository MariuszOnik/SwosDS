// Minimal stand-in for OpenSwos.Sim.Port.GameLoop, needed so GameTime.cs's
// NextPenalty (step 10) can compile here without pulling in the rest of
// GameLoop.cs (~1900+ lines -- the full per-tick orchestrator, its own
// future porting step, 11). NextPenalty calls exactly one member,
// GameLoop.PlayersLeavingPitch() (comment-filtered-grep verified) -- copied
// VERBATIM from the real
// openswos/game/scripts/Sim/Port/GameLoop.cs:1860-1888 (same source
// swos-vm-c/src/swos_game_loop.c independently ports to C).
namespace OpenSwos.Sim.Port;

using OpenSwos.SwosVm;

public static class GameLoop
{
    private const short kStPlayersGoingToShower = 24;
    private const short kStStopped              = 101;

    public static void PlayersLeavingPitch()
    {
        Memory.WriteWord(Memory.Addr.hideBall, 0);
        Memory.WriteWord(Memory.Addr.stoppageEventTimer, 275);
        Memory.WriteWord(Memory.Addr.gameState, kStPlayersGoingToShower);
        Memory.WriteWord(Memory.Addr.breakCameraMode, -1);
        Memory.WriteWord(Memory.Addr.gameStatePl, kStStopped);
        Memory.WriteWord(Memory.Addr.gameNotInProgressCounterWriteOnly, 0);
        Memory.WriteWord(Memory.Addr.cameraDirection, -1);
        Memory.WriteDword(Memory.Addr.lastTeamPlayedBeforeBreak, TeamData.TopBase);
        Memory.WriteWord(Memory.Addr.stoppageTimerTotal, 0);
        Memory.WriteWord(Memory.Addr.stoppageTimerActive, 0);
        TeamPort.StopAllPlayers();
        Memory.WriteWord(Memory.Addr.cameraXVelocity, 0);
        Memory.WriteWord(Memory.Addr.cameraYVelocity, 0);
        Memory.WriteWord(Memory.Addr.stateGoal, 0);
    }
}
