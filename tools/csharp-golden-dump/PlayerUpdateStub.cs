// Minimal stand-in for OpenSwos.Sim.Port.PlayerUpdate, needed ONLY so
// BallUpdate.cs (which calls PlayerUpdate.UpdateBallWithControllingGoalkeeper)
// can compile here without pulling in the rest of PlayerUpdate.cs's 1553
// lines / further dependency graph (dive counters, goalkeeperClaimedTheBall,
// etc. -- a later porting step).
//
// UpdateBallWithControllingGoalkeeper below is copied VERBATIM from the real
// openswos/game/scripts/Sim/Port/PlayerUpdate.cs:58-92 (the same source
// swos-vm-c/src/swos_player_update.c independently ports to C). Its own
// correctness is therefore out of scope for the BallUpdate differential
// test -- it exists purely so BallUpdate.cs's OWN logic (the thing being
// tested here) can run against a real, byte-truthful dependency instead of
// an approximation. When PlayerUpdate.cs itself is ported/verified, delete
// this file and add the real one to golden-dump.csproj instead.
namespace OpenSwos.Sim.Port;

using OpenSwos.SwosVm;

public static class PlayerUpdate
{
    public static void UpdateBallWithControllingGoalkeeper(int controllingPlayerAddr)
    {
        int dir = Memory.ReadSignedWord(controllingPlayerAddr + PlayerSprite.OffDirection);
        int byteOffset = dir << 2;

        int playerX = Memory.ReadSignedWord(controllingPlayerAddr + PlayerSprite.OffX + 2);
        int playerY = Memory.ReadSignedWord(controllingPlayerAddr + PlayerSprite.OffY + 2);

        int offsX = Memory.ReadSignedWord(Memory.Addr.kBallPlOffsetsBase + byteOffset);
        int offsY = Memory.ReadSignedWord(Memory.Addr.kBallPlOffsetsBase + byteOffset + 2);

        short newX = (short)(playerX + offsX);
        short newY = (short)(playerY + offsY);

        BallSprite.Speed = 0;
        BallSprite.XPixels = newX;
        BallSprite.YPixels = newY;
        BallSprite.DestX = newX;
        BallSprite.DestY = newY;

        int dz = BallSprite.DeltaZ;
        int dzHalf = dz >> 1;
        if (dzHalf > 0) dzHalf = -dzHalf;
        BallSprite.DeltaZ = dzHalf;

        BallUpdate.ResetBothTeamSpinTimers();
    }
}
