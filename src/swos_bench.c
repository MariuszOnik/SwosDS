#include "swos_bench.h"

#include "swos_addr.h"
#include "swos_memory.h"

bool swosBenchInBench(void)
{
    return swosReadSignedWord(ADDR_g_inSubstitutesMenu) != 0;
}
