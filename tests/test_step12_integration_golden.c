// ETAP 0 audit (2026-09-16): the missing test the audit called for --
// Memory.Init() -> a real match setup -> Kickoff.PrepareForInitialKick() ->
// N full GameLoop ticks -- byte-compared against real OpenSWOS C# running
// the identical setup (tools/csharp-golden-dump/Step12IntegrationGolden.cs).
// Every earlier golden test in this repo exercises one step's functions
// against hand-set Memory state; this is the first one that runs the
// cold-start pipeline steps 7B/10/11/12 all depend on together.
//
// Calls nds-app/source/match_bootstrap.c's real dsBootstrapMatch() --
// linked directly (see Makefile), not copied -- so this test also directly
// exercises the exact function the DS app calls at startup, catching any
// future regression of the ETAP 0 fix (dsBootstrapMatch() forcing
// gameStatePl/breakCameraMode past Kickoff.PrepareForInitialKick()'s own
// state, which skipped the real walk-to-kickoff-formation logic -- see
// match_bootstrap.c's header comment).
#include <stdio.h>
#include <stdlib.h>

#include "match_bootstrap.h"
#include "swos_game_loop.h"
#include "swos_memory.h"

static int compareAgainst(const char *path, const char *label)
{
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        printf("SKIP: %s not found -- run:\n"
               "  cd tools/csharp-golden-dump && dotnet run -c Release -- ../../build/golden\n",
               path);
        return 0;
    }

    uint8_t *golden = malloc(SWOS_MEM_SIZE);
    long got = (long)fread(golden, 1, SWOS_MEM_SIZE, f);
    fclose(f);
    if (got != SWOS_MEM_SIZE)
    {
        printf("FAIL: %s is %ld bytes, expected %d\n", path, got, SWOS_MEM_SIZE);
        free(golden);
        return 1;
    }

    const uint8_t *ours = swosMemoryView(0, SWOS_MEM_SIZE);
    int mismatches = 0;
    for (int i = 0; i < SWOS_MEM_SIZE; i++)
    {
        if (ours[i] != golden[i])
        {
            if (mismatches == 0)
                printf("FAIL: %s first mismatch at 0x%X: ours=0x%02X golden=0x%02X\n",
                       label, i, ours[i], golden[i]);
            mismatches++;
        }
    }
    free(golden);

    if (mismatches)
    {
        printf("FAIL: %s -- %d byte(s) differ (0x%X total)\n", label, mismatches, SWOS_MEM_SIZE);
        return 1;
    }
    printf("ok:   %s matches C# byte-for-byte (0x%X bytes)\n", label, SWOS_MEM_SIZE);
    return 0;
}

int main(void)
{
    int failures = 0;

    dsBootstrapMatch();
    failures += compareAgainst("build/golden/s12_after_setup.bin", "after_setup");

    swosGameLoopTick();
    failures += compareAgainst("build/golden/s12_after_tick1.bin", "after_tick1");

    for (int i = 0; i < 9; i++)
        swosGameLoopTick();
    failures += compareAgainst("build/golden/s12_after_tick10.bin", "after_tick10");

    if (failures)
    {
        printf("test_step12_integration_golden: %d FAILURE(S)\n", failures);
        return 1;
    }
    printf("test_step12_integration_golden: all checks passed\n");
    return 0;
}
