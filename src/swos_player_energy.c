// SOURCE: openswos game/scripts/Sim/Port/PlayerEnergy.cs (EffectEnabled,
// ShotPenalty, SpeedStep, DrainOnKeeperCatch, KeeperSkillPenalty ONLY --
// see swos_player_energy.h).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
#include "swos_player_energy.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"
#include "swos_rng.h"

bool g_swosPlayerEnergyEffectEnabled = false;

#define PLAYER_ENERGY_MAX 4096

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
