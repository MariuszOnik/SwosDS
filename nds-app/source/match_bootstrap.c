// See match_bootstrap.h for what this is and why it exists.
#include "match_bootstrap.h"

#include <stdbool.h>
#include <stdint.h>

#include "swos_addr.h"
#include "swos_bench.h"
#include "swos_camera.h"
#include "swos_game_time.h"
#include "swos_kickoff.h"
#include "swos_memory.h"
#include "swos_pitch.h"
#include "swos_player_energy.h"
#include "swos_player_sprite.h"
#include "swos_result.h"
#include "swos_rng.h"
#include "swos_tactics_loader.h"
#include "swos_team_data.h"
#include "swos_team_data_loader.h"
#include "swos_team_record.h"

// PHASE 1 BOOTSTRAP-COMPLETENESS FOLLOW-UP (2026-09-16, see README.md
// "Status: Phase 1"): this file now runs the REAL production sequence --
// Main.cs's InitSwosVmFromMatchSetup, read in full and matched call for
// call, mirrored exactly by Step12IntegrationGolden.cs's Bootstrap(seed) on
// the C# side -- rather than the old hand-poked PlayerInfo/sprite/TeamData
// stand-in. The ONLY input still synthesized here is the two SwosTeamRecord
// rosters (no team-FILE parser exists in this repo); everything downstream
// is the real, mechanically-ported production code path. This supersedes
// (and removes) the old seedPlayerInfo/seedTeamSprites/seedTeamData
// hand-poking -- swosPlayerSpriteInit() (already called by swosMemoryInit)
// already assigns every sprite's team number/ordinal, and
// swosKickoffStartingMatch()'s InitPlayersBeforeEnteringPitch positions all
// 22 sprites at the real entry line, so no manual sprite seeding is needed.

// Synthetic roster: flat mid-range skills and a simple back-four/midfield/
// attack position shape -- NOT a claim of realism, just enough shape (a
// valid position per slot, one goalkeeper) for the real
// WritePlayerInfos/SkillScaling/GoalieSkillFromPrice pipeline to have
// something sensible to scale. Same shape as Step12IntegrationGolden.cs's
// BuildTeamRecord (C#) -- keep both in sync if either changes.
static const int kSyntheticPositions[11] = {
    TDL_POS_GOALKEEPER, TDL_POS_RIGHT_BACK, TDL_POS_DEFENDER, TDL_POS_DEFENDER,
    TDL_POS_LEFT_BACK, TDL_POS_RIGHT_WING, TDL_POS_MIDFIELDER, TDL_POS_MIDFIELDER,
    TDL_POS_LEFT_WING, TDL_POS_ATTACKER, TDL_POS_ATTACKER,
};

static void buildSyntheticTeam(SwosTeamRecord *team, SwosPlayerRecord *players,
                                const char *names[11], const char *teamName)
{
    for (int i = 0; i < 11; i++) {
        players[i].shirtNumber = (uint8_t)(i + 1);
        players[i].name = names[i];
        players[i].position = kSyntheticPositions[i];
        players[i].passing = 4;
        players[i].shooting = 4;
        players[i].heading = 4;
        players[i].tackling = 4;
        players[i].control = 4;
        players[i].speed = 4;
        players[i].finishing = 4;
        players[i].valueCode = 20;
        players[i].stamina = 7;
        players[i].fatigueCarry = 0;
        players[i].injurySeverity = 0;
    }
    team->name = teamName;
    team->tactics = 0;
    team->players = players;
    team->playerCount = 11;
    team->lineupOrder = 0;
}

static void bootstrapCommon(void)
{
    // ---- Main.cs InitSwosVmFromMatchSetup, in its real order ------------
    swosGameTimeSaveTeams();
    swosGameTimeInitPlayerCardChance();
    swosGameTimeDetermineStartingTeamAndTeamPlayingUp();
    swosPitchSetPitchTypeAndNumber();
    swosGameTimeInitPitchBallFactors();

    // Main.cs: TimeDeltaOverride = clamp(2700 / max(1, SecondsPerHalf), 1, 70),
    // with the default match-length preset (180s total, 90s/half):
    // clamp(2700/90, 1, 70) = 30. Same constant as Step12IntegrationGolden.cs.
    g_swosGameTimeTimeDeltaOverride = 30;
    swosGameTimeInitGameVariables();

    static const char *kTopNames[11] = { "P1","P2","P3","P4","P5","P6","P7","P8","P9","P10","P11" };
    static const char *kBottomNames[11] = { "P1","P2","P3","P4","P5","P6","P7","P8","P9","P10","P11" };
    SwosPlayerRecord topPlayers[11], bottomPlayers[11];
    SwosTeamRecord topTeam, bottomTeam;
    buildSyntheticTeam(&topTeam, topPlayers, kTopNames, "TOP");
    buildSyntheticTeam(&bottomTeam, bottomPlayers, kBottomNames, "BOTTOM");
    const bool topIsHuman = false, bottomIsHuman = false; // AI-vs-AI, matches this repo's first milestone

    swosTeamDataLoaderWritePlayerInfos(ADDR_team1InGameTeamPlayers, &topTeam, topIsHuman);
    swosTeamDataLoaderWritePlayerInfos(ADDR_team2InGameTeamPlayers, &bottomTeam, bottomIsHuman);
    swosTeamDataLoaderWireTeamFields(true, &topTeam,
        ADDR_team1InGameTeamPlayers, ADDR_team1ShotChanceTable, ADDR_team1NameStorage,
        topIsHuman, 5 /* 4-3-3, TeamDataLoader.cs's own default */);
    swosTeamDataLoaderWireTeamFields(false, &bottomTeam,
        ADDR_team2InGameTeamPlayers, ADDR_team2ShotChanceTable, ADDR_team2NameStorage,
        bottomIsHuman, 5);

    // Main.cs: PlayerEnergy.EffectEnabled = _fatigueSim || _competitionMatchPending;
    // both default false for a non-career, non-fatigue-sim match.
    g_swosPlayerEnergyEffectEnabled = false;
    swosPlayerEnergySetMatchLength(180); // TotalMatchSeconds, default preset

    swosResultReset(topTeam.name, bottomTeam.name);

    swosTacticsLoaderLoadAllTactics();

    // Main.cs: team1Computer/team2Computer = topHuman/bottomHuman ? 0 : -1;
    // both AI here.
    swosWriteWord(ADDR_team1Computer, (uint16_t)(topIsHuman ? 0 : -1));
    swosWriteWord(ADDR_team2Computer, (uint16_t)(bottomIsHuman ? 0 : -1));

    swosKickoffStartingMatch();
    swosWriteWord(ADDR_playGame, 1);

    swosBenchInitBenchBeforeMatch();
    swosCameraSetToInitialPosition();
}

void dsBootstrapMatch(void)
{
    swosMemoryInit(true);
    bootstrapCommon();
}

void dsBootstrapMatchSeeded(int seed)
{
    swosMemoryInit(true);
    // Before bootstrapCommon(): swosGameTimeDetermineStartingTeamAndTeamPlayingUp
    // and other calls inside bootstrapCommon() draw real Rng bytes, so the
    // seed must be live before any of them to genuinely govern the whole
    // match -- see this function's header comment (match_bootstrap.h) for
    // the full reasoning, mirrored exactly on the C# side by
    // Step12IntegrationGolden.Bootstrap.
    swosRngReseed(seed);
    bootstrapCommon();
}
