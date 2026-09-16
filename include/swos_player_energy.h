// SOURCE: openswos game/scripts/Sim/Port/PlayerEnergy.cs (EffectEnabled,
// ShotPenalty, SpeedStep, DrainOnKeeperCatch, KeeperSkillPenalty ONLY --
// see below for why only these five).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
//
// PlayerEnergy is OpenSWOS's own OPTIONAL, off-by-default fatigue
// extension (its header explicitly says original SWOS has no in-match
// stamina mechanic -- see the C# file). It is still part of what OpenSWOS's
// PlayerActions.cs/PlayerUpdate.cs actually call today, so per this
// project's rule (OpenSWOS's C# is the sole source of behaviour for this
// phase) it gets ported like anything else reached from those files -- not
// treated as "not really SWOS" and skipped.
//
// FORWARD-PULLED DEPENDENCY, MINIMAL SLICE: EffectEnabled/ShotPenalty/
// SpeedStep (step 5, PlayerActions.cs) plus DrainOnKeeperCatch/
// KeeperSkillPenalty (step 5.5, PlayerUpdate.cs) are the only members
// referenced from either file (grep-verified). The rest of PlayerEnergy.cs
// -- SeedSlot, DrainSlot, DrainOnTackle, InjuryRiskDoubled,
// RecoverAtHalfTime, SetMatchLength, ResetForNewMatch -- is called from
// other files (UpdatePlayers.cs's own body, TeamDataLoader.cs, match-setup/
// half-time orchestration) not yet ported; port those calls when their
// callers are ported, per the file-by-file rule.
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
