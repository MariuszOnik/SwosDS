// SOURCE: openswos game/scripts/Sim/Port/PlayerEnergy.cs (EffectEnabled,
// ShotPenalty, SpeedStep ONLY -- see below for why only these three).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
//
// PlayerEnergy is OpenSWOS's own OPTIONAL, off-by-default fatigue
// extension (its header explicitly says original SWOS has no in-match
// stamina mechanic -- see the C# file). It is still part of what OpenSWOS's
// PlayerActions.cs actually calls today, so per this project's rule
// (OpenSWOS's C# is the sole source of behaviour for this phase) it gets
// ported like anything else PlayerActions.cs reaches -- not treated as
// "not really SWOS" and skipped.
//
// FORWARD-PULLED DEPENDENCY, MINIMAL SLICE: only EffectEnabled, ShotPenalty
// and SpeedStep are referenced from PlayerActions.cs (grep-verified). The
// rest of PlayerEnergy.cs -- SeedSlot, DrainSlot, DrainOnTackle,
// DrainOnKeeperCatch, KeeperSkillPenalty, InjuryRiskDoubled,
// RecoverAtHalfTime, SetMatchLength, ResetForNewMatch -- is called from
// other files (UpdatePlayers.cs, TeamDataLoader.cs, match-setup/half-time
// orchestration) not yet ported; port those calls when their callers are
// ported, per the file-by-file rule.
#pragma once

#include <stdbool.h>

// PlayerEnergy.cs:32 -- `public static bool EffectEnabled;` (C# implicit
// default false). Set once at match setup; never toggled mid-match.
extern bool g_swosPlayerEnergyEffectEnabled;

// PlayerEnergy.cs:119-126. energy read from PLSPR_OFF_ENERGY at spriteAddr.
int swosPlayerEnergySpeedStep(int spriteAddr);

// PlayerEnergy.cs:130-135. Gated on g_swosPlayerEnergyEffectEnabled.
int swosPlayerEnergyShotPenalty(int spriteAddr);
