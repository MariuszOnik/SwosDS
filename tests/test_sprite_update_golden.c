// Step-3 differential test, per review request: verifies EVERY public
// function SpriteUpdate.cs exposes (plus the forward-pulled
// PlayerActions.SetPlayerAnimationTable) against a golden file/binary dump
// produced by the REAL C# (not just C self-consistency). Generalizes the
// byte-exact Memory.Init() golden-dump technique (test_golden_dump.c) to
// individual function inputs/outputs and, for MoveAllPlayers, an entire
// memory region.
//
// Coverage (function -> scenario tag -> what it exercises):
//   CalculateDeltaXAndY              -> CALC      -> all 8 movement octants,
//                                                     zero movement/speed,
//                                                     table-halving
//   MoveSprite                       -> MOVE      -> +/- deltas, overshoot
//                                                     vs. not, stationary
//   UpdateSpriteAnimation            -> ANIM      -> opcode interpreter
//                                                     (loop/hold/pause/jump)
//   UpdateSpriteDirectionAndDeltas   -> SPRDIR    -> movement + no-movement,
//                                                     full 0..255 direction
//                                                     AND 0..7 quantised
//   SetPlayerAnimationTable          -> SETANIM   -> team1/team2/goalkeeper,
//                                                     null-frame-pointer path
//   SetNextPlayerFrame               -> NEXTFRAME -> normal tick, direction-
//                                                     change rebind, rebind
//                                                     suppression (goalie
//                                                     dive / injured), and
//                                                     both goal-cheer paths
//   MoveAllPlayers (+ both private     MOVEALL    -> full 22x128-byte sprite
//     helpers, exercised indirectly)              -> pool, byte-exact, after
//                                                     one tick -- also
//                                                     covers
//                                                     StopSpriteIfReached-
//                                                     Destination and
//                                                     UpdateAnimationTable-
//                                                     AndDestinationReached,
//                                                     which have no direct
//                                                     C# entry point of
//                                                     their own to call
//
// Regenerate build/golden/sprite_update_golden.txt +
// sprite_pool_after_moveall.bin with:
//   cd tools/csharp-golden-dump && dotnet run -c Release -- ../../build/golden
//
// CalculateDeltaXAndY has no PC/Amiga branch (see swos_sprite_update.c's
// header comment) -- OpenSWOS hard-locks it to PC mode, so there is no
// second variant to differential-test here, unlike Memory.Init().
//
// The ANIM fixtures (small literal opcode streams) are plain test data, not
// OpenSWOS source -- they are intentionally duplicated (not derived from
// the golden file) in tools/csharp-golden-dump/SpriteUpdateGolden.cs and
// here, matching by position; only the golden file's ticks/expected-state
// columns are parsed and compared. Likewise, NEXTFRAME's per-label setup
// (installDir/afterDir/playerState/goalScored/...) is duplicated by label
// name rather than encoded in the golden file -- the label is the only
// thing that ties a golden line back to its C# setup.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "swos_addr.h"
#include "swos_ball_sprite.h"
#include "swos_memory.h"
#include "swos_player_actions.h"
#include "swos_player_sprite.h"
#include "swos_sprite_update.h"

#define GOLDEN_PATH "build/golden/sprite_update_golden.txt"
#define GOLDEN_POOL_PATH "build/golden/sprite_pool_after_moveall.bin"

static int g_failures = 0;
static int g_checked = 0;
static int g_animFixtureIdx = 0;

#define CHECK(cond, msg) \
    do { \
        g_checked++; \
        if (!(cond)) { \
            printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            g_failures++; \
        } \
    } while (0)

static void handleCalcLine(const char *line) {
    int speed, x, y, destX, destY, expDeltaX, expDeltaY, expDir;
    if (sscanf(line, "CALC %d %d %d %d %d => %d %d %d",
               &speed, &x, &y, &destX, &destY, &expDeltaX, &expDeltaY, &expDir) != 8) {
        printf("FAIL: could not parse CALC line: %s\n", line);
        g_failures++;
        return;
    }
    SwosDeltasAndAngle r = swosCalculateDeltaXAndY(speed, x, y, destX, destY);
    char msg[256];
    snprintf(msg, sizeof(msg), "CALC(speed=%d,x=%d,y=%d,destX=%d,destY=%d) matches C#",
             speed, x, y, destX, destY);
    CHECK(r.deltaX == expDeltaX && r.deltaY == expDeltaY && r.direction == expDir, msg);
}

static void handleMoveLine(const char *line) {
    int xRaw, destX, deltaXRaw, yRaw, destY, deltaYRaw;
    int expX, expDeltaX, expY, expDeltaY;
    if (sscanf(line, "MOVE %d %d %d %d %d %d => %d %d %d %d",
               &xRaw, &destX, &deltaXRaw, &yRaw, &destY, &deltaYRaw,
               &expX, &expDeltaX, &expY, &expDeltaY) != 10) {
        printf("FAIL: could not parse MOVE line: %s\n", line);
        g_failures++;
        return;
    }
    swosMemoryInitStub();
    swosBallSpriteSetX(xRaw);
    swosBallSpriteSetDestX((int16_t)destX);
    swosBallSpriteSetDeltaX(deltaXRaw);
    swosBallSpriteSetY(yRaw);
    swosBallSpriteSetDestY((int16_t)destY);
    swosBallSpriteSetDeltaY(deltaYRaw);

    swosMoveSprite(BALLSPR_BASE);

    char msg[256];
    snprintf(msg, sizeof(msg), "MOVE(xRaw=%d,destX=%d,deltaXRaw=%d,yRaw=%d,destY=%d,deltaYRaw=%d) matches C#",
             xRaw, destX, deltaXRaw, yRaw, destY, deltaYRaw);
    CHECK(swosBallSpriteX() == expX && swosBallSpriteDeltaX() == expDeltaX
              && swosBallSpriteY() == expY && swosBallSpriteDeltaY() == expDeltaY,
          msg);
}

// Same literal fixtures as SpriteUpdateGolden.cs, in the same order -- see
// this file's header comment for why duplicating small test fixtures (not
// an algorithm) across languages is fine here.
static const int16_t kFixture0[] = { 10, 20, -999 };
static const int16_t kFixture1[] = { 30, -101 };
static const int16_t kFixture2[] = { 40, -50, 50, -999 };
static const int16_t kFixture3[] = { 100, 110, 120, -102, -999 };
static const struct { const int16_t *data; int len; } kFixtures[] = {
    { kFixture0, 3 }, { kFixture1, 2 }, { kFixture2, 4 }, { kFixture3, 5 },
};
#define SCRATCH_STREAM_ADDR 0x20000

static void handleAnimLine(const char *line) {
    const char *bracketEnd = strchr(line, ']');
    int ticks, expImageIndex, expFrameIndex, expFrameDelay, expCycleTimer, expFsCounter;
    if (!bracketEnd || sscanf(bracketEnd + 1, "%d => %d %d %d %d %d",
                               &ticks, &expImageIndex, &expFrameIndex, &expFrameDelay,
                               &expCycleTimer, &expFsCounter) != 6) {
        printf("FAIL: could not parse ANIM line: %s\n", line);
        g_failures++;
        return;
    }

    if (g_animFixtureIdx >= (int)(sizeof(kFixtures) / sizeof(kFixtures[0]))) {
        printf("FAIL: more ANIM golden lines than known fixtures\n");
        g_failures++;
        return;
    }
    const int16_t *stream = kFixtures[g_animFixtureIdx].data;
    int len = kFixtures[g_animFixtureIdx].len;

    swosMemoryInitStub();
    for (int i = 0; i < len; i++)
        swosWriteWord(SCRATCH_STREAM_ADDR + i * 2, (uint16_t)stream[i]);
    swosWriteWord(BALLSPR_BASE + 84, 1); // OffOnScreen (shared Sprite struct layout)
    swosBallSpriteSetFrameIndicesTable(SCRATCH_STREAM_ADDR);
    swosBallSpriteSetFrameIndex(-1);
    swosBallSpriteSetFrameDelay(1);
    swosBallSpriteSetCycleFramesTimer(1);
    swosWriteWord(BALLSPR_BASE + 28, (uint16_t)-1); // OffFrameSwitchCounter

    for (int t = 0; t < ticks; t++)
        swosUpdateSpriteAnimation(BALLSPR_BASE);

    int16_t fsCounter = swosReadSignedWord(BALLSPR_BASE + 28);
    char msg[256];
    snprintf(msg, sizeof(msg), "ANIM fixture %d (%d ticks) matches C#", g_animFixtureIdx, ticks);
    CHECK(swosBallSpriteImageIndex() == expImageIndex && swosBallSpriteFrameIndex() == expFrameIndex
              && swosBallSpriteFrameDelay() == expFrameDelay
              && swosBallSpriteCycleFramesTimer() == expCycleTimer && fsCounter == expFsCounter,
          msg);
    g_animFixtureIdx++;
}

static void handleSprDirLine(const char *line) {
    int x, y, destX, destY, speed;
    int expDeltaX, expDeltaY, expFullDir, expDir8;
    if (sscanf(line, "SPRDIR %d %d %d %d %d => %d %d %d %d",
               &x, &y, &destX, &destY, &speed,
               &expDeltaX, &expDeltaY, &expFullDir, &expDir8) != 9) {
        printf("FAIL: could not parse SPRDIR line: %s\n", line);
        g_failures++;
        return;
    }
    swosMemoryInitStub();
    swosBallSpriteSetXPixels((int16_t)x);
    swosBallSpriteSetYPixels((int16_t)y);
    swosBallSpriteSetDestX((int16_t)destX);
    swosBallSpriteSetDestY((int16_t)destY);
    swosBallSpriteSetSpeed((int16_t)speed);

    swosUpdateSpriteDirectionAndDeltas(BALLSPR_BASE);

    char msg[256];
    snprintf(msg, sizeof(msg), "SPRDIR(x=%d,y=%d,destX=%d,destY=%d,speed=%d) matches C#",
             x, y, destX, destY, speed);
    CHECK(swosBallSpriteDeltaX() == expDeltaX && swosBallSpriteDeltaY() == expDeltaY
              && swosBallSpriteFullDirection() == expFullDir && swosBallSpriteDirection() == expDir8,
          msg);
}

static void handleSetAnimLine(const char *line) {
    int slot, animTable, direction;
    int expAnimTablePtr, expFrameDelay, expFitPtr, expFsCounter, expFrameIndex, expCycleTimer, expStartDir;
    if (sscanf(line, "SETANIM %d %d %d => %d %d %d %d %d %d %d",
               &slot, &animTable, &direction, &expAnimTablePtr, &expFrameDelay, &expFitPtr,
               &expFsCounter, &expFrameIndex, &expCycleTimer, &expStartDir) != 10) {
        printf("FAIL: could not parse SETANIM line: %s\n", line);
        g_failures++;
        return;
    }
    swosMemoryInit(true);
    int b = swosPlayerSpriteBase(slot);
    swosWriteWord(b + PLSPR_OFF_DIRECTION, (uint16_t)direction);
    // Seed a known "before" state so the null-pointer early-return path's
    // PRESERVED fields are verifiable, not coincidentally 0.
    swosWriteWord(b + PLSPR_OFF_FRAME_INDEX, 77);
    swosWriteWord(b + PLSPR_OFF_CYCLE_FRAMES_TIMER, 77);
    swosWriteWord(b + PLSPR_OFF_FRAME_SWITCH_COUNTER, 77);
    swosWriteWord(b + PLSPR_OFF_STARTING_DIRECTION, 77);

    swosSetPlayerAnimationTable(b, animTable);

    int animTablePtr = swosReadSignedDword(b + PLSPR_OFF_ANIM_TABLE_PTR);
    int16_t frameDelay = swosReadSignedWord(b + PLSPR_OFF_FRAME_DELAY);
    int fitPtr = swosReadSignedDword(b + PLSPR_OFF_FRAME_INDICES_TABLE);
    int16_t fsCounter = swosReadSignedWord(b + PLSPR_OFF_FRAME_SWITCH_COUNTER);
    int16_t frameIndex = swosReadSignedWord(b + PLSPR_OFF_FRAME_INDEX);
    int16_t cycleTimer = swosReadSignedWord(b + PLSPR_OFF_CYCLE_FRAMES_TIMER);
    int16_t startDir = swosReadSignedWord(b + PLSPR_OFF_STARTING_DIRECTION);

    char msg[256];
    snprintf(msg, sizeof(msg), "SETANIM(slot=%d,animTable=%d,direction=%d) matches C#", slot, animTable, direction);
    CHECK(animTablePtr == expAnimTablePtr && frameDelay == expFrameDelay && fitPtr == expFitPtr
              && fsCounter == expFsCounter && frameIndex == expFrameIndex && cycleTimer == expCycleTimer
              && startDir == expStartDir,
          msg);
}

static void handleNextFrameLine(const char *line) {
    char label[64];
    int slot;
    int expImageIndex, expFrameIndex, expCycleTimer, expFrameDelay, expFsCounter, expAnimTablePtr, expFitPtr, expStartDir;
    if (sscanf(line, "NEXTFRAME %63s %d => %d %d %d %d %d %d %d %d",
               label, &slot, &expImageIndex, &expFrameIndex, &expCycleTimer, &expFrameDelay,
               &expFsCounter, &expAnimTablePtr, &expFitPtr, &expStartDir) != 10) {
        printf("FAIL: could not parse NEXTFRAME line: %s\n", line);
        g_failures++;
        return;
    }

    // Setup mirrors SpriteUpdateGolden.cs's RunNextFrameCase exactly, keyed
    // by label (the golden file's inputs aren't otherwise self-describing
    // enough to replay generically -- see the 8-tuple RunNextFrameCase
    // signature in the C# source for the parameters each label implies).
    int installDir = 0, afterDir = -1000 /* sentinel: no change */, playerState = 0;
    bool goalScored = false;
    int lastTeamScored = 0, lastPlayerScoredSlot = -1, tick = 0;
    int animTable = ADDR_kPlayerRunningAnimTableAddr;

    if (strcmp(label, "normal_no_rebind") == 0) {
        installDir = 2;
    } else if (strcmp(label, "direction_change_rebinds") == 0) {
        installDir = 2; afterDir = 6;
    } else if (strcmp(label, "goalie_diving_suppresses_rebind") == 0) {
        installDir = 0; afterDir = 2; playerState = 6;
    } else if (strcmp(label, "injured_suppresses_rebind") == 0) {
        installDir = 0; afterDir = 2; playerState = 13;
    } else if (strcmp(label, "scorer_cheers") == 0) {
        installDir = 0; goalScored = true; lastTeamScored = 1; lastPlayerScoredSlot = 1;
    } else if (strcmp(label, "teammate_cheers") == 0) {
        installDir = 0; goalScored = true; lastTeamScored = 1; lastPlayerScoredSlot = 2;
    } else if (strcmp(label, "keeper_never_cheers") == 0) {
        installDir = 0; goalScored = true; lastTeamScored = 1; lastPlayerScoredSlot = 0;
    } else {
        printf("FAIL: unknown NEXTFRAME label in golden file: %s\n", label);
        g_failures++;
        return;
    }

    swosMemoryInit(true);
    int b = swosPlayerSpriteBase(slot);
    swosWriteWord(b + PLSPR_OFF_DIRECTION, (uint16_t)installDir);
    swosSetPlayerAnimationTable(b, animTable); // startingDirection = installDir

    if (afterDir != -1000)
        swosWriteWord(b + PLSPR_OFF_DIRECTION, (uint16_t)afterDir);
    swosWriteByte(b + PLSPR_OFF_PLAYER_STATE, (uint8_t)playerState);

    swosWriteWord(ADDR_goalScored, goalScored ? 1 : 0);
    swosWriteWord(ADDR_lastTeamScoredNumber, (uint16_t)lastTeamScored);
    swosWriteDword(ADDR_lastPlayerScored, (uint32_t)(lastPlayerScoredSlot >= 0 ? swosPlayerSpriteBase(lastPlayerScoredSlot) : 0));
    swosWriteWord(ADDR_currentGameTick, (uint16_t)tick);

    swosSetNextPlayerFrame(b);

    int16_t imageIndex = swosReadSignedWord(b + PLSPR_OFF_IMAGE_INDEX);
    int16_t frameIndex = swosReadSignedWord(b + PLSPR_OFF_FRAME_INDEX);
    int16_t cycleTimer = swosReadSignedWord(b + PLSPR_OFF_CYCLE_FRAMES_TIMER);
    int16_t frameDelay = swosReadSignedWord(b + PLSPR_OFF_FRAME_DELAY);
    int16_t fsCounter = swosReadSignedWord(b + PLSPR_OFF_FRAME_SWITCH_COUNTER);
    int animTablePtr = swosReadSignedDword(b + PLSPR_OFF_ANIM_TABLE_PTR);
    int fitPtr = swosReadSignedDword(b + PLSPR_OFF_FRAME_INDICES_TABLE);
    int16_t startDir = swosReadSignedWord(b + PLSPR_OFF_STARTING_DIRECTION);

    char msg[256];
    snprintf(msg, sizeof(msg), "NEXTFRAME %s (slot %d) matches C#", label, slot);
    CHECK(imageIndex == expImageIndex && frameIndex == expFrameIndex && cycleTimer == expCycleTimer
              && frameDelay == expFrameDelay && fsCounter == expFsCounter && animTablePtr == expAnimTablePtr
              && fitPtr == expFitPtr && startDir == expStartDir,
          msg);
}

// Full-pool differential test for MoveAllPlayers: replicates
// SpriteUpdateGolden.cs's setup exactly (4 slots given interesting
// position/delta state, the other 18 left at PlayerSprite.Init()'s
// defaults), runs one tick, and byte-compares the ENTIRE 22x128-byte
// sprite pool against the C#-produced dump -- catches cross-slot
// interference (wrong SlotStride, off-by-one loop bounds) that a
// per-function unit test can't.
static void testMoveAllPlayers(void) {
    FILE *f = fopen(GOLDEN_POOL_PATH, "rb");
    if (!f) {
        fprintf(stderr, "FAIL: could not open %s -- run the C# golden-dump harness first: "
                         "cd tools/csharp-golden-dump && dotnet run -c Release -- ../../build/golden\n",
                GOLDEN_POOL_PATH);
        g_failures++;
        g_checked++;
        return;
    }
    long expectedLen = (long)PLSPR_TOTAL_SLOTS * PLSPR_SLOT_STRIDE;
    uint8_t *golden = malloc((size_t)expectedLen);
    long got = (long)fread(golden, 1, (size_t)expectedLen, f);
    fclose(f);
    if (got != expectedLen) {
        printf("FAIL: %s is %ld bytes, expected %ld\n", GOLDEN_POOL_PATH, got, expectedLen);
        g_failures++;
        g_checked++;
        free(golden);
        return;
    }

    swosMemoryInit(true);
    swosPlayerSpriteSetX(0, 10 << 16);
    swosPlayerSpriteSetY(0, 20 << 16);
    swosPlayerSpriteSetDestX(0, 15);
    swosPlayerSpriteSetDestY(0, 20);
    swosPlayerSpriteSetDeltaX(0, 0x00010000);
    swosPlayerSpriteSetDeltaY(0, 0x00010000);
    swosPlayerSpriteSetX(1, 14 << 16);
    swosPlayerSpriteSetDestX(1, 15);
    swosPlayerSpriteSetDeltaX(1, 0x00020000);
    swosPlayerSpriteSetX(11, 100 << 16);
    swosPlayerSpriteSetDestX(11, 90);
    swosPlayerSpriteSetDeltaX(11, -0x00010000);

    swosMoveAllPlayers();

    const uint8_t *ours = swosMemoryView(PLSPR_SPRITE_POOL_BASE, (int)expectedLen);
    int mismatches = 0, firstMismatch = -1;
    for (long i = 0; i < expectedLen; i++) {
        if (ours[i] != golden[i]) {
            if (firstMismatch < 0) firstMismatch = (int)i;
            mismatches++;
            if (mismatches <= 20)
                printf("  diff @ pool+0x%04lX (slot %ld, offset %ld): C=0x%02X golden(C#)=0x%02X\n",
                       i, i / PLSPR_SLOT_STRIDE, i % PLSPR_SLOT_STRIDE, ours[i], golden[i]);
        }
    }
    free(golden);

    g_checked++;
    if (mismatches == 0) {
        printf("ok:   MoveAllPlayers: full %ld-byte sprite pool matches C# after one tick\n", expectedLen);
    } else {
        printf("FAIL: MoveAllPlayers: %d mismatched byte(s) in sprite pool, first at +0x%04X\n",
               mismatches, firstMismatch);
        g_failures++;
    }
}

int main(void) {
    FILE *f = fopen(GOLDEN_PATH, "r");
    if (!f) {
        fprintf(stderr, "FAIL: could not open %s -- run the C# golden-dump harness first: "
                         "cd tools/csharp-golden-dump && dotnet run -c Release -- ../../build/golden\n",
                GOLDEN_PATH);
        printf("\n1 check(s) FAILED\n");
        return 1;
    }

    int calcCount = 0, moveCount = 0, animCount = 0, sprDirCount = 0, setAnimCount = 0, nextFrameCount = 0;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "CALC", 4) == 0) { handleCalcLine(line); calcCount++; }
        else if (strncmp(line, "MOVE", 4) == 0) { handleMoveLine(line); moveCount++; }
        else if (strncmp(line, "ANIM", 4) == 0) { handleAnimLine(line); animCount++; }
        else if (strncmp(line, "SPRDIR", 6) == 0) { handleSprDirLine(line); sprDirCount++; }
        else if (strncmp(line, "SETANIM", 7) == 0) { handleSetAnimLine(line); setAnimCount++; }
        else if (strncmp(line, "NEXTFRAME", 9) == 0) { handleNextFrameLine(line); nextFrameCount++; }
    }
    fclose(f);

    testMoveAllPlayers();

    printf("checked %d CALC, %d MOVE, %d ANIM, %d SPRDIR, %d SETANIM, %d NEXTFRAME scenarios, 1 MOVEALL\n",
           calcCount, moveCount, animCount, sprDirCount, setAnimCount, nextFrameCount);

    if (g_failures || g_checked == 0) {
        printf("\n%d check(s) FAILED (%d checked)\n", g_failures, g_checked);
        return 1;
    }
    printf("\nall %d checks passed\n", g_checked);
    return 0;
}
