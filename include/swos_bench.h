// Minimal real slice of Bench.cs (1868 lines -- the substitutes-menu UI/
// state machine, a different layer entirely) required by InputControls.cs
// (step 8): only InBench() (bench.cpp:42-45), the single member
// InputControls.UpdateTeamControls actually calls (comment-filtered-grep
// verified). The rest of Bench.cs lands with its own future step.
#pragma once

#include <stdbool.h>

// bench.cpp:42-45. True while the substitutes menu is open.
bool swosBenchInBench(void);
