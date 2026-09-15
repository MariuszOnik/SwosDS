// Step-2.5 gold-standard test: compares swosMemoryInit()'s full 0x60000-byte
// output against a byte-exact dump produced by actually RUNNING OpenSWOS's
// real Memory.Init(pcMode) in C# (tools/csharp-golden-dump). This is much
// stronger than the round-trip/offset tests elsewhere in this repo -- those
// only prove setter-C -> memory -> getter-C is self-consistent (a shared
// wrong offset in both would still pass); this proves the C port's output
// matches an independent execution of the actual source being ported.
//
// Regenerate the golden files after any OpenSWOS source change with:
//   cd tools/csharp-golden-dump && dotnet run -c Release -- ../../build/golden
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "swos_memory.h"

#define GOLDEN_PC_PATH    "build/golden/golden_pc.bin"
#define GOLDEN_AMIGA_PATH "build/golden/golden_amiga.bin"

static int g_failures = 0;

static uint8_t *readFile(const char *path, long *outLen) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "FAIL: could not open %s -- run the C# golden-dump harness first "
                         "(see tools/csharp-golden-dump/README or this file's header comment)\n", path);
        g_failures++;
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc((size_t)len);
    if (fread(buf, 1, (size_t)len, f) != (size_t)len) {
        fprintf(stderr, "FAIL: short read on %s\n", path);
        g_failures++;
    }
    fclose(f);
    *outLen = len;
    return buf;
}

// Compares the current swosMemoryView(0, SWOS_MEM_SIZE) against a golden
// file. Prints a summary of the first mismatches (if any) rather than just
// pass/fail, so a real regression is debuggable from test output alone.
static void compareAgainstGolden(const char *label, const char *goldenPath) {
    long goldenLen = 0;
    uint8_t *golden = readFile(goldenPath, &goldenLen);
    if (!golden) return;

    if (goldenLen != SWOS_MEM_SIZE) {
        printf("FAIL: %s golden file is %ld bytes, expected %d\n", label, goldenLen, SWOS_MEM_SIZE);
        g_failures++;
        free(golden);
        return;
    }

    const uint8_t *ours = swosMemoryView(0, SWOS_MEM_SIZE);
    int mismatches = 0;
    int firstMismatch = -1;
    for (int i = 0; i < SWOS_MEM_SIZE; i++) {
        if (ours[i] != golden[i]) {
            if (firstMismatch < 0) firstMismatch = i;
            mismatches++;
            if (mismatches <= 20) {
                printf("  diff @ 0x%06X: C=0x%02X golden(C#)=0x%02X\n", i, ours[i], golden[i]);
            }
        }
    }
    free(golden);

    if (mismatches == 0) {
        printf("ok:   %s matches OpenSWOS's real Memory.Init() byte-for-byte (0x%X bytes)\n",
               label, SWOS_MEM_SIZE);
    } else {
        printf("FAIL: %s has %d mismatched byte(s), first at 0x%06X (%s)\n",
               label, mismatches, firstMismatch, mismatches > 20 ? "showing first 20" : "shown above");
        g_failures++;
    }
}

int main(void) {
    swosMemoryInit(true);
    compareAgainstGolden("pcMode=true", GOLDEN_PC_PATH);

    swosMemoryInit(false);
    compareAgainstGolden("pcMode=false (Amiga)", GOLDEN_AMIGA_PATH);

    if (g_failures) {
        printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    printf("\nall checks passed\n");
    return 0;
}
