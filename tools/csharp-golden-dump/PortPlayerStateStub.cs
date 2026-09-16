// Minimal stand-in for OpenSwos.Sim.Port's `PortPlayerState` enum, needed
// so PlayerUpdate.cs (which compares PlayerSprite.OffPlayerState bytes
// against it) can compile here without pulling in the rest of
// UpdatePlayers.cs (4300+ lines, step 7, not otherwise touched).
//
// Copied VERBATIM from the real
// openswos/game/scripts/Sim/Port/UpdatePlayers.cs:4330-4348.
namespace OpenSwos.Sim.Port;

public enum PortPlayerState : byte
{
    kNormal             = 0,
    kTackling           = 1,
    kTackled            = 3,
    kGoalieCatchingBall = 4,
    kThrowIn            = 5,
    kGoalieDivingHigh   = 6,
    kGoalieDivingLow    = 7,
    kStaticHeader       = 8,
    kJumpHeader         = 9,
    kDown               = 10,
    kGoalieClaimed      = 11,
    kBooked             = 12,
    kInjured            = 13,
    kSad                = 14,
    kHappy              = 15,
    kUnknown            = 255,
}
