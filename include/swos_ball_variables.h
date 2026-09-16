// SOURCE: openswos game/scripts/Sim/Port/BallVariables.cs (full file, step
// 7A of the porting order -- a real local dependency of UpdatePlayers.cs).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes. The
// source itself repeats the same "gravity loop + swap + clamp + write"
// skeleton ~8 times across mirrored up/down/left/right branches rather than
// factoring it into a helper; ported as written, not DRY'd up.
//
// Per-tick ball world-state prediction, called by UpdatePlayers.cs (once
// per outfielder's "this player last played" miss branch). Populates the
// ballDefensive{X,Y,Z}/ballNotHigh{X,Y,Z}/strikeDestX/ballStrike{Y,Z}/
// ballNextGround{X,Y,ZDead} Memory slots that downstream AI/chase/shot
// decisions (already ported: ShouldGoalkeeperDive, RunShotTripWire, ...)
// read.
#pragma once

// updatePlayers.cpp:11198-12486. aPlayerSprite/aBallSprite/aTeamData are
// absolute Memory addresses (aBallSprite is always BALLSPR_BASE in
// practice; aTeamData is TEAMDATA_TOP_BASE or TEAMDATA_BOTTOM_BASE).
void swosUpdateBallVariables(int aPlayerSprite, int aBallSprite, int aTeamData);

// updatePlayers.cpp:12491-12697. Predicts ballNextGroundX/Y (or writes the
// ballNextGroundX == -1 "ball standing" sentinel).
void swosCalculateBallNextGroundXYPositions(int aBallSprite);
