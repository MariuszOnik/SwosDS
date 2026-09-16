// PHASE 3 (full atlas / global sprite index mapping) tests. New code, not
// a port of anything. Covers the plan's own explicit requirements:
// "verify every frame's anchor point" and "a completeness test: every
// imageIndex reached during a lockstep match must have a correct mapping".
#include <stdio.h>

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

// swos-port/docs/SWOS/sprites.txt: "16  2  x center, used range: [-8..34]"
// / "18  2  y center, used range: [0..27]". Real, documented bounds from
// the original format spec -- not invented.
#define SPRITE_CENTER_X_MIN -8
#define SPRITE_CENTER_X_MAX 34
#define SPRITE_CENTER_Y_MIN 0
#define SPRITE_CENTER_Y_MAX 27

static void test_every_entry_has_plausible_geometry(void) {
    int badCenter = 0, badSize = 0;
    for (int32_t i = 0; i < SWOS_RENDER_FRAME_COUNT; i++) {
        SwosRenderFrameInfo info;
        bool ok = swosRenderFramesLookup(i, &info);
        if (!ok) {
            printf("FAIL: RENDER_FRAMES[%d] is not valid -- every one of the 1334 real "
                   "sprite.dat ordinals should have real header data (%s:%d)\n",
                   (int)i, __FILE__, __LINE__);
            g_failures++;
            continue;
        }
        if (info.centerX < SPRITE_CENTER_X_MIN || info.centerX > SPRITE_CENTER_X_MAX ||
            info.centerY < SPRITE_CENTER_Y_MIN || info.centerY > SPRITE_CENTER_Y_MAX)
            badCenter++;
        if (info.width <= 0 || info.height <= 0)
            badSize++;
    }
    CHECK(badCenter == 0, "every RENDER_FRAMES anchor point is within sprites.txt's documented range (cx -8..34, cy 0..27)");
    CHECK(badSize == 0, "every RENDER_FRAMES entry has a positive width and height");
}

static void test_known_spot_checks(void) {
    SwosRenderFrameInfo info;

    // Global 341 = TEAM1.DAT's very first sprite (local frame 0), the
    // "stand N" pose per openswos/tools/sprite-anchors-extract's own
    // --annotate table. Known real header values (verified directly
    // against the GOG file while building the generator).
    CHECK(swosRenderFramesLookup(341, &info) && info.category == SWOS_RENDER_FRAME_CAT_PLAYER_TEAM1,
          "global 341 is categorized as a team1 player frame");
    CHECK(info.atlasId == SWOS_RENDER_ATLAS_PLAYER && info.atlasFrame == 0,
          "global 341 resolves to the player atlas, local frame 0");

    // PHASE 4: global 644 = TEAM2.DAT's own sprite 0 (self-declares ordinal
    // 644 in its own header, see tools/extract_render_frames.py's module
    // docstring) -- now resolves to the second real atlas
    // (tools/extract_team2_atlas.py), same local frame numbering as team1.
    SwosRenderFrameInfo t1, t2;
    CHECK(swosRenderFramesLookup(341, &t1) && swosRenderFramesLookup(644, &t2) &&
          t1.centerX == t2.centerX && t1.centerY == t2.centerY &&
          t1.width == t2.width && t1.height == t2.height,
          "team1/team2 outfield-player geometry matches at corresponding local frame 0 (same character pose, different kit pixels)");
    CHECK(t2.category == SWOS_RENDER_FRAME_CAT_PLAYER_TEAM2 &&
          t2.atlasId == SWOS_RENDER_ATLAS_PLAYER_TEAM2 && t2.atlasFrame == 0,
          "global 644 resolves to the team2 player atlas, local frame 0");

    // Global 947 = GOAL1.DAT's first sprite (team1 main goalkeeper).
    // PHASE 5: now resolves to the real keeper atlas (tools/extract_keeper_atlas.py).
    CHECK(swosRenderFramesLookup(947, &info) && info.category == SWOS_RENDER_FRAME_CAT_KEEPER_TEAM1,
          "global 947 is categorized as a team1 keeper frame");
    CHECK(info.atlasId == SWOS_RENDER_ATLAS_KEEPER && info.atlasFrame == 0,
          "global 947 resolves to the keeper atlas, local frame 0");

    // Global 1063 = the SAME goal1.dat sprite 0, mirrored for team2's keeper --
    // same atlas, same local frame numbering (ordinal - 1063 == ordinal - 947 here).
    SwosRenderFrameInfo keeper1, keeper2;
    CHECK(swosRenderFramesLookup(947, &keeper1) && swosRenderFramesLookup(1063, &keeper2) &&
          keeper1.centerX == keeper2.centerX && keeper1.centerY == keeper2.centerY &&
          keeper1.width == keeper2.width && keeper1.height == keeper2.height,
          "team1/team2 goalkeeper geometry mirrors (same physical goal1.dat sprites, per sprites.txt)");
    CHECK(keeper2.category == SWOS_RENDER_FRAME_CAT_KEEPER_TEAM2 &&
          keeper2.atlasId == SWOS_RENDER_ATLAS_KEEPER && keeper2.atlasFrame == 0,
          "global 1063 is categorized as a team2 keeper frame and resolves to the SAME keeper atlas, local frame 0");

    // Ball frames: 1179-1182 animate, 1183 is the single fixed shadow --
    // both extracted as real pixel atlas frames already (ball_atlas.png).
    CHECK(swosRenderFramesLookup(1179, &info) && info.category == SWOS_RENDER_FRAME_CAT_BALL &&
          info.atlasId == SWOS_RENDER_ATLAS_BALL && info.atlasFrame == 0,
          "global 1179 (first ball frame) resolves to the ball atlas, local frame 0");
    CHECK(swosRenderFramesLookup(1183, &info) && info.atlasId == SWOS_RENDER_ATLAS_BALL && info.atlasFrame == 4,
          "global 1183 (ball shadow) resolves to the ball atlas, local frame 4");

    // Referee frames (found by reading generated/swos_anim_streams.h's
    // s_Ref* arrays -- see tools/extract_render_frames.py's classify()).
    CHECK(swosRenderFramesLookup(1279, &info) && info.category == SWOS_RENDER_FRAME_CAT_REFEREE,
          "global 1279 (s_RefWaiting) is categorized as a referee frame");
    CHECK(info.atlasId == SWOS_RENDER_ATLAS_REFEREE && info.atlasFrame == 6,
          "global 1279 resolves to the referee atlas, local frame 6 (1279-1273)");
    CHECK(swosRenderFramesLookup(1273, &info) && info.atlasId == SWOS_RENDER_ATLAS_REFEREE && info.atlasFrame == 0,
          "global 1273 (first referee frame) resolves to the referee atlas, local frame 0");
    CHECK(swosRenderFramesLookup(1283, &info) && info.atlasId == SWOS_RENDER_ATLAS_REFEREE && info.atlasFrame == 10,
          "global 1283 (last referee frame) resolves to the referee atlas, local frame 10");

    // Corner flags: global 1184-1187 (GS_CORNER_FLAG_SPRITE_START), the 4
    // real wind-animation frames swosGameSpritesUpdateCornerFlags() cycles.
    CHECK(swosRenderFramesLookup(1184, &info) && info.atlasId == SWOS_RENDER_ATLAS_CORNERFLAG && info.atlasFrame == 0,
          "global 1184 (first corner-flag frame) resolves to the corner-flag atlas, local frame 0");
    CHECK(swosRenderFramesLookup(1187, &info) && info.atlasId == SWOS_RENDER_ATLAS_CORNERFLAG && info.atlasFrame == 3,
          "global 1187 (last corner-flag frame) resolves to the corner-flag atlas, local frame 3");

    // "Goal Post Split" (Phase 5 bugfix): global 1205/1206 = the real static
    // goal-frame overlay sprites (kTopGoalSprite/kBottomGoalSprite,
    // swos-port/src/sprites/sprites.h), inside BENCH.DAT's 1179-1333 range.
    CHECK(swosRenderFramesLookup(1205, &info) && info.atlasId == SWOS_RENDER_ATLAS_GOAL && info.atlasFrame == 0,
          "global 1205 (top goal frame) resolves to the goal atlas, local frame 0");
    CHECK(swosRenderFramesLookup(1206, &info) && info.atlasId == SWOS_RENDER_ATLAS_GOAL && info.atlasFrame == 1,
          "global 1206 (bottom goal frame) resolves to the goal atlas, local frame 1");

    // Out-of-range indices must fail cleanly, not read out of bounds.
    CHECK(!swosRenderFramesLookup(-1, &info), "negative index is rejected, not a silent substitute");
    CHECK(!swosRenderFramesLookup(SWOS_RENDER_FRAME_COUNT, &info), "one-past-the-end index is rejected");
    CHECK(!swosRenderFramesLookup(999999, &info), "wildly out-of-range index is rejected");
}

// The plan's own explicit completeness requirement: every imageIndex the
// real, already-ported VM animation system (AnimationTablesData.cs) can
// actually produce during a match must resolve to a valid RENDER_FRAMES
// entry. This is the mechanically-derived set from
// generated/swos_anim_streams.h (every non-negative literal across all 174
// real animation-table arrays) -- not a hand-picked sample.
static void test_completeness_against_used_indices(void) {
    int count = swosRenderFramesUsedIndexCount();
    CHECK(count > 0, "the used-indices set is non-empty (extraction actually ran)");

    int unresolved = 0;
    for (int i = 0; i < count; i++) {
        int32_t idx = swosRenderFramesUsedIndexAt(i);
        SwosRenderFrameInfo info;
        if (!swosRenderFramesLookup(idx, &info)) {
            printf("FAIL: used image index %d (reachable by the real VM animation system) "
                   "has NO RENDER_FRAMES mapping (%s:%d)\n", (int)idx, __FILE__, __LINE__);
            unresolved++;
        }
    }
    CHECK(unresolved == 0, "every imageIndex the VM's real animation tables can produce has a valid RENDER_FRAMES entry");
}

int main(void) {
    test_every_entry_has_plausible_geometry();
    test_known_spot_checks();
    test_completeness_against_used_indices();

    if (g_failures) {
        printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    printf("\nall checks passed\n");
    return 0;
}
