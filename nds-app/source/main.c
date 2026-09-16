// swos-vm-c step 12: DS platform layer for the mechanically-ported VM
// (../../src). This file is rendering/input plumbing ONLY -- all match
// simulation is the real ported code (game loop, AI, physics, referee,
// etc.), driven once per frame via swosGameLoopTick(). See
// match_bootstrap.h for the one deliberate gap (roster/formation seeding,
// since real team-file loading and the kickoff entrance ceremony were
// never ported -- out of scope for this project, see its header comment).
//
// Rendering pattern and graphics assets are copied from
// ../../swos-ds/source/main.c (a proven, working BlocksDS/GL2D setup) --
// swos-ds itself is untouched. Its OWN gameplay code is not used here.
//
// PHASE 4 (2026-09-16 -- see ../../README.md "Status: Phase 4"): the old
// hand-rolled running/standing animator (playerAnimGetFrame()/s_animTick[],
// direction+tick driven, no real VM state at all) is GONE. Every sprite
// drawn below comes from ONE call to swosRenderBuildFrame() (Phase 2's
// portable RenderCommand layer) per frame -- the exact same function the
// sprite laboratory (../../sprite-lab) uses to verify frame/anchor
// resolution, so this loop and that tool are provably consuming identical
// decisions, never picking animations separately. A command whose
// imageResolved is false (most goalkeeper/referee/bench/tackle/header/
// injury/celebration frames today -- see swos_render_frames.h's own
// scope note) is skipped rather than drawn with a wrong or substituted
// frame; swosRenderFramesLookup() already logs an explicit "MISSING IMAGE"
// line for it (this repo's console, via consoleDemoInit below).
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include <gl2d.h>
#include <nds.h>

#include "match_bootstrap.h"

#include "player_atlas.h"
#include "player_atlas_texture.h"
#include "player_atlas_team2_texture.h"
#include "keeper_atlas.h"
#include "keeper_atlas_texture.h"
#include "ball_atlas.h"
#include "ball_atlas_texture.h"
#include "pitch_map.h"
#include "pitch_tiles_texture.h"

#include "swos_ball_sprite.h"
#include "swos_game_loop.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"
#include "swos_render_commands.h"
#include "swos_render_frames.h"

#define SCREEN_W 256
#define SCREEN_H 192
#define WORLD_W  672
#define WORLD_H  880  // VM/gameplay coordinate range is y=0..879.
#define PITCH_BITMAP_WORLD_Y 16 // PITCH*.DAT row 0 represents VM world y=16.

// DS-adapter-only camera: an 8x-lerp follow-the-ball scroll, clamped to the
// pitch bounds. Deliberately NOT using the ported swos_camera.c's own
// camera state here -- that camera's clipping is tuned for the original
// game's own viewport, whose exact dimensions/convention weren't verified
// against this 256x192 DS screen, so reusing swos-ds's own already-proven
// simple follow-cam (copied from its main.c) is the lower-risk choice for
// what is purely a rendering concern. swos_camera.c still runs every tick
// as part of the real simulation (CoreGameUpdate calls it internally) --
// its state just isn't used for the on-screen scroll offset.
typedef struct { int32_t x, y; } Camera;

static glImage playerSprites[PLAYER_NUM_IMAGES];       // home team (global 341-441)
static glImage playerSpritesTeam2[PLAYER_NUM_IMAGES];  // away team (global 644-744, Phase 4's own extraction)
static glImage keeperSprites[KEEPER_NUM_IMAGES];        // both goalkeepers (global 947-1062 / 1063-1178, Phase 5)
static glImage ballSprites[BALL_NUM_IMAGES];
static glImage pitchTiles[256];

static void cameraUpdate(Camera *c, int followX, int followY)
{
    int32_t tx = inttof32(followX) - inttof32(SCREEN_W / 2);
    int32_t ty = inttof32(followY) - inttof32(SCREEN_H / 2);
    int32_t mx = inttof32(WORLD_W - SCREEN_W), my = inttof32(WORLD_H - SCREEN_H);
    if (tx < 0) tx = 0;
    if (tx > mx) tx = mx;
    if (ty < 0) ty = 0;
    if (ty > my) ty = my;
    c->x += (tx - c->x) >> 3;
    c->y += (ty - c->y) >> 3;
}

static void drawPitch(int cx, int cy)
{
    // The 672x848 PITCH bitmap is not rooted at VM world y=0. Original
    // SWOS has one invisible 16-pixel tile row above it, so bitmap row 0
    // maps to world y=16 (docs/SWOS/pitch.txt). Keep camera/player/ball in
    // VM coordinates and apply the offset only while drawing the bitmap.
    int pitchY = cy - PITCH_BITMAP_WORLD_Y;
    int firstX = cx >> 4, firstY = pitchY >> 4;
    int ox = -(cx & 15), oy = -(pitchY & 15);
    for (int r = 0; r < 13 && firstY + r < PITCH_MAP_HEIGHT; r++)
        for (int col = 0; col < 17 && firstX + col < PITCH_MAP_WIDTH; col++)
        {
            if (firstY + r < 0 || firstX + col < 0)
                continue;
            int tile = PITCH_MAP[(firstY + r) * PITCH_MAP_WIDTH + firstX + col];
            glSprite(ox + col * 16, oy + r * 16, GL_FLIP_NONE, &pitchTiles[tile]);
        }
}

// The ONE place this adapter turns a resolved SwosRenderCommand into a
// glSprite() call -- picks which of the (up to) three loaded atlases to
// bind based on cmd->atlasId (SWOS_RENDER_ATLAS_*, see swos_render_frames.h),
// never re-deriving a frame index from direction/tick/imageIndex itself.
static void drawResolvedCommand(const SwosRenderCommand *cmd)
{
    glImage *sheet;
    switch (cmd->atlasId)
    {
        case SWOS_RENDER_ATLAS_PLAYER:       sheet = playerSprites; break;
        case SWOS_RENDER_ATLAS_PLAYER_TEAM2: sheet = playerSpritesTeam2; break;
        case SWOS_RENDER_ATLAS_KEEPER:       sheet = keeperSprites; break;
        case SWOS_RENDER_ATLAS_BALL:         sheet = ballSprites; break;
        default: return; // SWOS_RENDER_ATLAS_NONE -- imageResolved is already false for this, caller filters it out
    }
    int drawX = cmd->screenX - cmd->anchorX;
    int drawY = cmd->screenY - cmd->anchorY;
    if (cmd->kind == SWOS_RENDER_KIND_BALL)
        drawY -= cmd->worldZ; // height lift -- shadow (worldZ always 0) stays on the ground
    glSprite(drawX, drawY, GL_FLIP_NONE, &sheet[cmd->atlasFrame]);
}

int main(int argc, char **argv)
{
    consoleDemoInit();
    videoSetMode(MODE_0_3D);
    glScreen2D();
    vramSetBankA(VRAM_A_TEXTURE);
    vramSetBankB(VRAM_B_TEXTURE);
    vramSetBankE(VRAM_E_TEX_PALETTE);

    glLoadSpriteSet(playerSprites, PLAYER_NUM_IMAGES, PLAYER_texcoords,
                     GL_RGB256, 256, 256,
                     TEXGEN_TEXCOORD | GL_TEXTURE_COLOR0_TRANSPARENT, 256,
                     player_atlas_texturePal, player_atlas_textureBitmap);
    glLoadSpriteSet(playerSpritesTeam2, PLAYER_NUM_IMAGES, PLAYER_texcoords,
                     GL_RGB256, 256, 256,
                     TEXGEN_TEXCOORD | GL_TEXTURE_COLOR0_TRANSPARENT, 256,
                     player_atlas_team2_texturePal, player_atlas_team2_textureBitmap);
    glLoadSpriteSet(keeperSprites, KEEPER_NUM_IMAGES, KEEPER_texcoords,
                     GL_RGB256, 256, 128,
                     TEXGEN_TEXCOORD | GL_TEXTURE_COLOR0_TRANSPARENT, 256,
                     keeper_atlas_texturePal, keeper_atlas_textureBitmap);
    glLoadSpriteSet(ballSprites, BALL_NUM_IMAGES, BALL_texcoords,
                     GL_RGB256, 32, 32,
                     TEXGEN_TEXCOORD | GL_TEXTURE_COLOR0_TRANSPARENT, 256,
                     ball_atlas_texturePal, ball_atlas_textureBitmap);
    glLoadTileSet(pitchTiles, 16, 16, 256, 256, GL_RGB256, 256, 256,
                  TEXGEN_TEXCOORD, 256,
                  pitch_tiles_texturePal, pitch_tiles_textureBitmap);

    dsBootstrapMatch();

    Camera camera;
    camera.x = inttof32(swosBallSpriteXPixels()) - inttof32(SCREEN_W / 2);
    camera.y = inttof32(swosBallSpriteYPixels()) - inttof32(SCREEN_H / 2);

    while (1)
    {
        swiWaitForVBlank();
        scanKeys();
        uint16_t held = keysHeld();
        if (held & KEY_START)
            break;

        // The real ported game loop -- AI, physics, referee, ball, camera
        // state machine. Everything below this call is rendering only.
        swosGameLoopTick();

        int ballX = swosBallSpriteXPixels();
        int ballY = swosBallSpriteYPixels();
        int ballZ = swosBallSpriteZPixels();

        cameraUpdate(&camera, ballX, ballY);
        int camX = f32toint(camera.x), camY = f32toint(camera.y);

        SwosRenderCommand cmds[SWOS_RENDER_MAX_COMMANDS];
        int cmdCount = swosRenderBuildFrame(cmds, SWOS_RENDER_MAX_COMMANDS, camX, camY);
        swosRenderSortCommands(cmds, cmdCount); // real SWOS behavior: gameSprites.cpp's
        // own sortDisplaySprites() ascending-Y-sorts every visible sprite before every
        // draw (see README's own "was this in the original game?" note) -- without this,
        // commands draw in slot order, so a player nearer the bottom of the pitch (should
        // draw in front) can be overdrawn by one further up whenever they cross paths.
        int missing = 0;
        for (int i = 0; i < cmdCount; i++)
            if (!cmds[i].imageResolved)
                missing++;

        consoleClear();
        printf("swos-vm-c DS adapter (step 12, Phase 4 renderer)\n"
               "AI vs AI -- mechanical VM port\n"
               "ball %d,%d z%d  sprites %d/%d resolved\n"
               "START to exit",
               ballX, ballY, ballZ, cmdCount - missing, cmdCount);

        glBegin2D();
        drawPitch(camX, camY);

        for (int i = 0; i < cmdCount; i++)
            if (cmds[i].imageResolved)
                drawResolvedCommand(&cmds[i]);

        glEnd2D();
        glFlush(0);
    }

    return 0;
}
