// SOURCE: openswos game/scripts/Sim/Port/TacticsLoader.cs (LoadAllTactics
// ONLY -- see below for why only this one function).
// FIDELITY: VERIFIED_PC -- literal 370-byte tactic structs extracted
// mechanically (tools/extract_all_arrays.py -> generated/swos_tactics_data.h),
// not hand-retyped; loop logic is a direct mechanical port.
//
// PHASE 1 BOOTSTRAP-COMPLETENESS FOLLOW-UP (2026-09-16): the Phase 1
// synthetic-setup lockstep (tools/lockstep_runner.c) proved the C VM port
// matches real OpenSWOS C# byte-for-byte, then separately confirmed (see
// README.md "Status: Phase 1") that match_bootstrap.c's synthetic setup
// never calls this function, leaving Memory.Addr.teamTacticsPool entirely
// zero -- a plausible root cause for both the observed AI-vs-AI stall and
// unrealistic-looking play, since every AI positioning lookup then
// dereferences an all-zero formation-position slot. This is the first,
// narrowly-scoped piece of the real InitSwosVmFromMatchSetup production
// chain to actually land in the C port -- LoadAllTactics() alone, not the
// other ~6 files that chain calls (GameTime.SaveTeams/InitPlayerCardChance/
// DetermineStartingTeamAndTeamPlayingUp, Pitch.SetPitchTypeAndNumber,
// GameTime.InitGameVariables, the real TeamDataLoader.WritePlayerInfos/
// WireTeamFields, PlayerEnergy.SetMatchLength, Kickoff.StartingMatch,
// Bench.InitBenchBeforeMatch), which remain a separate, larger follow-up.
// LoadAllTactics() was chosen first because it is fully self-contained
// (Memory only, no other unported dependency) and is the ONE piece the
// evidence directly implicates.
#pragma once

// Populates Memory.Addr.teamTacticsPool with all 19 tactic slots: 0-11 from
// the 12 built-in formations (literal data, TacticsLoader.cs's own
// swos.asm:208980-209340 citations), 12-18 (USER_A..F +
// editTacticsCurrentTactics) zeroed -- matching swos.asm's InitUserTactics.
void swosTacticsLoaderLoadAllTactics(void);
