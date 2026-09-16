"""Real match sound effects (2026-09-16). Converts the exact same .RAW PCM
files swos-port's own SoundSample.cpp plays at runtime into plain .wav
files BlocksDS's mmutil can bin-pack into a maxmod soundbank.

Format verified from two independent real sources, not guessed:
  - swos-port/docs/SWOS/sound.txt: "All sounds are in RAW format, 22kHz
    mono ... Only two exceptions are referee final whistle and foul
    whistle which are 11Khz always."
  - swos-port/src/audio/SoundSample::is11KhzSample() (the actual runtime
    check): only endgamew.raw and foul.raw are 11025Hz; everything else
    here is 22050Hz. Matches wavFormat.h's own WAV template: PCM format 1,
    mono, 8 bits per sample.

Real .RAW sizes cross-checked byte-for-byte against the user's own
legally-owned GOG install (SFX/FX/*.RAW) before writing anything --
bouncex.raw=2633, homegoal.raw=194187, whistle.raw=1080, foul.raw=2518,
endgamew.raw=23882, matching sound.txt's own file table exactly.

Six map 1:1 to the six real MatchAudio.* SwosAudioEvent values (see
include/swos_audio_events.h): ball bounce, goal, foul whistle, restart
whistle, end-game whistle, and kick (kickx.raw -- the same sound behind
all 8 of PlayKickSample's real call sites, matching the original engine's
own behavior of playing that one sound at every one of them).

Usage: python tools/extract_sound_effects.py [gog_dir]
Writes nds-app/audio/<name>.wav for each entry in SOUND_EFFECTS below --
mmutil (already wired in nds-app/Makefile) derives SFX_<NAME> constants
from these filenames automatically once AUDIODIRS := audio is set.
"""
import struct
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).parent.parent
DEFAULT_GOG_DIR = Path(r"C:\Program Files\GOG Galaxy\Games\Sensible World of Soccer 96-97")
OUT_DIR = REPO_ROOT / "nds-app" / "audio"

# out name -> (source .RAW filename under SFX/FX, sample rate Hz).
SOUND_EFFECTS = {
    "bounce":  ("BOUNCEX.RAW", 22050),
    "goal":    ("HOMEGOAL.RAW", 22050),
    "whistle": ("WHISTLE.RAW", 22050),
    "foul":    ("FOUL.RAW", 11025),
    "endgame": ("ENDGAMEW.RAW", 11025),
    "kick":    ("KICKX.RAW", 22050),
}

EXPECTED_SIZES = {
    "BOUNCEX.RAW": 2633,
    "HOMEGOAL.RAW": 194187,
    "WHISTLE.RAW": 1080,
    "FOUL.RAW": 2518,
    "ENDGAMEW.RAW": 23882,
    "KICKX.RAW": 2105,
}


def write_wav(pcm: bytes, sample_rate: int, out_path: Path):
    """8-bit unsigned mono PCM WAV -- same parameters as swos-port's own
    kWaveHeader (wavFormat.h): format tag 1 (PCM), 1 channel, 8 bits/sample."""
    num_channels = 1
    bits_per_sample = 8
    byte_rate = sample_rate * num_channels * bits_per_sample // 8
    block_align = num_channels * bits_per_sample // 8
    header = b"RIFF" + struct.pack("<I", 36 + len(pcm)) + b"WAVE"
    header += b"fmt " + struct.pack("<IHHIIHH", 16, 1, num_channels, sample_rate,
                                     byte_rate, block_align, bits_per_sample)
    header += b"data" + struct.pack("<I", len(pcm))
    out_path.write_bytes(header + pcm)


def main():
    gog_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_GOG_DIR
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    for name, (filename, rate) in SOUND_EFFECTS.items():
        src = gog_dir / "SFX" / "FX" / filename
        pcm = src.read_bytes()
        expected = EXPECTED_SIZES[filename]
        if len(pcm) != expected:
            raise SystemExit(f"{src}: expected {expected} bytes (per sound.txt's own table), "
                              f"got {len(pcm)} -- refusing to guess, this isn't the file documented")
        out_path = OUT_DIR / f"{name}.wav"
        write_wav(pcm, rate, out_path)
        print(f"wrote {out_path} ({len(pcm)} bytes PCM, {rate}Hz, from {filename})")


if __name__ == "__main__":
    main()
