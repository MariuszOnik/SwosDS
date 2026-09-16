// Stand-in for OpenSwos.Audio.MatchAudio, needed so BallUpdate.cs and
// BallOutOfPlay.cs (which call it directly) can compile here WITHOUT a
// Godot engine reference -- the real MatchAudio.cs is `partial class
// MatchAudio : Node` (a Godot scene node), which this headless console
// harness has no SDK for and, per this project's step-4 scope decision,
// deliberately doesn't need: every one of these calls is confirmed
// audio-only (`Instance?.DoX()` into a Godot audio player, zero Memory/
// game-state writes -- see swos-vm-c/include/swos_ball_update.h for the
// full rationale). The real MatchAudio.cs's own doc comment confirms this
// independently: "every public trigger is static and no-op safe when
// Instance is null (headless / harness / before a match)" -- Instance is
// never set outside a running Godot scene, so these calls already no-op
// exactly like this even in the real game. This stub just lets that happen
// without a Godot reference.
namespace OpenSwos.Audio;

public static class MatchAudio
{
    public static void PlayBounce() { }
    public static void PlayGoal() { }
    public static void PlayMissGoal() { }
    public static void PlayWhistle() { }
    public static void GoalComment() { }
    public static void OwnGoalComment() { }
    public static void NearMissComment() { }
    public static void PostHitComment() { }
    public static void BarHitComment() { }
    public static void EnqueueCorner() { }
    public static void EnqueueThrowIn() { }

    // Added for step 5 (PlayerActions.cs) -- same rationale as above, these
    // are the audio call sites PlayerActions.cs itself makes.
    public static void PlayKick() { }
    public static void GoodTackleComment() { }
    public static void HeaderComment() { }
    public static void CancelGoodPass() { }
    public static void EnqueueGoodPass() { }

    // Added for step 5.5 (PlayerUpdate.cs) -- same rationale as above.
    public static void KeeperClaimedComment() { }

    // Added for step 7A (PlayerTackle.cs) -- same rationale as above.
    public static void InjuryComment() { }
    public static void DangerousPlayComment() { }
    public static void PlayFoulWhistle() { }
    public static void PenaltyComment() { }
    public static void FoulComment() { }

    // Added for step 7B (UpdatePlayers.cs) -- same rationale as above.
    public static void KeeperSavedComment() { }

    // Added for step 10 (GameTime.cs/Referee.cs) -- same rationale as above.
    public static void PlayEndGameWhistle() { }
    public static void EnqueueRedCard() { }
    public static void EnqueueYellowCard() { }

    // Added for step 11 (Bench.cs) -- same rationale as above.
    public static void EnqueueSubstitute() { }
    public static void EnqueueTactics() { }
}
