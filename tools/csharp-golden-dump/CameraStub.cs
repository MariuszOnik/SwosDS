// Minimal stand-in for OpenSwos.Sim.Port.Camera, needed so Referee.cs's
// ActivateReferee (the only Referee member PlayerTackle.cs calls) can
// compile here without pulling in the rest of Camera.cs (573 lines,
// moveCamera/SetCameraX/etc -- not called from ActivateReferee, not
// otherwise touched).
//
// GetCameraYWhole copied VERBATIM from the real
// openswos/game/scripts/Sim/Port/Camera.cs:124-127.
namespace OpenSwos.Sim.Port;

using OpenSwos.SwosVm;

public static class Camera
{
    public static int GetCameraY() => Memory.ReadSignedDword(Memory.Addr.cameraY);
    public static int GetCameraYWhole() => GetCameraY() >> 16;
}
