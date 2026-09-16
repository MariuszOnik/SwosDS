// Real MatchAudio.* call sites the C# port deliberately OMITTED (not
// stubbed -- see swos_ball_update.h/swos_game_loop.h/swos_game_time.h/
// swos_player_tackle.c's own "Audio omitted" comments, each one confirmed
// by reading the site, same standard as every other omission in this
// port). This header turns five of those sites into a real, portable hook
// point instead of leaving them permanently silent.
//
// Unlike the g_swosAiSetControlsDirectionHook/g_swosAiKickHook pattern in
// swos_player_controlled.h, this hook is genuinely OPTIONAL, not asserted
// non-null: those hooks gate mandatory gameplay logic (a null AI hook is a
// real bug), while audio is cosmetic -- desktop `make test` has no audio
// subsystem at all and must keep working with the hook unset. Firing an
// event never touches Memory or affects control flow either way, so
// leaving it unset is always a safe, silent no-op.
//
// MatchAudio.PlayKickSample has 8 separate real call sites
// (src/swos_player_actions.c x5, src/swos_player_update.c,
// src/swos_player_tackle.c x2, each already cited with its own
// "PlayKickSample() omitted (audio)" comment) -- all 8 wired to
// SWOS_AUDIO_EVENT_KICK below, same single sound for every real kick
// event, matching the original engine's own behavior (every one of those
// 8 call sites played the exact same kickx.raw).
#pragma once

typedef enum {
    SWOS_AUDIO_EVENT_BALL_BOUNCE = 0,   // ball.cpp:537-547, src/swos_ball_update.c -- the dz<=40960 gate
    SWOS_AUDIO_EVENT_GOAL = 1,          // result.cpp, src/swos_result.c's swosResultRegisterScorer
    SWOS_AUDIO_EVENT_FOUL_WHISTLE = 2,  // updatePlayers.cpp:13404+, src/swos_player_tackle.c's testFoulForPenaltyAndFreeKick
    SWOS_AUDIO_EVENT_RESTART_WHISTLE = 3, // gameLoop.cpp:1811-1846 (mode8), src/swos_game_loop.c
    SWOS_AUDIO_EVENT_END_GAME_WHISTLE = 4, // swos.asm:112106-112139, src/swos_game_time.c
    SWOS_AUDIO_EVENT_KICK = 5,          // player.cpp PlayKickSample, 8 real call sites -- see swos_player_actions.c/swos_player_update.c/swos_player_tackle.c
} SwosAudioEvent;

// Platform hook, NULL by default. nds-app sets this once at startup to a
// real mmEffect()-calling function; desktop tests leave it unset.
typedef void (*SwosAudioEventHook)(SwosAudioEvent event);
extern SwosAudioEventHook g_swosAudioEventHook;

void swosAudioSetEventHook(SwosAudioEventHook hook);

// Calls g_swosAudioEventHook(event) if set; no-op otherwise. Called from
// the exact Memory/control-flow point each real MatchAudio.* call
// happened at in the original C++/C# -- never invented timing.
void swosAudioFireEvent(SwosAudioEvent event);
