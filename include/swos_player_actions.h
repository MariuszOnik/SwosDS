// SOURCE: openswos game/scripts/Sim/Port/PlayerActions.cs (full file, step 5
// of the porting order).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
//
// swosSetPlayerAnimationTable was pulled forward in step 3 (SpriteUpdate.cs
// calls it directly) -- see src/swos_player_actions.c's header for that
// history. Everything else below is step 5.
//
// Deliberately NOT ported (documented, not stubbed -- matches the pattern
// established for MatchAudio in steps 3/4):
//   - Pure C#-side telemetry: ResetShotCounters/TickShotCurveTracker/
//     TickOnTargetTracker/RecordShot/CurveErrorNow and their backing static
//     fields (shot counters, curve tracker, on-target tracker). Verified by
//     reading every one of these: they only mutate PlayerActions.cs's own
//     private C# statics (used for the --swos-smoke dev diagnostic) and
//     never write to Memory/BallSprite/PlayerSprite/TeamData. Their one
//     call site (RecordShot, from PlayerKickingBall) is simply omitted.
//   - `FaithfulBallControl` is consumed by PlayerControlled.cs and therefore
//     lands with step 6A in swos_player_controlled.{h,c}.
//   - MatchAudio.* calls (PlayGoodTackleComment/PlayHeaderComment/
//     CancelGoodPass/EnqueueGoodPass) -- host-side audio (commentary), zero
//     Memory effect, remain omitted at each call site. Where a function
//     mixes audio with real Memory writes (StopGoodPassSample,
//     EnqueuePlayingGoodPassSample), only the audio call is omitted -- the
//     Memory-affecting logic is ported in full. PlayKickSample -- this
//     file's own 6 real call sites -- is the one exception: now wired to a
//     real sound via swosAudioFireEvent(SWOS_AUDIO_EVENT_KICK), see
//     swos_audio_events.h.
// PlayerControlled's duel counters, deferred during step 5, are wired to the
// real step-6A telemetry structure by swos_player_actions.c.
//
// Forward-pulled MINIMAL slices (not full files -- see each header for why):
//   - swos_team_data_loader.h: TeamDataLoader.cs's PlayerInfo offset
//     constants only (GetPlayerInfoForSprite's consumers need to read
//     skill bytes). WritePlayerInfos/WireTeamFields (team-file loading) are
//     NOT pulled in -- different layer (match setup, not match simulation),
//     pulls in OpenSwos.Assets/SkillScaling.cs/TeamPort.cs.
//   - swos_player_energy.h: PlayerEnergy.cs's EffectEnabled/ShotPenalty/
//     SpeedStep only (the only PlayerEnergy members PlayerActions.cs
//     actually calls, grep-verified). The rest of PlayerEnergy.cs is called
//     from other not-yet-ported files.
#pragma once

// swos.asm:104309-104364. Rebinds a player sprite's animation table for its
// current (team, ordinal, direction) combination. Does NOT touch imageIndex
// (unlike SetPlayerAnimationTableAndPictureIndex).
void swosSetPlayerAnimationTable(int playerAddr, int animTable);

// player.cpp:3450-3558 (static). Sets player.animationTable AND recomputes
// imageIndex from the current frame.
void swosSetPlayerAnimationTableAndPictureIndex(int animTable, int playerAddr);

// player.cpp:3245-3251. Resolves a player sprite to its PlayerInfo record
// address (inGameTeamPtr + (ordinal-1) * TDL_PLAYER_INFO_SIZE), or 0 if the
// team isn't wired yet / ordinal out of range.
int swosGetPlayerInfoForSprite(int teamBase, int playerAddr);

// player.cpp:85-130. Pins the ball to the goalkeeper's "ball offset".
void swosUpdatePlayerWithBall(int playerAddr);

// player.cpp:135-191. Pins the ball to the outfielder (controlling player).
void swosUpdateControllingPlayer(int playerAddr);

// player.cpp:276-744. 50/50 ball-duel resolution against the opponent's
// controlling player.
void swosCalculateIfPlayerWinsBall(int direction, int teamBase, int playerAddr);

// player.cpp:750-1136. Issues a kick from the controlled player.
void swosPlayerKickingBall(int teamBase, int playerAddr);

// player.cpp:1146-1388. Standing-header impact.
void swosPlayerHittingStaticHeader(int teamBase, int playerAddr);

// player.cpp:1396-1671. Flying/lob header impact (picks between the two).
void swosPlayerHittingJumpHeader(int teamBase, int playerAddr);

// player.cpp:1684-1966. Strong tackle.
void swosPlayerTackledTheBallStrong(int teamBase, int playerAddr);

// player.cpp:1974-2231. Weak tackle.
void swosPlayerTackledTheBallWeak(int teamBase, int playerAddr);

// player.cpp:2530-2554. Flying-header ball-speed/deltaZ adjustment.
void swosDoFlyingHeader(int playerAddr);

// player.cpp:2560-3123. The pass routine.
void swosDoPass(int teamBase, int playerAddr);

// player.cpp:3132-3170. Sets player.playerDownTimer after a tackle.
void swosSetPlayerDowntimeAfterTackle(int teamBase, int playerAddr);

// player.cpp:3175-3243 (static). Conditionally installs the
// jump-header-hit animation table (distinct from
// SetPlayerJumpHeaderHitAnimationTable below).
void swosSetJumpHeaderHitAnimTable(int playerAddr);

// player.cpp:3257-3281 (static). Lob-header ball-speed/deltaZ adjustment.
void swosDoLobHeader(int playerAddr);

// player.cpp:3293-3420 (static). Nearest team-mate lookup for passing.
// Returns a player sprite address, or -1 if none found.
int swosGetClosestNonControlledPlayerInDirection(int dir, int teamBase, int playerAddr);

// player.cpp:3427-3444 (static). Tiny wrapper: only installs the
// jump-header-hit animation table when frameSwitchCounter <= 2.
void swosSetPlayerJumpHeaderHitAnimationTable(int playerAddr);

// player.cpp:3567-3720 (static). Picks the per-gameState kBallDestDelta
// table address.
int swosGetBallDestCoordinatesTable(void);

// player.cpp:3722-3762 (static). Reads goodPassSampleCommand and
// enqueues/stops the good-pass sample accordingly.
void swosPlayStopGoodPassSampleIfNeeded(void);

// player.cpp:3764-3767 (static). Memory side effect only (playingGoodPassTimer
// = -1); the MatchAudio.CancelGoodPass() call is omitted (audio, see above).
void swosStopGoodPassSample(void);

// player.cpp:3769-3793 (static). Memory side effects (goodPassTimer/
// playingGoodPassTimer bookkeeping); the MatchAudio.EnqueueGoodPass() call
// on the 5th good pass is omitted (audio, see above).
void swosEnqueuePlayingGoodPassSample(void);

// player.cpp:17-77. Sets player.speed from the per-state skill table plus
// injury/fatigue/pass-overlap/stoppage-time modifiers, then frameDelay from
// speed. Always tail-calls swosRecomputeSpriteDeltas, including on every
// early-out (matches the asm caller, which unconditionally runs the
// delta-recompute after this function regardless of how it returned).
void swosUpdatePlayerSpeedAndFrameDelay(int teamBase, int playerAddr);

// updatePlayers.cpp:10088-10356 (the post-UpdatePlayerSpeed asm tail).
// Recomputes deltaX/deltaY toward destination, updates direction/
// fullDirection, and switches between the standing/running animation
// tables. Extracted as its own function because
// swosUpdatePlayerSpeedAndFrameDelay's early-outs must still reach it.
void swosRecomputeSpriteDeltas(int teamBase, int playerAddr);
