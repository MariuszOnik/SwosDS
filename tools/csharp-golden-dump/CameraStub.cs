// Minimal stand-in for OpenSwos.Sim.Port.Camera, needed so Referee.cs (both
// ActivateReferee, forward-pulled at step 7A, and now the rest of the file,
// step 10) can compile here without pulling in the rest of Camera.cs (573
// lines, moveCamera/SetCameraX/etc -- not called from anything Referee.cs
// touches, not otherwise touched).
//
// GetCameraYWhole/GetCameraXWhole copied VERBATIM from the real
// openswos/game/scripts/Sim/Port/Camera.cs:124-127 (Y) and its X mirror.
namespace OpenSwos.Sim.Port;

using OpenSwos.SwosVm;

public static class Camera
{
    public static int GetCameraY() => Memory.ReadSignedDword(Memory.Addr.cameraY);
    public static int GetCameraYWhole() => GetCameraY() >> 16;
    public static int GetCameraX() => Memory.ReadSignedDword(Memory.Addr.cameraX);
    public static int GetCameraXWhole() => GetCameraX() >> 16;
}
