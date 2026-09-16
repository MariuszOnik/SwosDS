// SOURCE: openswos game/scripts/Sim/Port/Pitch.cs (full file).
// FIDELITY: VERIFIED_PC -- direct mechanical port; the two probability
// tables are byte-for-byte copies of pitch.cpp:226-240,261 literals (per
// that file's own license-rule comment: paraphrase algorithms, copy
// constants). Small enough (12x7 + 7 + 16 = 107 bytes) to hand-transcribe
// directly into this header rather than round-trip through the array
// extractor -- transcribed once, diffed digit-by-digit against the C#
// source before commit, same standard as any other constant table here.
//
// PHASE 1 BOOTSTRAP-COMPLETENESS FOLLOW-UP (2026-09-16): part of closing
// the gap the Phase 1 lockstep audit found -- see swos_tactics_loader.h and
// README.md "Status: Phase 1" for the full context. Pitch.cs is fully
// self-contained (Memory/Rng only, the one BallSim field it doesn't already
// share with GameTime.cs is added here as a matching plain C global), so
// ported whole rather than sliced.
#pragma once

// BallSim.CurrentPitchNumber -- same treatment as GameTime.cs's
// g_swosBallSimCurrentPitchType (swos_game_time.h): BallSim lives in the
// idiomatic BallState.cs layer, not the Memory-based mechanical-port
// family, so only the one field callers need is pulled forward as a plain
// C global, not the whole file. Default 0 (matches BallSim's own C# field
// default before anything sets it).
extern int g_swosBallSimCurrentPitchNumber;

// pitch.cpp:222-257 -- picks BallSim.CurrentPitchType (0..6, ball-physics
// row) from either the seasonal or fixed probability table, or honours a
// manual override. Draws exactly one Rng byte when rolling a table.
void swosPitchSetPitchType(void);

// pitch.cpp:259-284 -- picks BallSim.CurrentPitchNumber (0..4, visual pitch
// variant) via training pin / random / deterministic team-name+kit hash.
// Draws exactly one Rng byte in the "friendly" (plg_D0_param != 0) branch.
void swosPitchSetPitchNumber(void);

// pitch.cpp:58-62 -- wrapper, calls both in order. This is the one real
// call site Main.cs's InitSwosVmFromMatchSetup uses.
void swosPitchSetPitchTypeAndNumber(void);

int swosPitchGetPitchType(void);
int swosPitchGetPitchNumber(void);
