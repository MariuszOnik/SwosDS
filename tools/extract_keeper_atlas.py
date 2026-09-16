"""PHASE 5 (2026-09-16 -- see README.md "Status: Phase 5"). Builds the real
pixel atlas for BOTH goalkeepers, closing the gap Phase 3/4 left as
SWOS_RENDER_ATLAS_NONE and confirmed (not guessed) by Phase 4 bugfix's real
Memory-buffer dump: both keepers sit in their 947-1062/1063-1178 ranges at
every sampled tick, and neither has a texture.

Unlike tools/extract_team2_atlas.py (same 101-frame geometry as team1, only
the kit PIXEL PATTERN differs, so it could piggyback on the EXISTING
PLAYER_texcoords[] layout), goalkeeper sprites have their own, different
per-frame geometry -- there is no pre-existing packing to reuse, so this is
a genuinely new bin-packing pass. It is also simpler than the team1/team2
split in one respect: per tools/extract_render_frames.py's own module
docstring and swos-port/docs/SWOS/sprites.txt ("116 pointers to goal1.dat /
116 -||-"), team2's goalkeeper range (1063-1178) is the SAME PHYSICAL 116
GOAL1.DAT sprites reused for the other side -- not a separately colored kit
file like TEAM2.DAT. So only ONE atlas texture is built here (from
GOAL1.DAT alone); both keeper categories share it, at the same local frame
numbering (ordinal - 947 for team1, ordinal - 1063 for team2).

Pipeline (same decode step as tools/../../swos-ds/tools/extract_player_frames.py,
then real bin-packing instead of a hand-placed layout):
  1. decode_sprites.py (imported from ../../swos-ds/tools, not duplicated)
     decodes GOAL1.DAT's real 116 sprites into indexed PNGs (index 0 forced
     to magenta, grit's transparent color key), same technique proven
     already for the player atlases.
  2. BlocksDS's own `squeezer` tool (tools/squeezer/squeezerw.exe -- the
     SAME tool that packed player_atlas_texture.png/PLAYER_texcoords[],
     per DEVLOG.md's 2026-09-14 entry) bin-packs those 116 PNGs into one
     texture and emits matching texcoords C/H files.

Canvas size: 256x128 (both dimensions valid DS power-of-two texture sizes).
Chosen empirically -- real GOAL1.DAT sprite geometry sums to ~23720px of
actual content (max single frame 16x20), and squeezer's own occupancy
report at 256x128 (32768px capacity) is 0.72, i.e. it fits with headroom.
Deliberately NOT 256x256 (the size used for the two player atlases): DS
texture VRAM here is banks A+B (256KB pool total via vramSetBankA/B in
nds-app/source/main.c), already at ~193KB (two 256x256 player atlases +
ball atlas + pitch tiles) -- a third 256x256 (64KB) atlas would overflow
the remaining ~63KB budget by a small margin, while 256x128 (32KB) fits
comfortably.

Usage: python tools/extract_keeper_atlas.py [gog_dir] [squeezer_exe]
Writes:
  nds-app/graphics/keeper_atlas_texture.png (256x128, 8bpp indexed, index 0
    forced to magenta -- grit-converts automatically via the existing
    keeper_atlas_texture.grit + nds-app Makefile's generic GFXDIRS rule)
  nds-app/source/keeper_atlas.h / keeper_atlas.c (squeezer's own
    KEEPER_texcoords[]/KEEPER_NUM_IMAGES/KEEPER_BITMAP_WIDTH/HEIGHT output --
    named differently from the _texture.* pair per the SAME collision-
    avoidance convention DEVLOG.md documents for player_atlas.h/.c vs
    player_atlas_texture.h/.c, so grit's and squeezer's generated symbol
    names never clash)
"""
import shutil
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

NUM_FRAMES = 116
ATLAS_W, ATLAS_H = 256, 128  # see module docstring for why not 256x256

OUT_TEXTURE = REPO_ROOT / "nds-app" / "graphics" / "keeper_atlas_texture.png"
OUT_H = REPO_ROOT / "nds-app" / "source" / "keeper_atlas.h"
OUT_C = REPO_ROOT / "nds-app" / "source" / "keeper_atlas.c"


def main():
    gog_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_GOG_DIR
    squeezer = Path(sys.argv[2]) if len(sys.argv) > 2 else DEFAULT_SQUEEZER
    if not squeezer.is_file():
        raise SystemExit(f"squeezer not found at {squeezer} -- pass its path as the 2nd argument")

    raw_palette = load_palette(gog_dir / "PAL.256")
    palette = resolve_face_palette(raw_palette, "white")  # same default face as the two player atlases

    buf, sprites = scan_dat_file(gog_dir / "GOAL1.DAT", max_sprites=1000)
    assert len(sprites) == NUM_FRAMES, f"expected {NUM_FRAMES} sprites in GOAL1.DAT, got {len(sprites)}"

    with tempfile.TemporaryDirectory(prefix="swos_keeper_atlas_") as tmp:
        tmp_dir = Path(tmp)
        for i, header in enumerate(sprites):
            w, h = header["width_exact"], header["nlines"]
            pixels = decode_sprite_pixels(buf, header)
            sprite_to_indexed_png(pixels, w, h, palette, tmp_dir / f"spr{i:04d}.png")

        OUT_TEXTURE.parent.mkdir(parents=True, exist_ok=True)
        OUT_H.parent.mkdir(parents=True, exist_ok=True)
        result = subprocess.run(
            [str(squeezer),
             "--width", str(ATLAS_W), "--height", str(ATLAS_H),
             "--outputTexture", str(OUT_TEXTURE),
             "--outputH", str(OUT_H),
             "--outputC", str(OUT_C),
             "--outputInfo", str(tmp_dir / "squeezer.xml"),
             "--outputBaseName", "KEEPER",
             "."],
            cwd=tmp_dir, capture_output=True, text=True)
        if result.returncode != 0:
            raise SystemExit(f"squeezer failed (exit {result.returncode}):\n{result.stdout}\n{result.stderr}")

    print(f"wrote {OUT_TEXTURE}, {OUT_H}, {OUT_C} -- {NUM_FRAMES} goalkeeper frames packed "
          f"into a {ATLAS_W}x{ATLAS_H} atlas (shared by both team1 947-1062 and team2 1063-1178, "
          "same physical sprites per sprites.txt)")


if __name__ == "__main__":
    main()
