// SOURCE: openswos game/scripts/Sim/Port/GameLoop.cs:1860-1888
// (PlayersLeavingPitch ONLY -- see below for why only this one function).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
//
// FORWARD-PULLED DEPENDENCY, MINIMAL SLICE: GameTime.NextPenalty (step 10)
// calls GameLoop.PlayersLeavingPitch() when a penalty shootout is decided
// (game.cpp:884). GameLoop.cs itself (~1900+ lines -- the full per-tick
// orchestrator: UpdateGameTimersAndCameraBreakMode, the break-camera FSM,
// DispatchStoppageEventTriggered, gameOver, etc.) is its own future porting
// step (11), not touched here. PlayersLeavingPitch is small (29 lines) and
// fully self-contained (Memory/TeamData/TeamPort only, all already ported)
// -- forward-pulled whole, same pattern as GameTime.AmigaModeActive() in
// step 9 and UpdateBallWithControllingGoalkeeper in step 4.
#pragma once

// game.cpp:709-722 (via GameLoop.cs:1860-1888). Parks the match in the
// "players walking to the tunnel" state after a decided penalty shootout
// (or, in the original, after full time / extra time): clears the ball,
// sets gameState/gameStatePl/breakCameraMode/cameraDirection, resets the
// stoppage timers + camera velocities, stops every player, and points
// lastTeamPlayedBeforeBreak at the top team. The full leaving-pitch
// animation FSM (gameState 24 dispatch) is a step-11 concern; this function
// only performs the state transition itself.
void swosGameLoopPlayersLeavingPitch(void);
