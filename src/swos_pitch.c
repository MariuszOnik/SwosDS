// SOURCE: openswos game/scripts/Sim/Port/Pitch.cs (full file). See
// swos_pitch.h for the full FIDELITY/scope note.
#include "swos_pitch.h"

#include "swos_addr.h"
#include "swos_game_time.h"
#include "swos_memory.h"
#include "swos_rng.h"

#include "generated/swos_pitch_data.h"

#define K_MAX_PITCH_TYPE      6
#define K_TRAINING_PITCH_INDEX 5

int g_swosBallSimCurrentPitchNumber = 0;

void swosPitchSetPitchType(void) {
    int16_t typeOrSeason = swosReadSignedWord(ADDR_gamePitchTypeOrSeason);
    int16_t manualType = swosReadSignedWord(ADDR_gamePitchType);
    int16_t season = swosReadSignedWord(ADDR_gameSeason);

    int pitchType = 0;

    if (typeOrSeason != 0 || manualType == -1) {
        if (typeOrSeason != 0) {
            if (season < 0) season = 0;
            if (season > 11) season = 11;
        }

        int probability = swosRngNextByte() * 100 / 256;

        for (;;) {
            int weight = typeOrSeason != 0
                ? kPitchTypeSeasonalProbabilities[season][pitchType]
                : kPitchTypeProbabilities[pitchType];
            if (probability < weight) break;
            probability -= weight;
            pitchType++;
            if (pitchType >= K_MAX_PITCH_TYPE) { pitchType = K_MAX_PITCH_TYPE; break; }
        }
    } else {
        pitchType = manualType;
        if (pitchType < 0) pitchType = 0;
        if (pitchType > K_MAX_PITCH_TYPE) pitchType = K_MAX_PITCH_TYPE;
    }

    g_swosBallSimCurrentPitchType = pitchType;
}

void swosPitchSetPitchNumber(void) {
    if (swosReadSignedWord(ADDR_g_trainingGame) != 0) {
        g_swosBallSimCurrentPitchNumber = K_TRAINING_PITCH_INDEX;
        return;
    }

    int16_t plgD0 = swosReadSignedWord(ADDR_plg_D0_param);

    int index;
    if (plgD0 != 0) {
        index = swosRngNextByte() & 0xF;
    } else {
        // swos.h:296-311 TeamGame layout, same offsets as Result.cs's own
        // verified table (see Pitch.cs's comment).
        const int kOffPrShirtCol = 2;
        const int kOffPrShortsCol = 6;
        const int kOffTeamName = 22;

        int headerBase = ADDR_team1InGameTeamHeader;
        uint8_t name0 = swosReadByte(headerBase + kOffTeamName + 0);
        uint8_t name1 = swosReadByte(headerBase + kOffTeamName + 1);
        int16_t shirtCol = swosReadSignedWord(headerBase + kOffPrShirtCol);
        int16_t shortsCol = swosReadSignedWord(headerBase + kOffPrShortsCol);

        index = name0 | (name1 << 8);
        index ^= shirtCol;
        index ^= shortsCol;
        index &= 0xF;
    }

    g_swosBallSimCurrentPitchNumber = kPitchNumberProbabilities[index];
}

void swosPitchSetPitchTypeAndNumber(void) {
    swosPitchSetPitchType();
    swosPitchSetPitchNumber();
}

int swosPitchGetPitchType(void) { return g_swosBallSimCurrentPitchType; }
int swosPitchGetPitchNumber(void) { return g_swosBallSimCurrentPitchNumber; }
