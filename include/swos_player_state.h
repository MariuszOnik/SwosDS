// SOURCE: openswos game/scripts/Sim/Port/UpdatePlayers.cs:4320-4348
// (`PortPlayerState` enum ONLY).
// FIDELITY: VERIFIED_PC -- constants copied verbatim, no logic changes.
//
// FORWARD-PULLED DEPENDENCY, MINIMAL SLICE: PlayerUpdate.cs compares
// PlayerSprite.OffPlayerState bytes against this enum (BumpDiveCounter,
// TickGoalieDivingClaimCompletion). The enum itself is defined in
// UpdatePlayers.cs (step 7, not otherwise touched) -- named PortPlayerState
// there (not PlayerState) specifically to avoid clashing with a different
// OpenSwos.Sim.PlayerState type, per that file's own comment. Pulled
// forward whole because it is tiny (16 named byte values, no logic) and
// will be needed again, unchanged, by every future step that inspects
// PlayerSprite.OffPlayerState -- porting it once now avoids re-deriving the
// same 16 constants piecemeal later.
#pragma once

#define PLSTATE_NORMAL               0
#define PLSTATE_TACKLING             1
// 2 is unused in the source enum.
#define PLSTATE_TACKLED              3
#define PLSTATE_GOALIE_CATCHING_BALL 4
#define PLSTATE_THROW_IN             5
#define PLSTATE_GOALIE_DIVING_HIGH   6
#define PLSTATE_GOALIE_DIVING_LOW    7
#define PLSTATE_STATIC_HEADER        8
#define PLSTATE_JUMP_HEADER          9
#define PLSTATE_DOWN                 10
#define PLSTATE_GOALIE_CLAIMED       11
#define PLSTATE_BOOKED               12
#define PLSTATE_INJURED              13
#define PLSTATE_SAD                  14
#define PLSTATE_HAPPY                15
#define PLSTATE_UNKNOWN              255
