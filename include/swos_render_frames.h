// PHASE 3 (full atlas / global sprite index mapping, 2026-09-16 -- see
// README.md "Status: Phase 3"). Portable, no SDL/libnds dependency.
//
// RENDER_FRAMES[1334] (include/generated/swos_render_frames_data.h,
// produced by tools/extract_render_frames.py) is the single, centralized
// map from a global sprite ordinal (PlayerSprite.imageIndex/
// BallSprite.imageIndex -- the SAME numbering SWOS itself uses in
// sprite.dat, documented in swos-port/docs/SWOS/sprites.txt) to its real
// geometry/anchor point and, where a texture has actually been built, its
// atlas reference. Every one of the 1334 entries carries REAL data read
// from the user's own GOG .DAT files by that generator -- none of it is
// invented, and none of it is a "looks about right" guess.
//
// swosRenderFramesLookup() is the ONE place that resolves a global index --
// callers must not scatter their own `imageIndex - 341`-style arithmetic
// (the plan's own explicit rule). A `valid` entry with `atlasId ==
// SWOS_RENDER_ATLAS_NONE` is a real sprite whose geometry is known but
// whose PIXEL TEXTURE has not been extracted/built yet (goalkeepers,
// referee, bench, tackles/headers/injuries beyond the current 101-frame
// player atlas, ...) -- deliberately NOT the same thing as an invalid
// index, and NOT silently substituted with a standing frame. An `!valid`
// index (should not occur for anything the real VM can produce -- see the
// completeness test) means no sprite header exists there at all.
#pragma once

#include <stdbool.h>
#include <stdint.h>

// Category enum values -- MUST match tools/extract_render_frames.py's
// CAT_* constants exactly (that script emits raw integers, not symbolic
// names, into the generated data header).
typedef enum {
    SWOS_RENDER_FRAME_CAT_CHARSET = 0,      // 0-226: menu font
    SWOS_RENDER_FRAME_CAT_SCORE_UI = 1,     // 227-340: scoreboard digits/UI
    SWOS_RENDER_FRAME_CAT_PLAYER_TEAM1 = 2, // 341-643: outfield players, home
    SWOS_RENDER_FRAME_CAT_PLAYER_TEAM2 = 3, // 644-946: outfield players, away
    SWOS_RENDER_FRAME_CAT_KEEPER_TEAM1 = 4, // 947-1062: goalkeepers, home
    SWOS_RENDER_FRAME_CAT_KEEPER_TEAM2 = 5, // 1063-1178: goalkeepers, away
    SWOS_RENDER_FRAME_CAT_BALL = 6,         // 1179-1183: ball + shadow
    SWOS_RENDER_FRAME_CAT_REFEREE = 7,      // 1273-1283: referee
    SWOS_RENDER_FRAME_CAT_BENCH_OTHER = 8,  // rest of 1179-1333: bench players + other match/menu bits
} SwosRenderFrameCategory;

// atlasId values -- MUST match tools/extract_render_frames.py's ATLAS_*
// constants. Only two real pixel atlases exist today; everything else is
// SWOS_RENDER_ATLAS_NONE until a future extraction pass builds more.
#define SWOS_RENDER_ATLAS_NONE   (-1)
#define SWOS_RENDER_ATLAS_PLAYER 0  // nds-app/source/player_atlas.png, 101 frames (global 341-441)
#define SWOS_RENDER_ATLAS_BALL   1  // nds-app/source/ball_atlas.png, 5 frames (global 1179-1183)

typedef struct {
    bool valid;               // true iff a real SWOS sprite header exists at this ordinal
    int16_t centerX, centerY; // real anchor point from the original sprite header (sprites.txt: cx range [-8..34], cy range [0..27])
    int16_t width, height;    // pixel dimensions, informational (width is the "exact" nibble width, not the padded wquads*16)
    int16_t atlasId;          // SWOS_RENDER_ATLAS_* or SWOS_RENDER_ATLAS_NONE (see header note above)
    int16_t atlasFrame;       // local index within atlasId's texture; -1 if atlasId is NONE
    uint8_t category;         // SwosRenderFrameCategory
} SwosRenderFrameInfo;

#define SWOS_RENDER_FRAME_COUNT 1334

// The one, centralized resolver. Returns true and fills *out if
// globalImageIndex is in range AND has a real sprite header (RENDER_FRAMES[
// globalImageIndex].valid); returns false (and logs an explicit "MISSING
// IMAGE" line, per the plan's rule against silent standing-frame
// substitution) for anything out of range or genuinely unmapped -- which,
// per this module's own completeness test, should never happen for an
// index the real VM can actually produce.
bool swosRenderFramesLookup(int32_t globalImageIndex, SwosRenderFrameInfo *out);

// Exposed for tests/test_render_frames.c's completeness check: the full
// set of indices the VM's real animation system can actually assign,
// mechanically derived from generated/swos_anim_streams.h by the same
// generator that built RENDER_FRAMES.
int swosRenderFramesUsedIndexCount(void);
int32_t swosRenderFramesUsedIndexAt(int i);
