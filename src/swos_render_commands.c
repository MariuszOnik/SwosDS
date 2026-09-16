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

static void fillBallCommand(SwosRenderCommand *cmd, SwosRenderKind kind,
                             int32_t cameraX, int32_t cameraY) {
    cmd->kind = kind;
    cmd->layer = (kind == SWOS_RENDER_KIND_BALL_SHADOW) ? SWOS_RENDER_LAYER_SHADOW : SWOS_RENDER_LAYER_SPRITE;
    cmd->slot = -1;
    cmd->globalImageIndex = (kind == SWOS_RENDER_KIND_BALL_SHADOW)
        ? BALL_SHADOW_GLOBAL_INDEX
        : swosBallSpriteImageIndex();
    resolveFrame(cmd);
    cmd->worldX = swosBallSpriteXPixels();
    cmd->worldY = swosBallSpriteYPixels();
    cmd->worldZ = (kind == SWOS_RENDER_KIND_BALL) ? swosBallSpriteZPixels() : 0;
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
