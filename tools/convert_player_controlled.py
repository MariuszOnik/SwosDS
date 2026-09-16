"""Mechanical PlayerControlled.cs -> C body converter (step 6A).

The source is intentionally register-style C# with gotos.  This script keeps
the control flow and comments intact and only translates the small language/API
surface.  The generated file is compiled and differential-tested; do not edit
it by hand without updating this converter.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT.parent / "openswos/game/scripts/Sim/Port/PlayerControlled.cs"
OUTPUT = ROOT / "src/swos_player_controlled.c"

text = SOURCE.read_text(encoding="utf-8")
start = text.index("    public static Exit RunControlledBranch")
body = text[start:]
body = body[:body.rfind("\n}")]  # class closing brace

# Dedent the class body once.
body = "\n".join(line[4:] if line.startswith("    ") else line
                 for line in body.splitlines())

replacements = {
    "public static Exit RunControlledBranch": "SwosPlayerControlledExit swosRunControlledBranch",
    "public static void RunPassReceiptTrigger": "void swosRunPassReceiptTrigger",
    "public static void RunPassExpectingBranch": "void swosRunPassExpectingBranch",
    "private static void RunPassChaseTail": "static void runPassChaseTail",
    "private static void RunChaseBall": "static void runChaseBall",
    "private static void ApplyKBallFrictionOrFallback": "static void applyKBallFrictionOrFallback",
    "private static readonly short[] s_kBallFriction = new short[]": "static const int16_t kBallFriction[] =",
    "RunPassReceiptTrigger(": "swosRunPassReceiptTrigger(",
    "RunPassChaseTail(": "runPassChaseTail(",
    "RunChaseBall(": "runChaseBall(",
    "ApplyKBallFrictionOrFallback(": "applyKBallFrictionOrFallback(",
    "s_kBallFriction": "kBallFriction",
    "Exit.UpdateSpeed": "SWOS_PC_UPDATE_SPEED",
    "Exit.StopPlayer": "SWOS_PC_STOP_PLAYER",
    "Memory.ReadSignedWord(": "swosReadSignedWord(",
    "Memory.ReadWord(": "swosReadWord(",
    "Memory.ReadByte(": "swosReadByte(",
    "Memory.ReadSignedDword(": "swosReadSignedDword(",
    "Memory.WriteWord(": "swosWriteWord(",
    "Memory.WriteByte(": "swosWriteByte(",
    "Memory.WriteDword(": "swosWriteDword(",
    "Memory.Addr.": "ADDR_",
    "TeamData.TopBase": "TEAMDATA_TOP_BASE",
    "TeamData.BottomBase": "TEAMDATA_BOTTOM_BASE",
    "TeamData.Base(topTeam)": "swosTeamDataBase(topTeam)",
    "TeamData.Base(!topTeam)": "swosTeamDataBase(!topTeam)",
    "BallSprite.Base": "BALLSPR_BASE",
    "BallSprite.Speed": "swosBallSpriteSpeed()",
    "BallSprite.Direction": "swosBallSpriteDirection()",
    "BallSprite.FullDirection": "swosBallSpriteFullDirection()",
    "PlayerActions.CalculateIfPlayerWinsBall(": "swosCalculateIfPlayerWinsBall(",
    "PlayerActions.DoPass(": "swosDoPass(",
    "PlayerActions.PlayerKickingBall(": "swosPlayerKickingBall(",
    "PlayerActions.UpdateControllingPlayer(": "swosUpdateControllingPlayer(",
    "PlayerActions.UpdatePlayerWithBall(": "swosUpdatePlayerWithBall(",
    "PlayerActions.UpdatePlayerSpeedAndFrameDelay(": "swosUpdatePlayerSpeedAndFrameDelay(",
    "PlayerActions.FaithfulBallControl": "g_swosFaithfulBallControl",
    "PlayerUpdate.GoalkeeperClaimedTheBall(": "swosGoalkeeperClaimedTheBall(",
    "PlayerUpdate.UpdateBallWithControllingGoalkeeper(": "swosUpdateBallWithControllingGoalkeeper(",
    "PlayerHeader.AttemptStaticHeader(": "swosAttemptStaticHeader(",
    "PlayerHeader.PlayerAttemptingJumpHeader(": "swosPlayerAttemptingJumpHeader(",
    "PlayerTackle.PlayerBeginTackling(": "swosPlayerBeginTackling(",
    "AiBrain.SetControlsDirection(": "requireAiSetControlsDirection(",
    "AiHelpers.AI_Kick(": "requireAiKick(",
    "s_fireFallbackTop++": "g_swosPcTelemetry.fireFallbackTop++",
    "s_fireFallbackBot++": "g_swosPcTelemetry.fireFallbackBottom++",
    "s_enterCseg80FFD++": "g_swosPcTelemetry.enterCseg80FFD++",
    "s_reachedCwbCall++": "g_swosPcTelemetry.reachedCwbCall++",
    "s_pinControlledPerTick++": "g_swosPcTelemetry.pinControlledPerTick++",
}
for old, new in replacements.items():
    body = body.replace(old, new)

body = body.replace("swosswosRunPassReceiptTrigger", "swosRunPassReceiptTrigger")

for name in ["X", "Y", "DestX", "DestY", "FullDirection", "PlayerOrdinal"]:
    snake = re.sub(r"(?<!^)(?=[A-Z])", "_", name).upper()
    body = body.replace(f"PlayerSprite.Off{name}", f"PLSPR_OFF_{snake}")

# TeamData offset names map to the already-established C constants.
for name in [
    "OpponentsTeam", "PlayerNumber", "ControlledPlayer", "PassToPlayerPtr",
    "CurrentAllowedDirection", "FireThisFrame", "HeaderOrTackle", "Shooting",
    "PlVeryCloseToBall", "PlCloseToBall", "PassingBall", "PassingToPlayer",
    "PassingKickingPlayer", "PassKickTimer", "PlayerSwitchTimer", "LongPass",
    "LeftSpin", "RightSpin", "BallCanBeControlled", "BallX", "BallY",
    "GoalkeeperPlaying", "GoaliePlayingOrOut", "BallOutOfPlayOrKeeper",
]:
    snake = re.sub(r"(?<!^)(?=[A-Z])", "_", name).upper()
    body = body.replace(f"TeamData.Off{name}", f"TEAMDATA_OFF_{snake}")

# C scalar types/casts. Order matters for uint/ushort.
body = re.sub(r"\bushort\b", "uint16_t", body)
body = re.sub(r"\bsbyte\b", "int8_t", body)
body = re.sub(r"\bshort\b", "int16_t", body)
body = re.sub(r"\bbyte\b", "uint8_t", body)
body = re.sub(r"\buint\b", "uint32_t", body)

# C# permits implicit integer -> ushort in the Memory API; C uses explicit
# fixed-width parameters but ordinary integer expressions are intentionally
# left to the same low-bit truncation.
body = body.replace("(uint16_t)(int16_t)", "(uint16_t)(int16_t)")

header = r'''// GENERATED by tools/convert_player_controlled.py from the real
// OpenSWOS PlayerControlled.cs. Do not hand-edit the generated control flow.
#include "swos_player_controlled.h"

#include <assert.h>
#include <stdint.h>

#include "swos_addr.h"
#include "swos_ai_brain.h"
#include "swos_ai_helpers.h"
#include "swos_ball_sprite.h"
#include "swos_memory.h"
#include "swos_player_actions.h"
#include "swos_player_header.h"
#include "swos_player_sprite.h"
#include "swos_player_tackle.h"
#include "swos_player_update.h"
#include "swos_team_data.h"

enum {
    OffOpponentsTeam = 0, OffPlayerNumber = 4, OffControlledPlayer = 32,
    OffPassToPlayerPtr = 36, OffPlayerHasBall = 40, OffAllowedDirections = 42,
    OffCurrentAllowedDir = 44, OffQuickFire = 48, OffNormalFire = 49,
    OffFirePressed = 50, OffFireThisFrame = 51, OffHeaderOrTackle = 52,
    OffShooting = 58, OffPlVeryCloseToBall = 61, OffPlCloseToBall = 62,
    OffPlNotFarFromBall = 63, OffBallLessEqual4 = 64, OffBall4To8 = 65,
    OffBall8To12 = 66, OffBall12To17 = 67, OffBallAbove17 = 68,
    OffPrevPlVeryClose = 69, OffLastHeadingPlayer = 72, OffGoalieSavedTimer = 76,
    OffPassingToPlayer = 90, OffBallInPlay = 94, OffBallOutOfPlay = 96,
    OffPassKickTimer = 102, OffPassingKickingPlayer = 104, OffOfs108 = 108,
    OffBallCanBeControlled = 110, OffBallCtrlPlDirection = 112,
    OffSpinTimer = 118, OffWonTheBallTimer = 138,
    SOffPlayerOrdinal = 2, SOffX = 32, SOffY = 36, SOffDirection = 42,
    SOffShooting = 58, SOffDestX = 58, SOffDestY = 60,
    SOffBallDistance = 74, SOffZWhole = 40,
};

SwosPlayerControlledTelemetry g_swosPcTelemetry;
// Step 9 closes the 6A/6B boundary: these hooks are now statically wired to
// the real AiBrain.SetControlsDirection / AiHelpers.AI_Kick implementations
// (not left NULL for an assert to catch) -- kept as function pointers,
// rather than direct calls, because that's the stable call point 6A/7B
// already established and other code (tests, future overrides) may still
// want to substitute a different AI at this seam.
SwosAiSetControlsDirectionHook g_swosAiSetControlsDirectionHook = swosAiBrainSetControlsDirection;
SwosAiKickHook g_swosAiKickHook = swosAiHelpersAiKick;
bool g_swosFaithfulBallControl = true;

static void runPassChaseTail(int spriteAddr, int teamBase);
static void runChaseBall(int spriteAddr, int teamBase);
static void applyKBallFrictionOrFallback(int spriteAddr, int teamBase);

void swosPlayerControlledResetTelemetry(void) {
    g_swosPcTelemetry = (SwosPlayerControlledTelemetry){0};
}

void swosPlayerControlledIncSkillDuelOwnWin(void) {
    g_swosPcTelemetry.skillDuelOwnWin++;
}

void swosPlayerControlledIncSkillDuelOppWin(void) {
    g_swosPcTelemetry.skillDuelOppWin++;
}

static void requireAiSetControlsDirection(int teamBase) {
    // Defensive only, post-step-9: the hook is always wired (see the
    // static initializer above). Kept as a named call site for readability
    // and so a test can still null the hook out deliberately.
    assert(g_swosAiSetControlsDirectionHook != 0);
    if (g_swosAiSetControlsDirectionHook)
        g_swosAiSetControlsDirectionHook(teamBase);
}

static void requireAiKick(int spriteAddr, int teamBase) {
    assert(g_swosAiKickHook != 0);
    if (g_swosAiKickHook)
        g_swosAiKickHook(spriteAddr, teamBase);
}

'''

OUTPUT.write_text(header + body + "\n", encoding="utf-8", newline="\n")
print(f"wrote {OUTPUT} ({len((header + body).splitlines())} lines)")
