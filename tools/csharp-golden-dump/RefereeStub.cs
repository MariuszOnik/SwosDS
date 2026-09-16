// Minimal stand-in for OpenSwos.Sim.Port.Referee, needed so PlayerTackle.cs
// (which calls Referee.ActivateReferee(), the only Referee member it uses --
// grep-verified) can compile here without pulling in the rest of
// Referee.cs's per-tick referee-movement/card-animation state machine
// (UpdateReferee and friends, ~700 more lines -- a different layer, not
// called from anything ported so far; see swos_referee.h for the full
// scoping rationale).
//
// ActivateReferee + its private helpers (InitRefereeAnimationTable,
// MarkDisplaySpritesDirty, SwosRand) and RefereeSprite copied VERBATIM
// from the real openswos/game/scripts/Sim/Port/Referee.cs:41-99, 141-190,
// 563-593, 625-629, 688-706 -- the same source swos-vm-c/src/swos_referee.c
// independently ports to C. Its own correctness is therefore out of scope
// for this differential test -- it exists purely so PlayerTackle.cs's OWN
// logic (the thing being tested here) can run against a real, byte-truthful
// dependency instead of an approximation.
//
// Debug/telemetry counters (DbgActivations etc.) are omitted -- verified
// zero Memory effect, same as the C port.
namespace OpenSwos.Sim.Port;

using OpenSwos.SwosVm;

public static class Referee
{
    // Real Referee.cs:136-139 -- pure C#-side telemetry (DbgEnteredAboutToGive++
    // only), zero Memory effect. UpdatePlayers.cs (step 7B) calls this from
    // CheckIfThisPlayerGettingBooked; the counter itself is omitted on the C
    // side too (see swos_update_players.c) -- kept here as a true no-op so
    // this stub's behaviour matches what the omission assumes.
    public static void NotifyEnteredAboutToGiveCard() { }

    public static void ActivateReferee()
    {
        short foulX = Memory.ReadSignedWord(Memory.Addr.foulXCoordinate);
        short foulY = Memory.ReadSignedWord(Memory.Addr.foulYCoordinate);

        Memory.WriteWord(RefereeSprite.Base + RefereeSprite.OffDestX, foulX + 28);
        Memory.WriteWord(RefereeSprite.Base + RefereeSprite.OffDestY, foulY + 5);

        int xOffset = SwosRand() / 8;
        if (foulX >= 336) // kPitchCenterX
            xOffset = -xOffset;

        int cameraY = Camera.GetCameraYWhole();
        int refStartY = cameraY - 20;
        if (foulY <= 449) // kPitchCenterY
            refStartY = cameraY + 215;

        int destX = Memory.ReadSignedWord(RefereeSprite.Base + RefereeSprite.OffDestX);
        Memory.WriteWord(RefereeSprite.Base + RefereeSprite.OffX + 2, destX + xOffset);
        Memory.WriteWord(RefereeSprite.Base + RefereeSprite.OffY + 2, refStartY);

        Memory.WriteWord(RefereeSprite.Base + RefereeSprite.OffSpeed, 1024); // kRefereeSpeed

        Memory.WriteWord(RefereeSprite.Base + RefereeSprite.OffVisible, 1);

        MarkDisplaySpritesDirty();
        InitRefereeAnimationTable(Memory.Addr.refComingAnimTable);

        Memory.WriteWord(Memory.Addr.refState, 1); // kRefIncoming
    }

    private static void InitRefereeAnimationTable(int animTableAddr)
    {
        short delay = Memory.ReadSignedWord(animTableAddr);
        short direction = Memory.ReadSignedWord(RefereeSprite.Base + RefereeSprite.OffDirection);

        int frameTablePtr = Memory.ReadSignedDword(animTableAddr + 2 + direction * 4);

        Memory.WriteWord(RefereeSprite.Base + RefereeSprite.OffFrameDelay, delay);
        Memory.WriteDword(RefereeSprite.Base + RefereeSprite.OffFrameIndicesTable, frameTablePtr);

        Memory.WriteWord(RefereeSprite.Base + RefereeSprite.OffFrameSwitchCounter, -1);
        Memory.WriteWord(RefereeSprite.Base + RefereeSprite.OffFrameIndex, -1);
        Memory.WriteWord(RefereeSprite.Base + RefereeSprite.OffCycleFramesTimer, 1);
    }

    private static void MarkDisplaySpritesDirty()
    {
        Memory.WriteWord(Memory.Addr.displaySpritesDirtyFlag, 1);
    }

    private static int SwosRand() => Rng.NextByte();
}

public static class RefereeSprite
{
    public const int Base = 0x4FD00;

    public const int OffPlayerState         = PlayerSprite.OffPlayerState;
    public const int OffPlayerDownTimer     = PlayerSprite.OffPlayerDownTimer;
    public const int OffFrameIndicesTable   = PlayerSprite.OffFrameIndicesTable;
    public const int OffFrameIndex          = PlayerSprite.OffFrameIndex;
    public const int OffFrameDelay          = PlayerSprite.OffFrameDelay;
    public const int OffCycleFramesTimer    = PlayerSprite.OffCycleFramesTimer;
    public const int OffFrameSwitchCounter  = PlayerSprite.OffFrameSwitchCounter;
    public const int OffX                   = PlayerSprite.OffX;
    public const int OffY                   = PlayerSprite.OffY;
    public const int OffZ                   = PlayerSprite.OffZ;
    public const int OffDirection           = PlayerSprite.OffDirection;
    public const int OffSpeed               = PlayerSprite.OffSpeed;
    public const int OffDeltaX              = PlayerSprite.OffDeltaX;
    public const int OffDeltaY              = PlayerSprite.OffDeltaY;
    public const int OffDestX               = PlayerSprite.OffDestX;
    public const int OffDestY               = PlayerSprite.OffDestY;
    public const int OffVisible             = PlayerSprite.OffVisible;
    public const int OffImageIndex          = PlayerSprite.OffImageIndex;
    public const int OffOnScreen            = PlayerSprite.OffOnScreen;
}
