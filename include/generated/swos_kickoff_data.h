// Mechanically extracted from D:/Programowanie/SWOS_DS/openswos/game/scripts/Sim/Port/Kickoff.cs -- DO NOT EDIT BY HAND.
// KTeamsStartingCoordinates is a `(short, short)[]` tuple array -- outside
// extract_all_arrays.py's regex (single-type `T[]` declarations only, see
// its own docstring), so pulled out with the same one-off Python
// regex-extract + re.findall technique already used for Pitch.cs's 2-D
// table (see swos_pitch_data.h's header note) -- still 100% mechanical,
// zero hand-retyped digits, flattened to 44 int16_t (x0,y0,x1,y1,...).
static const int16_t kKTeamsStartingCoordinates[44] = {
    300, 69, 280, 46, 260, 34, 240, 24, 220, 16, 200, 9,
    180, 3, 160, -2, 140, -8, 120, -9, 100, -11,
    300, -65, 280, -42, 260, -30, 240, -20, 220, -12, 200, -5,
    180, 1, 160, 6, 140, 12, 120, 13, 100, 15,
};
