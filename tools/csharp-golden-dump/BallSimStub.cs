// Minimal stand-in for OpenSwos.Sim.BallSim, needed so GameTime.cs (step 10)
// can compile here without pulling in the rest of BallState.cs (470 lines --
// an entirely different, idiomatic ball-physics simulation layer built on
// a custom `Fixed`/`BallState`/`PlayerInfluence` type family, NOT part of
// the SwosVm.Memory-based mechanical port family at all). GameTime.cs's
// InitPitchBallFactors reads exactly one member, BallSim.CurrentPitchType
// (comment-filtered-grep verified) -- copied VERBATIM (default value only)
// from the real openswos/game/scripts/Sim/BallState.cs:127.
//
// PHASE 1 BOOTSTRAP-COMPLETENESS FOLLOW-UP (2026-09-16): CurrentPitchNumber
// added, needed by the newly-included Pitch.cs -- also copied VERBATIM
// (default value only) from BallState.cs:132.
namespace OpenSwos.Sim;

public static class BallSim
{
    public static int CurrentPitchType = 4;  // Normal (default in swos.ini)
    public static int CurrentPitchNumber = 0;
}
