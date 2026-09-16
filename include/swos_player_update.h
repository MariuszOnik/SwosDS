// SOURCE: openswos game/scripts/Sim/Port/PlayerUpdate.cs (full file, step
// 5.5 of the porting order).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
//
// swosUpdateBallWithControllingGoalkeeper was pulled forward in step 4
// (BallUpdate.cs's Section3 calls it directly) -- see src/swos_player_update.c's
// header for that history. Everything else below is step 5.5.
//
// Deliberately NOT ported (documented, not stubbed -- same pattern as
// PlayerActions.cs in step 5):
//   - Keeper-dive telemetry counters: s_diveCallsHigh/s_diveCallsLow/
//     s_shouldDiveTrueCount and their bump/reset/getter surface
//     (DiveCallsHigh/DiveCallsLow/ShouldDiveTrueCount/ResetDiveCounters/
//     BumpDiveCounter/BumpShouldDiveTrue). Verified by reading: these are
//     PlayerUpdate.cs's own private C# statics ("useful for tracking
//     regression on the dive bug fix" -- a dev diagnostic), never written
//     to Memory. Their three call sites (two in GoalkeeperJumping, one in
//     ShouldGoalkeeperDive) are simply omitted, exactly like PlayerActions.cs's
//     shot-counter telemetry in step 5.
//   - MatchAudio.KeeperClaimedComment()/PlayKick() -- host-side audio, zero
//     Memory effect, omitted at each call site with a comment.
//   - The `Godot.GD.Print("[PORT-SAFETY] ...")` debug line in
//     TickGoalkeeperHoldAutoRelease -- a log statement, zero Memory effect,
//     omitted.
//
// Forward-pulled MINIMAL slices (not full files -- see each header for why):
//   - swos_team_port.h: TeamPort.cs's StopAllPlayers/StopPlayers only.
//   - swos_player_state.h: UpdatePlayers.cs's PortPlayerState enum values
//     (pulled forward whole -- 16 tiny named bytes, no logic, will be
//     needed unchanged by later steps too).
//   - swos_team_data_loader.h: extended with TDL_OFF_GOALIE_SKILL (this
//     step's own addition -- RunShotAtGoal reads PlayerInfo.goalieSkill).
//   - swos_player_energy.h: extended with DrainOnKeeperCatch/
//     KeeperSkillPenalty (this step's own addition).
#pragma once

#include <stdbool.h>
#include <stdint.h>

// player.cpp:200-265. Called when gameState==ST_KEEPER_HOLDS_BALL: pins the
// ball to the keeper's hand position (keeper.x/y + a per-direction offset),
// zeroes ball speed, and damps deltaZ toward falling.
void swosUpdateBallWithControllingGoalkeeper(int controllingPlayerAddr);

// player.cpp:2238-2336. Keeper successfully claims the ball: match-state
// transition to "keeper holds ball" stoppage, camera direction/turn-flag
// masks, and (unless the keeper is still mid-dive) team.controlledPlayer +
// updatePlayerWithBall. See the .c file for the mid-dive early-out.
void swosGoalkeeperClaimedTheBall(int keeperSpriteAddr, bool isTopTeam);

// updatePlayers.cpp:3212-3242 (l_goalkeeper_still_diving, goalkeeperDivingRight
// branch). Faithful completion of a claim deferred while the keeper was
// mid-dive. Runs the PREDICTED post-decrement playerDownTimer (the caller's
// own decrement happens after this) and consumes it itself on completion --
// see the .c file's header comment for the exact wiring contract with
// TickGoalkeeperHoldAutoRelease. Returns true if the deferred claim
// completed this tick.
bool swosTickGoalieDivingClaimCompletion(int keeperSpriteAddr, bool isTopTeam);

// updatePlayers.cpp:11022-11089. Initiates the catch animation when the
// keeper reaches the ball (state/timer/speed/destination).
void swosGoalkeeperCaughtTheBall(int keeperSpriteAddr, bool isTopTeam);

// updatePlayers.cpp:3059-3093. Per-tick handler for kGoalieCatchingBall (4).
// Returns true if the keeper transitioned out of the catching state this
// tick (finished catching, or deflected).
bool swosTickGoalieCatchingBall(int keeperSpriteAddr, bool isTopTeam);

// updatePlayers.cpp:3163-3186. Per-tick handler for kGoalieClaimed (11).
// Returns true if the keeper transitioned back to Normal this tick.
bool swosTickGoalieClaimed(int keeperSpriteAddr);

// PORT_EXTENSION (per the C# source's own header comment -- not part of
// the original SWOS, a port-only safety net for CPU teams so a stuck
// keeper-holds-ball stoppage always resolves). ALSO hosts the faithful
// TickGoalieDivingClaimCompletion call (see above), which is the real
// per-tick hook UpdatePlayers.cs calls before its own dive-state branch.
// Returns true if the keeper's state was resolved this tick.
bool swosTickGoalkeeperHoldAutoRelease(int keeperSpriteAddr, bool isTopTeam);

// updatePlayers.cpp:11105-11191. Repeated-subtraction division (no asm DIV):
// frames needed to cover integer-pixel distance d4 at per-frame delta d0
// (Q16.16). d0==0 returns 0.
int swosGetFramesNeededToCoverDistance(int d0, int d4);

// updatePlayers.cpp:10485-10816. Decides whether the keeper can attempt a
// save this frame (distance/frame-race checks, with a separate penalty
// branch). true means the caller should call swosGoalkeeperJumping.
bool swosShouldGoalkeeperDive(int a1KeeperAddr, int a2BallAddr, int a6TeamBase);

// updatePlayers.cpp:10829-11017. Commits the keeper to a dive: sets speed
// (near/far/slower/random by ball distance), playerState (diving high/low),
// animation table, playerDownTimer, team.controlledPlDirection, and
// destX/Y (unclamped -- see the .c file for why a clamp here was wrong).
void swosGoalkeeperJumping(int d0Dir, int d1SpeedFlag, int d3DestDir,
                            int a1KeeperAddr, int a2BallAddr, int a6TeamBase);

// player.cpp:2342-2528. Parry path: keeper touches the ball but doesn't
// claim it -- deflects it away with jittered destination/speed/deltaZ.
void swosGoalkeeperDeflectedBall(int a2BallAddr, int a6TeamBase);

// updatePlayers.cpp:2072-2544 (cseg_7F7BC / l_shot_at_goal). Named exits
// mirror the asm's jump targets exactly -- see the .c file's header comment
// for what each one means to the (not yet ported) caller,
// UpdatePlayers.RunGoalkeeperInAreaChain.
typedef enum {
    SWOS_SHOT_CHAIN_SHOT_AT_GOAL, // jmp @@shot_at_goal
    SWOS_SHOT_CHAIN_C7FC48,       // jmp cseg_7FC48 (track ball at row[+6] speed)
    SWOS_SHOT_CHAIN_C7FBEF,       // jmp cseg_7FBEF (speed = dseg_1105EF)
    SWOS_SHOT_CHAIN_C7FC01,       // jmp cseg_7FC01 (ballNotHigh dest at row[+8] speed)
    SWOS_SHOT_CHAIN_CLAMP,        // jmp @@clamp_ball_y_inside_pitch
} SwosShotChainExit;

// updatePlayers.cpp:2072 (`cseg_7F7BC` entry).
SwosShotChainExit swosRunShotTripWire(int a1KeeperAddr, int a2BallAddr,
                                       int a6TeamBase, bool isTopTeam);

// updatePlayers.cpp:2196 (`l_shot_at_goal`).
SwosShotChainExit swosRunShotAtGoal(int a1KeeperAddr, int a2BallAddr,
                                     int a6TeamBase, bool isTopTeam);
