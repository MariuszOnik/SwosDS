// Minimal stand-in for OpenSwos.Sim.Port.PlayerActions, needed ONLY so
// SpriteUpdate.cs (which calls PlayerActions.SetPlayerAnimationTable) can
// compile here without pulling in the rest of PlayerActions.cs's dependency
// graph (BallUpdate, PlayerControlled, PlayerEnergy, MatchAudio -- steps
// 4/5/6, not ported/verified yet).
//
// This is NOT simplified/approximated logic: SetPlayerAnimationTable below
// is copied VERBATIM from the real
// openswos/game/scripts/Sim/Port/PlayerActions.cs:2054-2104 (the same
// source swos-vm-c/src/swos_player_actions.c independently ports to C).
// Its own correctness is therefore out of scope for this differential test
// -- it exists purely so SpriteUpdate.cs's OWN logic (the thing being
// tested here) can run against a real, byte-truthful dependency instead of
// an approximation. When PlayerActions.cs itself is ported/verified (step
// 5), delete this file and add the real one to golden-dump.csproj instead.
namespace OpenSwos.Sim.Port;

using OpenSwos.SwosVm;

public static class PlayerActions
{
    public static void SetPlayerAnimationTable(int a1PlayerAddr, int a0AnimTable)
    {
        Memory.WriteDword(a1PlayerAddr + PlayerSprite.OffAnimTablePtr, a0AnimTable);

        short teamNum = Memory.ReadSignedWord(a1PlayerAddr + PlayerSprite.OffTeamNumber);
        short d0Index = (short)(teamNum - 1);
        short ord = Memory.ReadSignedWord(a1PlayerAddr + PlayerSprite.OffPlayerOrdinal);
        if (ord == 1)
        {
            d0Index = (short)(d0Index + 2);
        }

        d0Index = (short)(d0Index << 3);
        short pDir = Memory.ReadSignedWord(a1PlayerAddr + PlayerSprite.OffDirection);
        d0Index = (short)(d0Index + pDir);
        int d0Off = (d0Index & 0xFFFF) << 2;

        short frameDelay = Memory.ReadSignedWord(a0AnimTable);
        Memory.WriteWord(a1PlayerAddr + PlayerSprite.OffFrameDelay, frameDelay);

        int fitPtr = Memory.ReadSignedDword(a0AnimTable + 2 + d0Off);
        Memory.WriteDword(a1PlayerAddr + PlayerSprite.OffFrameIndicesTable, fitPtr);

        if (fitPtr == 0)
        {
            return;
        }

        Memory.WriteWord(a1PlayerAddr + PlayerSprite.OffFrameSwitchCounter, -1);
        Memory.WriteWord(a1PlayerAddr + PlayerSprite.OffFrameIndex, -1);
        Memory.WriteWord(a1PlayerAddr + PlayerSprite.OffCycleFramesTimer, 1);
        Memory.WriteWord(a1PlayerAddr + PlayerSprite.OffStartingDirection, pDir);
    }
}
