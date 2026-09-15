// SOURCE: openswos game/scripts/SwosVm/Rng.cs (full file)
// FIDELITY: VERIFIED_PC -- direct mechanical port; kRandomTable values
// extracted mechanically (tools/extract_table.py), not retyped by hand.
//
// Deterministic table-driven xor-stream PRNG replicating SWOS::rand()/rand2()
// (external/swos-port/src/util/random.cpp, not present in this workspace --
// see audyt-openswos-sim.md). Two independent streams, matching the original
// (stream 2 used by AssignFakeGoalsToScorers, NextPenalty, ApplyTeamTactics).
#pragma once

// Reset to a known seed. Call once per match boot so replays are
// reproducible: same input -> same byte stream on both streams.
void swosRngReseed(int seed);

int swosRngNextByte(void);
int swosRngNextWord(void);
int swosRngNext(void);
int swosRngNextRange(int max);   // [0, max)

int swosRngNextByte2(void);
int swosRngNextWord2(void);
int swosRngNextRange2(int max);  // [0, max)
