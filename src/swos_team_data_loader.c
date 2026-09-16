// SOURCE: openswos game/scripts/Sim/Port/TeamDataLoader.cs (WritePlayerInfos/
// WireTeamFields/GoalieSkillFromPrice -- see swos_team_data_loader.h for the
// full scope note; the offset-constants-only slice from steps 5/5.5/11 is
// unchanged above this file).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes. The
// one Godot.GD.Print debug line (a skill-sum diagnostic at the end of
// WritePlayerInfos) is omitted -- zero Memory effect, same pattern as every
// other GD.Print in this port.
#include "swos_team_data_loader.h"

#include <string.h>

#include "swos_addr.h"
#include "swos_memory.h"
#include "swos_player_energy.h"
#include "swos_skill_scaling.h"
#include "swos_team_data.h"
#include "swos_team_record.h"

#include "generated/swos_team_port_tables.h"

static uint8_t clamp7(int v) {
    if (v < 0) return 0;
    if (v > 7) return 7;
    return (uint8_t)v;
}

// TeamDataLoader.cs:389-402 (WriteAsciiTrunc).
static void writeAsciiTrunc(int addr, const char *s, int maxLen) {
    if (s == NULL) s = "";
    int len = (int)strlen(s);
    int n = len < maxLen ? len : maxLen;
    for (int i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        uint8_t b = c <= 0x7F ? (uint8_t)c : (uint8_t)'?';
        swosWriteByte(addr + i, b);
    }
    for (int i = n; i <= maxLen; i++)
        swosWriteByte(addr + i, 0);
}

// TeamDataLoader.cs:346-357 (GoalieSkillFromPrice).
uint8_t swosTeamDataLoaderGoalieSkillFromPrice(int priceCode, bool top) {
    int d0 = ((int)(int8_t)(uint8_t)priceCode + 3) / 7;
    int rand = swosReadWord(ADDR_gameRandValue);
    int d1 = top ? (rand & 1) : ((rand & 2) >> 1);
    d0 += d1 + 1;
    if (d0 < 0) d0 = 0;
    if (d0 > 7) d0 = 7;
    return (uint8_t)d0;
}

// TeamDataLoader.cs:85-263 (WritePlayerInfos).
void swosTeamDataLoaderWritePlayerInfos(int playersBase, const SwosTeamRecord *team,
                                         bool isHumanControlled) {
    if (team == NULL) return;

    bool top = playersBase == ADDR_team1InGameTeamPlayers;

    int playerCount = team->playerCount;

    int teamAppPercent = g_swosSkillScalingEnabled ? swosSkillScalingTeamAppPercent(team) : 100;

    const uint8_t *order = team->lineupOrder;
    bool orderValid = order != NULL;
    if (orderValid) {
        for (int i = 0; i < 16; i++)
            if (order[i] >= playerCount) { orderValid = false; break; }
    }

    for (int i = 0; i < 16; i++) {
        int slotAddr = playersBase + i * TDL_PLAYER_INFO_SIZE;

        for (int b = 0; b < TDL_PLAYER_INFO_SIZE; b++)
            swosWriteByte(slotAddr + b, 0);

        int rosterIdx = orderValid ? order[i] : i;
        if (rosterIdx >= playerCount) {
            swosWriteByte(slotAddr + TDL_OFF_SUBSTITUTED, 1);
            continue;
        }

        const SwosPlayerRecord *p = &team->players[rosterIdx];

        if (i < 11)
            swosPlayerEnergySeedSlot((top ? 0 : 11) + i, p->stamina, p->fatigueCarry);

        swosWriteByte(slotAddr + TDL_OFF_SUBSTITUTED, 0);
        swosWriteByte(slotAddr + TDL_OFF_INDEX, (uint8_t)rosterIdx);
        swosWriteByte(slotAddr + TDL_OFF_GOALS_SCORED, 0);
        swosWriteByte(slotAddr + TDL_OFF_SHIRT_NUMBER, p->shirtNumber);

        uint8_t positionByte = (uint8_t)p->position;
        swosWriteByte(slotAddr + TDL_OFF_POSITION, positionByte);

        if (g_swosSkillScalingEnabled && i <= 10) {
            int computedPrice = i == 0
                ? p->valueCode
                : swosSkillScalingComputePlayerPrice(p, team, i, isHumanControlled);
            int pricePercent = swosSkillScalingComputePricePercent(
                computedPrice, p->valueCode, isHumanControlled);

            swosWriteByte(slotAddr + TDL_OFF_PASSING,
                clamp7(swosSkillScalingScaleSkill(clamp7(p->passing), pricePercent, teamAppPercent)));
            swosWriteByte(slotAddr + TDL_OFF_SHOOTING,
                clamp7(swosSkillScalingScaleSkill(clamp7(p->shooting), pricePercent, teamAppPercent)));
            swosWriteByte(slotAddr + TDL_OFF_HEADING,
                clamp7(swosSkillScalingScaleSkill(clamp7(p->heading), pricePercent, teamAppPercent)));
            swosWriteByte(slotAddr + TDL_OFF_TACKLING,
                clamp7(swosSkillScalingScaleSkill(clamp7(p->tackling), pricePercent, teamAppPercent)));
            swosWriteByte(slotAddr + TDL_OFF_BALL_CONTROL,
                clamp7(swosSkillScalingScaleSkill(clamp7(p->control), pricePercent, teamAppPercent)));
            swosWriteByte(slotAddr + TDL_OFF_SPEED,
                clamp7(swosSkillScalingScaleSkill(clamp7(p->speed), pricePercent, teamAppPercent)));
            swosWriteByte(slotAddr + TDL_OFF_FINISHING,
                clamp7(swosSkillScalingScaleSkill(clamp7(p->finishing), pricePercent, teamAppPercent)));
        } else {
            swosWriteByte(slotAddr + TDL_OFF_PASSING, clamp7(p->passing));
            swosWriteByte(slotAddr + TDL_OFF_SHOOTING, clamp7(p->shooting));
            swosWriteByte(slotAddr + TDL_OFF_HEADING, clamp7(p->heading));
            swosWriteByte(slotAddr + TDL_OFF_TACKLING, clamp7(p->tackling));
            swosWriteByte(slotAddr + TDL_OFF_BALL_CONTROL, clamp7(p->control));
            swosWriteByte(slotAddr + TDL_OFF_SPEED, clamp7(p->speed));
            swosWriteByte(slotAddr + TDL_OFF_FINISHING, clamp7(p->finishing));
        }

        if (positionByte == TDL_POS_GOALKEEPER) {
            swosWriteByte(slotAddr + TDL_OFF_GOALIE_SKILL,
                          swosTeamDataLoaderGoalieSkillFromPrice(p->valueCode, top));
        }

        if (p->injurySeverity >= 1)
            swosWriteByte(slotAddr + TDL_OFF_INJURIES_BITS, (uint8_t)((p->injurySeverity & 7) << 5));

        writeAsciiTrunc(slotAddr + TDL_OFF_SHORT_NAME, p->name, 14);
        writeAsciiTrunc(slotAddr + TDL_OFF_FULL_NAME, p->name, 22);
    }
}

// TeamDataLoader.cs:268-322 (WireTeamFields).
void swosTeamDataLoaderWireTeamFields(bool top, const SwosTeamRecord *team,
                                       int playersBaseAddr, int shotChanceTableAddr,
                                       int nameStorageAddr, bool isHumanControlled,
                                       int defaultTacticsIndex) {
    int teamBase = swosTeamDataBase(top);
    int inGameTeamBase = playersBaseAddr;

    swosWriteDword(teamBase + TEAMDATA_OFF_IN_GAME_TEAM_PTR, (uint32_t)inGameTeamBase);
    swosWriteDword(top ? ADDR_topTeamInGame : ADDR_bottomTeamInGame, (uint32_t)inGameTeamBase);
    swosWriteWord(teamBase + TEAMDATA_OFF_TEAM_NUMBER, (uint16_t)(top ? 1 : 2));
    swosWriteWord(teamBase + TEAMDATA_OFF_TACTICS, (uint16_t)defaultTacticsIndex);

    int playerNumberValue = isHumanControlled ? (top ? 1 : 2) : 0;
    swosWriteWord(teamBase + TEAMDATA_OFF_PLAYER_NUMBER, (uint16_t)playerNumberValue);

    for (int i = 0; i < 30; i++)
        swosWriteWord(shotChanceTableAddr + i * 2, (uint16_t)swos_kPlayerShotChanceTable[i]);
    swosWriteDword(teamBase + TEAMDATA_OFF_SHOT_CHANCE_TABLE, (uint32_t)shotChanceTableAddr);

    const char *name = (team != NULL) ? team->name : "";
    if (name == NULL) name = "";
    writeAsciiTrunc(nameStorageAddr, name, 17);
    swosWriteDword(top ? ADDR_res_team1Name : ADDR_res_team2Name, (uint32_t)nameStorageAddr);

    if (top) {
        int len = (int)strlen(name);
        swosWriteWord(ADDR_res_team1NameLength, (uint16_t)(len < 17 ? len : 17));
    }
}
