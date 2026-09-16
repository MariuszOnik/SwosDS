// SOURCE: openswos game/scripts/Sim/Port/SkillScaling.cs (full file). See
// swos_skill_scaling.h for the full FIDELITY/scope note.
#include "swos_skill_scaling.h"

#include <stdint.h>
#include <stddef.h>

#include "swos_rng.h"
#include "swos_tactics_loader.h"

#include "generated/swos_skill_scaling_data.h"

bool g_swosSkillScalingEnabled = true;
bool g_swosSkillScalingAllPlayerTeamsEqual = false;
int g_swosSkillScalingLeagueAvgTeamValue = 0;

// tact == NULL stands in for SkillScaling.cs's ZeroTactics (an unbuilt
// USER_A..F tactics slot, see swos_skill_scaling_data.h's header note) --
// every packed coordinate reads as 0 (-> quadrant 0 via CoordToQuadrant[0]).
static uint8_t tactByte(const uint8_t *tact, int idx) {
    return tact ? tact[idx] : 0;
}

// SkillScaling.cs:438-445 (PackSkills).
static uint32_t packSkills(const SwosPlayerRecord *p) {
    return (uint32_t)(p->finishing & 7)
        | ((uint32_t)(p->speed    & 7) << 4)
        | ((uint32_t)(p->control  & 7) << 8)
        | ((uint32_t)(p->tackling & 7) << 12)
        | ((uint32_t)(p->heading  & 7) << 16)
        | ((uint32_t)(p->shooting & 7) << 20)
        | ((uint32_t)(p->passing  & 7) << 24);
}

// SkillScaling.cs:309-326 (CountNearnessFactor).
static int countNearnessFactor(int ballQ, int dest, int acc, const uint8_t *tact, int posRow) {
    int near = 0, nearDest = 0;
    for (int q = 34; q >= 0; q--) {
        if (q == ballQ) continue;
        if (QuadrantDistanceFactor[ballQ * 35 + q] < 8) continue;
        near++;
        int dq = CoordToQuadrant[tactByte(tact, posRow + q)];
        if (QuadrantDistanceFactor[dest * 35 + dq] < 4) continue;
        nearDest++;
    }
    if (near == 0) return acc;
    return acc * nearDest / near;
}

// SkillScaling.cs:201-303 (ComputePlayerPrice).
int swosSkillScalingComputePlayerPrice(const SwosPlayerRecord *p, const SwosTeamRecord *team,
                                        int playerNo, bool humanControlled) {
    int pos = p->position;
    if (pos < 1 || pos > 7 || playerNo < 1 || playerNo > 10 || team == NULL)
        return p->valueCode;

    uint32_t skills = packSkills(p) + 0x1111111u;

    const uint8_t *tact = (unsigned)team->tactics < TACTICS_LOADER_NUM_BUILTIN_TACTICS
        ? swosTacticsLoaderBuiltinTactics(team->tactics)
        : NULL;
    int posRow = 9 + (playerNo - 1) * 35;
    int rowBase = (pos - 1) * 35;

    int sum = 0;
    for (int ballQ = 34; ballQ >= 0; ballQ--) {
        int dest = CoordToQuadrant[tactByte(tact, posRow + ballQ)];

        int numPlayersAtQuadrant = 1;
        if (humanControlled && ballQ >= 5 && ballQ < 30 && ballQ != 7) {
            int count = 0;
            for (int pl = 0; pl < 10; pl++)
                if (CoordToQuadrant[tactByte(tact, 9 + pl * 35 + ballQ)] == dest)
                    count++;
            numPlayersAtQuadrant = count;
        }

        uint32_t sk = skills;
        uint32_t ponders = PriceSkillPonders[dest];
        int acc = 0;
        for (int k = 0; k < 7; k++) {
            int ponder = (int)(ponders & 0xF);
            if (ponder != 0)
                acc += (int)(sk & 0xF) * SkillValuePonders[ponder];
            sk >>= 4;
            ponders >>= 4;
        }

        acc >>= 1;
        acc *= SkillsPerQuadrant[rowBase + dest];
        acc *= QuadrantDistanceFactor[ballQ * 35 + dest];
        acc >>= 3;

        if (humanControlled) {
            if (numPlayersAtQuadrant - 1 > 0)
                acc /= numPlayersAtQuadrant - 1;
            acc = countNearnessFactor(ballQ, dest, acc, tact, posRow);
        }

        sum = (sum + acc) & 0xFFFF;
    }

    // Sign-extend the 16-bit word before an UNSIGNED divide, matching the
    // original's cwde-then-unsigned-div quirk (see the C# comment this
    // mirrors) -- sum is always 0..0xFFFF (masked above), so (int16_t)sum
    // reinterprets its bit pattern exactly as C#'s (short)sum does.
    int16_t sumS16 = (int16_t)sum;
    uint32_t dividend = (uint32_t)(int32_t)sumS16;
    uint32_t divResult = dividend / 350u;
    int16_t priceS16 = (int16_t)(uint16_t)divResult;
    int price = (int)priceS16;
    price -= 8;
    if (price < 0) price = 0;
    else if (price > 49) price = 49;

    int diff = price - p->valueCode;
    if (diff < 0) return price;
    return PlPricesIncreaseTable[diff] + p->valueCode;
}

// SkillScaling.cs:341-360 (ComputePricePercent).
int swosSkillScalingComputePricePercent(int computedPrice, int filePrice, bool humanControlled) {
    int file = (int8_t)(uint8_t)filePrice;
    if (file == 0) file = 1;

    int percent = computedPrice * 100 / file;
    if (!humanControlled) percent = 100;

    percent += (swosRngNextByte2() * 24 >> 8) - 12;
    return percent;
}

// SkillScaling.cs:374-393 (ScaleSkill).
int swosSkillScalingScaleSkill(int rawSkill, int pricePercent, int teamAppPercent) {
    int pct = pricePercent;
    if (pct < 75) pct = 75;
    else if (pct > 125) pct = 125;

    pct = pct * teamAppPercent / 100;

    int scaled = rawSkill * pct;
    int result = scaled / 100;
    int remainder = scaled % 100;
    if (swosRngNextByte2() * 100 >> 8 < remainder)
        result++;
    return result;
}

// SkillScaling.cs:402-410 (ComputeTeamValue / GetAveragePlayerPrice).
int swosSkillScalingComputeTeamValue(const SwosTeamRecord *t) {
    if (t == NULL || t->players == NULL) return 0;
    int sum = 0;
    int n = t->playerCount < 16 ? t->playerCount : 16;
    for (int i = 0; i < n; i++)
        sum += (uint8_t)t->players[i].valueCode;
    return sum / 16;
}

// SkillScaling.cs:425-431 (TeamAppPercent).
int swosSkillScalingTeamAppPercent(const SwosTeamRecord *t) {
    if (!g_swosSkillScalingAllPlayerTeamsEqual || g_swosSkillScalingLeagueAvgTeamValue <= 0) return 100;
    int teamAvg = swosSkillScalingComputeTeamValue(t);
    if (teamAvg <= 0) return 100;
    return g_swosSkillScalingLeagueAvgTeamValue * 100 / teamAvg + 6;
}
