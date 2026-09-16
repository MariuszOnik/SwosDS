// SOURCE: openswos game/scripts/Sim/Port/PlayerEnergy.cs (see
// swos_player_energy.h for the exact slice ported).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
#include "swos_player_energy.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"
#include "swos_rng.h"

bool g_swosPlayerEnergyEffectEnabled = false;

#define PLAYER_ENERGY_MAX 4096

// PlayerEnergy.cs:70-84.
void swosPlayerEnergySeedSlot(int globalSlot, int stamina, int fatigueCarry) {
    if (globalSlot < 0 || globalSlot >= PLSPR_TOTAL_SLOTS) return;
    int s = stamina < 0 ? 0 : (stamina > 7 ? 7 : stamina);
    int fc = fatigueCarry < 0 ? 0 : (fatigueCarry > 100 ? 100 : fatigueCarry);
    int initial = PLAYER_ENERGY_MAX - fc * (PLAYER_ENERGY_MAX * 6 / 10) / 100 - (7 - s) * 24;
    int floor = PLAYER_ENERGY_MAX * 4 / 10;
    if (initial < floor) initial = floor;
    if (initial > PLAYER_ENERGY_MAX) initial = PLAYER_ENERGY_MAX;
    int base_ = swosPlayerSpriteBase(globalSlot);
    swosWriteWord(base_ + PLSPR_OFF_ENERGY, (uint16_t)initial);
    swosWriteWord(base_ + PLSPR_OFF_ENERGY_ACC, 0);
    swosWriteByte(base_ + PLSPR_OFF_STAMINA, s);
}

// PlayerEnergy.cs:119-126.
int swosPlayerEnergySpeedStep(int spriteAddr) {
    int energy = swosReadWord(spriteAddr + PLSPR_OFF_ENERGY);
    if (energy > PLAYER_ENERGY_MAX * 50 / 100) return 0;
    if (energy > PLAYER_ENERGY_MAX * 25 / 100) return 1;
    if (energy > PLAYER_ENERGY_MAX * 10 / 100) return 2;
    return 3;
}

// PlayerEnergy.cs:130-135.
int swosPlayerEnergyShotPenalty(int spriteAddr) {
    if (!g_swosPlayerEnergyEffectEnabled) return 0;
    int energy = swosReadWord(spriteAddr + PLSPR_OFF_ENERGY);
    return energy <= PLAYER_ENERGY_MAX * 10 / 100 ? 1 : 0;
}

// PlayerEnergy.cs:204-212.
void swosPlayerEnergyDrainOnKeeperCatch(int spriteAddr) {
    if (!g_swosPlayerEnergyEffectEnabled) return;
    int energy = swosReadWord(spriteAddr + PLSPR_OFF_ENERGY);
    if (energy <= 0) return;
    int pct = 2 + (swosRngNextByte() % 3);   // 2..4
    int drained = energy - energy * pct / 100;
    if (drained < 0) drained = 0;
    swosWriteWord(spriteAddr + PLSPR_OFF_ENERGY, (uint16_t)drained);
}

// PlayerEnergy.cs:214-223.
int swosPlayerEnergyKeeperSkillPenalty(int spriteAddr) {
    if (!g_swosPlayerEnergyEffectEnabled) return 0;
    int energy = swosReadWord(spriteAddr + PLSPR_OFF_ENERGY);
    if (energy <= PLAYER_ENERGY_MAX * 20 / 100) return 2;
    if (energy <= PLAYER_ENERGY_MAX * 50 / 100) return 1;
    return 0;
}

#define PLAYER_ENERGY_KEEPER_EFFORT 10
#define PLAYER_ENERGY_MOVE_EFFORT   100
#define PLAYER_ENERGY_STAMINA_FLOOR 8
#define PLAYER_ENERGY_DIVISOR_SCALE 24

// PlayerEnergy.cs:57 -- `_lenNum`/`_lenDen`. Default 1/1 (no-op) until
// swosPlayerEnergySetMatchLength() below is called.
static int s_lenNum = 1, s_lenDen = 1;

#define PLAYER_ENERGY_REF_MATCH_SECONDS 360

// PlayerEnergy.cs:58-63.
void swosPlayerEnergySetMatchLength(int totalMatchSeconds) {
    if (totalMatchSeconds <= 0) { s_lenNum = 1; s_lenDen = 1; return; }
    s_lenNum = totalMatchSeconds;
    s_lenDen = PLAYER_ENERGY_REF_MATCH_SECONDS;
}

// PlayerEnergy.cs:89-109.
void swosPlayerEnergyDrainSlot(int spriteAddr) {
    int isMoving = swosReadByte(spriteAddr + PLSPR_OFF_IS_MOVING);
    if (isMoving == 0) return;

    int gslot = (spriteAddr - PLSPR_SPRITE_POOL_BASE) / PLSPR_SLOT_STRIDE;
    bool keeper = (gslot == PLSPR_SLOT_GOALIE1) || (gslot == PLSPR_SLOT_GOALIE2);
    int effort = keeper ? PLAYER_ENERGY_KEEPER_EFFORT : PLAYER_ENERGY_MOVE_EFFORT;

    int stamina = swosReadByte(spriteAddr + PLSPR_OFF_STAMINA);
    if (stamina < 0) stamina = 0;
    if (stamina > 7) stamina = 7;
    int divisor = (PLAYER_ENERGY_STAMINA_FLOOR + stamina) * PLAYER_ENERGY_DIVISOR_SCALE;
    divisor = divisor * s_lenNum / s_lenDen;
    if (divisor < 1) divisor = 1;

    int acc = swosReadWord(spriteAddr + PLSPR_OFF_ENERGY_ACC) + effort;
    int energy = swosReadWord(spriteAddr + PLSPR_OFF_ENERGY);
    while (acc >= divisor && energy > 0) { acc -= divisor; energy--; }
    if (energy < 0) energy = 0;
    swosWriteWord(spriteAddr + PLSPR_OFF_ENERGY_ACC, (uint16_t)acc);
    swosWriteWord(spriteAddr + PLSPR_OFF_ENERGY, (uint16_t)energy);
}

// PlayerEnergy.cs:190-198.
void swosPlayerEnergyDrainOnTackle(int spriteAddr) {
    if (!g_swosPlayerEnergyEffectEnabled) return;
    int energy = swosReadWord(spriteAddr + PLSPR_OFF_ENERGY);
    if (energy <= 0) return;
    int pct = 1 + (swosRngNextByte() % 5); // 1..5
    int drained = energy - energy * pct / 100;
    if (drained < 0) drained = 0;
    swosWriteWord(spriteAddr + PLSPR_OFF_ENERGY, (uint16_t)drained);
}

// PlayerEnergy.cs:139-144.
bool swosPlayerEnergyInjuryRiskDoubled(int spriteAddr) {
    if (!g_swosPlayerEnergyEffectEnabled) return false;
    int energy = swosReadWord(spriteAddr + PLSPR_OFF_ENERGY);
    return energy < PLAYER_ENERGY_MAX * 20 / 100;
}

#define PLAYER_ENERGY_HALF_TIME_RECOVER_PCT 40

// PlayerEnergy.cs:160-179.
void swosPlayerEnergyRecoverAtHalfTime(void) {
    for (int gslot = 0; gslot < PLSPR_TOTAL_SLOTS; gslot++) {
        int b = swosPlayerSpriteBase(gslot);
        int energy = swosReadWord(b + PLSPR_OFF_ENERGY);
        if (energy <= 0 || energy >= PLAYER_ENERGY_MAX) continue;
        int recovered = (PLAYER_ENERGY_MAX - energy) * PLAYER_ENERGY_HALF_TIME_RECOVER_PCT / 100;
        energy = energy + recovered;
        if (energy > PLAYER_ENERGY_MAX) energy = PLAYER_ENERGY_MAX;
        swosWriteWord(b + PLSPR_OFF_ENERGY, (uint16_t)energy);
        swosWriteWord(b + PLSPR_OFF_ENERGY_ACC, 0);
    }
}
