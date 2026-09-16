// Mechanically extracted from D:/Programowanie/SWOS_DS/openswos/game/scripts/Sim/Port/Pitch.cs -- DO NOT EDIT BY HAND.
// kPitchTypeProbabilities/kPitchNumberProbabilities: tools/extract_all_arrays.py
// (handles flat byte[] declarations, same as every other generated table here).
// kPitchTypeSeasonalProbabilities: extract_all_arrays.py's regex only
// handles single-bracket `T[]` declarations, not C#'s 2-D `byte[,]` syntax,
// so this one 12x7 table was pulled out with a one-off Python snippet
// (regex-extract the array body, strip comments, re.findall all digit
// runs, flatten row-major) instead -- still 100% mechanical, zero
// hand-retyped digits; not folded into the shared extractor since this
// repo has exactly one 2-D literal table and it isn't worth a permanent
// tool feature for one caller.
static const uint8_t kPitchTypeProbabilities[] = { 5, 5, 10, 20, 30, 20, 10 };
static const uint8_t kPitchNumberProbabilities[] = { 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 4, 4, 2, 2, 3, 3 };
static const uint8_t kPitchTypeSeasonalProbabilities[12][7] = {
    { 30, 20, 30, 20, 0, 0, 0 },
    { 20, 30, 20, 20, 10, 0, 0 },
    { 10, 30, 10, 30, 20, 0, 0 },
    { 0, 10, 10, 30, 40, 10, 0 },
    { 0, 0, 0, 10, 40, 40, 10 },
    { 0, 0, 0, 0, 40, 40, 20 },
    { 0, 0, 0, 0, 30, 30, 40 },
    { 0, 0, 0, 0, 50, 30, 20 },
    { 0, 0, 0, 20, 40, 30, 10 },
    { 0, 20, 0, 40, 30, 10, 0 },
    { 10, 30, 10, 40, 10, 0, 0 },
    { 20, 30, 20, 30, 0, 0, 0 },
};
