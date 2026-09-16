// SOURCE: openswos game/scripts/Sim/Port/Referee.cs (full file, step 10 of
// the porting order). ActivateReferee + its private helpers + RefereeSprite
// were forward-pulled in step 7A (see below); this step ports the rest --
// the per-tick state machine (UpdateReferee), card-handing/booking-sprite
// animation, and sending players off.
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
//
// Referee.NotifyEnteredAboutToGiveCard (called from UpdatePlayers.cs) and
// EVERY Dbg* counter in this file (DbgActivations/DbgEnteredIncoming/
// DbgEnteredWaiting/DbgEnteredAboutToGive/DbgEnteredBooking/DbgEnteredLeaving/
// DbgEnteredOffScreen/DbgYellowCards/DbgRedCards/DbgSecondYellowCards/
// DbgPlayersSentAway + ResetDebugCounters) are NOT ported -- verified by
// reading each one: pure C#-side ints/increments with public getters for
// the smoke test, zero Memory effect, same pattern as every other *Golden
// telemetry omission in this port.
//
// Audio omitted (StubEnqueueRedCardSample/StubEnqueueYellowCardSample --
// pure playback into MatchAudio, zero Memory effect, same pattern as every
// other MatchAudio omission in this port).
#pragma once

#include <stdbool.h>

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

// Memory-backed sprite view for the booked-player-number digit (a small
// floating sprite painted above the booked player during card animation).
// Mirrors swos-port's `static Sprite m_bookedPlayerNumberSprite{3}`.
// Allocated at 0x4FD80 -- 128 bytes after REFSPR_BASE.
#define BKPLSPR_BASE 0x4FD80

// referee.cpp:24-31 -- RefereeState enum.
#define REF_ST_OFF_SCREEN        0
#define REF_ST_INCOMING          1
#define REF_ST_WAITING_PLAYER    2
#define REF_ST_ABOUT_TO_GIVE_CARD 3
#define REF_ST_BOOKING           4
#define REF_ST_LEAVING           5

// referee.cpp:33-39 -- CardHanding enum.
#define REF_CARD_NONE          0
#define REF_CARD_YELLOW        1
#define REF_CARD_RED           2
#define REF_CARD_SECOND_YELLOW 3

// referee.cpp:77-80 -- refereeActive.
bool swosRefereeActive(void);

// referee.cpp:82-85 -- cardHandingInProgress.
bool swosRefereeCardHandingInProgress(void);

// ---- Read-only render accessors (task #181, mechanical port) --------------
int  swosRefereeState(void);
int  swosRefereeWhichCard(void);
bool swosRefereeVisible(void);
int  swosRefereeImageIndex(void);
int  swosRefereeWorldX(void);
int  swosRefereeWorldY(void);
int  swosRefereeWorldZ(void);

// referee.cpp:87-99 -- updateReferee. Main per-tick entry point.
void swosRefereeUpdateReferee(void);

// referee.cpp:101-151 -- updateBookedPlayerNumberSprite. Renders + blinks
// the player-number sprite over the booked player's head during the
// kRefBooking phase.
void swosRefereeUpdateBookedPlayerNumberSprite(void);

// referee.cpp:163-185 -- removeReferee. Hides referee + resets to hiding
// position. Called when the leaving animation completes.
void swosRefereeRemoveReferee(void);
