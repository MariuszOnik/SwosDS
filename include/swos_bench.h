// Minimal real slice of Bench.cs (1868 lines -- the substitutes-menu UI/
// state machine, a different layer entirely). InBench() (bench.cpp:42-45)
// was pulled in step 8 for InputControls.cs. Step 11 (GameLoop.cs's real
// dependencies) extends this with InBenchMenus() (bench.cpp:47-50, the one
// member SpinningLogo.cs's UpdateSpinningLogo calls) and its own dependency
// GetBenchState() (updateBench.cpp:194-197). GameLoop.cs's own real
// dependencies -- UpdateBench/CheckIfGoalkeeperClaimedTheBall -- reach
// almost the entire rest of the file transitively and are NOT a minimal-
// slice candidate; they land in a dedicated full port of Bench.cs.
#pragma once

#include <stdbool.h>

// bench.cpp:42-45. True while the substitutes menu is open.
bool swosBenchInBench(void);

// updateBench.h:3-10 -- BenchState enum. Only kBenchStateInitial (0) has a
// caller so far (InBenchMenus' comparison).
#define BENCH_STATE_INITIAL 0

// updateBench.cpp:194-197. Reads the raw m_benchState slot.
int swosBenchGetBenchState(void);

// bench.cpp:47-50. The "menu is visible" predicate that the spinning-logo /
// drawBench / camera-bench-mode all gate on.
bool swosBenchInBenchMenus(void);
