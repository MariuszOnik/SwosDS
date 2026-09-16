// SOURCE: openswos game/scripts/Sim/Port/PlayerEnergy.cs (EffectEnabled,
// ShotPenalty, SpeedStep, DrainOnKeeperCatch, KeeperSkillPenalty, DrainSlot,
// DrainOnTackle, InjuryRiskDoubled ONLY -- see below for why only these).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
//
// PlayerEnergy is OpenSWOS's own OPTIONAL, off-by-default fatigue
// extension (its header explicitly says original SWOS has no in-match
// stamina mechanic -- see the C# file). It is still part of what OpenSWOS's
// PlayerActions.cs/PlayerUpdate.cs/UpdatePlayers.cs/PlayerTackle.cs
// actually call today, so per this project's rule (OpenSWOS's C# is the
// sole source of behaviour for this phase) it gets ported like anything
// else reached from those files -- not treated as "not really SWOS" and
// skipped.
//
// FORWARD-PULLED DEPENDENCY, MINIMAL SLICE: EffectEnabled/ShotPenalty/
// SpeedStep (step 5, PlayerActions.cs), DrainOnKeeperCatch/KeeperSkillPenalty
// (step 5.5, PlayerUpdate.cs), and DrainSlot/DrainOnTackle/InjuryRiskDoubled
// (step 7A, UpdatePlayers.cs/PlayerTackle.cs) are the only members
// referenced from any file ported so far (grep-verified). The rest of
// PlayerEnergy.cs -- SeedSlot, RecoverAtHalfTime, SetMatchLength,
// ResetForNewMatch -- is called from other files (TeamDataLoader.cs,
// match-setup/half-time orchestration) not yet ported; port those calls
// when their callers are ported.
#pragma once

#include <stdbool.h>

// PlayerEnergy.cs:32 -- `public static bool EffectEnabled;` (C# implicit
// default false). Set once at match setup; never toggled mid-match.
extern bool g_swosPlayerEnergyEffectEnabled;

// PlayerEnergy.cs:119-126. energy read from PLSPR_OFF_ENERGY at spriteAddr.
int swosPlayerEnergySpeedStep(int spriteAddr);

// PlayerEnergy.cs:130-135. Gated on g_swosPlayerEnergyEffectEnabled.
int swosPlayerEnergyShotPenalty(int spriteAddr);

// PlayerEnergy.cs:204-212. Drains 2..4% of current energy (Rng-driven).
// Gated on g_swosPlayerEnergyEffectEnabled.
void swosPlayerEnergyDrainOnKeeperCatch(int spriteAddr);

// PlayerEnergy.cs:214-223. Gated on g_swosPlayerEnergyEffectEnabled.
int swosPlayerEnergyKeeperSkillPenalty(int spriteAddr);

// PlayerEnergy.cs:89-109. Per-tick energy drain while a player is moving
// (always runs -- not gated on EffectEnabled, so the energy bar reflects
// drain even when the speed EFFECT is off). SetMatchLength (PlayerEnergy.cs:58-63)
// is not called from any file ported so far, so the match-length
// normalisation this uses stays at its C# default (1/1, i.e. no-op).
void swosPlayerEnergyDrainSlot(int spriteAddr);

// PlayerEnergy.cs:190-198. Costs the tackled player a random 1..5% of
// current energy. Gated on g_swosPlayerEnergyEffectEnabled.
void swosPlayerEnergyDrainOnTackle(int spriteAddr);

// PlayerEnergy.cs:139-144. True when energy < 20% (doubles injury risk in
// PlayerTackle.PlayerTackled). Gated on g_swosPlayerEnergyEffectEnabled.
bool swosPlayerEnergyInjuryRiskDoubled(int spriteAddr);
