// Phase 1 lockstep (see ../../DEVLOG.md and ../README.md "Status: Phase 1"):
// replays the SAME synthetic setup as nds-app/source/match_bootstrap.c
// (linked directly below, not copied) through the real ported C VM, tick by
// tick, and compares each tick against a golden per-tick summary log
// produced by the real, unmodified OpenSWOS C# running the identical setup
// (tools/csharp-golden-dump/Step12IntegrationGolden.cs's RunLockstepLog,
// via `dotnet run -- --lockstep-log <seed> <maxTicks> <outPath>`).
//
// Per-tick record layout (must match WriteTickRecord in
// Step12IntegrationGolden.cs EXACTLY, field for field, both little-endian):
//   uint32  tick
//   uint64  fnv1a64 hash of the full 0x60000-byte Memory buffer
//   uint8   rng: seed, xorKey, xorIndex, seed2, xorKey2, xorIndex2
//   int16   gameState, gameStatePl, breakCameraMode
//   int32   ball X, Y, Z (Q16.16)
//   int16   team1TotalGoals, team2TotalGoals
//   uint16  currentGameTick
//   uint32  gt_gameTimeInMinutes
//   int32   top controlled-player pointer, bottom controlled-player pointer
//
// On the FIRST mismatch (hash or any compared field): stop immediately,
// dump this run's own full Memory buffer to <log>.mismatch_c.bin, print the
// first differing tick's field-by-field comparison plus the RNG state and
// all 22 players' key fields, and print the exact command to regenerate the
// matching C# full dump at that tick
// (`dotnet run -- --lockstep-dump <seed> <tick> <outPath>`) for a full
// byte-level Memory diff -- this tool alone only has ITS OWN full buffer,
// not C#'s, since the golden log stores compact per-tick summaries, not a
// full dump every tick (that would be tens of GB at the 100000-tick tier).
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "match_bootstrap.h"
#include "swos_addr.h"
#include "swos_ball_sprite.h"
#include "swos_game_loop.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"
#include "swos_rng.h"
#include "swos_team_data.h"

typedef struct {
    uint32_t tick;
    uint64_t hash;
    uint8_t seed, xorKey, xorIndex, seed2, xorKey2, xorIndex2;
    int16_t gameState, gameStatePl, breakCameraMode;
    int32_t ballX, ballY, ballZ;
    int16_t goals1, goals2;
    uint16_t currentGameTick;
    uint32_t gtMinutes;
    int32_t ctrlTop, ctrlBottom;
} TickRecord;

static bool readRecord(FILE *f, TickRecord *r) {
    if (fread(&r->tick, sizeof(r->tick), 1, f) != 1) return false;
    if (fread(&r->hash, sizeof(r->hash), 1, f) != 1) return false;
    if (fread(&r->seed, 1, 1, f) != 1) return false;
    if (fread(&r->xorKey, 1, 1, f) != 1) return false;
    if (fread(&r->xorIndex, 1, 1, f) != 1) return false;
    if (fread(&r->seed2, 1, 1, f) != 1) return false;
    if (fread(&r->xorKey2, 1, 1, f) != 1) return false;
    if (fread(&r->xorIndex2, 1, 1, f) != 1) return false;
    if (fread(&r->gameState, sizeof(r->gameState), 1, f) != 1) return false;
    if (fread(&r->gameStatePl, sizeof(r->gameStatePl), 1, f) != 1) return false;
    if (fread(&r->breakCameraMode, sizeof(r->breakCameraMode), 1, f) != 1) return false;
    if (fread(&r->ballX, sizeof(r->ballX), 1, f) != 1) return false;
    if (fread(&r->ballY, sizeof(r->ballY), 1, f) != 1) return false;
    if (fread(&r->ballZ, sizeof(r->ballZ), 1, f) != 1) return false;
    if (fread(&r->goals1, sizeof(r->goals1), 1, f) != 1) return false;
    if (fread(&r->goals2, sizeof(r->goals2), 1, f) != 1) return false;
    if (fread(&r->currentGameTick, sizeof(r->currentGameTick), 1, f) != 1) return false;
    if (fread(&r->gtMinutes, sizeof(r->gtMinutes), 1, f) != 1) return false;
    if (fread(&r->ctrlTop, sizeof(r->ctrlTop), 1, f) != 1) return false;
    if (fread(&r->ctrlBottom, sizeof(r->ctrlBottom), 1, f) != 1) return false;
    return true;
}

static uint64_t fnv1a64(const uint8_t *data, size_t len) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < len; i++) {
        h ^= data[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

static void snapshotOurRecord(uint32_t tick, TickRecord *r) {
    r->tick = tick;
    r->hash = fnv1a64(swosMemoryView(0, SWOS_MEM_SIZE), SWOS_MEM_SIZE);
    SwosRngState rng = swosRngGetState();
    r->seed = rng.seed; r->xorKey = rng.xorKey; r->xorIndex = rng.xorIndex;
    r->seed2 = rng.seed2; r->xorKey2 = rng.xorKey2; r->xorIndex2 = rng.xorIndex2;
    r->gameState = swosReadSignedWord(ADDR_gameState);
    r->gameStatePl = swosReadSignedWord(ADDR_gameStatePl);
    r->breakCameraMode = swosReadSignedWord(ADDR_breakCameraMode);
    r->ballX = swosBallSpriteX();
    r->ballY = swosBallSpriteY();
    r->ballZ = swosBallSpriteZ();
    r->goals1 = swosReadSignedWord(ADDR_team1TotalGoals);
    r->goals2 = swosReadSignedWord(ADDR_team2TotalGoals);
    r->currentGameTick = swosReadWord(ADDR_currentGameTick);
    r->gtMinutes = swosReadDword(ADDR_gt_gameTimeInMinutes);
    r->ctrlTop = swosTeamDataControlledPlayer(true);
    r->ctrlBottom = swosTeamDataControlledPlayer(false);
}

// Field-by-field compare; returns true if identical. On mismatch, prints
// which field(s) differ (hash mismatch alone doesn't say WHERE the byte
// difference is -- that needs the two-step full-dump diagnose flow -- but
// the tracked fields below usually pinpoint the SUBSYSTEM immediately).
static bool compareRecords(const TickRecord *ours, const TickRecord *golden) {
    bool ok = true;
#define CHK(field, fmt) \
    if (ours->field != golden->field) { \
        printf("  FIELD MISMATCH %-16s ours=" fmt " golden=" fmt "\n", #field, ours->field, golden->field); \
        ok = false; \
    }
    CHK(hash, "0x%016llX")
    CHK(seed, "%u") CHK(xorKey, "%u") CHK(xorIndex, "%u")
    CHK(seed2, "%u") CHK(xorKey2, "%u") CHK(xorIndex2, "%u")
    CHK(gameState, "%d") CHK(gameStatePl, "%d") CHK(breakCameraMode, "%d")
    CHK(ballX, "%d") CHK(ballY, "%d") CHK(ballZ, "%d")
    CHK(goals1, "%d") CHK(goals2, "%d")
    CHK(currentGameTick, "%u") CHK(gtMinutes, "%u")
    CHK(ctrlTop, "%d") CHK(ctrlBottom, "%d")
#undef CHK
    return ok;
}

static void dumpAll22Players(void) {
    for (int slot = 0; slot < 22; slot++) {
        printf("  slot %2d: team=%d ord=%d x=%d y=%d dir=%d state=%d img=%d\n",
               slot,
               swosPlayerSpriteTeamNumber(slot),
               swosPlayerSpritePlayerOrdinal(slot),
               swosPlayerSpriteX(slot),
               swosPlayerSpriteY(slot),
               swosPlayerSpriteDirection(slot),
               swosPlayerSpritePlayerState(slot),
               swosPlayerSpriteImageIndex(slot));
    }
}

// Ball + all 22 players' whole positions. PHASE 1 BOOTSTRAP-COMPLETENESS
// FOLLOW-UP (2026-09-16): was 300 (matching sdl-debug's own stall
// convention and Step12IntegrationGolden.cs's old threshold), which proved
// to be a FALSE POSITIVE -- see this file's Step12IntegrationGolden.cs
// counterpart (kStallTicks) for the full trace of why 2000.
#define STALL_TICKS 2000

static bool positionsEqual(const int32_t *a, const int32_t *b, int n) {
    return memcmp(a, b, (size_t)n * sizeof(int32_t)) == 0;
}

static void snapshotPositions(int32_t *pos) {
    pos[0] = swosBallSpriteX();
    pos[1] = swosBallSpriteY();
    for (int slot = 0; slot < 22; slot++) {
        pos[2 + slot * 2] = swosPlayerSpriteX(slot);
        pos[3 + slot * 2] = swosPlayerSpriteY(slot);
    }
}

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr,
                "usage: lockstep_runner <seed> <maxTicks> <golden-log-path> <mismatch-dump-prefix>\n"
                "  Replays the shared synthetic setup + up to maxTicks ticks through the\n"
                "  C port, comparing each tick against the C# golden log at\n"
                "  <golden-log-path> (see tools/csharp-golden-dump's --lockstep-log mode).\n");
        return 2;
    }
    int seed = atoi(argv[1]);
    uint32_t maxTicks = (uint32_t)strtoul(argv[2], NULL, 10);
    const char *goldenPath = argv[3];
    const char *dumpPrefix = argv[4];

    FILE *golden = fopen(goldenPath, "rb");
    if (!golden) {
        printf("SKIP: golden log not found: %s\n"
               "  regenerate with:\n"
               "  cd tools/csharp-golden-dump && dotnet run -c Release -- --lockstep-log %d <maxTicks> %s\n",
               goldenPath, seed, goldenPath);
        return 0;
    }

    dsBootstrapMatchSeeded(seed);

    int32_t lastPos[46];
    int32_t curPos[46];
    bool havePos = false;
    int unchangedTicks = 0;
    int stallStartTick = -1;

    uint32_t tick = 0;
    uint32_t comparedTicks = 0;
    bool mismatch = false;
    TickRecord ours, gold;

    while (tick < maxTicks) {
        if (!readRecord(golden, &gold)) {
            printf("lockstep seed=%d: golden log ended at tick %u (no more records) -- C matched every tick up to here.\n",
                   seed, tick);
            break;
        }
        tick++;
        swosGameLoopTick();
        snapshotOurRecord(tick, &ours);

        if (ours.tick != gold.tick) {
            printf("FATAL: tick counter desync -- ours=%u golden=%u (log format mismatch?)\n", ours.tick, gold.tick);
            mismatch = true;
            break;
        }

        if (!compareRecords(&ours, &gold)) {
            printf("\n=== LOCKSTEP MISMATCH seed=%d tick=%u ===\n", seed, tick);
            printf("RNG (ours):   seed=%u xorKey=%u xorIndex=%u seed2=%u xorKey2=%u xorIndex2=%u\n",
                   ours.seed, ours.xorKey, ours.xorIndex, ours.seed2, ours.xorKey2, ours.xorIndex2);
            printf("RNG (golden): seed=%u xorKey=%u xorIndex=%u seed2=%u xorKey2=%u xorIndex2=%u\n",
                   gold.seed, gold.xorKey, gold.xorIndex, gold.seed2, gold.xorKey2, gold.xorIndex2);
            printf("22 players (ours, C port):\n");
            dumpAll22Players();

            char path[1024];
            snprintf(path, sizeof(path), "%s_seed%d_tick%u_c.bin", dumpPrefix, seed, tick);
            FILE *out = fopen(path, "wb");
            if (out) {
                fwrite(swosMemoryView(0, SWOS_MEM_SIZE), 1, SWOS_MEM_SIZE, out);
                fclose(out);
                printf("Wrote our full Memory buffer to: %s\n", path);
            }
            printf("To get the matching C# full buffer for a byte-level diff, run:\n"
                   "  cd tools/csharp-golden-dump && dotnet run -c Release -- --lockstep-dump %d %u %s_seed%d_tick%u_csharp.bin\n"
                   "then diff the two files (first differing offset, ranges) with any binary diff tool.\n",
                   seed, tick, dumpPrefix, seed, tick);
            mismatch = true;
            break;
        }

        comparedTicks = tick;

        snapshotPositions(curPos);
        if (havePos && positionsEqual(curPos, lastPos, 46)) {
            if (unchangedTicks == 0) stallStartTick = (int)tick - 1;
            unchangedTicks++;
        } else {
            unchangedTicks = 0;
            stallStartTick = -1;
        }
        memcpy(lastPos, curPos, sizeof(lastPos));
        havePos = true;

        if (unchangedTicks >= STALL_TICKS) {
            printf("lockstep seed=%d: C port stalled at tick %d (ball+22 players unchanged for %d ticks) -- matched %u/%u compared ticks up to the stall.\n",
                   seed, stallStartTick, STALL_TICKS, comparedTicks, tick);
            break;
        }
    }

    fclose(golden);

    if (mismatch) {
        printf("lockstep_runner seed=%d: FAIL at tick %u (%u prior ticks matched byte-for-byte)\n", seed, tick, comparedTicks);
        return 1;
    }

    printf("lockstep_runner seed=%d: OK -- %u tick(s) matched C# byte-for-byte%s\n",
           seed, comparedTicks, unchangedTicks >= STALL_TICKS ? " (stopped at stall)" : "");
    return 0;
}
