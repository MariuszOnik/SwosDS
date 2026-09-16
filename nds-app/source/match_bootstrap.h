// DS-adapter-only match bootstrap. NOT part of swos-vm-c's mechanical
// C#-port fidelity -- there is no ported equivalent of this file.
//
// OpenSWOS's real match setup (Main.cs's InitSwosVmFromMatchSetup ->
// TeamDataLoader.WritePlayerInfos/WireTeamFields, populating PlayerInfo
// records from a parsed team file) was never ported anywhere in this
// project (see swos_team_data_loader.h's own header comment) -- that is a
// team-FILE-LOADING subsystem, a different layer from per-tick match
// simulation, and porting a full SWOS team-file parser is out of scope
// here. Likewise Kickoff.cs's real "players walk onto the pitch from the
// tunnel" entrance ceremony (StartingMatch/InitPlayersBeforeEnteringPitch)
// was deliberately excluded from the step-11A Kickoff.cs slice.
//
// This file fills both gaps with the minimum needed to make the REAL,
// mechanically-ported simulation (AI brain, physics, referee, ball, game
// loop -- all in ../../src) runnable and visible: a placeholder 11-a-side
// roster with flat mid-range skills for each team, placed at an arbitrary
// initial layout (NOT OpenSWOS's own starting-position tables -- just a
// starting point for the walk-in below), then calls the real
// Kickoff.PrepareForInitialKick() and lets it stand.
//
// ETAP 0 audit fix (2026-09-16): this function used to ALSO force
// gameStatePl straight to K_ST_GAME_IN_PROGRESS and breakCameraMode to 0
// immediately after PrepareForInitialKick(), skipping the real
// waiting-on-player/break-camera state machine entirely. That silently
// skipped the one piece of already-ported, already byte-tested logic that
// actually places players at OpenSWOS's real kickoff formation
// (`setPlayerPositionsForGameBreak()` in swos_update_players.c, driven by
// the real kTopStartingPositions/kBottomStartingPositions tables) --
// confirmed to be one cause of the broken-looking on-screen kickoff. A
// separate renderer bug treated bitmap row 0 as world y=0 instead of y=16.
// Fixed here by simply NOT overriding gameStatePl/
// breakCameraMode here: PrepareForInitialKick's own state (101/-1/0)
// stands, and the real GameLoop tick machinery (already fully ported,
// step 11) carries both AI teams from there into a correct kickoff
// formation and then into live play on its own, exactly like a real match.
// The integration test proves C/C# parity through tick 10 for this shared
// synthetic setup; it is not a long-run proof of the entire match.
#pragma once

// Sets up PlayerInfo records (both teams), sprite ordinals/positions,
// TeamData wiring, ball position, camera, and game state so
// swosGameLoopTick() can run immediately. Call once after swosMemoryInit().
void dsBootstrapMatch(void);

// Phase 1 lockstep (tools/lockstep_runner.c): identical to dsBootstrapMatch()
// -- same formation, same PlayerInfo, same Kickoff/Camera calls -- except
// Rng.Reseed(seed) (swosRngReseed) runs immediately after swosMemoryInit(),
// BEFORE Kickoff.PrepareForInitialKick() (which itself draws real Rng
// bytes). See Step12IntegrationGolden.cs's Bootstrap(int seed) header
// comment (C# side) for why this ordering -- both sides must match exactly
// for seed != 0 to mean the same thing on both engines. seed=0 reproduces
// dsBootstrapMatch()'s existing behaviour byte-for-byte (swosMemoryInit's
// own internal reseed already reseeds with 0 at a fresh init).
void dsBootstrapMatchSeeded(int seed);
