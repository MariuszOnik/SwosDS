// SOURCE: openswos game/scripts/Sim/Port/GameLoop.cs (full file, step 11B
// of the porting order -- the final piece of step 11). Per-tick game
// orchestrator, mechanically ported from
// external/swos-port/src/game/gameLoop.cpp.
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
//
// PlayersLeavingPitch (game.cpp:707-723) was already forward-pulled whole
// in step 10 (GameTime.NextPenalty's one real dependency) -- kept exactly
// as it was; this step's GameLoop.cs body calls the SAME function from its
// own DispatchStoppageEventTriggered dispatch (gs == ST_GAME_ENDED), not a
// second copy.
//
// SCOPE (matches the C#'s own header comment): the OUTER SWOS game loop
// (init/fade/replay/exit, gameLoop.cpp:84-135) is NOT ported -- a future
// DS-adapter concern (step 12), not this VM layer. This file ports the
// INNER per-tick body: Tick() (the entry point) and everything it reaches.
//
// Genuinely audio/render-only and omitted, confirmed by reading each site
// (matches the C#'s own stub framing, same standard as every other MatchAudio
// omission in this port): StubHandleKeys (truly a no-op, zero effect even in
// the C# -- input pumping is host-owned); the fadeOut/drawPitchAtCurrentCamera
// calls inside GameOver; MatchAudio.PlayEndGameCrowd/PlayWhistle/Tick/
// LoadCrowdChant. Where a "stub" function ALSO has a real Memory
// side-effect (StubLoadCrowdChantSampleIfNeeded's one-shot flag clear,
// StubHandlePauseAndStats' statsEnqueued clear, PlayEnqueuedSamples'
// goalCounter decrement), that side-effect IS ported -- only the audio/
// render half is omitted.
#pragma once

#include <stdbool.h>

// gameControls.cpp:60 -- team selector. true = top, matching
// SelectedTeam.Top == 0 / UpdatePlayers(team) teamIndex mapping.
#define GL_TEAM_TOP true
#define GL_TEAM_BOTTOM false

// gameLoop.cpp:709-722 (via game.cpp:707-723). See swos_game_loop.h's
// original step-10 comment -- forward-pulled whole there; unchanged here.
void swosGameLoopPlayersLeavingPitch(void);

// gameLoop.cpp:95-130 -- Tick(). Per-tick entry from the (not-yet-ported)
// outer loop / a future DS main-loop adapter.
void swosGameLoopTick(void);

// gameLoop.cpp:265-273 -- updateTimers.
void swosGameLoopUpdateTimers(void);

// gameLoop.cpp:289-317 -- coreGameUpdate. The deterministic "advance one
// match-tick" pipeline.
void swosGameLoopCoreGameUpdate(void);

// gameControls.cpp:48-56 -- updateFireBlocked.
bool swosGameLoopUpdateFireBlocked(void);

// gameControls.cpp:58-62 -- selectTeamForUpdate. Returns true for the top
// team, false for bottom (GL_TEAM_TOP/GL_TEAM_BOTTOM).
bool swosGameLoopSelectTeamForUpdate(void);

// gameLoop.cpp:426-1853 -- updateGameTimersAndCameraBreakMode. The FULL
// stoppage/restart state machine: penalty-shootout inter-pen pause,
// in-progress fast path, ST_WAITING_ON_PLAYER accumulator + CPU safety
// net, the fire-press ceremony-skip paths, the stoppageEventTimer
// countdown, and (via the two dispatchers below) the full state/
// break-camera-mode ladder.
void swosGameLoopUpdateGameTimersAndCameraBreakMode(void);

// gameLoop.cpp:2012-2072 -- setCameraMovingToShowerState. Public because
// GameTime.cs (step 10) would call it from its own dispatch if that file's
// EndSecondHalf path is ever re-wired to the faithful ladder; kept as a
// named, stable entry point matching the C#'s own public visibility.
void swosGameLoopSetCameraMovingToShowerState(void);

// gameLoop.cpp:2074-2090 -- firstHalfJustEnded.
void swosGameLoopFirstHalfJustEnded(void);

// gameLoop.cpp:2092-2111 -- goToHalftime.
void swosGameLoopGoToHalftime(void);

// gameLoop.cpp:361-395 -- gameOver.
void swosGameLoopGameOver(void);

// gameLoop.cpp:165-168 / 134,204 -- isMatchRunning / the m_playingMatch flag.
bool swosGameLoopIsMatchRunning(void);
void swosGameLoopSetMatchRunning(bool running);

// gameLoop.cpp:170-188 -- the four FSM-interval setters.
void swosGameLoopSetPenaltiesInterval(int interval);
void swosGameLoopSetInitalKickInterval(int interval);
void swosGameLoopSetGoalCameraInterval(int interval);
void swosGameLoopSetAllowPlayerControlCameraInterval(int interval);
