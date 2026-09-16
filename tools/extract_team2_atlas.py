"""PHASE 4 (2026-09-16 -- see README.md "Status: Phase 4"). Bounded, same-
technique extension of what Phase 3's scope conversation explicitly
deferred (a full new pixel-extraction pass for every unresolved frame):
just the away team's (global 644-744) outfield-player atlas, reusing the
EXACT SAME decode/palette logic already proven by
../../swos-ds/tools/extract_player_frames.py (imported from there, not
duplicated -- swos-ds itself stays untouched, this only reads its tools/)
and, critically, the EXACT SAME per-frame (x,y,w,h) placement as the
existing player_atlas_texture.png/PLAYER_texcoords[] -- TEAM1.DAT and
TEAM2.DAT's sprite headers carry IDENTICAL width/height per corresponding
local index (verified in tools/extract_render_frames.py's own cross-check
against TEAM3.DAT; TEAM2.DAT's own headers self-declare ordinals 644-946,
i.e. it already knows it's "the other side"), so team2's pixels can be
pasted into a second 256x256 canvas at the SAME coordinates without a new
packing pass -- no new texcoords array needed, the existing
PLAYER_texcoords[] already describes both atlases' layout.

Does NOT touch goalkeepers, referee, bench, or anything else Phase 3 left
as SWOS_RENDER_ATLAS_NONE -- those have no existing atlas layout to piggy-
back on (different frame sizes, no pre-existing packing) and stay
explicitly out of scope, per the same conversation.

Usage: python tools/extract_team2_atlas.py [gog_dir]
Writes nds-app/graphics/player_atlas_team2_texture.png (same 256x256
canvas size, dimensions, and palette convention as player_atlas_texture.png
-- index 0 forced to magenta, grit's transparent color key).
"""
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).parent.parent
SWOS_DS_TOOLS = REPO_ROOT.parent / "swos-ds" / "tools"
sys.path.insert(0, str(SWOS_DS_TOOLS))
from decode_sprites import scan_dat_file, decode_sprite_pixels, load_palette, resolve_face_palette  # noqa: E402

from PIL import Image  # noqa: E402

DEFAULT_GOG_DIR = Path(r"C:\Program Files\GOG Galaxy\Games\Sensible World of Soccer 96-97")
ATLAS_SIZE = 256
NUM_FRAMES = 101
OUT_PATH = REPO_ROOT / "nds-app" / "graphics" / "player_atlas_team2_texture.png"
TEXCOORDS_C = REPO_ROOT / "nds-app" / "source" / "player_atlas.c"


def load_texcoords():
    """Parse the EXISTING PLAYER_texcoords[] out of the already-generated
    player_atlas.c (4 uint16 per frame: x, y, w, h) -- reused verbatim, not
    regenerated, so both atlases are guaranteed pixel-aligned."""
    import re
    text = TEXCOORDS_C.read_text(encoding="utf-8")
    start = text.index("{") + 1
    end = text.index("};", start)
    body = text[start:end]
    body = re.sub(r"//[^\n]*", "", body)  # strip `// sprNNNN.png` comments before scanning for digits
    nums = [int(t) for t in re.findall(r"\d+", body)]
    assert len(nums) == NUM_FRAMES * 4, f"expected {NUM_FRAMES * 4} texcoord numbers, got {len(nums)}"
    return [tuple(nums[i:i + 4]) for i in range(0, len(nums), 4)]


def main():
    gog_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_GOG_DIR

    raw_palette = load_palette(gog_dir / "PAL.256")
    palette = resolve_face_palette(raw_palette, "white")  # same default face as player_atlas_texture.png

    buf, sprites = scan_dat_file(gog_dir / "TEAM2.DAT", max_sprites=1000)
    assert len(sprites) == 303, f"expected 303 sprites in TEAM2.DAT, got {len(sprites)}"
    team2_frames = sprites[0:NUM_FRAMES]

    texcoords = load_texcoords()

    canvas = Image.new("P", (ATLAS_SIZE, ATLAS_SIZE))
    pal_bytes = []
    for i in range(256):
        if i == 0:
            pal_bytes.extend((255, 0, 255))
        elif i < len(palette):
            pal_bytes.extend(palette[i])
        else:
            pal_bytes.extend((0, 0, 0))
    canvas.putpalette(pal_bytes)
    # Fill with index 0 (transparent) everywhere first.
    canvas.putdata(bytes(ATLAS_SIZE * ATLAS_SIZE))

    for i, header in enumerate(team2_frames):
        w, h = header["width_exact"], header["nlines"]
        tx, ty, tw, th = texcoords[i]
        assert (tw, th) == (w, h), (
            f"frame {i}: team1 atlas slot is {tw}x{th} but team2's real sprite is {w}x{h} -- "
            "the 'identical geometry across kit files' assumption doesn't hold, stop")
        pixels = decode_sprite_pixels(buf, header)
        frame_img = Image.new("P", (w, h))
        frame_img.putpalette(pal_bytes)
        frame_img.putdata(bytes(pixels))
        canvas.paste(frame_img, (tx, ty))

    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(OUT_PATH)
    print(f"wrote {OUT_PATH} -- {len(team2_frames)} frames pasted at the existing PLAYER_texcoords layout")


if __name__ == "__main__":
    main()
