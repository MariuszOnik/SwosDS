// SOURCE: openswos game/scripts/SwosVm/Rng.cs (full file)
//
// FIDELITY of NextByte()/NextByte2() (the actual table-xor algorithm) is
// VERIFIED_PC: cross-checked line-for-line against
// ../swos-port/src/util/random.cpp's `random()` (both streams call the same
// helper with different state), and kRandomTable's 256 values match
// random.cpp's kRandomTable exactly -- extracted mechanically here
// (tools/extract_table.py), not retyped by hand, either way.
//
// FIDELITY of swosRngReseed() is PORT_EXTENSION, not verified original
// behaviour. random.cpp has no reseed function at all: `swos.seed`/`seed2`/
// `randXorKey2`/`randXorIndex2` are just plain fields that start at 0 (their
// static/struct default) and mutate only as rand()/rand2() get called --
// the original never re-derives them from an external seed. OpenSWOS's
// Rng.Reseed(int) -- splitting one int into seed/xorIndex per stream via
// byte-shifts -- is an OpenSWOS-only policy for getting a reproducible match
// boot, not something read off swos.asm. Do not treat its exact
// bit-splitting as authoritative until corroborated (e.g. against a
// disassembly of the match-init code). Note Reseed(0) happens to reproduce
// the original's true zero-initialised start state for *both* streams
// exactly (every split piece of 0 is 0) -- see tests/test_memory.c's golden
// vectors -- so it is a safe, verified choice for that one input.
//
// Two independent streams, matching the original (stream 2 used by
// AssignFakeGoalsToScorers, NextPenalty, ApplyTeamTactics -- see Rng.cs
// comments; not yet wired into any ported caller here).
#pragma once

#include <stdint.h>

// Reset to a known seed. See the FIDELITY note above: verified to match the
// original's natural start state for seed=0 only; the general splitting
// policy for other seeds is an OpenSWOS extension, not confirmed original
// behaviour.
void swosRngReseed(int seed);

int swosRngNextByte(void);
int swosRngNextWord(void);
int swosRngNext(void);
int swosRngNextRange(int max);   // [0, max)

int swosRngNextByte2(void);
int swosRngNextWord2(void);
int swosRngNextRange2(int max);  // [0, max)

// PORT-ONLY diagnostic accessor (not part of any OpenSWOS surface -- Rng.cs's
// m_seed/m_xorKey/m_xorIndex/m_seed2/m_xorKey2/m_xorIndex2 are private with
// no public getter). Added for the Phase 1 lockstep harness
// (tools/lockstep_runner.c), which needs to log/compare the FULL RNG state
// every tick, not just its consumed byte stream. The C# side reads the same
// six fields via reflection (see LockstepGolden.cs) since Rng.cs itself must
// stay unmodified.
typedef struct {
    uint8_t seed, xorKey, xorIndex;
    uint8_t seed2, xorKey2, xorIndex2;
} SwosRngState;

SwosRngState swosRngGetState(void);
