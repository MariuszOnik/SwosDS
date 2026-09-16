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
// roster with flat mid-range skills for each team, placed directly at a
// hand-picked formation (NOT derived from OpenSWOS's own starting-position
// tables -- their coordinate scale/offset convention wasn't verified
// against swos-ds's WORLD_W/WORLD_H=672x848 pitch, so a fresh, honestly
// simple layout was used instead of risking a subtly-wrong mechanical
// value), then forces the game state straight to "live play" (skipping the
// referee whistle/waiting-on-player handshake, since driving that from a
// cold Init() would need Main.cs-level orchestration this project doesn't
// have).
#pragma once

// Sets up PlayerInfo records (both teams), sprite ordinals/positions,
// TeamData wiring, ball position, camera, and game state so
// swosGameLoopTick() can run immediately. Call once after swosMemoryInit().
void dsBootstrapMatch(void);
