// SOURCE: openswos game/scripts/SwosVm/AnimationTablesData.cs (full file)
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
//
// Player/referee animation tables -- ported from swos-port/swos/swos.asm:
// 218368-219641 (see the OpenSWOS source file's header comment for the full
// PlayerAnimationTable/RefereeAnimationTable byte layout). The frame-indices
// stream literals were extracted mechanically
// (tools/extract_all_arrays.py -> generated/swos_anim_streams.h), not
// retyped by hand -- a single wrong animation frame index would silently
// desync from OpenSWOS with no compiler error.
//
// swosAnimTablesInit() is order-dependent: it interns each stream into a
// bump-allocated arena (Memory.Addr.kFrameIndicesArraysBase..End) in the
// EXACT same call sequence as OpenSWOS's Init(), because each call's
// returned address depends on the allocator's running cursor. Do not
// reorder, dedupe, or "clean up" the call sequence relative to the source.
#pragma once

// Called from swosMemoryInit(). Interns every frame-indices stream, then
// writes each animation-table struct at its allotted address.
void swosAnimTablesInit(void);
