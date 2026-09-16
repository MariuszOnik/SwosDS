// Minimal real slice of GameTime.cs (1736 lines -- match-clock orchestration,
// a different layer entirely) required by AiBrain.cs (step 9): only
// AmigaModeActive() (GameTime.cs:770, `=> false`), the single member
// AiBrain.cs actually calls (comment-filtered-grep verified). The rest of
// GameTime.cs lands with its own future step (it also unblocks
// Result.RegisterScorer's PORT_PENDING hook -- see swos_ball_update.h).
#pragma once

#include <stdbool.h>

// GameTime.cs:770. PC-locked to false until Amiga support lands (matches
// the C#'s own hardcoded `=> false`) -- kept as a real function, not a
// literal `false` inlined at each call site, so the two Amiga-mode helper
// functions in swos_ai_brain.c stay a one-flag flip away from real Amiga
// support, exactly like the C#'s own documented intent.
bool swosGameTimeAmigaModeActive(void);
