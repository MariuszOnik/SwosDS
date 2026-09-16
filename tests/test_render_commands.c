// PHASE 2 (RenderCommand layer) tests. New code, not a port of anything --
// covers the two functions the plan explicitly calls out ("world->screen"
// and "sorting") plus a full swosRenderBuildFrame() pass against a
// synthetic Memory snapshot, matching this repo's usual "seed a minimal
// state, call the real function, assert the result" pattern.
#include <stdio.h>

#include "swos_ball_sprite.h"
#include "swos_game_sprites.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"
#include "swos_referee.h"
#include "swos_render_commands.h"
#include "swos_render_frames.h"

static int g_failures = 0;

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            g_failures++; \
        } else { \
            printf("ok:   %s\n", msg); \
        } \
    } while (0)

static void test_world_to_screen(void) {
    int32_t sx, sy;

    swosRenderWorldToScreen(336, 449, 0, 0, &sx, &sy);
    CHECK(sx == 336 && sy == 449, "world->screen: zero camera is identity");

    swosRenderWorldToScreen(336, 449, 100, 50, &sx, &sy);
    CHECK(sx == 236 && sy == 399, "world->screen: subtracts camera offset");

    swosRenderWorldToScreen(50, 60, 100, 100, &sx, &sy);
    CHECK(sx == -50 && sy == -40, "world->screen: goes negative off the left/top edge (caller's job to cull)");

    swosRenderWorldToScreen(0, 0, -20, -30, &sx, &sy);
    CHECK(sx == 20 && sy == 30, "world->screen: negative camera pans the other way");
}

static void test_sort_key(void) {
    CHECK(swosRenderSortKeyForWorldY(449) == 449, "sort key is identity on worldY this phase");
    CHECK(swosRenderSortKeyForWorldY(-5) == -5, "sort key passes through negative worldY unchanged");
}

static SwosRenderCommand makeCmd(int32_t sortKey, int slot) {
    SwosRenderCommand c = {0};
    c.sortKey = sortKey;
    c.slot = slot; // used as an identity tag to check stability
    return c;
}

static void test_sort_commands_order(void) {
    SwosRenderCommand cmds[5] = {
        makeCmd(500, 0),
        makeCmd(100, 1),
        makeCmd(300, 2),
        makeCmd(100, 3),
        makeCmd(200, 4),
    };
    swosRenderSortCommands(cmds, 5);

    CHECK(cmds[0].sortKey == 100 && cmds[1].sortKey == 100 &&
          cmds[2].sortKey == 200 && cmds[3].sortKey == 300 && cmds[4].sortKey == 500,
          "sort commands: ascending by sortKey");
    // Stability: the two sortKey==100 entries (original slots 1 and 3) must
    // keep their original relative order.
    CHECK(cmds[0].slot == 1 && cmds[1].slot == 3,
          "sort commands: stable for equal keys (original order preserved)");
}

static void test_sort_commands_edge_cases(void) {
    swosRenderSortCommands(NULL, 0); // must not crash
    SwosRenderCommand one = makeCmd(42, 0);
    swosRenderSortCommands(&one, 1);
    CHECK(one.sortKey == 42, "sort commands: count 0/1 are no-ops, not crashes");

    SwosRenderCommand already[3] = { makeCmd(1, 0), makeCmd(2, 1), makeCmd(3, 2) };
    swosRenderSortCommands(already, 3);
    CHECK(already[0].sortKey == 1 && already[1].sortKey == 2 && already[2].sortKey == 3,
          "sort commands: already-sorted input stays sorted");

    SwosRenderCommand reversed[3] = { makeCmd(3, 0), makeCmd(2, 1), makeCmd(1, 2) };
    swosRenderSortCommands(reversed, 3);
    CHECK(reversed[0].sortKey == 1 && reversed[1].sortKey == 2 && reversed[2].sortKey == 3,
          "sort commands: fully-reversed input sorts correctly");
}

// Regression test for the real bug this caught: fillBallCommand's shadow
// command has a HIGHER worldY/sortKey than the ball's own (ballY + ballZ/4
// + 1 vs ballY -- see BALL_SHADOW_OFFSET_Y's own comment), so a sort by
// sortKey alone puts the shadow AFTER (visually on top of) the ball --
// backwards. layer must win over sortKey so the shadow (SWOS_RENDER_LAYER_
// SHADOW) always sorts before the sprite it belongs to (SWOS_RENDER_LAYER_
// SPRITE), no matter which one has the larger worldY.
static void test_sort_commands_layer_beats_sortkey(void) {
    SwosRenderCommand shadow = makeCmd(451, 0); // shadow's own sortKey is LARGER...
    shadow.layer = SWOS_RENDER_LAYER_SHADOW;
    SwosRenderCommand ball = makeCmd(450, 1); // ...than the ball's, on purpose
    ball.layer = SWOS_RENDER_LAYER_SPRITE;

    SwosRenderCommand cmds[2] = { ball, shadow }; // start in the "wrong" order too
    swosRenderSortCommands(cmds, 2);
    CHECK(cmds[0].layer == SWOS_RENDER_LAYER_SHADOW && cmds[1].layer == SWOS_RENDER_LAYER_SPRITE,
          "sort commands: shadow layer always sorts before sprite layer, even with a larger sortKey");
}

static void test_build_frame(void) {
    swosMemoryInit(true);
    // Corner flags are computed per-tick by swosGameSpritesUpdateCornerFlags()
    // (called every real frame from swos_game_loop.c) -- swosMemoryInit()
    // alone leaves ADDR_cornerFlags zeroed, not at the real fixed positions.
    // Call it explicitly to match the state swosRenderBuildFrame() actually
    // sees after any real tick.
    swosGameSpritesUpdateCornerFlags();

    swosBallSpriteSetXPixels(336);
    swosBallSpriteSetYPixels(449);
    swosBallSpriteSetZPixels(20);
    swosBallSpriteSetImageIndex(5);

    // Slot 0: top team, a resolved player-atlas frame (341 + 7 = local 7).
    swosPlayerSpriteSetTeamNumber(0, 1);
    swosPlayerSpriteSetXPixels(0, 100);
    swosPlayerSpriteSetYPixels(0, 200);
    swosPlayerSpriteSetImageIndex(0, 348);

    // Slot 11: bottom team, an image index with known geometry but no built
    // atlas texture (bench player, still SWOS_RENDER_ATLAS_NONE as of Phase 5
    // -- unlike 947-1178, which now resolves to the real keeper atlas) --
    // must come back unresolved, not silently substituted.
    swosPlayerSpriteSetTeamNumber(11, 2);
    swosPlayerSpriteSetXPixels(11, 300);
    swosPlayerSpriteSetYPixels(11, 400);
    swosPlayerSpriteSetImageIndex(11, 1300);

    // Every other slot keeps swosMemoryInit's own default team assignment
    // (PlayerSprite.Init() already assigns all 22 slots a valid team 1/2,
    // not team 0 -- confirmed empirically, not assumed), so
    // swosRenderBuildFrame's team-number filter doesn't skip anyone here:
    // shadow + ball + 2 goal frames + all 22 players + 4 corner flags = 30.
    // NOT SWOS_RENDER_MAX_COMMANDS (31) -- that also budgets for the
    // referee, which stays absent here (swosRefereeVisible() is false
    // right after a fresh swosMemoryInit(), see
    // test_build_frame_referee_visible below for the case where it's
    // active). Corner flags themselves are unconditional, unlike the
    // referee -- all 4 exist for the whole match.

    SwosRenderCommand cmds[SWOS_RENDER_MAX_COMMANDS];
    int count = swosRenderBuildFrame(cmds, SWOS_RENDER_MAX_COMMANDS, 50, 60);

    CHECK(count == 30,
          "build frame: shadow + ball + 2 goal frames + all 22 players + 4 corner flags (referee inactive)");

    CHECK(cmds[0].kind == SWOS_RENDER_KIND_BALL_SHADOW && cmds[0].layer == SWOS_RENDER_LAYER_SHADOW,
          "build frame: command 0 is the ball's shadow");
    CHECK(cmds[1].kind == SWOS_RENDER_KIND_BALL && cmds[1].worldZ == 20,
          "build frame: command 1 is the ball, carrying its height (shadow does not)");
    CHECK(cmds[1].worldX == 336 && cmds[1].worldY == 449 &&
          cmds[1].screenX == 286 && cmds[1].screenY == 389,
          "build frame: ball world/screen position matches what was seeded");

    // "Goal Post Split" (Phase 5 bugfix): the two static goal-frame
    // commands, real fixed world position (swos-port's own kGoalX/kTopGoalY/
    // kBottomGoalY), plain SWOS_RENDER_LAYER_SPRITE (not a special
    // foreground layer -- see swos_render_commands.c's fillGoalCommand).
    CHECK(cmds[2].kind == SWOS_RENDER_KIND_GOAL && cmds[2].layer == SWOS_RENDER_LAYER_SPRITE &&
          cmds[2].globalImageIndex == 1205 && cmds[2].worldX == 300 && cmds[2].worldY == 129,
          "build frame: command 2 is the top goal frame at its real fixed position");
    CHECK(cmds[3].kind == SWOS_RENDER_KIND_GOAL && cmds[3].layer == SWOS_RENDER_LAYER_SPRITE &&
          cmds[3].globalImageIndex == 1206 && cmds[3].worldX == 300 && cmds[3].worldY == 778,
          "build frame: command 3 is the bottom goal frame at its real fixed position");
    CHECK(cmds[2].imageResolved && cmds[2].atlasId == SWOS_RENDER_ATLAS_GOAL && cmds[2].atlasFrame == 0 &&
          cmds[3].imageResolved && cmds[3].atlasId == SWOS_RENDER_ATLAS_GOAL && cmds[3].atlasFrame == 1,
          "build frame: both goal frames resolve to the real goal atlas");

    // PHASE 4 BUGFIX regression: the shadow must NOT sit at the ball's own
    // worldX/worldY (that put them at the identical screen rect whenever
    // the ball was near the ground, and the opaque ball -- drawn second --
    // fully hid the shadow behind it). Formula shape from swos-port's
    // ball.cpp:updateBallShadow: shadowX = ballX + ballZ/2 + 1,
    // shadowY = ballY + ballZ/4 + 1 + BALL_SHADOW_OFFSET_Y (0, recalibrated
    // against a real melonDS run -- see swos_render_commands.c's own
    // comment on that constant). Ball here is (336, 449, z=20):
    // shadowX = 336 + 10 + 1 = 347, shadowY = 449 + 5 + 1 + 0 = 455.
    CHECK(cmds[0].worldX == 347 && cmds[0].worldY == 455 && cmds[0].worldZ == 0,
          "build frame: shadow is diagonally offset from the ball (never coincident, even accounting for height), matching the real engine's own formula shape");

    // Find the two player commands by slot (order among players follows
    // the slot scan, 0 before 11, so this should also just be cmds[2]/cmds[3]
    // -- checked both ways for robustness against a future scan-order change).
    const SwosRenderCommand *p0 = NULL, *p11 = NULL;
    for (int i = 0; i < count; i++) {
        if (cmds[i].kind == SWOS_RENDER_KIND_PLAYER && cmds[i].slot == 0) p0 = &cmds[i];
        if (cmds[i].kind == SWOS_RENDER_KIND_PLAYER && cmds[i].slot == 11) p11 = &cmds[i];
    }
    CHECK(p0 != NULL && p11 != NULL, "build frame: both seeded player slots are present");

    if (p0) {
        CHECK(p0->team == 1 && p0->palette == 1, "build frame: slot 0 team/palette");
        CHECK(p0->worldX == 100 && p0->worldY == 200 && p0->screenX == 50 && p0->screenY == 140,
              "build frame: slot 0 world/screen position");
        CHECK(p0->imageResolved && p0->atlasId == 0 && p0->atlasFrame == 7,
              "build frame: slot 0 image 348 resolves to local frame 7 (348-341)");
    }
    if (p11) {
        CHECK(p11->team == 2, "build frame: slot 11 team");
        CHECK(!p11->imageResolved && p11->atlasFrame == -1,
              "build frame: slot 11 image 1300 has no built atlas texture yet -- explicitly unresolved, not a silent standing-frame fallback");
    }

    // Corner flags: real fixed positions (swos-port's own GS_LEFT/RIGHT_
    // CORNER_FLAG_X, GS_TOP/BOTTOM_CORNER_FLAG_Y), all 4 always present,
    // all resolving to the real corner-flag atlas.
    int cornerFlagCount = 0;
    bool sawTopLeft = false, sawTopRight = false, sawBottomLeft = false, sawBottomRight = false;
    for (int i = 0; i < count; i++) {
        if (cmds[i].kind != SWOS_RENDER_KIND_CORNER_FLAG)
            continue;
        cornerFlagCount++;
        CHECK(cmds[i].imageResolved && cmds[i].atlasId == SWOS_RENDER_ATLAS_CORNERFLAG,
              "build frame: a corner flag command resolves to the real corner-flag atlas");
        if (cmds[i].worldX == 81 && cmds[i].worldY == 129) sawTopLeft = true;
        if (cmds[i].worldX == 590 && cmds[i].worldY == 129) sawTopRight = true;
        if (cmds[i].worldX == 81 && cmds[i].worldY == 769) sawBottomLeft = true;
        if (cmds[i].worldX == 590 && cmds[i].worldY == 769) sawBottomRight = true;
    }
    CHECK(cornerFlagCount == 4, "build frame: exactly 4 corner-flag commands");
    CHECK(sawTopLeft && sawTopRight && sawBottomLeft && sawBottomRight,
          "build frame: all 4 corner flags sit at their real fixed pitch-corner positions");

    // maxCommands cap: pass a buffer too small to hold everything and
    // confirm the count is clamped, not overrun.
    SwosRenderCommand small[2];
    int smallCount = swosRenderBuildFrame(small, 2, 0, 0);
    CHECK(smallCount == 2, "build frame: respects a caller-supplied maxCommands cap");
}

// Referee wiring: swosRenderBuildFrame() only emits a referee command when
// swosRefereeVisible() is true (real VM state, gated the same way the
// player-slot team-number filter gates player commands) -- confirmed above
// that a fresh Memory leaves it absent; this confirms the opposite case,
// poking REFSPR_BASE directly the same way other tests poke raw Memory
// state (no public "set" accessor exists for the referee -- it's meant to
// be driven by swosRefereeActivate()/swosRefereeUpdateReferee() only, this
// is test-only low-level access).
static void test_build_frame_referee_visible(void) {
    swosMemoryInit(true);
    swosGameSpritesUpdateCornerFlags(); // see test_build_frame's own comment on why

    swosWriteWord(REFSPR_BASE + PLSPR_OFF_VISIBLE, 1);
    swosWriteWord(REFSPR_BASE + PLSPR_OFF_IMAGE_INDEX, 1279);
    swosWriteDword(REFSPR_BASE + PLSPR_OFF_X, (uint32_t)(250 << 16));
    swosWriteDword(REFSPR_BASE + PLSPR_OFF_Y, (uint32_t)(450 << 16));

    SwosRenderCommand cmds[SWOS_RENDER_MAX_COMMANDS];
    int count = swosRenderBuildFrame(cmds, SWOS_RENDER_MAX_COMMANDS, 0, 0);
    CHECK(count == SWOS_RENDER_MAX_COMMANDS,
          "build frame: referee command appears when swosRefereeVisible() is true, filling every slot (31)");

    const SwosRenderCommand *ref = NULL;
    for (int i = 0; i < count; i++)
        if (cmds[i].kind == SWOS_RENDER_KIND_REFEREE) ref = &cmds[i];
    CHECK(ref != NULL, "build frame: a SWOS_RENDER_KIND_REFEREE command is present");
    if (ref) {
        CHECK(ref->worldX == 250 && ref->worldY == 450, "build frame: referee world position matches what was seeded");
        CHECK(ref->imageResolved && ref->atlasId == SWOS_RENDER_ATLAS_REFEREE && ref->atlasFrame == 6,
              "build frame: referee image 1279 resolves to the referee atlas, local frame 6 (1279-1273)");
    }
}

int main(void) {
    test_world_to_screen();
    test_sort_key();
    test_sort_commands_order();
    test_sort_commands_edge_cases();
    test_sort_commands_layer_beats_sortkey();
    test_build_frame();
    test_build_frame_referee_visible();

    if (g_failures) {
        printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    printf("\nall checks passed\n");
    return 0;
}
