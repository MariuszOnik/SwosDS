// SOURCE: openswos game/scripts/Sim/Port/InputControls.cs (full file, step
// 8 of the porting order). Original: external/swos-port/src/controls/
// gameControls.cpp (332 LOC).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
//
// Comment-filtered dependency scan (whole file) found exactly ONE new real
// external call: Bench.InBench() (UpdateTeamControls's bench-reset branch).
// Pulled forward as a minimal slice -- swos_bench.h -- not the rest of
// Bench.cs (1868 lines, the substitutes-menu UI/state machine, a different
// layer). Everything else the C# calls (Memory/TeamData/PlayerSprite/
// BallSprite accessors) is already ported.
//
// Deliberately NOT ported (documented, not stubbed):
//   - DebugForceP1Direction/DebugForceP1Fire (C#:104-105) -- declared but
//     never READ anywhere in the whole OpenSWOS tree (grep-verified), so
//     they have zero effect on Memory/control flow by construction. Not
//     even telemetry -- dead fields.
//   - StubZoomIn/StubZoomOut (gameControls.cpp -> camera.cpp zoomIn/zoomOut)
//     -- the C# source's own bodies are empty (`/* TODO */`); porting an
//     intentional no-op as a no-op isn't a fidelity gap, it's fidelity.
//     StubRequestFadeAndInstantReplay/StubRequestFadeAndSaveReplay are
//     DIFFERENT -- they write real Memory words (the replay-request flags)
//     even though the replay *consumer* isn't wired yet -- ported for real,
//     see swosInputControlsRequestFadeAndInstantReplay/...SaveReplay below.
//
// Two ic_* diagnostic counter pairs (CtrlSwapHuman{Top,Bot}/CtrlSwapAi{Top,Bot})
// are real port-only telemetry (the C#'s own comment: "the original keeps no
// such counters") -- kept as a small struct + reset function, mirroring the
// established SwosPlayerControlledTelemetry pattern from step 6A.
#pragma once

#include <stdbool.h>
#include <stdint.h>

// gameControlEvents.h:5-20. Bitmask, stored as a plain int32_t (matches the
// C#'s `(int)events` cast at every Memory.WriteDword call site).
#define IC_EVENT_NONE           0
#define IC_EVENT_UP             1
#define IC_EVENT_DOWN           2
#define IC_EVENT_LEFT           4
#define IC_EVENT_RIGHT          8
#define IC_EVENT_KICK           16
#define IC_EVENT_BENCH          32
#define IC_EVENT_PAUSE          64
#define IC_EVENT_REPLAY         128
#define IC_EVENT_SAVE_HIGHLIGHT 256
#define IC_EVENT_ZOOM_IN        512
#define IC_EVENT_ZOOM_OUT       1024
#define IC_EVENT_MAX            2048  // synthesised sentinel after last bit

// gameControlEvents.h:24 -- movement-only mask.
#define IC_EVENT_MOVEMENT_MASK \
    (IC_EVENT_UP | IC_EVENT_DOWN | IC_EVENT_LEFT | IC_EVENT_RIGHT | \
     IC_EVENT_ZOOM_IN | IC_EVENT_ZOOM_OUT)

// swos.h:136-147 -- direction codes (8-way + sentinel).
#define IC_NO_DIRECTION       (-1)
#define IC_FACING_TOP         0
#define IC_FACING_TOP_RIGHT   1
#define IC_FACING_RIGHT       2
#define IC_FACING_BOTTOM_RIGHT 3
#define IC_FACING_BOTTOM      4
#define IC_FACING_BOTTOM_LEFT 5
#define IC_FACING_LEFT        6
#define IC_FACING_TOP_LEFT    7

// controls.h:3-7.
#define IC_NO_PLAYER (-1)
#define IC_PLAYER1   0
#define IC_PLAYER2   1

typedef struct {
    int ctrlSwapHumanTop, ctrlSwapHumanBot;
    int ctrlSwapAiTop, ctrlSwapAiBot;
} SwosInputControlsTelemetry;

extern SwosInputControlsTelemetry g_swosIcTelemetry;

void swosInputControlsResetTelemetry(void);

// ----- Godot input bridge -----------------------------------------------
// gameControls.cpp:107-124 -- getPlayerEvents originally dispatches across
// keyboard/mouse/joypad sources. Collapsed into one memory-backed bitmask
// per player; the DS input layer (not yet ported) calls this once per tick
// after sampling physical input, before the per-tick sim runs.
// direction: 0..7 (IC_FACING_*) or IC_NO_DIRECTION (-1) for centred.
void swosInputControlsSetJoystickState(int teamIndex, int direction,
                                        bool fireDown, bool fireTriggered);

// ----- Ported functions, in source order ---------------------------------

// gameControls.cpp:33-46 -- resetGameControls.
void swosResetGameControls(void);

// gameControls.cpp:48-56 -- updateFireBlocked.
bool swosUpdateFireBlocked(void);

// gameControls.cpp:58-62 -- selectTeamForUpdate. Returns true for top team.
bool swosSelectTeamForUpdate(void);

// gameControls.cpp:67-90 -- updateTeamControls(team).
void swosUpdateTeamControls(bool top);

// gameControls.cpp:92-99 -- postUpdateTeamControls.
void swosPostUpdateTeamControls(bool top);

// gameControls.cpp:101-127 -- getPlayerEvents. player: IC_PLAYER1/IC_PLAYER2.
int32_t swosGetPlayerEvents(int player);

// gameControls.cpp:129-133 -- isPlayerFiring.
bool swosIsPlayerFiring(int player);

// gameControls.cpp:135-161 -- getFireStartedAndBumpFireCounter. The C#
// default (player = kPlayer1) has no real caller (grep-verified) -- player
// is a required argument here.
bool swosGetFireStartedAndBumpFireCounter(bool currentFire, int player);

// gameControls.cpp:163-190 -- eventsToDirection.
int16_t swosEventsToDirection(int32_t events);

// gameControls.cpp:192-216 -- directionToEvents.
int32_t swosDirectionToEvents(int16_t direction);

// gameControls.cpp:218-221 -- isAnyPlayerFiring.
bool swosIsAnyPlayerFiring(void);

// gameLoop.cpp:155-158 -- requestFadeAndInstantReplay. Real Memory write
// (the replay-request flag); the replay *consumer* isn't ported yet, but
// the producer side is a genuine, non-omittable state change.
void swosInputControlsRequestFadeAndInstantReplay(void);

// gameLoop.cpp:150-153 -- requestFadeAndSaveReplay. Same rationale.
void swosInputControlsRequestFadeAndSaveReplay(void);
