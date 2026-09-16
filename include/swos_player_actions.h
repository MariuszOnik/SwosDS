// SOURCE: openswos game/scripts/Sim/Port/PlayerActions.cs:2054-2104
// (SetPlayerAnimationTable ONLY -- PlayerActions.cs as a whole is step 5 of
// the porting order, not ported yet).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
//
// FORWARD-PULLED DEPENDENCY: step 3 (SpriteUpdate.cs) calls this function
// directly (SetNextPlayerFrame's direction-rebind, and
// UpdateAnimationTableAndDestinationReached). Rather than stub it with
// approximated logic -- which the project's standing rule forbids -- this
// one self-contained function (only touches Memory/PlayerSprite, both
// already ported) is pulled forward from its real source. When step 5
// ports the rest of PlayerActions.cs, EXTEND this file with the remaining
// functions rather than re-porting this one into a new file -- do not end
// up with two copies.
#pragma once

// swos.asm:104309-104364. Rebinds a player sprite's animation table for its
// current (team, ordinal, direction) combination. Does NOT touch imageIndex
// (unlike SetPlayerAnimationTableAndPictureIndex, not ported yet).
void swosSetPlayerAnimationTable(int playerAddr, int animTable);
