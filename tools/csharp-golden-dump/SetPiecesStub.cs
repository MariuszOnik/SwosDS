// SetPieces throw-stub, needed so UpdatePlayers.cs (step 7B) can compile
// here without pulling in the rest of SetPieces.cs (step 10, not otherwise
// touched). Deliberately throws rather than no-oping -- matches the 6A/6B/
// 7B boundary convention (see AiStub.cs): entering a not-yet-ported
// SetPieces path is a hard failure on both sides of the differential test,
// not a silently-faked gameplay outcome. Every scenario in
// UpdatePlayersGolden.cs avoids the throw-in state/branch, so these are
// never actually invoked.
namespace OpenSwos.Sim.Port;

public static class SetPieces
{
    public static void SetThrowInPlayerDestinationCoordinates(int spriteAddr) =>
        throw new InvalidOperationException("SetPieces belongs to step 10");

    public static void TickThrowIn(int throwerSpriteAddr, int ballSpriteAddr, int teamBase) =>
        throw new InvalidOperationException("SetPieces belongs to step 10");
}
