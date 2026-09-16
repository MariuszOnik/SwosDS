// PHASE 2 (RenderCommand layer) + PHASE 3 (full atlas mapping). See
// swos_render_commands.h for the full scope note -- no Memory writes, no
// SDL/libnds, no gameplay decisions.
#include "swos_render_commands.h"

#include "swos_ball_sprite.h"
#include "swos_game_sprites.h"
#include "swos_player_sprite.h"
#include "swos_referee.h"
#include "swos_render_frames.h"

void swosRenderWorldToScreen(int32_t worldX, int32_t worldY,
                              int32_t cameraX, int32_t cameraY,
                              int32_t *outScreenX, int32_t *outScreenY) {
    *outScreenX = worldX - cameraX;
    *outScreenY = worldY - cameraY;
}

int32_t swosRenderSortKeyForWorldY(int32_t worldY) {
    return worldY;
}

// true if `a` must draw strictly before `b`: layer first (a shadow is
// SWOS_RENDER_LAYER_SHADOW, always below SWOS_RENDER_LAYER_SPRITE, no
// matter what its worldY-derived sortKey says -- see fillBallCommand's own
// BALL_SHADOW_OFFSET_Y comment: the real engine's updateBallShadow() gives
// the shadow sprite its own large negative Z specifically to keep it
// sorting before the ball despite sharing the same Y-sort as everything
// else ("the shadow sprite itself stays on a fixed drawing layer" --
// swos-port/src/game/ball/ball.cpp:1079); this port's own
// BALL_SHADOW_OFFSET_Y was recalibrated to 0 for a DIFFERENT reason (the
// on-screen diagonal offset magnitude, per the Phase 4 bugfix), which as a
// side effect put the shadow's sortKey ABOVE the ball's -- the `layer`
// field exists precisely to keep those two concerns independent instead of
// overloading one Y offset for both), then sortKey within a layer.
static bool commandOrdersBefore(const SwosRenderCommand *a, const SwosRenderCommand *b) {
    if (a->layer != b->layer)
        return a->layer < b->layer;
    return a->sortKey < b->sortKey;
}

void swosRenderSortCommands(SwosRenderCommand *commands, int count) {
    // Stable insertion sort -- see header note on why (small n, stability
    // over speed).
    for (int i = 1; i < count; i++) {
        SwosRenderCommand key = commands[i];
        int j = i - 1;
        while (j >= 0 && commandOrdersBefore(&key, &commands[j])) {
            commands[j + 1] = commands[j];
            j--;
        }
        commands[j + 1] = key;
    }
}

// PHASE 3: the ONE place a global image index is resolved -- via
// swos_render_frames.h's RENDER_FRAMES table (real geometry/anchor for all
// 1334 ordinals, real atlas reference for the ~106 that have a built
// texture today). No scattered `imageIndex - 341` arithmetic anywhere else
// in this file, per the plan's explicit rule. A `false` return (index out
// of range or genuinely unmapped) logs its own "MISSING IMAGE" line inside
// swosRenderFramesLookup and leaves the command's fields at safe defaults
// here -- never a silent standing-frame substitute.
static void resolveFrame(SwosRenderCommand *cmd) {
    SwosRenderFrameInfo info;
    if (swosRenderFramesLookup(cmd->globalImageIndex, &info)) {
        cmd->atlasId = info.atlasId;
        cmd->atlasFrame = info.atlasFrame;
        cmd->imageResolved = (info.atlasId != SWOS_RENDER_ATLAS_NONE);
        cmd->anchorX = info.centerX;
        cmd->anchorY = info.centerY;
    } else {
        cmd->atlasId = SWOS_RENDER_ATLAS_NONE;
        cmd->atlasFrame = -1;
        cmd->imageResolved = false;
        cmd->anchorX = 0;
        cmd->anchorY = 0;
    }
}

// The real match ball's 4 animation frames are global 1179-1182
// (BallSprite.imageIndex, a real per-tick VM field); its shadow is a
// SINGLE fixed sprite, global 1183 -- not an animated/VM-tracked field at
// all (ball_atlas.h only has one BALL_shadow_png frame; confirmed by
// ../../swos-ds/tools/extract_ball_frames.py, which hardcodes
// BALL_SHADOW_INDEX = 1183 rather than reading it from any Sprite struct).
// So the shadow command's globalImageIndex is this constant, NOT
// swosBallSpriteImageIndex() -- reusing the ball's own frame index for the
// shadow would have been wrong (a Phase 2 oversight this phase fixes,
// harmless at the time since ball atlas resolution wasn't implemented yet).
#define BALL_SHADOW_GLOBAL_INDEX 1183

// PHASE 4 BUGFIX (2026-09-16, reported after the first real melonDS run):
// the shadow's real sprite geometry (RENDER_FRAMES[1179..1183]) is
// IDENTICAL to the ball's own (4x4, anchor (1,3) -- verified against the
// generated table) -- so placing the shadow at the ball's own worldX/worldY
// put them at the exact same screen rect whenever the ball is near the
// ground, and the opaque ball (drawn second) fully covered the shadow.
// The real engine never draws them at the same spot to begin with: see
// ../../swos-port/src/game/ball/ball.cpp:updateBallShadow -- the shadow is
// diagonally offset from the ball, growing with height (the original's own
// comment: "Height shifts the shadow diagonally to fake the original
// game's oblique projection"):
//   shadowX = ballX + ballZ/2 + 1
//   shadowY = ballY + ballZ/4 + 1 + BALL_SHADOW_OFFSET_Y
//
// BALL_SHADOW_OFFSET_Y itself is NOT swos-port's literal `kBallShadowZ`
// (-10): applied as-is, it put the shadow ~9 whole pixels above a 4x4-pixel
// ball at rest -- confirmed too large on a real melonDS run (reported:
// "two ball diameters too much"), most likely because swos-port's own
// internal unit for that constant doesn't map 1:1 onto this port's already
// lockstep-verified whole-pixel `*Pixels()` accessors (a plausible,
// unconfirmed scale/rounding difference in how swos-port's own renderer
// consumes it, not something this project's ball-position simulation gets
// wrong -- that simulation is proven correct independently, see the Phase 1
// lockstep). Recalibrated to 0 against that real measurement, which also
// matches the ORIGINAL bug report's own wording ("cień powinien być lekko
// widoczny" -- the shadow should be SLIGHTLY visible, not offset by a large
// gap): at rest this nets to a 1px diagonal nudge (from the formula's own
// "+1" terms alone), growing with height exactly as the real formula's
// shape intends. The STRUCTURE is the real, ported formula; only this one
// constant was empirically corrected against real hardware, not "tuned
// until it looked right" from scratch.
#define BALL_SHADOW_OFFSET_Y (0)

static void fillBallCommand(SwosRenderCommand *cmd, SwosRenderKind kind,
                             int32_t cameraX, int32_t cameraY) {
    cmd->kind = kind;
    cmd->layer = (kind == SWOS_RENDER_KIND_BALL_SHADOW) ? SWOS_RENDER_LAYER_SHADOW : SWOS_RENDER_LAYER_SPRITE;
    cmd->slot = -1;
    cmd->globalImageIndex = (kind == SWOS_RENDER_KIND_BALL_SHADOW)
        ? BALL_SHADOW_GLOBAL_INDEX
        : swosBallSpriteImageIndex();
    resolveFrame(cmd);

    int32_t ballX = swosBallSpriteXPixels();
    int32_t ballY = swosBallSpriteYPixels();
    int32_t ballZ = swosBallSpriteZPixels();
    if (kind == SWOS_RENDER_KIND_BALL_SHADOW) {
        cmd->worldX = ballX + ballZ / 2 + 1;
        cmd->worldY = ballY + ballZ / 4 + 1 + BALL_SHADOW_OFFSET_Y;
        cmd->worldZ = 0;
    } else {
        cmd->worldX = ballX;
        cmd->worldY = ballY;
        cmd->worldZ = ballZ;
    }
    swosRenderWorldToScreen(cmd->worldX, cmd->worldY, cameraX, cameraY, &cmd->screenX, &cmd->screenY);
    cmd->sortKey = swosRenderSortKeyForWorldY(cmd->worldY);
    cmd->team = 0;
    cmd->palette = 0;
}

// PHASE 5 BUGFIX ("Goal Post Split", 2026-09-16). Real, fixed world
// position and ordinals -- NOT read from Memory at all, these sprites never
// move -- cross-checked against two independent real sources:
//   swos-port/src/sprites/gameSprites.cpp's initGameSprites():
//     kGoalX=300, kTopGoalY=129, kBottomGoalY=778,
//     kTopGoalSprite=1205, kBottomGoalSprite=1206.
//   swos-port/src/game/pitch/pitchConstants.h: kTopPitchLine=129 (matches
//     kTopGoalY exactly); kGoalX=300 sits inside the real 296-372 goal-post
//     X range there, and this port's own measured sprite width (73px, see
//     tools/extract_goal_atlas.py) is close to that same ~76px span.
// These two commands use plain SWOS_RENDER_LAYER_SPRITE, NOT a separate
// foreground layer -- gameSprites.cpp's drawSprites() has no such split
// (see SwosRenderLayer's own comment in the header): a fixed-position goal
// sprite occludes anything behind it purely because that object's own
// worldY sorts before the goal's fixed worldY, and draws on top of
// anything still in front of the goal line for the same reason.
#define SWOS_GOAL_WORLD_X 300
#define SWOS_GOAL_TOP_WORLD_Y 129
#define SWOS_GOAL_BOTTOM_WORLD_Y 778
#define SWOS_GOAL_TOP_GLOBAL_INDEX 1205
#define SWOS_GOAL_BOTTOM_GLOBAL_INDEX 1206

static void fillGoalCommand(SwosRenderCommand *cmd, bool top,
                             int32_t cameraX, int32_t cameraY) {
    cmd->kind = SWOS_RENDER_KIND_GOAL;
    cmd->layer = SWOS_RENDER_LAYER_SPRITE;
    cmd->slot = -1;
    cmd->globalImageIndex = top ? SWOS_GOAL_TOP_GLOBAL_INDEX : SWOS_GOAL_BOTTOM_GLOBAL_INDEX;
    resolveFrame(cmd);
    cmd->worldX = SWOS_GOAL_WORLD_X;
    cmd->worldY = top ? SWOS_GOAL_TOP_WORLD_Y : SWOS_GOAL_BOTTOM_WORLD_Y;
    cmd->worldZ = 0;
    swosRenderWorldToScreen(cmd->worldX, cmd->worldY, cameraX, cameraY, &cmd->screenX, &cmd->screenY);
    cmd->sortKey = swosRenderSortKeyForWorldY(cmd->worldY);
    cmd->team = 0;
    cmd->palette = 0;
}

// Referee: real per-tick simulated position/animation, already ported
// (swos_referee.c, VERIFIED_PC) and already ticked every frame by
// swosGameLoopTick() -> swosRefereeUpdateReferee(). Only ever emitted when
// swosRefereeVisible() is true -- the referee is off-pitch/invisible most
// of a match (only activated for a foul/card, see swosRefereeActivate),
// same "real VM state gate, not a stub" pattern as the team-number filter
// below for player slots.
static void fillRefereeCommand(SwosRenderCommand *cmd, int32_t cameraX, int32_t cameraY) {
    cmd->kind = SWOS_RENDER_KIND_REFEREE;
    cmd->layer = SWOS_RENDER_LAYER_SPRITE;
    cmd->slot = -1;
    cmd->globalImageIndex = swosRefereeImageIndex();
    resolveFrame(cmd);
    cmd->worldX = swosRefereeWorldX();
    cmd->worldY = swosRefereeWorldY();
    cmd->worldZ = swosRefereeWorldZ();
    swosRenderWorldToScreen(cmd->worldX, cmd->worldY, cameraX, cameraY, &cmd->screenX, &cmd->screenY);
    cmd->sortKey = swosRenderSortKeyForWorldY(cmd->worldY);
    cmd->team = 0;
    cmd->palette = 0;
}

// Corner flags: real per-tick position + wind-animation frame, already
// ported (swos_game_sprites.c, VERIFIED_PC) and already ticked every frame
// by swosGameLoopTick() -> swosGameSpritesUpdateCornerFlags(). Always
// visible (unlike the referee) -- all 4 exist for the whole match, no gate
// needed.
static void fillCornerFlagCommand(SwosRenderCommand *cmd, int index,
                                   int32_t cameraX, int32_t cameraY) {
    int x, y, imageIndex;
    swosGameSpritesGetCornerFlag(index, &x, &y, &imageIndex);

    cmd->kind = SWOS_RENDER_KIND_CORNER_FLAG;
    cmd->layer = SWOS_RENDER_LAYER_SPRITE;
    cmd->slot = index;
    cmd->globalImageIndex = imageIndex;
    resolveFrame(cmd);
    cmd->worldX = x;
    cmd->worldY = y;
    cmd->worldZ = 0;
    swosRenderWorldToScreen(cmd->worldX, cmd->worldY, cameraX, cameraY, &cmd->screenX, &cmd->screenY);
    cmd->sortKey = swosRenderSortKeyForWorldY(cmd->worldY);
    cmd->team = 0;
    cmd->palette = 0;
}

static void fillPlayerCommand(SwosRenderCommand *cmd, int slot,
                               int32_t cameraX, int32_t cameraY) {
    cmd->kind = SWOS_RENDER_KIND_PLAYER;
    cmd->layer = SWOS_RENDER_LAYER_SPRITE;
    cmd->slot = slot;
    cmd->globalImageIndex = swosPlayerSpriteImageIndex(slot);
    resolveFrame(cmd);
    cmd->worldX = swosPlayerSpriteXPixels(slot);
    cmd->worldY = swosPlayerSpriteYPixels(slot);
    cmd->worldZ = 0;
    swosRenderWorldToScreen(cmd->worldX, cmd->worldY, cameraX, cameraY, &cmd->screenX, &cmd->screenY);
    cmd->sortKey = swosRenderSortKeyForWorldY(cmd->worldY);
    cmd->team = swosPlayerSpriteTeamNumber(slot);
    cmd->palette = cmd->team;
}

int swosRenderBuildFrame(SwosRenderCommand *outCommands, int maxCommands,
                          int32_t cameraX, int32_t cameraY) {
    int count = 0;

    if (count < maxCommands) {
        fillBallCommand(&outCommands[count], SWOS_RENDER_KIND_BALL_SHADOW, cameraX, cameraY);
        count++;
    }
    if (count < maxCommands) {
        fillBallCommand(&outCommands[count], SWOS_RENDER_KIND_BALL, cameraX, cameraY);
        count++;
    }
    if (count < maxCommands) {
        fillGoalCommand(&outCommands[count], true, cameraX, cameraY);
        count++;
    }
    if (count < maxCommands) {
        fillGoalCommand(&outCommands[count], false, cameraX, cameraY);
        count++;
    }

    for (int slot = 0; slot < PLSPR_TOTAL_SLOTS; slot++) {
        int16_t teamNumber = swosPlayerSpriteTeamNumber(slot);
        if (teamNumber != 1 && teamNumber != 2)
            continue;
        if (count >= maxCommands)
            break;
        fillPlayerCommand(&outCommands[count], slot, cameraX, cameraY);
        count++;
    }

    if (count < maxCommands && swosRefereeVisible()) {
        fillRefereeCommand(&outCommands[count], cameraX, cameraY);
        count++;
    }

    for (int i = 0; i < 4; i++) {
        if (count >= maxCommands)
            break;
        fillCornerFlagCommand(&outCommands[count], i, cameraX, cameraY);
        count++;
    }

    return count;
}
