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
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include <gl2d.h>
#include <nds.h>

#include "match_bootstrap.h"
#include "player_anim.h"

#include "player_atlas.h"
#include "player_atlas_texture.h"
#include "player_frame_centers.h"
#include "ball_atlas.h"
#include "ball_atlas_texture.h"
#include "pitch_map.h"
#include "pitch_tiles_texture.h"

#include "swos_ball_sprite.h"
#include "swos_game_loop.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"

#define SCREEN_W 256
#define SCREEN_H 192
#define WORLD_W  672
#define WORLD_H  880  // VM/gameplay coordinate range is y=0..879.
#define PITCH_BITMAP_WORLD_Y 16 // PITCH*.DAT row 0 represents VM world y=16.
#define BALL_HALF_SIZE 2

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

static glImage playerSprites[PLAYER_NUM_IMAGES], ballSprites[BALL_NUM_IMAGES], pitchTiles[256];
static int s_animTick[PLSPR_TOTAL_SLOTS];

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

        consoleClear();
        printf("swos-vm-c DS adapter (step 12)\n"
               "AI vs AI -- mechanical VM port\n"
               "ball %d,%d z%d\n"
               "START to exit",
               ballX, ballY, ballZ);

        glBegin2D();
        drawPitch(camX, camY);

        glSprite(ballX - camX - BALL_HALF_SIZE, ballY - camY - BALL_HALF_SIZE,
                 GL_FLIP_NONE, &ballSprites[BALL_shadow_png]);
        glSprite(ballX - camX - BALL_HALF_SIZE, ballY - ballZ - camY - BALL_HALF_SIZE,
                 GL_FLIP_NONE, &ballSprites[0]);

        for (int slot = 0; slot < PLSPR_TOTAL_SLOTS; slot++)
        {
            int16_t teamNumber = swosPlayerSpriteTeamNumber(slot);
            if (teamNumber != 1 && teamNumber != 2)
                continue;

            int spriteAddr = swosPlayerSpriteBase(slot);
            int px = swosPlayerSpriteXPixels(slot);
            int py = swosPlayerSpriteYPixels(slot);
            int dir = swosPlayerSpriteDirection(slot);
            bool moving = swosReadSignedWord(spriteAddr + PLSPR_OFF_IS_MOVING) != 0;

            if (moving)
                s_animTick[slot]++;
            int frame = playerAnimGetFrame(dir, s_animTick[slot], moving);

            glSprite(px - camX - PLAYER_FRAME_CENTER_X[frame],
                     py - camY - PLAYER_FRAME_CENTER_Y[frame],
                     GL_FLIP_NONE, &playerSprites[frame]);
        }

        glEnd2D();
        glFlush(0);
    }

    return 0;
}
