"""PHASE 5 bugfix (2026-09-16 -- see README.md "Status: Phase 5 bugfix"),
"Goal Post Split": adds the real static goal-frame overlay sprites (posts +
crossbar, drawn as a single sprite per goal) so the ball/goalkeeper are
correctly occluded when they cross behind the goal line, instead of always
drawing on top of the net.

Checked the real engine before writing any code (per this project's own
rule against inventing render tricks): swos-port/src/sprites/gameSprites.cpp
has NO separate foreground/background split at all -- initGameSprites()
places `swos.goal1TopSprite`/`swos.goal2BottomSprite` at a FIXED world
position (kGoalX=300, kTopGoalY=129, kBottomGoalY=778) as two more entries
in the SAME flat, plain-ascending-worldY-sorted `kAllSprites` list as every
player and the ball. The "ball disappears behind the net" illusion is
nothing more than: an object standing further into the goal (smaller
worldY at the top goal / larger at the bottom) sorts BEFORE the fixed-
position goal sprite and gets drawn under it; an object still in front of
the goal line sorts AFTER it and draws on top. No new sort rule, no new
layer -- this project's own `SWOS_RENDER_LAYER_FOREGROUND` placeholder
(reserved back in Phase 2 for a "goal foreground slice" that turned out not
to exist) goes unused; the two new commands use plain
`SWOS_RENDER_LAYER_SPRITE`, same as everyone else.

Real ordinals (swos-port/src/sprites/sprites.h): kTopGoalSprite=1205,
kBottomGoalSprite=1206 -- both inside BENCH.DAT's 1179-1333 range (local
index 26/27), still genuinely unresolved pixel-wise
(SWOS_RENDER_ATLAS_NONE) before this. Real position constants cross-checked
against swos-port/src/game/pitch/pitchConstants.h: kTopPitchLine=129
matches gameSprites.cpp's own kTopGoalY exactly; kGoalX=300 sits inside the
296-372 real goal-post X range there, and the sprite's own measured width
(73px) is close to that same 76px span -- not a coincidence, corroborates
both numbers are the real thing, not misread.

Pipeline: identical technique to tools/extract_keeper_atlas.py (decode
real sprites via decode_sprites.py, bin-pack via BlocksDS's own squeezer).
Canvas is 128x64 (8KB) -- real content is only ~2555px (73x7 + 73x28), tiny
next to the DS VRAM budget the keeper atlas already had to watch.

Usage: python tools/extract_goal_atlas.py [gog_dir] [squeezer_exe]
Writes nds-app/graphics/goal_atlas_texture.png and
nds-app/source/goal_atlas.h / goal_atlas.c.
"""
import subprocess
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).parent.parent
SWOS_DS_TOOLS = REPO_ROOT.parent / "swos-ds" / "tools"
sys.path.insert(0, str(SWOS_DS_TOOLS))
from decode_sprites import scan_dat_file, decode_sprite_pixels, sprite_to_indexed_png, load_palette, resolve_face_palette  # noqa: E402

DEFAULT_GOG_DIR = Path(r"C:\Program Files\GOG Galaxy\Games\Sensible World of Soccer 96-97")
DEFAULT_SQUEEZER = Path(r"C:\msys64\opt\wonderful\thirdparty\blocksds\core\tools\squeezer\squeezerw.exe")

# Local BENCH.DAT indices (global ordinal - 1179), per sprites.txt/sprites.h's
# kTopGoalSprite=1205 / kBottomGoalSprite=1206.
GOAL_LOCAL_INDICES = {"top": 26, "bottom": 27}
ATLAS_W, ATLAS_H = 128, 64

OUT_TEXTURE = REPO_ROOT / "nds-app" / "graphics" / "goal_atlas_texture.png"
OUT_H = REPO_ROOT / "nds-app" / "source" / "goal_atlas.h"
OUT_C = REPO_ROOT / "nds-app" / "source" / "goal_atlas.c"


def main():
    gog_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_GOG_DIR
    squeezer = Path(sys.argv[2]) if len(sys.argv) > 2 else DEFAULT_SQUEEZER
    if not squeezer.is_file():
        raise SystemExit(f"squeezer not found at {squeezer} -- pass its path as the 2nd argument")

    raw_palette = load_palette(gog_dir / "PAL.256")
    palette = resolve_face_palette(raw_palette, "white")  # same convention as every other atlas

    buf, sprites = scan_dat_file(gog_dir / "BENCH.DAT", max_sprites=1000)
    assert len(sprites) == 155, f"expected 155 sprites in BENCH.DAT, got {len(sprites)}"

    with tempfile.TemporaryDirectory(prefix="swos_goal_atlas_") as tmp:
        tmp_dir = Path(tmp)
        # spr0000.png = top goal, spr0001.png = bottom goal -- fixed order,
        # matches GOAL_texcoords[0]/[1] indexing swos_render_commands.c will use.
        for out_i, key in enumerate(("top", "bottom")):
            header = sprites[GOAL_LOCAL_INDICES[key]]
            w, h = header["width_exact"], header["nlines"]
            pixels = decode_sprite_pixels(buf, header)
            sprite_to_indexed_png(pixels, w, h, palette, tmp_dir / f"spr{out_i:04d}.png")

        OUT_TEXTURE.parent.mkdir(parents=True, exist_ok=True)
        OUT_H.parent.mkdir(parents=True, exist_ok=True)
        result = subprocess.run(
            [str(squeezer),
             "--width", str(ATLAS_W), "--height", str(ATLAS_H),
             "--outputTexture", str(OUT_TEXTURE),
             "--outputH", str(OUT_H),
             "--outputC", str(OUT_C),
             "--outputInfo", str(tmp_dir / "squeezer.xml"),
             "--outputBaseName", "GOAL",
             "."],
            cwd=tmp_dir, capture_output=True, text=True)
        if result.returncode != 0:
            raise SystemExit(f"squeezer failed (exit {result.returncode}):\n{result.stdout}\n{result.stderr}")

    print(f"wrote {OUT_TEXTURE}, {OUT_H}, {OUT_C} -- top (global 1205) + bottom (global 1206) "
          f"goal-frame sprites packed into a {ATLAS_W}x{ATLAS_H} atlas")


if __name__ == "__main__":
    main()
