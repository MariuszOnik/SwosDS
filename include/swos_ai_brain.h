// SOURCE: openswos game/scripts/Sim/Port/AiBrain.cs (full file, step 9 of
// the porting order). AI_SetControlsDirection -- updatePlayers.cpp:15980
// (~3333 LOC of the original asm; 1772 lines in the C#). The largest single
// AI function in SWOS: runs once per team per tick and decides whether to
// press fire, what direction to face, and what ball-spin/after-touch
// strength to apply.
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes. The
// C# source's own top-of-file comment describes an earlier, partially-
// stubbed porting increment ("Port scope (this commit)" / "Stubs: ...");
// that language is now stale documentation -- verified by reading the
// entire current file: every goto label resolves to a real terminal
// `return` or a real continuation, with zero remaining `// TODO`/stub
// markers in the function body itself (only in that historical header
// comment, which the C# source never updated). Ported in full.
//
// Comment-filtered dependency scan found one new real call:
// GameTime.AmigaModeActive() -- a literal `=> false` one-liner, pulled
// forward as a minimal slice (swos_game_time.h), not the rest of the
// 1736-line GameTime.cs (match-clock orchestration, its own future step).
// Everything else (AiHelpers.*, Memory/TeamData/PlayerSprite/BallSprite
// accessors, Rng.NextByte, SpriteUpdate.CalculateDeltaXAndY) is already
// ported.
//
// Telemetry (8 per-branch entry counters + ResetFireSiteCounters) omitted:
// pure C#-side ints with public getters ("so the smoke test can verify the
// AI's real fire paths are reached"), never read by any Memory write or
// control-flow branch, zero RNG consumption -- same pattern as every prior
// step's telemetry omissions.
#pragma once

// updatePlayers.cpp:15980. The step-9 real implementation wired into
// g_swosAiSetControlsDirectionHook (swos_player_controlled.h) -- see that
// header for the 6A/6B boundary this closes.
void swosAiBrainSetControlsDirection(int a6TeamBase);
