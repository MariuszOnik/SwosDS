// Step-3 differential test, per review request: "positive and negative
// deltas, stopping, directions" for SpriteUpdate.cs's ported functions,
// verified against a golden file produced by the REAL C# (not just C
// self-consistency). Generalizes the byte-exact Memory.Init() golden-dump
// technique (test_golden_dump.c) to individual function inputs/outputs.
//
// Regenerate build/golden/sprite_update_golden.txt with:
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
// columns are parsed and compared.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "swos_ball_sprite.h"
#include "swos_memory.h"
#include "swos_sprite_update.h"

#define GOLDEN_PATH "build/golden/sprite_update_golden.txt"

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

int main(void) {
    FILE *f = fopen(GOLDEN_PATH, "r");
    if (!f) {
        fprintf(stderr, "FAIL: could not open %s -- run the C# golden-dump harness first: "
                         "cd tools/csharp-golden-dump && dotnet run -c Release -- ../../build/golden\n",
                GOLDEN_PATH);
        printf("\n1 check(s) FAILED\n");
        return 1;
    }

    int calcCount = 0, moveCount = 0, animCount = 0;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "CALC", 4) == 0) { handleCalcLine(line); calcCount++; }
        else if (strncmp(line, "MOVE", 4) == 0) { handleMoveLine(line); moveCount++; }
        else if (strncmp(line, "ANIM", 4) == 0) { handleAnimLine(line); animCount++; }
    }
    fclose(f);

    printf("checked %d CALC, %d MOVE, %d ANIM scenarios\n", calcCount, moveCount, animCount);

    if (g_failures || g_checked == 0) {
        printf("\n%d check(s) FAILED (%d checked)\n", g_failures, g_checked);
        return 1;
    }
    printf("\nall %d checks passed\n", g_checked);
    return 0;
}
