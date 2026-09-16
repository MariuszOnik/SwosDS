// swos-vm-c step 12.5: minimal SDL2 diagnostic frontend -- a debugger for
// the mechanically-ported VM (../../src, unmodified), NOT a second game
// implementation. Built after the ETAP 0 audit (see ../../DEVLOG.md) that
// found and fixed the real cause of the broken-looking DS kickoff: this
// frontend exists to keep observing/debugging the same real simulation
// going forward, on a machine with no DS emulator.
//
// Renders exactly what the VM's own accessors report -- no gameplay logic,
// no invented positions/state. Reuses ../../nds-app/source/match_bootstrap.c
// directly (not copied) for match setup, exactly like
// tests/test_step12_integration_golden.c does, so this frontend, the
// integration test, and the real DS app all run the identical setup path.
//
// Camera: deliberately shows the WHOLE 672x848 pitch scaled to fit the
// window, not a scrolling follow-cam -- this sidesteps needing any new
// camera/scroll math to debug (the real ported swos_camera.c still runs
// every tick as part of the simulation; its X/Y is only shown in the HUD
// for reference, per the task's own "camera position, if it's already part
// of the running path" wording, not used to drive rendering).
//
// Text: no SDL_ttf is available on this machine (checked -- only SDL2
// itself is installed under devkitPro's mingw64). A tiny hand-authored
// digit-only bitmap font (digit_font.h) draws the on-screen HUD (tick,
// gameState/gameStatePl, score, selected slot ordinal) -- all-numeric by
// design. The full per-slot detail panel (dest XY, direction, state,
// frame/picture index, anim table pointer, flags) is printed to the
// console instead, which the task's own wording explicitly allows
// ("Overlay LUB panel tekstowy").
#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "digit_font.h"
#include "match_bootstrap.h"
#include "sim_log.h"

#include "swos_addr.h"
#include "swos_ball_sprite.h"
#include "swos_camera.h"
#include "swos_game_loop.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"
#include "swos_result.h"
#include "swos_team_data.h"

#define WORLD_W 672
#define WORLD_H 880
#define PITCH_BITMAP_WORLD_Y 16
#define PITCH_BITMAP_H 848
#define PITCH_CENTER_Y 449
#define HUD_H 20
#define WIN_W 440
#define WIN_H (WIN_W * WORLD_H / WORLD_W + HUD_H)

typedef struct
{
    int gameState, gameStatePl;
    int32_t ctrlTop, ctrlBot;
} PrevState;

typedef struct
{
    int ballX, ballY, ballZ;
    int playerX[PLSPR_TOTAL_SLOTS];
    int playerY[PLSPR_TOTAL_SLOTS];
    unsigned unchangedTicks;
} StallWatch;

static float g_scale;
static int g_originX, g_originY;

static void computeTransform(void)
{
    float scaleX = (float)WIN_W / (float)WORLD_W;
    float scaleY = (float)(WIN_H - HUD_H) / (float)WORLD_H;
    g_scale = scaleX < scaleY ? scaleX : scaleY;
    g_originX = (int)((WIN_W - WORLD_W * g_scale) / 2.0f);
    g_originY = HUD_H + (int)(((WIN_H - HUD_H) - WORLD_H * g_scale) / 2.0f);
}

static void worldToScreen(int wx, int wy, int *sx, int *sy)
{
    *sx = g_originX + (int)(wx * g_scale);
    *sy = g_originY + (int)(wy * g_scale);
}

static int findControlledSlot(int32_t controlledAddr)
{
    if (controlledAddr == 0) return -1;
    for (int slot = 0; slot < PLSPR_TOTAL_SLOTS; slot++)
        if (swosPlayerSpriteBase(slot) == controlledAddr)
            return slot;
    return -1;
}

static int sumGoals(const SwosResultScorerInfo *scorers)
{
    int total = 0;
    for (int i = 0; i < SWOS_RESULT_MAX_SCORERS; i++)
        if (scorers[i].shirtNum != 0)
            total += scorers[i].numGoals;
    return total;
}

// ---- console diagnostic dump -----------------------------------------
static void diagDumpStage(const char *label)
{
    printf("\n=== STAGE: %s ===\n", label);
    printf("gameState=%d gameStatePl=%d breakCameraMode=%d\n",
           swosReadSignedWord(ADDR_gameState),
           swosReadSignedWord(ADDR_gameStatePl),
           swosReadSignedWord(ADDR_breakCameraMode));
    printf("ball: x=%d y=%d z=%d\n",
           swosBallSpriteXPixels(), swosBallSpriteYPixels(), swosBallSpriteZPixels());
    printf("camera: x=%d y=%d\n", swosCameraGetXWhole(), swosCameraGetYWhole());

    int activeTop = 0, activeBot = 0;
    for (int slot = 0; slot < PLSPR_TOTAL_SLOTS; slot++)
    {
        int16_t team = swosPlayerSpriteTeamNumber(slot);
        if (team == 1) activeTop++;
        else if (team == 2) activeBot++;
    }
    printf("active slots (team number set): top=%d/11 bottom=%d/11\n", activeTop, activeBot);

    printf("tactics: top=%d bottom=%d\n",
           swosReadSignedWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_TACTICS),
           swosReadSignedWord(TEAMDATA_BOTTOM_BASE + TEAMDATA_OFF_TACTICS));

    for (int slot = 0; slot < PLSPR_TOTAL_SLOTS; slot++)
    {
        int16_t team = swosPlayerSpriteTeamNumber(slot);
        if (team != 1 && team != 2) continue;
        printf("  slot%2d team%d ord%2d pos=(%4d,%4d) dest=(%4d,%4d) dir=%d state=%d img=%d onScreen=%d moving=%d\n",
               slot, team, swosPlayerSpritePlayerOrdinal(slot),
               swosPlayerSpriteXPixels(slot), swosPlayerSpriteYPixels(slot),
               swosPlayerSpriteDestX(slot), swosPlayerSpriteDestY(slot),
               swosPlayerSpriteDirection(slot), swosPlayerSpritePlayerState(slot),
               swosPlayerSpriteImageIndex(slot),
               swosReadSignedWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_ON_SCREEN),
               swosReadSignedWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_IS_MOVING));
    }

    int32_t ctrlTop = swosTeamDataControlledPlayer(true);
    int32_t ctrlBot = swosTeamDataControlledPlayer(false);
    printf("controlled: top=slot%d bottom=slot%d\n",
           findControlledSlot(ctrlTop), findControlledSlot(ctrlBot));
}

// Console detail dump for one selected slot (D key) -- includes the
// fields the task asks for that a numeric-only on-screen HUD can't show
// legibly: dest XY, direction, state, frame/picture index, anim table
// pointer, basic flags.
static void dumpSlotDetail(int slot)
{
    int base = swosPlayerSpriteBase(slot);
    printf("\n--- slot %d detail ---\n", slot);
    printf("team=%d ordinal=%d pos=(%d,%d) dest=(%d,%d) dir=%d fullDir=%d\n",
           swosPlayerSpriteTeamNumber(slot), swosPlayerSpritePlayerOrdinal(slot),
           swosPlayerSpriteXPixels(slot), swosPlayerSpriteYPixels(slot),
           swosPlayerSpriteDestX(slot), swosPlayerSpriteDestY(slot),
           swosPlayerSpriteDirection(slot), swosPlayerSpriteFullDirection(slot));
    printf("playerState=%d imageIndex=%d animTablePtr=0x%X frameIndex=%d frameDelay=%d\n",
           swosPlayerSpritePlayerState(slot), swosPlayerSpriteImageIndex(slot),
           swosReadSignedDword(base + PLSPR_OFF_ANIM_TABLE_PTR),
           swosReadSignedWord(base + PLSPR_OFF_FRAME_INDEX),
           swosReadSignedWord(base + PLSPR_OFF_FRAME_DELAY));
    printf("flags: onScreen=%d visible=%d isMoving=%d destReachedState=%d tackleState=%d cards=%d sentAway=%d\n",
           swosReadSignedWord(base + PLSPR_OFF_ON_SCREEN),
           swosReadSignedWord(base + PLSPR_OFF_VISIBLE),
           swosReadSignedWord(base + PLSPR_OFF_IS_MOVING),
           swosReadSignedWord(base + PLSPR_OFF_DEST_REACHED_STATE),
           swosPlayerSpriteTackleState(slot),
           swosReadSignedWord(base + PLSPR_OFF_CARDS),
           swosReadSignedWord(base + PLSPR_OFF_SENT_AWAY));
}

// ---- event-edge detection (for both console + CSV log) ----------------
static void detectEvents(PrevState *prev, char *outEvents, size_t outSize)
{
    outEvents[0] = '\0';
    int gs = swosReadSignedWord(ADDR_gameState);
    int gsp = swosReadSignedWord(ADDR_gameStatePl);
    int32_t ctrlTop = swosTeamDataControlledPlayer(true);
    int32_t ctrlBot = swosTeamDataControlledPlayer(false);

    char buf[256];
    buf[0] = '\0';

    if (gs != prev->gameState)
    {
        char part[64];
        snprintf(part, sizeof(part), "gameState:%d->%d;", prev->gameState, gs);
        strncat(buf, part, sizeof(buf) - strlen(buf) - 1);
        if (gs == 0)
            strncat(buf, "kickoff_prepared;", sizeof(buf) - strlen(buf) - 1);
    }
    if (gsp != prev->gameStatePl)
    {
        char part[64];
        snprintf(part, sizeof(part), "gameStatePl:%d->%d;", prev->gameStatePl, gsp);
        strncat(buf, part, sizeof(buf) - strlen(buf) - 1);
    }
    if (ctrlTop != prev->ctrlTop)
    {
        char part[64];
        snprintf(part, sizeof(part), "ctrlTop_slot->%d;", findControlledSlot(ctrlTop));
        strncat(buf, part, sizeof(buf) - strlen(buf) - 1);
    }
    if (ctrlBot != prev->ctrlBot)
    {
        char part[64];
        snprintf(part, sizeof(part), "ctrlBot_slot->%d;", findControlledSlot(ctrlBot));
        strncat(buf, part, sizeof(buf) - strlen(buf) - 1);
    }

    if (buf[0] != '\0')
    {
        strncpy(outEvents, buf, outSize - 1);
        outEvents[outSize - 1] = '\0';
        printf("[event] tick-edge: %s\n", buf);
    }

    prev->gameState = gs;
    prev->gameStatePl = gsp;
    prev->ctrlTop = ctrlTop;
    prev->ctrlBot = ctrlBot;
}

// ---- rendering ----------------------------------------------------------
static void drawFilledCircle(SDL_Renderer *ren, int cx, int cy, int r)
{
    for (int dy = -r; dy <= r; dy++)
    {
        int dx = (int)SDL_sqrt((double)(r * r - dy * dy));
        SDL_RenderDrawLine(ren, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

static void drawDigitChar(SDL_Renderer *ren, int x, int y, char c, int px)
{
    int idx = digitFontGlyphIndex(c);
    for (int row = 0; row < DIGIT_FONT_ROWS; row++)
    {
        uint8_t bits = kDigitFont[idx][row];
        for (int col = 0; col < DIGIT_FONT_COLS; col++)
        {
            if (bits & (1 << (DIGIT_FONT_COLS - 1 - col)))
            {
                SDL_Rect r = { x + col * px, y + row * px, px, px };
                SDL_RenderFillRect(ren, &r);
            }
        }
    }
}

static void drawDigitText(SDL_Renderer *ren, int x, int y, const char *text, int px)
{
    int cx = x;
    for (const char *p = text; *p; p++)
    {
        drawDigitChar(ren, cx, y, *p, px);
        cx += (DIGIT_FONT_COLS + 1) * px;
    }
}

static void render(SDL_Renderer *ren, uint64_t tick, bool paused, bool fastForward,
                   int selectedSlot)
{
    SDL_SetRenderDrawColor(ren, 10, 10, 14, 255);
    SDL_RenderClear(ren);

    // Pitch.
    int pitchLeft, pitchTop;
    worldToScreen(0, PITCH_BITMAP_WORLD_Y, &pitchLeft, &pitchTop);
    SDL_Rect pitch = { pitchLeft, pitchTop, (int)(WORLD_W * g_scale),
                       (int)(PITCH_BITMAP_H * g_scale) };
    SDL_SetRenderDrawColor(ren, 20, 90, 40, 255);
    SDL_RenderFillRect(ren, &pitch);
    SDL_SetRenderDrawColor(ren, 220, 220, 220, 255);
    SDL_RenderDrawRect(ren, &pitch);
    // Halfway line + centre circle, purely visual reference (no gameplay meaning).
    int ignoredX, centreY;
    worldToScreen(0, PITCH_CENTER_Y, &ignoredX, &centreY);
    SDL_RenderDrawLine(ren, pitch.x, centreY, pitch.x + pitch.w, centreY);

    // Ball.
    int bx, by;
    worldToScreen(swosBallSpriteXPixels(), swosBallSpriteYPixels(), &bx, &by);
    int bz = swosBallSpriteZPixels();
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 120);
    drawFilledCircle(ren, bx, by, 3);
    SDL_SetRenderDrawColor(ren, 255, 230, 80, 255);
    drawFilledCircle(ren, bx, by - bz / 4, 3);

    int32_t ctrlTop = swosTeamDataControlledPlayer(true);
    int32_t ctrlBot = swosTeamDataControlledPlayer(false);

    for (int slot = 0; slot < PLSPR_TOTAL_SLOTS; slot++)
    {
        int16_t team = swosPlayerSpriteTeamNumber(slot);
        if (team != 1 && team != 2) continue;

        int sx, sy;
        worldToScreen(swosPlayerSpriteXPixels(slot), swosPlayerSpriteYPixels(slot), &sx, &sy);

        bool isGoalie = swosPlayerSpriteIsGoalie(slot);
        bool isControlled = (swosPlayerSpriteBase(slot) == ctrlTop) || (swosPlayerSpriteBase(slot) == ctrlBot);

        if (team == 1)
            SDL_SetRenderDrawColor(ren, isGoalie ? 255 : 60, isGoalie ? 160 : 120, isGoalie ? 0 : 240, 255);
        else
            SDL_SetRenderDrawColor(ren, isGoalie ? 255 : 230, isGoalie ? 160 : 60, isGoalie ? 0 : 60, 255);
        drawFilledCircle(ren, sx, sy, 5);

        if (isControlled)
        {
            SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
            SDL_Rect ring = { sx - 7, sy - 7, 14, 14 };
            SDL_RenderDrawRect(ren, &ring);
        }
        if (slot == selectedSlot)
        {
            SDL_SetRenderDrawColor(ren, 0, 255, 0, 255);
            SDL_Rect ring = { sx - 9, sy - 9, 18, 18 };
            SDL_RenderDrawRect(ren, &ring);
        }

        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        char ordText[8];
        snprintf(ordText, sizeof(ordText), "%d", swosPlayerSpritePlayerOrdinal(slot));
        drawDigitText(ren, sx + 6, sy - 10, ordText, 1);
    }

    // HUD strip (numeric only -- see file header for why).
    SDL_SetRenderDrawColor(ren, 30, 30, 34, 255);
    SDL_Rect hud = { 0, 0, WIN_W, HUD_H };
    SDL_RenderFillRect(ren, &hud);

    SDL_SetRenderDrawColor(ren, 0, 255, 120, 255);
    char tickText[24];
    snprintf(tickText, sizeof(tickText), "%llu", (unsigned long long)tick);
    drawDigitText(ren, 4, 4, tickText, 2);

    SDL_SetRenderDrawColor(ren, 255, 200, 0, 255);
    char stateText[24];
    snprintf(stateText, sizeof(stateText), "%d/%d",
             swosReadSignedWord(ADDR_gameState), swosReadSignedWord(ADDR_gameStatePl));
    drawDigitText(ren, 140, 4, stateText, 2);

    SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
    char scoreText[24];
    int scoreTop = sumGoals(swosResultGetTeam1Scorers());
    int scoreBot = sumGoals(swosResultGetTeam2Scorers());
    snprintf(scoreText, sizeof(scoreText), "%d-%d", scoreTop, scoreBot);
    drawDigitText(ren, 300, 4, scoreText, 2);

    // Pause indicator: red square = paused, green = running.
    SDL_SetRenderDrawColor(ren, paused ? 220 : 40, paused ? 40 : 200, 40, 255);
    SDL_Rect pauseDot = { WIN_W - 14, 4, 10, 10 };
    SDL_RenderFillRect(ren, &pauseDot);

    // Blue = fast-forward (100 VM ticks per displayed frame).
    SDL_SetRenderDrawColor(ren, fastForward ? 40 : 60, fastForward ? 120 : 60,
                           fastForward ? 255 : 60, 255);
    SDL_Rect fastDot = { WIN_W - 46, 4, 10, 10 };
    SDL_RenderFillRect(ren, &fastDot);

    // Logging indicator: yellow = on, dark gray = off.
    SDL_SetRenderDrawColor(ren, simLogIsOpen() ? 240 : 60, simLogIsOpen() ? 220 : 60, simLogIsOpen() ? 0 : 60, 255);
    SDL_Rect logDot = { WIN_W - 30, 4, 10, 10 };
    SDL_RenderFillRect(ren, &logDot);

    SDL_RenderPresent(ren);
}

// ---- match setup / reset -------------------------------------------------
static void resetMatch(uint64_t *tickCount, PrevState *prev)
{
    swosMemoryInit(true);
    diagDumpStage("after memory init");

    dsBootstrapMatch();
    diagDumpStage("after setup/kickoff");

    swosGameLoopTick();
    diagDumpStage("after first full tick");

    *tickCount = 1;
    prev->gameState = swosReadSignedWord(ADDR_gameState);
    prev->gameStatePl = swosReadSignedWord(ADDR_gameStatePl);
    prev->ctrlTop = swosTeamDataControlledPlayer(true);
    prev->ctrlBot = swosTeamDataControlledPlayer(false);
}

static void resetStallWatch(StallWatch *watch)
{
    watch->ballX = swosBallSpriteXPixels();
    watch->ballY = swosBallSpriteYPixels();
    watch->ballZ = swosBallSpriteZPixels();
    for (int slot = 0; slot < PLSPR_TOTAL_SLOTS; slot++)
    {
        watch->playerX[slot] = swosPlayerSpriteXPixels(slot);
        watch->playerY[slot] = swosPlayerSpriteYPixels(slot);
    }
    watch->unchangedTicks = 0;
}

static bool updateStallWatch(StallWatch *watch)
{
    bool unchanged = watch->ballX == swosBallSpriteXPixels() &&
                     watch->ballY == swosBallSpriteYPixels() &&
                     watch->ballZ == swosBallSpriteZPixels();
    for (int slot = 0; slot < PLSPR_TOTAL_SLOTS && unchanged; slot++)
        unchanged = watch->playerX[slot] == swosPlayerSpriteXPixels(slot) &&
                    watch->playerY[slot] == swosPlayerSpriteYPixels(slot);

    if (unchanged)
        watch->unchangedTicks++;
    else
        resetStallWatch(watch);

    return watch->unchangedTicks >= 300;
}

static bool tickOnce(uint64_t *tickCount, PrevState *prev, StallWatch *watch)
{
    swosGameLoopTick();
    (*tickCount)++;
    char events[256];
    detectEvents(prev, events, sizeof(events));
    simLogWriteTick(*tickCount, events);
    return updateStallWatch(watch);
}

int main(int argc, char **argv)
{
    bool logRequested = false;
    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], "--log") == 0) logRequested = true;

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    computeTransform();

    SDL_Window *win = SDL_CreateWindow("swos-vm-c SDL debug (step 12.5)",
                                        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                        WIN_W, WIN_H, 0);
    if (!win)
    {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!ren)
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);

    uint64_t tickCount = 0;
    PrevState prev = { 0, 0, 0, 0 };
    bool paused = false, fastForward = false, quit = false;
    int selectedSlot = 0;
    StallWatch stallWatch;

    resetMatch(&tickCount, &prev);
    resetStallWatch(&stallWatch);
    if (logRequested)
    {
        simLogOpen("sdl_debug_log.tsv");
        simLogWriteTick(tickCount, "startup");
    }

    while (!quit)
    {
        Uint32 frameStart = SDL_GetTicks();

        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT) quit = true;
            else if (e.type == SDL_KEYDOWN)
            {
                switch (e.key.keysym.sym)
                {
                    case SDLK_ESCAPE: quit = true; break;
                    case SDLK_SPACE: paused = !paused; break;
                    case SDLK_f: fastForward = !fastForward; break;
                    case SDLK_n:
                    {
                        if (tickOnce(&tickCount, &prev, &stallWatch))
                        {
                            paused = true;
                            diagDumpStage("automatic stall detection (300 unchanged ticks)");
                        }
                        break;
                    }
                    case SDLK_r:
                        resetMatch(&tickCount, &prev);
                        resetStallWatch(&stallWatch);
                        break;
                    case SDLK_l:
                        if (simLogIsOpen()) simLogClose();
                        else simLogOpen("sdl_debug_log.tsv");
                        break;
                    case SDLK_TAB:
                    case SDLK_RIGHT:
                        selectedSlot = (selectedSlot + 1) % PLSPR_TOTAL_SLOTS;
                        break;
                    case SDLK_LEFT:
                        selectedSlot = (selectedSlot + PLSPR_TOTAL_SLOTS - 1) % PLSPR_TOTAL_SLOTS;
                        break;
                    case SDLK_d:
                        dumpSlotDetail(selectedSlot);
                        break;
                    default: break;
                }
            }
        }

        if (!paused)
        {
            int ticksThisFrame = fastForward ? 100 : 1;
            for (int i = 0; i < ticksThisFrame; i++)
            {
                if (tickOnce(&tickCount, &prev, &stallWatch))
                {
                    paused = true;
                    diagDumpStage("automatic stall detection (300 unchanged ticks)");
                    break;
                }
            }
        }

        render(ren, tickCount, paused, fastForward, selectedSlot);

        Uint32 elapsed = SDL_GetTicks() - frameStart;
        if (elapsed < 16) SDL_Delay(16 - elapsed);
    }

    simLogClose();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
