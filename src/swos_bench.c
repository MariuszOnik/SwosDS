#include "swos_bench.h"

#include "swos_addr.h"
#include "swos_memory.h"

bool swosBenchInBench(void)
{
    return swosReadSignedWord(ADDR_g_inSubstitutesMenu) != 0;
}

int swosBenchGetBenchState(void)
{
    return swosReadSignedWord(ADDR_m_benchState);
}

bool swosBenchInBenchMenus(void)
{
    return swosBenchInBench() && swosBenchGetBenchState() == BENCH_STATE_INITIAL;
}
