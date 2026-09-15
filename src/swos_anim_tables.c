// SOURCE: openswos game/scripts/SwosVm/AnimationTablesData.cs (full file)
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes. Call
// order inside swosAnimTablesInit() is preserved exactly (see header note).
#include "swos_anim_tables.h"
#include "swos_addr.h"
#include "swos_memory.h"
#include "generated/swos_anim_streams.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// All-zero direction slots -- used when an animation table only carries
// entries for a subset of the (player1/player2/goalie1/goalie2) groups.
static const int32_t s_Zero8[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

#define STREAM_LEN(a) ((int)(sizeof(a) / sizeof((a)[0])))

// ---- Bump-allocator state -------------------------------------------------
static int s_bumpCursor;

// Intern a frame-indices stream into Memory and return its absolute
// address. Each entry is a word (2 bytes). Aborts loudly if allocation runs
// past [kFrameIndicesArraysBase, kFrameIndicesArraysEnd) -- should never
// trigger, matches OpenSWOS's InvalidOperationException throw.
static int internStream(const int16_t *stream, int count) {
    int addr = s_bumpCursor;
    for (int i = 0; i < count; i++)
        swosWriteWord(addr + i * 2, (uint16_t)stream[i]);
    s_bumpCursor += count * 2;
    if (s_bumpCursor > ADDR_kFrameIndicesArraysEnd) {
        fprintf(stderr,
                "AnimationTablesData frame-indices arena overflow: cursor=0x%X, limit=0x%X\n",
                s_bumpCursor, ADDR_kFrameIndicesArraysEnd);
        abort();
    }
    return addr;
}

// Write a PlayerAnimationTable struct at `tableAddr`. Layout matches
// swos.asm:218920+ (frameDelay word, then 32 dword pointers).
static void writeTable(int tableAddr, int16_t frameDelay,
                        const int32_t team1Player[8], const int32_t team2Player[8],
                        const int32_t team1Goalie[8], const int32_t team2Goalie[8]) {
    swosWriteWord(tableAddr, (uint16_t)frameDelay);
    for (int i = 0; i < 8; i++) swosWriteDword(tableAddr + 2 + i * 4, (uint32_t)team1Player[i]);
    for (int i = 0; i < 8; i++) swosWriteDword(tableAddr + 34 + i * 4, (uint32_t)team2Player[i]);
    for (int i = 0; i < 8; i++) swosWriteDword(tableAddr + 66 + i * 4, (uint32_t)team1Goalie[i]);
    for (int i = 0; i < 8; i++) swosWriteDword(tableAddr + 98 + i * 4, (uint32_t)team2Goalie[i]);
}

// Write a RefereeAnimationTable struct at `tableAddr` (numCycles at +0, then
// one dword frame-stream pointer per direction 0..7 at +2 + dir*4).
static void writeRefTable(int tableAddr, int16_t numCycles, const int32_t dirPointers[8]) {
    swosWriteWord(tableAddr, (uint16_t)numCycles);
    for (int i = 0; i < 8; i++) swosWriteDword(tableAddr + 2 + i * 4, (uint32_t)dirPointers[i]);
}

void swosAnimTablesInit(void) {
    s_bumpCursor = ADDR_kFrameIndicesArraysBase;

    // --- Player running -------------------------------------------------
    int32_t runT1Pl[8] = {
        internStream(s_PlayerRunningUpTeam1, STREAM_LEN(s_PlayerRunningUpTeam1)),
        internStream(s_PlayerRunningUpRightTeam1, STREAM_LEN(s_PlayerRunningUpRightTeam1)),
        internStream(s_PlayerRunningRightTeam1, STREAM_LEN(s_PlayerRunningRightTeam1)),
        internStream(s_PlayerRunningDownRightTeam1, STREAM_LEN(s_PlayerRunningDownRightTeam1)),
        internStream(s_PlayerRunningDownTeam1, STREAM_LEN(s_PlayerRunningDownTeam1)),
        internStream(s_PlayerRunningDownLeftTeam1, STREAM_LEN(s_PlayerRunningDownLeftTeam1)),
        internStream(s_PlayerRunningLeftTeam1, STREAM_LEN(s_PlayerRunningLeftTeam1)),
        internStream(s_PlayerRunningUpLeftTeam1, STREAM_LEN(s_PlayerRunningUpLeftTeam1)),
    };
    int32_t runT2Pl[8] = {
        internStream(s_PlayerRunningUpTeam2, STREAM_LEN(s_PlayerRunningUpTeam2)),
        internStream(s_PlayerRunningUpRightTeam2, STREAM_LEN(s_PlayerRunningUpRightTeam2)),
        internStream(s_PlayerRunningRightTeam2, STREAM_LEN(s_PlayerRunningRightTeam2)),
        internStream(s_PlayerRunningDownRightTeam2, STREAM_LEN(s_PlayerRunningDownRightTeam2)),
        internStream(s_PlayerRunningDownTeam2, STREAM_LEN(s_PlayerRunningDownTeam2)),
        internStream(s_PlayerRunningDownLeftTeam2, STREAM_LEN(s_PlayerRunningDownLeftTeam2)),
        internStream(s_PlayerRunningLeftTeam2, STREAM_LEN(s_PlayerRunningLeftTeam2)),
        internStream(s_PlayerRunningUpLeftTeam2, STREAM_LEN(s_PlayerRunningUpLeftTeam2)),
    };
    int32_t runT1Gk[8] = {
        internStream(s_KeeperRunningUpTeam1, STREAM_LEN(s_KeeperRunningUpTeam1)),
        internStream(s_KeeperRunningUpRightTeam1, STREAM_LEN(s_KeeperRunningUpRightTeam1)),
        internStream(s_KeeperRunningRightTeam1, STREAM_LEN(s_KeeperRunningRightTeam1)),
        internStream(s_KeeperRunningDownRightTeam1, STREAM_LEN(s_KeeperRunningDownRightTeam1)),
        internStream(s_KeeperRunningDownTeam1, STREAM_LEN(s_KeeperRunningDownTeam1)),
        internStream(s_KeeperRunningDownLeftTeam1, STREAM_LEN(s_KeeperRunningDownLeftTeam1)),
        internStream(s_KeeperRunningLeftTeam1, STREAM_LEN(s_KeeperRunningLeftTeam1)),
        internStream(s_KeeperRunningUpLeftTeam1, STREAM_LEN(s_KeeperRunningUpLeftTeam1)),
    };
    int32_t runT2Gk[8] = {
        internStream(s_KeeperRunningUpTeam2, STREAM_LEN(s_KeeperRunningUpTeam2)),
        internStream(s_KeeperRunningUpRightTeam2, STREAM_LEN(s_KeeperRunningUpRightTeam2)),
        internStream(s_KeeperRunningRightTeam2, STREAM_LEN(s_KeeperRunningRightTeam2)),
        internStream(s_KeeperRunningDownRightTeam2, STREAM_LEN(s_KeeperRunningDownRightTeam2)),
        internStream(s_KeeperRunningDownTeam2, STREAM_LEN(s_KeeperRunningDownTeam2)),
        internStream(s_KeeperRunningDownLeftTeam2, STREAM_LEN(s_KeeperRunningDownLeftTeam2)),
        internStream(s_KeeperRunningLeftTeam2, STREAM_LEN(s_KeeperRunningLeftTeam2)),
        internStream(s_KeeperRunningUpLeftTeam2, STREAM_LEN(s_KeeperRunningUpLeftTeam2)),
    };
    writeTable(ADDR_kPlayerRunningAnimTableAddr, 5, runT1Pl, runT2Pl, runT1Gk, runT2Gk);

    // --- Player normal standing -----------------------------------------
    int32_t standT1Pl[8] = {
        internStream(s_StandTeam1Up, STREAM_LEN(s_StandTeam1Up)),
        internStream(s_StandTeam1UpRight, STREAM_LEN(s_StandTeam1UpRight)),
        internStream(s_StandTeam1Right, STREAM_LEN(s_StandTeam1Right)),
        internStream(s_StandTeam1BottomRight, STREAM_LEN(s_StandTeam1BottomRight)),
        internStream(s_StandTeam1Bottom, STREAM_LEN(s_StandTeam1Bottom)),
        internStream(s_StandTeam1BottomLeft, STREAM_LEN(s_StandTeam1BottomLeft)),
        internStream(s_StandTeam1Left, STREAM_LEN(s_StandTeam1Left)),
        internStream(s_StandTeam1TopLeft, STREAM_LEN(s_StandTeam1TopLeft)),
    };
    int32_t standT2Pl[8] = {
        internStream(s_StandTeam2Up, STREAM_LEN(s_StandTeam2Up)),
        internStream(s_StandTeam2UpRight, STREAM_LEN(s_StandTeam2UpRight)),
        internStream(s_StandTeam2Right, STREAM_LEN(s_StandTeam2Right)),
        internStream(s_StandTeam2BottomRight, STREAM_LEN(s_StandTeam2BottomRight)),
        internStream(s_StandTeam2Bottom, STREAM_LEN(s_StandTeam2Bottom)),
        internStream(s_StandTeam2BottomLeft, STREAM_LEN(s_StandTeam2BottomLeft)),
        internStream(s_StandTeam2Left, STREAM_LEN(s_StandTeam2Left)),
        internStream(s_StandTeam2TopLeft, STREAM_LEN(s_StandTeam2TopLeft)),
    };
    int32_t standT1Gk[8] = {
        internStream(s_StandKeeper1Up, STREAM_LEN(s_StandKeeper1Up)),
        internStream(s_StandKeeper1UpRight, STREAM_LEN(s_StandKeeper1UpRight)),
        internStream(s_StandKeeper1Right, STREAM_LEN(s_StandKeeper1Right)),
        internStream(s_StandKeeper1BottomRight, STREAM_LEN(s_StandKeeper1BottomRight)),
        internStream(s_StandKeeper1Bottom, STREAM_LEN(s_StandKeeper1Bottom)),
        internStream(s_StandKeeper1BottomLeft, STREAM_LEN(s_StandKeeper1BottomLeft)),
        internStream(s_StandKeeper1Left, STREAM_LEN(s_StandKeeper1Left)),
        internStream(s_StandKeeper1TopLeft, STREAM_LEN(s_StandKeeper1TopLeft)),
    };
    int32_t standT2Gk[8] = {
        internStream(s_StandKeeper2Up, STREAM_LEN(s_StandKeeper2Up)),
        internStream(s_StandKeeper2UpRight, STREAM_LEN(s_StandKeeper2UpRight)),
        internStream(s_StandKeeper2Right, STREAM_LEN(s_StandKeeper2Right)),
        internStream(s_StandKeeper2BottomRight, STREAM_LEN(s_StandKeeper2BottomRight)),
        internStream(s_StandKeeper2Bottom, STREAM_LEN(s_StandKeeper2Bottom)),
        internStream(s_StandKeeper2BottomLeft, STREAM_LEN(s_StandKeeper2BottomLeft)),
        internStream(s_StandKeeper2Left, STREAM_LEN(s_StandKeeper2Left)),
        internStream(s_StandKeeper2TopLeft, STREAM_LEN(s_StandKeeper2TopLeft)),
    };
    writeTable(ADDR_kPlayerStandingAnimTableAddr, 5, standT1Pl, standT2Pl, standT1Gk, standT2Gk);

    // --- Player tackling (only outfielders defined; goalies = 0) --------
    int32_t tackleT1Pl[8] = {
        internStream(s_Tackle1Up, STREAM_LEN(s_Tackle1Up)),
        internStream(s_Tackle1UpRight, STREAM_LEN(s_Tackle1UpRight)),
        internStream(s_Tackle1Right, STREAM_LEN(s_Tackle1Right)),
        internStream(s_Tackle1BottomRight, STREAM_LEN(s_Tackle1BottomRight)),
        internStream(s_Tackle1Bottom, STREAM_LEN(s_Tackle1Bottom)),
        internStream(s_Tackle1BottomLeft, STREAM_LEN(s_Tackle1BottomLeft)),
        internStream(s_Tackle1Left, STREAM_LEN(s_Tackle1Left)),
        internStream(s_Tackle1TopLeft, STREAM_LEN(s_Tackle1TopLeft)),
    };
    int32_t tackleT2Pl[8] = {
        internStream(s_Tackle2Up, STREAM_LEN(s_Tackle2Up)),
        internStream(s_Tackle2UpRight, STREAM_LEN(s_Tackle2UpRight)),
        internStream(s_Tackle2Right, STREAM_LEN(s_Tackle2Right)),
        internStream(s_Tackle2BottomRight, STREAM_LEN(s_Tackle2BottomRight)),
        internStream(s_Tackle2Bottom, STREAM_LEN(s_Tackle2Bottom)),
        internStream(s_Tackle2BottomLeft, STREAM_LEN(s_Tackle2BottomLeft)),
        internStream(s_Tackle2Left, STREAM_LEN(s_Tackle2Left)),
        internStream(s_Tackle2TopLeft, STREAM_LEN(s_Tackle2TopLeft)),
    };
    writeTable(ADDR_kPlTacklingAnimTableAddr, 5, tackleT1Pl, tackleT2Pl, s_Zero8, s_Zero8);

    // --- Player tackled (only outfielders) ------------------------------
    int32_t tackledT1Pl[8] = {
        internStream(s_Tackled1Up, STREAM_LEN(s_Tackled1Up)),
        internStream(s_Tackled1UpRight, STREAM_LEN(s_Tackled1UpRight)),
        internStream(s_Tackled1Right, STREAM_LEN(s_Tackled1Right)),
        internStream(s_Tackled1BottomRight, STREAM_LEN(s_Tackled1BottomRight)),
        internStream(s_Tackled1Bottom, STREAM_LEN(s_Tackled1Bottom)),
        internStream(s_Tackled1BottomLeft, STREAM_LEN(s_Tackled1BottomLeft)),
        internStream(s_Tackled1Left, STREAM_LEN(s_Tackled1Left)),
        internStream(s_Tackled1TopLeft, STREAM_LEN(s_Tackled1TopLeft)),
    };
    int32_t tackledT2Pl[8] = {
        internStream(s_Tackled2Up, STREAM_LEN(s_Tackled2Up)),
        internStream(s_Tackled2UpRight, STREAM_LEN(s_Tackled2UpRight)),
        internStream(s_Tackled2Right, STREAM_LEN(s_Tackled2Right)),
        internStream(s_Tackled2BottomRight, STREAM_LEN(s_Tackled2BottomRight)),
        internStream(s_Tackled2Bottom, STREAM_LEN(s_Tackled2Bottom)),
        internStream(s_Tackled2BottomLeft, STREAM_LEN(s_Tackled2BottomLeft)),
        internStream(s_Tackled2Left, STREAM_LEN(s_Tackled2Left)),
        internStream(s_Tackled2TopLeft, STREAM_LEN(s_Tackled2TopLeft)),
    };
    writeTable(ADDR_kPlayerTackledAnimTableAddr, 5, tackledT1Pl, tackledT2Pl, s_Zero8, s_Zero8);

    // --- Goalie catching ball (only goalies; players = 0) ---------------
    int32_t catch1A = internStream(s_KeeperCatch1A, STREAM_LEN(s_KeeperCatch1A));
    int32_t catch1B = internStream(s_KeeperCatch1B, STREAM_LEN(s_KeeperCatch1B));
    int32_t catch2A = internStream(s_KeeperCatch2A, STREAM_LEN(s_KeeperCatch2A));
    int32_t catch2B = internStream(s_KeeperCatch2B, STREAM_LEN(s_KeeperCatch2B));
    int32_t catchT1Gk[8] = { catch1A, catch1A, catch1A, catch1B, catch1B, catch1B, catch1B, catch1A };
    int32_t catchT2Gk[8] = { catch2A, catch2A, catch2A, catch2B, catch2B, catch2B, catch2B, catch2A };
    writeTable(ADDR_kGoalieCatchingBallAnimTableAddr, 5, s_Zero8, s_Zero8, catchT1Gk, catchT2Gk);

    // --- About to throw in (only outfielders) ---------------------------
    int32_t throwReadyT1Pl[8] = {
        internStream(s_ThrowInReady1Up, STREAM_LEN(s_ThrowInReady1Up)),
        internStream(s_ThrowInReady1UpRight, STREAM_LEN(s_ThrowInReady1UpRight)),
        internStream(s_ThrowInReady1Right, STREAM_LEN(s_ThrowInReady1Right)),
        internStream(s_ThrowInReady1BottomRight, STREAM_LEN(s_ThrowInReady1BottomRight)),
        internStream(s_ThrowInReady1Bottom, STREAM_LEN(s_ThrowInReady1Bottom)),
        internStream(s_ThrowInReady1BottomLeft, STREAM_LEN(s_ThrowInReady1BottomLeft)),
        internStream(s_ThrowInReady1Left, STREAM_LEN(s_ThrowInReady1Left)),
        internStream(s_ThrowInReady1TopLeft, STREAM_LEN(s_ThrowInReady1TopLeft)),
    };
    int32_t throwReadyT2Pl[8] = {
        internStream(s_ThrowInReady2Up, STREAM_LEN(s_ThrowInReady2Up)),
        internStream(s_ThrowInReady2UpRight, STREAM_LEN(s_ThrowInReady2UpRight)),
        internStream(s_ThrowInReady2Right, STREAM_LEN(s_ThrowInReady2Right)),
        internStream(s_ThrowInReady2BottomRight, STREAM_LEN(s_ThrowInReady2BottomRight)),
        internStream(s_ThrowInReady2Bottom, STREAM_LEN(s_ThrowInReady2Bottom)),
        internStream(s_ThrowInReady2BottomLeft, STREAM_LEN(s_ThrowInReady2BottomLeft)),
        internStream(s_ThrowInReady2Left, STREAM_LEN(s_ThrowInReady2Left)),
        internStream(s_ThrowInReady2TopLeft, STREAM_LEN(s_ThrowInReady2TopLeft)),
    };
    writeTable(ADDR_kAboutToThrowInAnimTableAddr, 5, throwReadyT1Pl, throwReadyT2Pl, s_Zero8, s_Zero8);

    // --- Throw-in pass (only outfielders) -------------------------------
    int32_t throwPassT1Pl[8] = {
        internStream(s_ThrowPass1Up, STREAM_LEN(s_ThrowPass1Up)),
        internStream(s_ThrowPass1UpRight, STREAM_LEN(s_ThrowPass1UpRight)),
        internStream(s_ThrowPass1Right, STREAM_LEN(s_ThrowPass1Right)),
        internStream(s_ThrowPass1BottomRight, STREAM_LEN(s_ThrowPass1BottomRight)),
        internStream(s_ThrowPass1Bottom, STREAM_LEN(s_ThrowPass1Bottom)),
        internStream(s_ThrowPass1BottomLeft, STREAM_LEN(s_ThrowPass1BottomLeft)),
        internStream(s_ThrowPass1Left, STREAM_LEN(s_ThrowPass1Left)),
        internStream(s_ThrowPass1TopLeft, STREAM_LEN(s_ThrowPass1TopLeft)),
    };
    int32_t throwPassT2Pl[8] = {
        internStream(s_ThrowPass2Up, STREAM_LEN(s_ThrowPass2Up)),
        internStream(s_ThrowPass2UpRight, STREAM_LEN(s_ThrowPass2UpRight)),
        internStream(s_ThrowPass2Right, STREAM_LEN(s_ThrowPass2Right)),
        internStream(s_ThrowPass2BottomRight, STREAM_LEN(s_ThrowPass2BottomRight)),
        internStream(s_ThrowPass2Bottom, STREAM_LEN(s_ThrowPass2Bottom)),
        internStream(s_ThrowPass2BottomLeft, STREAM_LEN(s_ThrowPass2BottomLeft)),
        internStream(s_ThrowPass2Left, STREAM_LEN(s_ThrowPass2Left)),
        internStream(s_ThrowPass2TopLeft, STREAM_LEN(s_ThrowPass2TopLeft)),
    };
    writeTable(ADDR_kThrowInPassAnimTableAddr, 5, throwPassT1Pl, throwPassT2Pl, s_Zero8, s_Zero8);

    // --- Throw-in kick (only outfielders) -------------------------------
    int32_t throwKickT1Pl[8] = {
        internStream(s_ThrowKick1Up, STREAM_LEN(s_ThrowKick1Up)),
        internStream(s_ThrowKick1UpRight, STREAM_LEN(s_ThrowKick1UpRight)),
        internStream(s_ThrowKick1Right, STREAM_LEN(s_ThrowKick1Right)),
        internStream(s_ThrowKick1BottomRight, STREAM_LEN(s_ThrowKick1BottomRight)),
        internStream(s_ThrowKick1Bottom, STREAM_LEN(s_ThrowKick1Bottom)),
        internStream(s_ThrowKick1BottomLeft, STREAM_LEN(s_ThrowKick1BottomLeft)),
        internStream(s_ThrowKick1Left, STREAM_LEN(s_ThrowKick1Left)),
        internStream(s_ThrowKick1TopLeft, STREAM_LEN(s_ThrowKick1TopLeft)),
    };
    int32_t throwKickT2Pl[8] = {
        internStream(s_ThrowKick2Up, STREAM_LEN(s_ThrowKick2Up)),
        internStream(s_ThrowKick2UpRight, STREAM_LEN(s_ThrowKick2UpRight)),
        internStream(s_ThrowKick2Right, STREAM_LEN(s_ThrowKick2Right)),
        internStream(s_ThrowKick2BottomRight, STREAM_LEN(s_ThrowKick2BottomRight)),
        internStream(s_ThrowKick2Bottom, STREAM_LEN(s_ThrowKick2Bottom)),
        internStream(s_ThrowKick2BottomLeft, STREAM_LEN(s_ThrowKick2BottomLeft)),
        internStream(s_ThrowKick2Left, STREAM_LEN(s_ThrowKick2Left)),
        internStream(s_ThrowKick2TopLeft, STREAM_LEN(s_ThrowKick2TopLeft)),
    };
    writeTable(ADDR_kThrowInKickAnimTableAddr, 5, throwKickT1Pl, throwKickT2Pl, s_Zero8, s_Zero8);

    // --- Goalie jumping (left/right + low/high) -------------------------
    int32_t jumpHighLeftT1Gk[8] = { 0, 0, internStream(s_Keeper1DiveHighLeftE, STREAM_LEN(s_Keeper1DiveHighLeftE)), 0, 0, 0, internStream(s_Keeper1DiveHighLeftW, STREAM_LEN(s_Keeper1DiveHighLeftW)), 0 };
    int32_t jumpHighLeftT2Gk[8] = { 0, 0, internStream(s_Keeper2DiveHighLeftE, STREAM_LEN(s_Keeper2DiveHighLeftE)), 0, 0, 0, internStream(s_Keeper2DiveHighLeftW, STREAM_LEN(s_Keeper2DiveHighLeftW)), 0 };
    writeTable(ADDR_kLeftGoalieJumpingHighAnimTableAddr, 5, s_Zero8, s_Zero8, jumpHighLeftT1Gk, jumpHighLeftT2Gk);
    int32_t jumpHighRightT1Gk[8] = { 0, 0, internStream(s_Keeper1DiveHighRightE, STREAM_LEN(s_Keeper1DiveHighRightE)), 0, 0, 0, internStream(s_Keeper1DiveHighRightW, STREAM_LEN(s_Keeper1DiveHighRightW)), 0 };
    int32_t jumpHighRightT2Gk[8] = { 0, 0, internStream(s_Keeper2DiveHighRightE, STREAM_LEN(s_Keeper2DiveHighRightE)), 0, 0, 0, internStream(s_Keeper2DiveHighRightW, STREAM_LEN(s_Keeper2DiveHighRightW)), 0 };
    writeTable(ADDR_kRightGoalieJumpingHighAnimTableAddr, 5, s_Zero8, s_Zero8, jumpHighRightT1Gk, jumpHighRightT2Gk);
    int32_t jumpLowLeftT1Gk[8] = { 0, 0, internStream(s_Keeper1DiveLowLeftE, STREAM_LEN(s_Keeper1DiveLowLeftE)), 0, 0, 0, internStream(s_Keeper1DiveLowLeftW, STREAM_LEN(s_Keeper1DiveLowLeftW)), 0 };
    int32_t jumpLowLeftT2Gk[8] = { 0, 0, internStream(s_Keeper2DiveLowLeftE, STREAM_LEN(s_Keeper2DiveLowLeftE)), 0, 0, 0, internStream(s_Keeper2DiveLowLeftW, STREAM_LEN(s_Keeper2DiveLowLeftW)), 0 };
    writeTable(ADDR_kLeftGoalieJumpingLowAnimTableAddr, 5, s_Zero8, s_Zero8, jumpLowLeftT1Gk, jumpLowLeftT2Gk);
    int32_t jumpLowRightT1Gk[8] = { 0, 0, internStream(s_Keeper1DiveLowRightE, STREAM_LEN(s_Keeper1DiveLowRightE)), 0, 0, 0, internStream(s_Keeper1DiveLowRightW, STREAM_LEN(s_Keeper1DiveLowRightW)), 0 };
    int32_t jumpLowRightT2Gk[8] = { 0, 0, internStream(s_Keeper2DiveLowRightE, STREAM_LEN(s_Keeper2DiveLowRightE)), 0, 0, 0, internStream(s_Keeper2DiveLowRightW, STREAM_LEN(s_Keeper2DiveLowRightW)), 0 };
    writeTable(ADDR_kRightGoalieJumpingLowAnimTableAddr, 5, s_Zero8, s_Zero8, jumpLowRightT1Gk, jumpLowRightT2Gk);

    // --- Static / jump header tables -------------------------------------
    // Re-uses the standing-table pointers (matching OpenSWOS: our port doesn't
    // read these yet, so re-using stand frames is safe and keeps the dword
    // pointers in-range) -- does NOT call internStream again.
    writeTable(ADDR_kStaticHeaderAttemptAnimTableAddr, 5, standT1Pl, standT2Pl, standT1Gk, standT2Gk);
    writeTable(ADDR_kStaticHeaderHitAnimTableAddr, 5, standT1Pl, standT2Pl, standT1Gk, standT2Gk);
    writeTable(ADDR_kJumpHeaderAttemptAnimTableAddr, 5, standT1Pl, standT2Pl, standT1Gk, standT2Gk);
    writeTable(ADDR_kJumpHeaderHitAnimTableAddr, 5, standT1Pl, standT2Pl, standT1Gk, standT2Gk);

    // --- Injured player (only outfielders; goalies = 0) ------------------
    int32_t inj1TopLeft = internStream(s_Injured1TopLeft, STREAM_LEN(s_Injured1TopLeft));
    int32_t inj1TopRight = internStream(s_Injured1TopRight, STREAM_LEN(s_Injured1TopRight));
    int32_t inj2TopLeft = internStream(s_Injured2TopLeft, STREAM_LEN(s_Injured2TopLeft));
    int32_t inj2TopRight = internStream(s_Injured2TopRight, STREAM_LEN(s_Injured2TopRight));
    int32_t injuredT1Pl[8] = { inj1TopLeft, inj1TopLeft, inj1TopLeft, inj1TopLeft,
                                inj1TopRight, inj1TopRight, inj1TopRight, inj1TopRight };
    int32_t injuredT2Pl[8] = { inj2TopLeft, inj2TopLeft, inj2TopLeft, inj2TopLeft,
                                inj2TopRight, inj2TopRight, inj2TopRight, inj2TopRight };
    writeTable(ADDR_kPlInjuredAnimTableAddr, 5, injuredT1Pl, injuredT2Pl, s_Zero8, s_Zero8);

    // --- Referee animation tables -----------------------------------------
    int32_t refComingA = internStream(s_RefComingA, STREAM_LEN(s_RefComingA));
    int32_t refComingB = internStream(s_RefComingB, STREAM_LEN(s_RefComingB));
    int32_t refWaiting = internStream(s_RefWaiting, STREAM_LEN(s_RefWaiting));
    int32_t refYellow = internStream(s_RefYellowCard, STREAM_LEN(s_RefYellowCard));
    int32_t refRed = internStream(s_RefRedCard, STREAM_LEN(s_RefRedCard));
    int32_t refSecond = internStream(s_RefSecondYellow, STREAM_LEN(s_RefSecondYellow));
    // refComingAnimTable -- dirs 0-2,7 walk one way (A), dirs 3-6 the other (B).
    int32_t refComingDirs[8] = { refComingA, refComingA, refComingA, refComingB,
                                  refComingB, refComingB, refComingB, refComingA };
    writeRefTable(ADDR_refComingAnimTable, 5, refComingDirs);
    // refWaitingAnimTable -- standing, all directions.
    int32_t refWaitingDirs[8] = { refWaiting, refWaiting, refWaiting, refWaiting,
                                   refWaiting, refWaiting, refWaiting, refWaiting };
    writeRefTable(ADDR_refWaitingAnimTable, 5, refWaitingDirs);
    // refYellowCardAnimTable.
    int32_t refYellowDirs[8] = { refYellow, refYellow, refYellow, refYellow,
                                  refYellow, refYellow, refYellow, refYellow };
    writeRefTable(ADDR_refYellowCardAnimTable, 5, refYellowDirs);
    // refRedCardAnimTable.
    int32_t refRedDirs[8] = { refRed, refRed, refRed, refRed, refRed, refRed, refRed, refRed };
    writeRefTable(ADDR_refRedCardAnimTable, 5, refRedDirs);
    // refSecondYellowAnimTable.
    int32_t refSecondDirs[8] = { refSecond, refSecond, refSecond, refSecond,
                                  refSecond, refSecond, refSecond, refSecond };
    writeRefTable(ADDR_refSecondYellowAnimTable, 5, refSecondDirs);
}
