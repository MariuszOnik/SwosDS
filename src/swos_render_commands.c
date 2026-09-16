// PHASE 2 (RenderCommand layer). See swos_render_commands.h for the full
// scope note -- no Memory writes, no SDL/libnds, no gameplay decisions.
#include "swos_render_commands.h"

#include "swos_ball_sprite.h"
#include "swos_player_sprite.h"

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

// Matches nds-app/source/player_anim.h's own documented shift for THIS
// atlas only (101 frames, global indices 341..441 per that file's header
// comment) -- not a new mapping, just the same one-line rule already
// established, applied here instead of duplicated per renderer. Anything
// outside that range is unresolved -- see swos_render_commands.h's header
// note on why this is temporary and deliberately not a full table.
#define PLAYER_ATLAS_GLOBAL_BASE 341
#define PLAYER_ATLAS_FRAME_COUNT 101

static void resolvePlayerAtlasFrame(int32_t globalImageIndex, int16_t *outAtlasId,
                                     int16_t *outAtlasFrame, bool *outResolved) {
    int32_t local = globalImageIndex - PLAYER_ATLAS_GLOBAL_BASE;
    if (local >= 0 && local < PLAYER_ATLAS_FRAME_COUNT) {
        *outAtlasId = 0;
        *outAtlasFrame = (int16_t)local;
        *outResolved = true;
    } else {
        *outAtlasId = 0;
        *outAtlasFrame = -1;
        *outResolved = false;
    }
}

static void fillBallCommand(SwosRenderCommand *cmd, SwosRenderKind kind,
                             int32_t cameraX, int32_t cameraY) {
    cmd->kind = kind;
    cmd->layer = (kind == SWOS_RENDER_KIND_BALL_SHADOW) ? SWOS_RENDER_LAYER_SHADOW : SWOS_RENDER_LAYER_SPRITE;
    cmd->slot = -1;
    cmd->globalImageIndex = swosBallSpriteImageIndex();
    cmd->atlasId = 0;
    cmd->atlasFrame = -1;
    cmd->imageResolved = false; // ball atlas resolution is not part of this phase's scope (nds-app still special-cases the ball sprite directly)
    cmd->worldX = swosBallSpriteXPixels();
    cmd->worldY = swosBallSpriteYPixels();
    cmd->worldZ = (kind == SWOS_RENDER_KIND_BALL) ? swosBallSpriteZPixels() : 0;
    swosRenderWorldToScreen(cmd->worldX, cmd->worldY, cameraX, cameraY, &cmd->screenX, &cmd->screenY);
    cmd->anchorX = 0;
    cmd->anchorY = 0;
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
    resolvePlayerAtlasFrame(cmd->globalImageIndex, &cmd->atlasId, &cmd->atlasFrame, &cmd->imageResolved);
    cmd->worldX = swosPlayerSpriteXPixels(slot);
    cmd->worldY = swosPlayerSpriteYPixels(slot);
    cmd->worldZ = 0;
    swosRenderWorldToScreen(cmd->worldX, cmd->worldY, cameraX, cameraY, &cmd->screenX, &cmd->screenY);
    cmd->anchorX = 0;
    cmd->anchorY = 0;
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
