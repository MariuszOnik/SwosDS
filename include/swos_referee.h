// SOURCE: openswos game/scripts/Sim/Port/Referee.cs:41-99, 141-190,
// 563-593, 625-629, 688-706 (ActivateReferee + its private helpers +
// RefereeSprite ONLY -- see below for why only these).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
//
// FORWARD-PULLED DEPENDENCY, MINIMAL SLICE: PlayerTackle.PlayerTacklingTestFoul
// calls Referee.ActivateReferee() when a foul draws a card (grep-verified:
// the only Referee member PlayerTackle.cs uses). ActivateReferee's own
// private helpers (InitRefereeAnimationTable, MarkDisplaySpritesDirty,
// SwosRand) and the RefereeSprite Memory-view class it writes through are
// pulled in alongside it. The rest of Referee.cs -- the full referee
// state-machine tick (UpdateReferee), card-handing/booking-sprite
// animation, sending players off, etc. -- is a different layer (per-tick
// referee movement/rendering, not "a foul just happened, register it"),
// not called from anything ported so far; port it when its own caller
// (UpdatePlayers.cs's per-tick referee update, step 10 per the porting
// order) is ported.
//
// Referee.NotifyEnteredAboutToGiveCard (called from UpdatePlayers.cs) is
// NOT ported here -- verified by reading it: `DbgEnteredAboutToGive++`,
// one of Referee.cs's own debug/telemetry counters (zero Memory effect,
// same pattern as every other *Golden telemetry omission in this port).
// Its one call site is simply omitted, documented, not stubbed.
//
// Also NOT ported: ActivateReferee's own Dbg* counter increments
// (DbgActivations/DbgYellowCards/DbgRedCards/DbgSecondYellowCards/
// DbgEnteredIncoming) -- same telemetry pattern, zero Memory effect.
#pragma once

// referee.cpp:50-75. Called when a foul draws a card: points the referee
// sprite at the foul position (with a randomised approach angle/side) and
// starts the "walking in" animation/state.
void swosRefereeActivate(void);

// Memory-backed sprite view for the referee -- a Sprite outside the
// 22-player pool, allocated at 0x4FD00 (after TeamData-bottom, which ends
// at 0x4FCFF; well clear of the swos_player_actions.c/swos_player_update.c
// test scratch PlayerInfo region at 0x4FE60+). Same 110-byte Sprite struct
// layout as BallSprite/PlayerSprite -- field offsets reuse the PLSPR_OFF_*
// macros directly rather than redefining identical constants.
#define REFSPR_BASE 0x4FD00
