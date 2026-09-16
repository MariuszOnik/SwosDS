// PHASE 2 (RenderCommand layer) + PHASE 3 (full atlas mapping), 2026-09-16
// -- see README.md "Status: Phase 2"/"Status: Phase 3" and the top-level
// plan this session is following. Portable module with NO SDL/libnds
// dependency -- only the already-portable VM headers
// (swos_ball_sprite.h/swos_player_sprite.h, pure read accessors) and
// swos_render_frames.h (also portable -- a generated data table + a pure
// lookup function). Builds on desktop (this repo's normal `make test`
// host) and on ARM (nds-app's BlocksDS build, which globs `../src`
// automatically).
//
// Purpose: read a VM Memory snapshot and produce an ordered list of
// RenderCommand values -- ball, shadow, the two static goal frames, the
// referee (when active), all 22 players -- so that EVERY
// renderer (nds-app's GL2D loop, a future SDL frontend, the "sprite
// laboratory" Phase 4 adds) consumes the exact same decisions instead of
// each platform separately picking its own animation frame or draw order.
// This module does NOT:
//   - write to Memory (read-only, per the plan's explicit rule),
//   - decide gameplay (no AI, no physics, no state transitions),
//   - depend on GL2D, SDL2, or any platform graphics API,
//   - "fix" the existing hand-rolled running/standing animator
//     (nds-app/source/player_anim.c) -- that file is untouched by this
//     module and still owns its own frame choice for now; Phase 4 is where
//     the DS adapter switches to reading real VM animation state instead.
//
// Atlas/frame/anchor resolution goes through swos_render_frames.h's
// swosRenderFramesLookup() -- the ONE place a global image index turns
// into geometry, never scattered `imageIndex - 341`-style arithmetic in
// this file. Real anchor points (centerX/centerY) and real geometry are
// known for all 1334 global indices (real data from the user's GOG .DAT
// files); an actual pixel TEXTURE only exists for the ~106 indices Phase 3
// found already extracted (the 101-frame player atlas, the 5-frame ball
// atlas) -- see SwosRenderCommand.imageResolved's own comment for how that
// distinction surfaces here. Anything genuinely unmapped logs an explicit
// "MISSING IMAGE" line (inside swosRenderFramesLookup) rather than
// silently drawing a standing frame.
#pragma once

#include <stdbool.h>
#include <stdint.h>

// Draw layer, coarse-grained. PHASE 5 BUGFIX ("Goal Post Split") checked
// the real engine before extending this: swos-port/src/sprites/
// gameSprites.cpp's drawSprites() has NO separate foreground/background
// split at all -- the two goal-frame sprites are just two more entries in
// the SAME flat, plain-worldY-sorted list as the ball and every player, at
// a fixed position (see swos_render_commands.c's fillGoalCommand). So
// SWOS_RENDER_LAYER_FOREGROUND below turned out to be unneeded -- kept
// as an unused, reserved value rather than silently repurposed, so this
// correction is visible in the diff instead of erasing the earlier
// (wrong) assumption.
typedef enum {
    SWOS_RENDER_LAYER_PITCH = 0,   // not emitted by this module (nds-app draws its own tilemap); reserved so callers can sort a full frame consistently
    SWOS_RENDER_LAYER_SHADOW = 1,
    SWOS_RENDER_LAYER_SPRITE = 2,  // ball + players + goal frames, all depth-sortable by worldY together
    SWOS_RENDER_LAYER_FOREGROUND = 3, // reserved, currently unused -- see comment above
} SwosRenderLayer;

typedef enum {
    SWOS_RENDER_KIND_BALL = 0,
    SWOS_RENDER_KIND_BALL_SHADOW = 1,
    SWOS_RENDER_KIND_PLAYER = 2,
    SWOS_RENDER_KIND_GOAL = 3,
    SWOS_RENDER_KIND_REFEREE = 4,
} SwosRenderKind;

typedef struct {
    SwosRenderKind kind;
    SwosRenderLayer layer;

    // Which VM slot this command came from: 0..21 for players
    // (PLSPR_TOTAL_SLOTS), -1 for the ball/shadow/goal frames/referee
    // (each a single fixed sprite, not a pool).
    int slot;

    // Raw VM state, unmodified -- the actual PlayerSprite.imageIndex /
    // BallSprite.imageIndex field (real per-tick animation state already
    // written by the ported SpriteUpdate/PlayerActions system -- NOT the
    // hand animator's guess). For SWOS_RENDER_KIND_PLAYER this is read
    // straight from Memory but NOT currently used to pick atlasFrame (see
    // header note) -- carried through so Phase 3/4 can switch over without
    // this module's shape changing again.
    int32_t globalImageIndex;

    // Resolved atlas reference. atlasId is always 0 in this phase (single
    // player atlas); atlasFrame is a LOCAL index into that atlas, or -1 if
    // imageResolved is false.
    int16_t atlasId;
    int16_t atlasFrame;
    // false in TWO distinct cases, both handled the same way by a renderer
    // (nothing to draw, never a silent standing-frame substitute):
    //   1. globalImageIndex has no real sprite at all (genuinely invalid --
    //      swos_render_frames.h's swosRenderFramesLookup() already logged
    //      an explicit "MISSING IMAGE" line for this case);
    //   2. it IS a real, valid SWOS sprite (real anchorX/anchorY below) but
    //      only its geometry has been mapped, not a pixel texture -- most
    //      referee/bench/tackle/header/injury/celebration frames today
    //      (goalkeepers got a real atlas in Phase 5; see swos_render_frames.h's
    //      SWOS_RENDER_ATLAS_NONE).
    // atlasId/atlasFrame are only meaningful when this is true.
    bool imageResolved;

    // World-space position, whole pixels (same convention as
    // swosPlayerSpriteXPixels()/swosBallSpriteXPixels() -- Q16.16 truncated
    // to its integer part, matching the VM's own on-screen coordinate
    // space). worldZ is the ball's height lift only (0 for players).
    int32_t worldX, worldY, worldZ;

    // Screen-space position: worldX/Y minus the camera offset the caller
    // passed to swosRenderBuildFrame(), via swosRenderWorldToScreen(). Does
    // NOT subtract worldZ (the caller applies height-lift as it sees fit --
    // nds-app currently offsets Y by ballZ only for the ball sprite itself,
    // never for its shadow).
    int32_t screenX, screenY;

    // Sprite-local pivot offset -- the real anchor point (sprites.txt's
    // "x center"/"y center", the same point the original engine's ball
    // bounces off / draws the sprite anchored at) from
    // RENDER_FRAMES[globalImageIndex], regardless of whether a pixel
    // texture exists yet for this frame (0,0 only when the lookup itself
    // failed -- see imageResolved).
    int16_t anchorX, anchorY;

    // Depth-sort key: plain worldY (see swosRenderSortKeyForWorldY).
    // "Shadow before object" is handled by .layer, not this field (see
    // SwosRenderLayer's own comment) -- confirmed correct in Phase 5
    // bugfix's shadow-ordering fix and again by "Goal Post Split", which
    // found the real engine needs no foot-point or goal-layer special case
    // beyond plain worldY either.
    int32_t sortKey;

    // team: 0 (ball), 1 (top), 2 (bottom) -- mirrors PlayerSprite.OffTeamNumber.
    // palette: always equal to team for now (Phase 6 builds real per-team
    // palettes; until then this is just an identity placeholder a caller
    // could use to pick SOMETHING team-distinguishing, not a real palette id).
    int16_t team;
    int16_t palette;
} SwosRenderCommand;

// Ball + shadow + 2 goal frames + referee + up to 22 players = 27
// commands, fixed upper bound so callers can size a stack array without a
// separate count query. The referee slot is only ever filled when
// swosRefereeVisible() is true (see swosRenderBuildFrame) -- most ticks it
// stays unused, this is just the worst case.
#define SWOS_RENDER_MAX_COMMANDS 27

// Pure coordinate transform: worldX/Y minus cameraX/Y. No clamping, no
// screen-bounds logic (a renderer decides whether/how to cull
// off-screen commands) -- deliberately trivial so it is exhaustively
// testable and never a source of platform-specific drift.
void swosRenderWorldToScreen(int32_t worldX, int32_t worldY,
                              int32_t cameraX, int32_t cameraY,
                              int32_t *outScreenX, int32_t *outScreenY);

// This phase's depth-sort key: identity on worldY. Exists as its own
// function (rather than inlining worldY as the key) so Phase 5 can replace
// the RULE here without touching call sites or the sort itself.
int32_t swosRenderSortKeyForWorldY(int32_t worldY);

// Stable ascending sort by .sortKey (insertion sort -- count is at most
// SWOS_RENDER_MAX_COMMANDS, so O(n^2) is irrelevant, and stability matters
// more than speed: two commands with equal sortKey must keep their
// original relative order, e.g. a shadow immediately before the object it
// belongs to).
void swosRenderSortCommands(SwosRenderCommand *commands, int count);

// Reads the ball, its shadow, the two static goal frames (fixed real-world
// position, not read from Memory at all), the referee (only when
// swosRefereeVisible() -- most ticks it's off, this is a real VM state
// check, not a stub), and all 22 player slots from the CURRENT VM Memory
// state (via the existing ported BallSprite/PlayerSprite/Referee read
// accessors -- no Memory writes) and writes up to SWOS_RENDER_MAX_COMMANDS
// entries into outCommands, world-space and screen-space (against
// cameraX/cameraY) but NOT yet sorted -- call swosRenderSortCommands()
// separately if depth order is needed. Player slots whose team number is
// neither 1 nor 2 (unused slots) are skipped, matching nds-app/main.c's own
// existing filter. Returns the number of commands written.
int swosRenderBuildFrame(SwosRenderCommand *outCommands, int maxCommands,
                          int32_t cameraX, int32_t cameraY);
