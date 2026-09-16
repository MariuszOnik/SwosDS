// DS-adapter-only match bootstrap driver. NOT part of swos-vm-c's mechanical
// C#-port fidelity in the usual sense -- there is no single OpenSWOS FILE
// this file is "the port of" -- but as of the Phase 1 bootstrap-completeness
// follow-up (2026-09-16), the SEQUENCE of real, mechanically-ported
// functions it calls IS a faithful mirror of Main.cs's real production
// match-boot entry point, InitSwosVmFromMatchSetup. See README.md
// "Status: Phase 1" for the full audit that motivated this.
//
// History: this file originally (step 12) hand-poked PlayerInfo bytes and
// player-sprite positions directly, bypassing TeamDataLoader.
// WritePlayerInfos/WireTeamFields and Kickoff.StartingMatch/
// InitPlayersBeforeEnteringPitch entirely (see swos_team_data_loader.h's
// OLD header comment, before this follow-up, for why -- those functions
// weren't ported yet). The Phase 1 lockstep proved the C port matches real
// OpenSWOS C# byte-for-byte on that hand-poked setup, then separately found
// (by reading Main.cs and comparing) that the hand-poked setup itself
// skipped large parts of real production initialization -- most notably
// Memory.Addr.teamTacticsPool being entirely zero, since
// TacticsLoader.LoadAllTactics() was never called. That follow-up ported
// the remaining production-chain pieces (Pitch.cs, PlayerEnergy.
// SetMatchLength, the real TeamDataLoader.WritePlayerInfos/WireTeamFields
// including its SkillScaling.cs dependency, Kickoff.StartingMatch/
// InitPlayersBeforeEnteringPitch) and this file now calls all of them, in
// Main.cs's real order.
//
// What is STILL synthetic here (deliberately, not a gap to silently patch):
// the two team rosters themselves. No SWOS team-FILE parser (TEAM.*/ADF)
// exists anywhere in this project, so buildSyntheticTeam() constructs a
// plausible-shaped SwosTeamRecord (11 players, one goalkeeper, flat
// mid-range skills) by hand rather than loading one from the user's real
// GOG team files. Every function downstream of that -- skill scaling, price
// calculation, goalkeeper-skill derivation, tactics, pitch-side entry
// positioning, the whole GameLoop tick pipeline -- is the real,
// unmodified-behaviour production code path, not a stand-in for it.
//
// ETAP 0 audit fix (2026-09-16, predates the Phase 1 follow-up above): this
// function used to ALSO force gameStatePl straight to K_ST_GAME_IN_PROGRESS
// and breakCameraMode to 0 immediately after PrepareForInitialKick(),
// skipping the real waiting-on-player/break-camera state machine entirely.
// Fixed by simply not overriding those fields -- StartingMatch()'s own
// state stands, and the real GameLoop tick machinery (already fully
// ported, step 11) carries both AI teams into a correct kickoff formation
// and live play on its own, exactly like a real match.
#pragma once

// Sets up PlayerInfo records (both teams, via the real WritePlayerInfos/
// WireTeamFields), tactics pool, pitch selection, game variables, ball
// position, camera, and game state so swosGameLoopTick() can run
// immediately. Call once after swosMemoryInit().
void dsBootstrapMatch(void);

// Phase 1 lockstep (tools/lockstep_runner.c): identical to dsBootstrapMatch()
// except Rng.Reseed(seed) (swosRngReseed) runs immediately after
// swosMemoryInit(), BEFORE any of bootstrapCommon()'s real production calls
// (several of which draw real Rng bytes -- DetermineStartingTeamAndTeam
// PlayingUp, Pitch.SetPitchTypeAndNumber, WritePlayerInfos via
// SkillScaling, StartingMatch's InitPlayersBeforeEnteringPitch). See
// Step12IntegrationGolden.cs's Bootstrap(int seed) header comment (C# side)
// for why this ordering -- both sides must match exactly for seed != 0 to
// mean the same thing on both engines. seed=0 reproduces dsBootstrapMatch()'s
// existing behaviour byte-for-byte (swosMemoryInit's own internal reseed
// already reseeds with 0 at a fresh init).
void dsBootstrapMatchSeeded(int seed);
