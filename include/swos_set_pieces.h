// Step-10 deferral boundary: UpdatePlayers.cs (step 7B) has two genuinely
// executed call sites into SetPieces.cs (not yet ported):
//   - SetPieces.SetThrowInPlayerDestinationCoordinates (l_player_taking_throw_in
//     setup tail, TickPassExpectingStopped's throw-in branch)
//   - SetPieces.TickThrowIn (l_player_taking_throw_in per-tick body, the
//     PortPlayerState.kThrowIn dispatch arm)
// Both are real, comment-verified calls (confirmed via a comment-filtered
// grep across the whole file), not comment-only references -- so per the
// project's 6A/6B assert-backed-hook convention (established for
// AiBrain/AiHelpers), they get explicit, assert-backed hooks here rather
// than a silent no-op or a premature partial port of SetPieces.cs. Step 10
// wires the real implementations into these hook globals.
#pragma once

#include <stdbool.h>

typedef void (*SwosSetThrowInPlayerDestHook)(int spriteAddr);
typedef void (*SwosTickThrowInHook)(int throwerSpriteAddr, int ballSpriteAddr,
                                     int teamBase);

extern SwosSetThrowInPlayerDestHook g_swosSetThrowInPlayerDestHook;
extern SwosTickThrowInHook g_swosTickThrowInHook;

// Asserts (aborts) if the step-10 hook isn't wired yet under debug/test
// builds; degrades to a safe no-op under NDEBUG release builds. Mirrors
// swosRunControlledBranch's AiBrain/AiHelpers hook calls exactly.
void swosSetPiecesSetThrowInPlayerDestinationCoordinates(int spriteAddr);
void swosSetPiecesTickThrowIn(int throwerSpriteAddr, int ballSpriteAddr,
                               int teamBase);
