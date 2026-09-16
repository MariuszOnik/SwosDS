// SOURCE: openswos game/scripts/Sim/Port/PlayerUpdate.cs:58-92
// (UpdateBallWithControllingGoalkeeper ONLY -- PlayerUpdate.cs as a whole,
// 1553 lines, is a later porting step; this one function is pulled forward
// because BallUpdate.cs's Section3 calls it directly when the goalkeeper
// holds the ball. Fully self-contained (Memory/BallSprite/PlayerSprite only,
// all already ported), unlike the two BallUpdate.cs dependencies that
// needed BallOutOfPlay.cs/UpdateGoals.cs alongside them.)
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
#pragma once

// player.cpp:200-265. Called when gameState==ST_KEEPER_HOLDS_BALL: pins the
// ball to the keeper's hand position (keeper.x/y + a per-direction offset),
// zeroes ball speed, and damps deltaZ toward falling.
void swosUpdateBallWithControllingGoalkeeper(int controllingPlayerAddr);
