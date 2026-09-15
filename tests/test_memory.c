// Smoke test for the step-1 mechanical port (Memory/Flags/Rng/Tables). Not a
// port of anything in OpenSWOS -- new code written for this repo to catch
// transcription mistakes before building on top of this layer.
#include <stdio.h>
#include <string.h>

#include "swos_addr.h"
#include "swos_flags.h"
#include "swos_memory.h"
#include "swos_rng.h"
#include "swos_tables.h"

static int g_failures = 0;

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            g_failures++; \
        } else { \
            printf("ok:   %s\n", msg); \
        } \
    } while (0)

static void test_memory_roundtrip(void) {
    swosMemoryInitStub();
    swosWriteByte(0x100, 0xAB);
    CHECK(swosReadByte(0x100) == 0xAB, "byte round-trip");

    swosWriteWord(0x200, 0x1234);
    CHECK(swosReadWord(0x200) == 0x1234, "word round-trip");
    CHECK(swosReadByte(0x200) == 0x34, "word is little-endian (low byte first)");

    swosWriteWord(0x210, (uint16_t)-1);
    CHECK(swosReadSignedWord(0x210) == -1, "signed word round-trip (-1)");

    swosWriteDword(0x300, 0xDEADBEEFu);
    CHECK(swosReadDword(0x300) == 0xDEADBEEFu, "dword round-trip");

    // Exercises the byte >= 0x80 case at the dword read's <<24 step, which
    // used to be undefined behaviour before casting each byte to uint32_t
    // first (see swos_memory.c's checkBounds/Read comments).
    swosWriteDword(0x310, 0xFF000000u);
    CHECK(swosReadDword(0x310) == 0xFF000000u, "dword round-trip with top byte >= 0x80");
}

static void test_addr_offsets(void) {
    // Cross-checks against the literal comments in Memory.cs -- catches a
    // convert_addr.py regression without needing the C# runtime.
    CHECK(ADDR_kHighKickBallSpeed - ADDR_kBallKickingSpeed == 2,
          "kHighKickBallSpeed is 2 bytes after kBallKickingSpeed");
    CHECK(ADDR_team1InGameTeamPlayers - ADDR_team1InGameTeamHeader == 42,
          "team1InGameTeamPlayers = header + 42 (players[0])");
    CHECK(ADDR_aboutToThrowInAnimTable == ADDR_kAboutToThrowInAnimTableAddr,
          "legacy alias resolves to the same address as its target (forward ref)");
}

// Golden vectors, independently computed in Python from the SAME
// kRandomTable + algorithm as ../swos-port/src/util/random.cpp (not by
// calling this repo's C code) -- see the review that asked for this
// (2026-09-15): a same-seed-twice test only proves an implementation is
// self-consistent, not that it matches the original. seed=0 additionally
// matches the ORIGINAL engine's true state (random.cpp's statics all
// default to 0; swosRngReseed(0) splits 0 into all-zero pieces too, so it
// reproduces that exactly -- see swos_rng.h's fidelity note). seed=42 and
// seed=0x12345678 only test swosRngReseed()'s OpenSWOS-only splitting
// policy for self-consistency, same caveat as swos_rng.h documents.
static const int kGoldenSeed0Stream1[16] = {
    230, 0, 8, 220, 255, 96, 55, 195, 239, 128, 217, 150, 116, 9, 120, 184,
};
static const int kGoldenSeed0Stream2[16] = {
    230, 0, 8, 220, 255, 96, 55, 195, 239, 128, 217, 150, 116, 9, 120, 184,
};
static const int kGoldenSeed42Stream1[16] = {
    186, 222, 252, 108, 33, 131, 232, 118, 71, 74, 211, 162, 182, 133, 3, 157,
};
static const int kGoldenSeed0x12345678Stream1[16] = {
    227, 80, 150, 114, 155, 53, 213, 28, 188, 132, 240, 43, 60, 123, 97, 153,
};
static const int kGoldenSeed0x12345678Stream2[16] = {
    82, 35, 55, 4, 130, 28, 81, 18, 245, 43, 115, 22, 90, 180, 209, 250,
};

static void fillBytes(int out[16], int (*next)(void)) {
    for (int i = 0; i < 16; i++) out[i] = next();
}

static void test_rng_golden_vectors(void) {
    int buf[16];

    swosRngReseed(0);
    fillBytes(buf, swosRngNextByte);
    CHECK(memcmp(buf, kGoldenSeed0Stream1, sizeof(buf)) == 0,
          "seed=0 stream1 matches golden vector (== original's natural zero-start state)");
    swosRngReseed(0);
    fillBytes(buf, swosRngNextByte2);
    CHECK(memcmp(buf, kGoldenSeed0Stream2, sizeof(buf)) == 0,
          "seed=0 stream2 matches golden vector (== original's natural zero-start state)");

    swosRngReseed(42);
    fillBytes(buf, swosRngNextByte);
    CHECK(memcmp(buf, kGoldenSeed42Stream1, sizeof(buf)) == 0,
          "seed=42 stream1 matches golden vector (tests Reseed() policy, not original engine)");

    swosRngReseed(0x12345678);
    fillBytes(buf, swosRngNextByte);
    CHECK(memcmp(buf, kGoldenSeed0x12345678Stream1, sizeof(buf)) == 0,
          "seed=0x12345678 stream1 matches golden vector (tests Reseed() policy)");
    swosRngReseed(0x12345678);
    fillBytes(buf, swosRngNextByte2);
    CHECK(memcmp(buf, kGoldenSeed0x12345678Stream2, sizeof(buf)) == 0,
          "seed=0x12345678 stream2 matches golden vector (tests Reseed() policy)");
}

static void test_rng_determinism(void) {
    swosRngReseed(42);
    int a[8], b[8];
    for (int i = 0; i < 8; i++) a[i] = swosRngNextByte();
    swosRngReseed(42);
    for (int i = 0; i < 8; i++) b[i] = swosRngNextByte();
    CHECK(memcmp(a, b, sizeof(a)) == 0, "same seed produces the same byte stream (determinism)");
}

static void test_tables(void) {
    // Peak of the sine table is at angle 64 (quarter turn) per Tables.cs comment.
    int peakIndex = 0;
    for (int i = 0; i < 256; i++)
        if (swos_sineCosineTable[i] > swos_sineCosineTable[peakIndex]) peakIndex = i;
    CHECK(peakIndex == 64, "sine table peaks at angle 64 (quarter turn)");
    CHECK(swos_sineCosineTable[64] == 32767, "sine peak value is 32767");

    CHECK(swos_angleTangent[0][0] == -1, "angleTangent[0][0] is the no-movement sentinel (-1)");
    CHECK(swos_angleTangent[1][1] == 32, "angleTangent[1][1] is the 45-degree value (32)");
}

static void test_flags(void) {
    short r = swosFlagsSetFromSub16(5, 3);
    CHECK(r == 2 && !g_swosFlags.zero && !g_swosFlags.sign, "sub16(5,3) = 2, no zero/sign");

    r = swosFlagsSetFromSub16(3, 5);
    CHECK(r == -2 && g_swosFlags.sign && g_swosFlags.carry, "sub16(3,5) = -2, sign+carry (borrow)");

    r = swosFlagsSetFromSub16(3, 3);
    CHECK(r == 0 && g_swosFlags.zero, "sub16(3,3) = 0, zero flag set");

    // Boundary cases requested in review: signed overflow, unsigned
    // carry-out, and their interaction with zero/sign.
    r = swosFlagsSetFromAdd16(0x7FFF, 1);
    CHECK(r == (short)0x8000 && g_swosFlags.overflow && g_swosFlags.sign && !g_swosFlags.carry,
          "add16(0x7fff,1) = INT16_MIN, overflow+sign, no carry");

    r = swosFlagsSetFromAdd16(0xFFFF, 1);
    CHECK(r == 0 && g_swosFlags.carry && g_swosFlags.zero && !g_swosFlags.overflow,
          "add16(0xffff,1) = 0, carry+zero, no overflow");

    r = swosFlagsSetFromSub16(0x8000, 1);
    CHECK(r == 0x7FFF && g_swosFlags.overflow && !g_swosFlags.sign && !g_swosFlags.carry,
          "sub16(0x8000,1) = INT16_MAX, overflow, no sign/carry");

    r = swosFlagsSetFromSub16(0, 1);
    CHECK(r == -1 && g_swosFlags.carry && g_swosFlags.sign && !g_swosFlags.overflow,
          "sub16(0,1) = -1, carry+sign (borrow), no overflow");
}

int main(void) {
    test_memory_roundtrip();
    test_addr_offsets();
    test_rng_golden_vectors();
    test_rng_determinism();
    test_tables();
    test_flags();

    if (g_failures) {
        printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    printf("\nall checks passed\n");
    return 0;
}
