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

    swosWriteWord(0x210, -1);
    CHECK(swosReadSignedWord(0x210) == -1, "signed word round-trip (-1)");

    swosWriteDword(0x300, 0xDEADBEEFu);
    CHECK(swosReadDword(0x300) == 0xDEADBEEFu, "dword round-trip");
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

static void test_rng_determinism(void) {
    swosRngReseed(0);
    int first = swosRngNextByte();
    // seed=0 forces the "advance xorIndex, refresh key" branch on the very
    // first call: xorIndex 0->1, key=table[1]=154; result = table[0]^154.
    CHECK(first == (swos_rngTable[0] ^ swos_rngTable[1]), "first byte after Reseed(0) matches hand-derived value");

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
}

int main(void) {
    test_memory_roundtrip();
    test_addr_offsets();
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
