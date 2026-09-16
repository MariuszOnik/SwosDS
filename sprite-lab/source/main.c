// PHASE 4 "sprite laboratory" (2026-09-16 -- see ../README.md "Status:
// Phase 4"). Standalone SDL2 frame browser: step through global sprite
// indices one at a time, see the REAL pixels (when a texture exists) drawn
// at their REAL anchor point against a fixed on-screen reference position,
// with a cross marking that reference point -- so a wrong anchor or a
// wrong atlas slice is visually obvious before the same RENDER_FRAMES
// lookup gets wired into GL2D on the DS. No VM/gameplay/Memory at all --
// this only calls swosRenderFramesLookup() (swos_render_frames.c, the same
// portable code the DS build links) against a fixed table, never ticks a
// match.
#include <SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "digit_font.h"
#include "grit_texture.h"
#include "swos_render_frames.h"

#include "player_atlas_texture.h"
#include "player_atlas_team2_texture.h"
#include "keeper_atlas_texture.h"
#include "ball_atlas_texture.h"

#define WIN_W 480
#define WIN_H 420
#define HUD_H 24
// Fixed on-screen reference point every frame's anchor is aligned to --
// analogous to where a player's feet sit relative to their world X/Y in
// the real renderer.
#define REF_X (WIN_W / 2)
#define REF_Y (HUD_H + 260)

// player_atlas.c's PLAYER_texcoords[] (x,y,w,h per local frame, 4 uint16
// each) -- the SAME table both real player atlases (team1 and team2) are
// packed against, see tools/extract_team2_atlas.py. Declared here rather
// than pulled from nds-app/source/player_atlas.h to avoid a GL2D
// (glImage-shaped) dependency in this desktop-only tool; the raw numbers
// are identical, this is just a plain reinterpretation of the same data.
extern const unsigned short PLAYER_texcoords[];
#define PLAYER_ATLAS_FRAME_COUNT 101

// ball_atlas.c's BALL_texcoords[] -- same idea, 5 frames.
extern const unsigned short BALL_texcoords[];
#define BALL_ATLAS_FRAME_COUNT 5

// keeper_atlas.c's KEEPER_texcoords[] (Phase 5) -- a genuinely new
// bin-packing (GOAL1.DAT geometry, unrelated to PLAYER_texcoords), shared
// by both team1 and team2 keeper categories (same physical sprites).
extern const unsigned short KEEPER_texcoords[];
#define KEEPER_ATLAS_FRAME_COUNT 116

static const char *categoryName(int cat) {
    switch (cat) {
        case SWOS_RENDER_FRAME_CAT_CHARSET: return "CHARSET";
        case SWOS_RENDER_FRAME_CAT_SCORE_UI: return "SCORE_UI";
        case SWOS_RENDER_FRAME_CAT_PLAYER_TEAM1: return "PLAYER_TEAM1";
        case SWOS_RENDER_FRAME_CAT_PLAYER_TEAM2: return "PLAYER_TEAM2";
        case SWOS_RENDER_FRAME_CAT_KEEPER_TEAM1: return "KEEPER_TEAM1";
        case SWOS_RENDER_FRAME_CAT_KEEPER_TEAM2: return "KEEPER_TEAM2";
        case SWOS_RENDER_FRAME_CAT_BALL: return "BALL";
        case SWOS_RENDER_FRAME_CAT_REFEREE: return "REFEREE";
        case SWOS_RENDER_FRAME_CAT_BENCH_OTHER: return "BENCH_OTHER";
        default: return "?";
    }
}

static SDL_Texture *loadGritTexture(SDL_Renderer *ren, const unsigned int *bitmap,
                                     const unsigned short *pal, int w, int h) {
    uint8_t *rgba = (uint8_t *)SDL_malloc((size_t)w * h * 4);
    gritDecodeToRgba(bitmap, pal, w, h, rgba);
    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormatFrom(
        rgba, w, h, 32, w * 4, SDL_PIXELFORMAT_RGBA32);
    SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, surf);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_FreeSurface(surf);
    SDL_free(rgba);
    return tex;
}

static void drawDigitChar(SDL_Renderer *ren, int x, int y, char c, int px) {
    int idx = digitFontGlyphIndex(c);
    for (int row = 0; row < DIGIT_FONT_ROWS; row++) {
        uint8_t bits = kDigitFont[idx][row];
        for (int col = 0; col < DIGIT_FONT_COLS; col++) {
            if (bits & (1 << (DIGIT_FONT_COLS - 1 - col))) {
                SDL_Rect r = { x + col * px, y + row * px, px, px };
                SDL_RenderFillRect(ren, &r);
            }
        }
    }
}

static void drawDigitText(SDL_Renderer *ren, int x, int y, const char *text, int px) {
    int cx = x;
    for (const char *p = text; *p; p++) {
        drawDigitChar(ren, cx, y, *p, px);
        cx += (DIGIT_FONT_COLS + 1) * px;
    }
}

static void drawCross(SDL_Renderer *ren, int x, int y, int r) {
    SDL_SetRenderDrawColor(ren, 255, 40, 40, 255);
    SDL_RenderDrawLine(ren, x - r, y, x + r, y);
    SDL_RenderDrawLine(ren, x, y - r, x, y + r);
}

// Picks a texture (or NULL) for an atlasId this tool has actually loaded,
// and looks up that frame's (x,y,w,h) source rect from the matching
// texcoords table -- the ONE place this tool interprets an atlasId, same
// spirit as swos_render_commands.c centralizing frame resolution.
static SDL_Texture *textureAndSrcRectFor(int16_t atlasId, int16_t atlasFrame,
                                          SDL_Texture *texPlayer, SDL_Texture *texPlayerTeam2,
                                          SDL_Texture *texKeeper, SDL_Texture *texBall, SDL_Rect *outSrc) {
    const unsigned short *coords = NULL;
    SDL_Texture *tex = NULL;
    if (atlasId == SWOS_RENDER_ATLAS_PLAYER && atlasFrame >= 0 && atlasFrame < PLAYER_ATLAS_FRAME_COUNT) {
        coords = PLAYER_texcoords; tex = texPlayer;
    } else if (atlasId == SWOS_RENDER_ATLAS_PLAYER_TEAM2 && atlasFrame >= 0 && atlasFrame < PLAYER_ATLAS_FRAME_COUNT) {
        coords = PLAYER_texcoords; tex = texPlayerTeam2;
    } else if (atlasId == SWOS_RENDER_ATLAS_KEEPER && atlasFrame >= 0 && atlasFrame < KEEPER_ATLAS_FRAME_COUNT) {
        coords = KEEPER_texcoords; tex = texKeeper;
    } else if (atlasId == SWOS_RENDER_ATLAS_BALL && atlasFrame >= 0 && atlasFrame < BALL_ATLAS_FRAME_COUNT) {
        coords = BALL_texcoords; tex = texBall;
    } else {
        return NULL;
    }
    outSrc->x = coords[atlasFrame * 4 + 0];
    outSrc->y = coords[atlasFrame * 4 + 1];
    outSrc->w = coords[atlasFrame * 4 + 2];
    outSrc->h = coords[atlasFrame * 4 + 3];
    return tex;
}

static int32_t stepIndex(int32_t idx, int dir, int categoryFilter, bool usedOnly) {
    for (int guard = 0; guard < SWOS_RENDER_FRAME_COUNT * 2; guard++) {
        idx += dir;
        if (idx < 0) idx = SWOS_RENDER_FRAME_COUNT - 1;
        if (idx >= SWOS_RENDER_FRAME_COUNT) idx = 0;

        if (usedOnly) {
            bool found = false;
            int n = swosRenderFramesUsedIndexCount();
            for (int i = 0; i < n; i++)
                if (swosRenderFramesUsedIndexAt(i) == idx) { found = true; break; }
            if (!found) continue;
        }
        if (categoryFilter >= 0) {
            SwosRenderFrameInfo info;
            if (!swosRenderFramesLookup(idx, &info) || info.category != categoryFilter)
                continue;
        }
        return idx;
    }
    return idx; // filter matched nothing; give up rather than spin forever
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow("swos-vm-c sprite laboratory (Phase 4)",
                                        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                        WIN_W, WIN_H, 0);
    if (!win) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);

    SDL_Texture *texPlayer = loadGritTexture(ren, player_atlas_textureBitmap, player_atlas_texturePal, 256, 256);
    SDL_Texture *texPlayerTeam2 = loadGritTexture(ren, player_atlas_team2_textureBitmap, player_atlas_team2_texturePal, 256, 256);
    SDL_Texture *texKeeper = loadGritTexture(ren, keeper_atlas_textureBitmap, keeper_atlas_texturePal, 256, 128);
    SDL_Texture *texBall = loadGritTexture(ren, ball_atlas_textureBitmap, ball_atlas_texturePal, 32, 32);

    int32_t index = 341; // first team1 player frame -- a reasonable, always-resolved starting point
    int categoryFilter = -1; // -1 = all categories
    bool usedOnly = false;
    bool quit = false;

    printf("swos-vm-c sprite laboratory -- controls:\n"
           "  Left/Right  prev/next global index\n"
           "  Up/Down     cycle category filter (ALL, then each category)\n"
           "  Tab         toggle used-only navigation (only indices the real VM can produce)\n"
           "  Esc         quit\n\n");

    while (!quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) quit = true;
            else if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE: quit = true; break;
                    case SDLK_LEFT: index = stepIndex(index, -1, categoryFilter, usedOnly); break;
                    case SDLK_RIGHT: index = stepIndex(index, +1, categoryFilter, usedOnly); break;
                    case SDLK_UP: categoryFilter = (categoryFilter >= SWOS_RENDER_FRAME_CAT_BENCH_OTHER) ? -1 : categoryFilter + 1; break;
                    case SDLK_DOWN: categoryFilter = (categoryFilter <= -1) ? SWOS_RENDER_FRAME_CAT_BENCH_OTHER : categoryFilter - 1; break;
                    case SDLK_TAB: usedOnly = !usedOnly; break;
                    default: break;
                }
            }
        }

        SwosRenderFrameInfo info;
        bool resolved = swosRenderFramesLookup(index, &info);

        SDL_SetRenderDrawColor(ren, 24, 24, 32, 255);
        SDL_RenderClear(ren);

        // Reference axes.
        SDL_SetRenderDrawColor(ren, 60, 60, 80, 255);
        SDL_RenderDrawLine(ren, 0, HUD_H, WIN_W, HUD_H);

        bool hasTexture = false;
        if (resolved && info.atlasId != SWOS_RENDER_ATLAS_NONE) {
            SDL_Rect src;
            SDL_Texture *tex = textureAndSrcRectFor(info.atlasId, info.atlasFrame,
                                                     texPlayer, texPlayerTeam2, texKeeper, texBall, &src);
            if (tex) {
                hasTexture = true;
                // Anchor the sprite's real (centerX, centerY) pixel at the
                // fixed reference point -- if this is wrong, the sprite
                // visibly floats away from the cross as frames are stepped.
                SDL_Rect dst = { REF_X - info.centerX, REF_Y - info.centerY, src.w, src.h };
                SDL_RenderCopy(ren, tex, &src, &dst);
            }
        }
        if (!hasTexture) {
            // Explicit "no pixels" marker -- never a silent substitute.
            // Sized to the frame's real (known) geometry even without a
            // texture, so the placeholder's extent is still meaningful.
            int w = resolved ? info.width : 8, h = resolved ? info.height : 8;
            SDL_Rect box = { REF_X - (resolved ? info.centerX : w / 2),
                              REF_Y - (resolved ? info.centerY : h / 2), w, h };
            SDL_SetRenderDrawColor(ren, 255, 0, 255, 255);
            SDL_RenderDrawRect(ren, &box);
        }

        drawCross(ren, REF_X, REF_Y, 10);

        char line[128];
        snprintf(line, sizeof(line), "%d", (int)index);
        drawDigitText(ren, 4, 4, line, 2);

        SDL_RenderPresent(ren);

        static int32_t s_lastPrinted = -2; // never equals a real starting index
        if (index != s_lastPrinted) {
            if (resolved) {
                printf("index=%-5d cat=%-13s valid=1 atlas=%-2d frame=%-4d center=(%d,%d) size=%dx%d texture=%s\n",
                       (int)index, categoryName(info.category), info.atlasId, info.atlasFrame,
                       info.centerX, info.centerY, info.width, info.height,
                       hasTexture ? "yes" : "NO (geometry only)");
            } else {
                printf("index=%-5d  -- MISSING (no sprite at all) --\n", (int)index);
            }
            s_lastPrinted = index;
        }

        SDL_Delay(16);
    }
    printf("\n");

    SDL_DestroyTexture(texPlayer);
    SDL_DestroyTexture(texPlayerTeam2);
    SDL_DestroyTexture(texKeeper);
    SDL_DestroyTexture(texBall);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
