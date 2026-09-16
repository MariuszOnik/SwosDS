// PHASE 2 (RenderCommand layer) + PHASE 3 (full atlas mapping). See
// swos_render_commands.h for the full scope note -- no Memory writes, no
// SDL/libnds, no gameplay decisions.
#include "swos_render_commands.h"

#include "swos_ball_sprite.h"
#include "swos_player_sprite.h"
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

void swosRenderSortCommands(SwosRenderCommand *commands, int count) {
    // Stable insertion sort -- see header note on why (small n, stability
    // over speed).
    for (int i = 1; i < count; i++) {
        SwosRenderCommand key = commands[i];
        int j = i - 1;
        while (j >= 0 && commands[j].sortKey > key.sortKey) {
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

    for (int slot = 0; slot < PLSPR_TOTAL_SLOTS; slot++) {
        int16_t teamNumber = swosPlayerSpriteTeamNumber(slot);
        if (teamNumber != 1 && teamNumber != 2)
            continue;
        if (count >= maxCommands)
            break;
        fillPlayerCommand(&outCommands[count], slot, cameraX, cameraY);
        count++;
    }

    return count;
}
