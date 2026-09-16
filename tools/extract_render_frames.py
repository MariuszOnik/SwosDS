#!/usr/bin/env python3
"""PHASE 3 (RENDER_FRAMES[1334] atlas mapping, 2026-09-16 -- see
README.md "Status: Phase 3"). Offline generator: reads REAL sprite
geometry/anchor headers straight out of the user's legally-owned GOG PC
.DAT files (same 24-byte header format documented in
../../swos-port/docs/SWOS/sprites.txt and reimplemented here in the same
spirit as openswos/tools/sprite-anchors-extract -- that C# tool currently
fails to build on this machine's .NET 8 SDK since its project targets
.NET 9, so this is a from-scratch reimplementation of the same trivial,
well-documented format, not a guess) plus the VM's own real animation-table
data (generated/swos_anim_streams.h, mechanically extracted from
AnimationTablesData.cs in an earlier step) to emit
generated/swos_render_frames_data.h: a full RENDER_FRAMES[1334] table
(every global sprite ordinal 0..1333) and the set of ordinals the VM's
ported animation system can actually produce (for the completeness test).

Ordinal ranges (verified empirically against the real files below, not
assumed -- every file's own sprite count and, where the format embeds it,
its own declared ordinal field were cross-checked to match exactly):
    0-226      CHARSET.DAT   (227 sprites)                 menu font
    227-340    SCORE.DAT     (114 sprites)                 scoreboard digits/UI
    341-643    TEAM1.DAT     (303 sprites)                 team "1"/home player+keeper source pool (home always loads from this range at runtime)
    644-946    TEAM2.DAT     (303 sprites, self-declares ordinals 644-946)  team "2"/away pool
    947-1062   GOAL1.DAT     (116 sprites)                 team1 goalkeepers (main 947-1004, reserve 1005-1062)
    1063-1178  GOAL1.DAT again (mirrored, team2 goalkeepers -- same physical
               116 sprites reused for the other side, per sprites.txt's
               "116 pointers to goal1.dat / 116 -||-")
    1179-1333  BENCH.DAT     (155 sprites)                 ball (1179-1183,
               confirmed by ../../swos-ds/tools/extract_ball_frames.py),
               referee (1273-1283, confirmed by reading
               generated/swos_anim_streams.h's s_Ref* arrays), bench
               players (1310-1333) and other match/menu bits

TEAM1.DAT/TEAM2.DAT/TEAM3.DAT were cross-checked to carry IDENTICAL
width/height/wquads/centerX/centerY per corresponding local sprite index --
only the pixel PATTERN differs (vertical stripe / sleeves / horizontal
stripe kit). So geometry doesn't depend on which kit a real match actually
picks; this generator still reads TEAM1.DAT (for 341-643) and TEAM2.DAT
(for 644-946, which self-declares those exact ordinals in its own sprite
headers) rather than assuming, since real files were available.

Usage:
    python tools/extract_render_frames.py [gog_dir] > /dev/null
(writes directly to include/generated/swos_render_frames_data.h; gog_dir
defaults to this machine's known GOG install path.)

PHASE 4 addendum (2026-09-16): tools/extract_team2_atlas.py built a second
real pixel texture (nds-app/graphics/player_atlas_team2_texture.png,
global 644-744, reusing this exact atlas layout) -- atlas_for() below now
maps that range to ATLAS_PLAYER_TEAM2 instead of ATLAS_NONE.

PHASE 5 addendum (2026-09-16): tools/extract_keeper_atlas.py built a real
pixel texture for both goalkeepers (nds-app/graphics/keeper_atlas_texture.png,
a genuinely new bin-packing of GOAL1.DAT's 116 sprites, not a layout reuse
like Phase 4's team2 atlas -- goalkeeper geometry is unrelated to the
outfield-player atlas). Team2's keeper range (1063-1178) is the SAME
physical 116 sprites mirrored, per this file's own ordinal-range comment
above, so atlas_for() maps BOTH 947-1062 and 1063-1178 onto the one
ATLAS_KEEPER texture, with local frame = ordinal - 947 / ordinal - 1063
respectively (same numbering space, since they're the same pixels).

PHASE 5 BUGFIX addendum (2026-09-16, "Goal Post Split"): tools/
extract_goal_atlas.py built a 2-frame texture for the two static goal-frame
overlay sprites (global 1205 top / 1206 bottom, inside BENCH.DAT's
1179-1333 catch-all range -- see tools/extract_goal_atlas.py's own module
docstring for why no separate foreground/background layer was needed, only
these two ordinary Y-sortable sprites at a fixed real position).
"""
import re
import struct
import sys
from pathlib import Path

DEFAULT_GOG_DIR = Path(r"C:\Program Files\GOG Galaxy\Games\Sensible World of Soccer 96-97")
REPO_ROOT = Path(__file__).parent.parent
ANIM_STREAMS_HEADER = REPO_ROOT / "include" / "generated" / "swos_anim_streams.h"
OUT_HEADER = REPO_ROOT / "include" / "generated" / "swos_render_frames_data.h"

TOTAL_FRAMES = 1334

# Category enum values -- MUST match SWOS_RENDER_FRAME_CAT_* in
# include/swos_render_frames.h.
CAT_CHARSET = 0
CAT_SCORE_UI = 1
CAT_PLAYER_TEAM1 = 2
CAT_PLAYER_TEAM2 = 3
CAT_KEEPER_TEAM1 = 4
CAT_KEEPER_TEAM2 = 5
CAT_BALL = 6
CAT_REFEREE = 7
CAT_BENCH_OTHER = 8

# atlasId values -- MUST match SWOS_RENDER_ATLAS_* in swos_render_frames.h.
ATLAS_NONE = -1        # geometry known, no pixel texture built yet
ATLAS_PLAYER = 0       # nds-app/graphics/player_atlas_texture.png, 101 frames, global 341-441 (home)
ATLAS_BALL = 1         # nds-app/graphics/ball_atlas_texture.png, 5 frames, global 1179-1183
ATLAS_PLAYER_TEAM2 = 2 # nds-app/graphics/player_atlas_team2_texture.png (Phase 4), 101 frames, global 644-744 (away)
ATLAS_KEEPER = 3       # nds-app/graphics/keeper_atlas_texture.png (Phase 5), 116 frames, global 947-1062 (team1) / 1063-1178 (team2, same physical sprites)
ATLAS_GOAL = 4          # nds-app/graphics/goal_atlas_texture.png (Phase 5 bugfix, "Goal Post Split"), 2 frames, global 1205 (top) / 1206 (bottom)


def parse_dat(path, start_ordinal, expect_count=None):
    """Mechanical reimplementation of the 24-byte SWOS sprite header format
    (sprites.txt / openswos/tools/sprite-anchors-extract's own comment).
    Returns a dict: ordinal -> (width, nlines, centerX, centerY, hdrOrdinal)."""
    data = path.read_bytes()
    off = 0
    ordinal = start_ordinal
    out = {}
    while off + 24 <= len(data):
        width, nlines, wquads, cx, cy = struct.unpack_from('<hhhhh', data, off + 10)
        hdr_ord, = struct.unpack_from('<h', data, off + 22)
        if nlines < 0 or wquads < 0 or nlines > 256 or wquads > 64:
            break
        pixel_bytes = nlines * wquads * 8
        if off + 24 + pixel_bytes > len(data):
            break
        out[ordinal] = (width, nlines, cx, cy, hdr_ord)
        off += 24 + pixel_bytes
        ordinal += 1
    if expect_count is not None and len(out) != expect_count:
        raise SystemExit(f"{path}: expected {expect_count} sprites, parsed {len(out)} "
                          f"({off}/{len(data)} bytes consumed) -- refusing to guess, fix ordinal/count")
    if off != len(data):
        raise SystemExit(f"{path}: {off}/{len(data)} bytes consumed, file not fully parsed -- refusing to guess")
    return out


def parse_used_indices(header_path):
    """Union of every non-negative int16 literal across ALL arrays in the
    generated animation-streams header -- these are the ONLY image indices
    the VM's real, already-ported animation system (AnimationTablesData.cs)
    can ever assign to PlayerSprite.imageIndex/BallSprite.imageIndex.
    Negative values are control opcodes (-999 reset, -101 pause, <=-100
    skip-forward, -99..-1 negate-and-delay -- see sprites.txt's "in the
    table some special values exist" section), never real image indices."""
    text = header_path.read_text(encoding='utf-8')
    used = set()
    for m in re.finditer(r'\{([^}]*)\}', text):
        for tok in m.group(1).split(','):
            tok = tok.strip()
            if not tok:
                continue
            v = int(tok)
            if v >= 0:
                used.add(v)
    return used


def build_table(gog_dir):
    charset = parse_dat(gog_dir / "CHARSET.DAT", 0, expect_count=227)
    score = parse_dat(gog_dir / "SCORE.DAT", 227, expect_count=114)
    team1 = parse_dat(gog_dir / "TEAM1.DAT", 341, expect_count=303)
    team2 = parse_dat(gog_dir / "TEAM2.DAT", 644, expect_count=303)
    goal1 = parse_dat(gog_dir / "GOAL1.DAT", 947, expect_count=116)
    bench = parse_dat(gog_dir / "BENCH.DAT", 1179, expect_count=155)

    # Team2.dat's own sprite headers self-declare ordinals 644-946 (verified
    # -- see this file's module docstring), so no extra check needed there.
    # Cross-check team1/team3 geometry equivalence at a few sample offsets
    # as a sanity guard against a future re-run with a swapped-out file.
    team3 = parse_dat(gog_dir / "TEAM3.DAT", 341, expect_count=303)
    for k in (0, 50, 100, 200, 302):
        o = 341 + k
        if team1[o][:4] != team3[o][:4]:
            raise SystemExit(f"TEAM1.DAT/TEAM3.DAT geometry mismatch at ordinal {o} -- "
                              "the 'kit pattern doesn't affect geometry' assumption doesn't hold, stop")

    entries = {}  # ordinal -> (width, nlines, cx, cy)

    def absorb(d):
        for k, v in d.items():
            entries[k] = (v[0], v[1], v[2], v[3])

    absorb(charset)
    absorb(score)
    absorb(team1)
    absorb(team2)
    absorb(goal1)
    # Team2 goalkeepers (1063-1178): same physical 116 goal1.dat sprites,
    # reused for the other side -- sprites.txt "116 pointers to goal1.dat /
    # 116 -||-". Mirror goal1's own 947-1062 geometry, offset +116.
    for ordinal, v in goal1.items():
        entries[ordinal + 116] = (v[0], v[1], v[2], v[3])
    absorb(bench)

    assert set(entries.keys()) == set(range(TOTAL_FRAMES)), \
        f"gap or overlap in the assembled table: have {len(entries)} of {TOTAL_FRAMES}"

    used_indices = parse_used_indices(ANIM_STREAMS_HEADER)
    out_of_range = sorted(i for i in used_indices if i < 0 or i >= TOTAL_FRAMES)
    if out_of_range:
        raise SystemExit(f"animation streams reference indices outside 0..{TOTAL_FRAMES - 1}: {out_of_range}")

    return entries, used_indices


def classify(ordinal):
    if 0 <= ordinal <= 226:
        return CAT_CHARSET
    if 227 <= ordinal <= 340:
        return CAT_SCORE_UI
    if 341 <= ordinal <= 643:
        return CAT_PLAYER_TEAM1
    if 644 <= ordinal <= 946:
        return CAT_PLAYER_TEAM2
    if 947 <= ordinal <= 1062:
        return CAT_KEEPER_TEAM1
    if 1063 <= ordinal <= 1178:
        return CAT_KEEPER_TEAM2
    if 1179 <= ordinal <= 1183:
        return CAT_BALL
    if 1273 <= ordinal <= 1283:
        return CAT_REFEREE
    return CAT_BENCH_OTHER


def atlas_for(ordinal):
    if 341 <= ordinal <= 441:
        return ATLAS_PLAYER, ordinal - 341
    if 644 <= ordinal <= 744:
        return ATLAS_PLAYER_TEAM2, ordinal - 644
    if 947 <= ordinal <= 1062:
        return ATLAS_KEEPER, ordinal - 947
    if 1063 <= ordinal <= 1178:
        return ATLAS_KEEPER, ordinal - 1063  # same physical goal1.dat sprites, mirrored
    if 1179 <= ordinal <= 1183:
        return ATLAS_BALL, ordinal - 1179
    if ordinal == 1205:
        return ATLAS_GOAL, 0  # top goal frame (kTopGoalSprite)
    if ordinal == 1206:
        return ATLAS_GOAL, 1  # bottom goal frame (kBottomGoalSprite)
    return ATLAS_NONE, -1


def main():
    gog_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_GOG_DIR
    entries, used_indices = build_table(gog_dir)

    lines = []
    lines.append("// Mechanically extracted by tools/extract_render_frames.py from the user's "
                  "real GOG SWOS .DAT files -- DO NOT EDIT BY HAND.")
    lines.append("// See that script's module docstring for the full source/ordinal-range trace.")
    lines.append(f"// {len(used_indices)} of {TOTAL_FRAMES} indices are actually reachable by the "
                  "VM's real ported animation system (AnimationTablesData.cs) -- see "
                  "RENDER_FRAMES_USED_INDICES below, consumed by tests/test_render_frames.c's "
                  "completeness check.")
    lines.append("")
    lines.append(f"static const SwosRenderFrameInfo RENDER_FRAMES[{TOTAL_FRAMES}] = {{")
    for ordinal in range(TOTAL_FRAMES):
        width, nlines, cx, cy = entries[ordinal]
        cat = classify(ordinal)
        atlas_id, atlas_frame = atlas_for(ordinal)
        lines.append(f"    {{ true, {cx}, {cy}, {width}, {nlines}, {atlas_id}, {atlas_frame}, {cat} }}, // {ordinal}")
    lines.append("};")
    lines.append("")

    used_sorted = sorted(used_indices)
    lines.append(f"#define RENDER_FRAMES_USED_COUNT {len(used_sorted)}")
    lines.append("static const int32_t RENDER_FRAMES_USED_INDICES[RENDER_FRAMES_USED_COUNT] = {")
    for i in range(0, len(used_sorted), 16):
        lines.append("    " + ", ".join(str(v) for v in used_sorted[i:i + 16]) + ",")
    lines.append("};")
    lines.append("")

    OUT_HEADER.parent.mkdir(parents=True, exist_ok=True)
    OUT_HEADER.write_text("\n".join(lines) + "\n", encoding='utf-8')
    print(f"wrote {OUT_HEADER} -- {TOTAL_FRAMES} frame entries, {len(used_sorted)} used indices "
          f"(range {used_sorted[0]}..{used_sorted[-1]})")


if __name__ == "__main__":
    main()
