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
// constants. Only four real pixel atlases exist today; everything else is
// SWOS_RENDER_ATLAS_NONE until a future extraction pass builds more.
#define SWOS_RENDER_ATLAS_NONE   (-1)
#define SWOS_RENDER_ATLAS_PLAYER       0  // nds-app/graphics/player_atlas_texture.png, 101 frames (global 341-441, home team)
#define SWOS_RENDER_ATLAS_BALL         1  // nds-app/graphics/ball_atlas_texture.png, 5 frames (global 1179-1183)
// PHASE 4 (2026-09-16): same 101-frame layout/texcoords as ATLAS_PLAYER
// (TEAM1.DAT/TEAM2.DAT share identical per-frame geometry, only the kit
// pixel pattern differs -- see tools/extract_team2_atlas.py), but a
// SEPARATE texture (nds-app/graphics/player_atlas_team2_texture.png) since
// the pixels themselves differ. atlasFrame for this atlas uses the SAME
// local numbering (0-100) and can be looked up in the SAME PLAYER_texcoords[]
// table nds-app already has -- only which texture to bind differs.
#define SWOS_RENDER_ATLAS_PLAYER_TEAM2 2  // nds-app/graphics/player_atlas_team2_texture.png, 101 frames (global 644-744, away team)
// PHASE 5 (2026-09-16): both goalkeepers' real pixel texture, a genuinely
// new bin-packing of GOAL1.DAT (see tools/extract_keeper_atlas.py) -- not a
// layout reuse like ATLAS_PLAYER_TEAM2, since goalkeeper geometry is
// unrelated to the outfield-player atlas. Team2's keeper range (1063-1178)
// is the SAME PHYSICAL 116 sprites mirrored (sprites.txt: "116 pointers to
// goal1.dat / 116 -||-"), so BOTH SWOS_RENDER_FRAME_CAT_KEEPER_TEAM1 and
// _KEEPER_TEAM2 resolve into this ONE atlas/texture, each with its own
// atlasFrame (0-115) into the SAME KEEPER_texcoords[] table.
#define SWOS_RENDER_ATLAS_KEEPER       3  // nds-app/graphics/keeper_atlas_texture.png, 116 frames (global 947-1062 team1 / 1063-1178 team2, same physical sprites)

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
