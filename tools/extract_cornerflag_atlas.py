"""Corner-flag texture atlas (2026-09-16 -- see README.md "Status" for the
current dated entry). Same already-documented shape as the referee gap:
`swos_game_sprites.c`'s `swosGameSpritesUpdateCornerFlags()` (VERIFIED_PC
port of gameSprites.cpp:252-279) already computes real fixed position + a
real wind-animation frame for all 4 corner flags every tick, and
`swosGameSpritesGetCornerFlag()` already exposes it -- but the flags' 4
real animation frames (global 1184-1187, inside BENCH.DAT's 1179-1333
catch-all range) never had a pixel texture built, so
RENDER_FRAMES[1184..1187].atlasId was SWOS_RENDER_ATLAS_NONE.

Same technique as every other atlas this session: decode the real sprites
(decode_sprites.py) and bin-pack via BlocksDS's own squeezer. Local
BENCH.DAT indices 5-8 (global ordinal - 1179). Real content is tiny (4
frames, 16x12 each, 768px total) so a 64x32 canvas (2KB) is plenty.

Usage: python tools/extract_cornerflag_atlas.py [gog_dir] [squeezer_exe]
Writes nds-app/graphics/cornerflag_atlas_texture.png and
nds-app/source/cornerflag_atlas.h / cornerflag_atlas.c.
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

BENCH_LOCAL_START = 5  # global 1184 - 1179
NUM_FRAMES = 4         # global 1184-1187
ATLAS_W, ATLAS_H = 64, 32

OUT_TEXTURE = REPO_ROOT / "nds-app" / "graphics" / "cornerflag_atlas_texture.png"
OUT_H = REPO_ROOT / "nds-app" / "source" / "cornerflag_atlas.h"
OUT_C = REPO_ROOT / "nds-app" / "source" / "cornerflag_atlas.c"


def main():
    gog_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_GOG_DIR
    squeezer = Path(sys.argv[2]) if len(sys.argv) > 2 else DEFAULT_SQUEEZER
    if not squeezer.is_file():
        raise SystemExit(f"squeezer not found at {squeezer} -- pass its path as the 2nd argument")

    raw_palette = load_palette(gog_dir / "PAL.256")
    palette = resolve_face_palette(raw_palette, "white")

    buf, sprites = scan_dat_file(gog_dir / "BENCH.DAT", max_sprites=1000)
    assert len(sprites) == 155, f"expected 155 sprites in BENCH.DAT, got {len(sprites)}"

    with tempfile.TemporaryDirectory(prefix="swos_cornerflag_atlas_") as tmp:
        tmp_dir = Path(tmp)
        for out_i in range(NUM_FRAMES):
            header = sprites[BENCH_LOCAL_START + out_i]
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
             "--outputBaseName", "CORNERFLAG",
             "."],
            cwd=tmp_dir, capture_output=True, text=True)
        if result.returncode != 0:
            raise SystemExit(f"squeezer failed (exit {result.returncode}):\n{result.stdout}\n{result.stderr}")

    print(f"wrote {OUT_TEXTURE}, {OUT_H}, {OUT_C} -- {NUM_FRAMES} corner-flag frames (global 1184-1187) "
          f"packed into a {ATLAS_W}x{ATLAS_H} atlas")


if __name__ == "__main__":
    main()
