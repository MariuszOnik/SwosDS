// See match_bootstrap.h for what this is and why it exists.
#include "match_bootstrap.h"

#include <stdbool.h>
#include <stdint.h>

#include "swos_addr.h"
#include "swos_camera.h"
#include "swos_kickoff.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"
#include "swos_rng.h"
#include "swos_tactics_loader.h"
#include "swos_team_data.h"
#include "swos_team_data_loader.h"

// Well clear of the sprite pool (0x50000..0x50B00) and everything else in
// swos_addr.h, comfortably inside SWOS_MEM_SIZE (0x60000). Two 11 x 61-byte
// PlayerInfo arrays, one per team.
#define PLAYERINFO_TOP_BASE    0x51000
#define PLAYERINFO_BOTTOM_BASE 0x51400

// swos-ds's WORLD_W/WORLD_H (source/main.c) -- the real 672x848 pitch image
// this renderer scrolls over. Hand-picked 1-4-4-2 formation, NOT derived
// from OpenSWOS's own kTopStartingPositions/kBottomStartingPositions tables
// (see match_bootstrap.h header comment for why).
// Index 0 = goalkeeper (ordinal 1), 1..10 = outfielders (ordinal 2..11).
static const int16_t kTopFormationX[11] = { 336, 150, 280, 392, 522, 150, 280, 392, 522, 250, 422 };
static const int16_t kTopFormationY[11] = {  40, 160, 160, 160, 160, 280, 280, 280, 280, 380, 380 };

static const int16_t kBottomFormationX[11] = { 336, 150, 280, 392, 522, 150, 280, 392, 522, 250, 422 };
static const int16_t kBottomFormationY[11] = { 808, 688, 688, 688, 688, 568, 568, 568, 568, 468, 468 };

static void seedPlayerInfo(int base)
{
    for (int i = 0; i < 11; i++)
    {
        int addr = base + i * TDL_PLAYER_INFO_SIZE;
        swosWriteByte(addr + TDL_OFF_SUBSTITUTED, 0);
        swosWriteByte(addr + TDL_OFF_CARDS, 0);
        swosWriteByte(addr + TDL_OFF_FACE, 0);
        swosWriteByte(addr + TDL_OFF_POSITION, i == 0 ? 0 : 1);
        swosWriteByte(addr + TDL_OFF_PASSING, 4);
        swosWriteByte(addr + TDL_OFF_SHOOTING, 4);
        swosWriteByte(addr + TDL_OFF_HEADING, 4);
        swosWriteByte(addr + TDL_OFF_TACKLING, 4);
        swosWriteByte(addr + TDL_OFF_BALL_CONTROL, 4);
        swosWriteByte(addr + TDL_OFF_SPEED, 4);
        swosWriteByte(addr + TDL_OFF_FINISHING, 4);
        swosWriteByte(addr + TDL_OFF_GOALIE_SKILL, 4);
    }
}

static void seedTeamSprites(bool top, const int16_t *fx, const int16_t *fy)
{
    int firstSlot = swosPlayerSpriteFirstSlotForTeam(top);
    int16_t initialDir = top ? 4 /* facing down-field */ : 0 /* facing up-field */;

    for (int i = 0; i < 11; i++)
    {
        int slot = firstSlot + i;
        swosPlayerSpriteSetTeamNumber(slot, (int16_t)(top ? 1 : 2));
        swosPlayerSpriteSetPlayerOrdinal(slot, (int16_t)(i + 1));
        swosPlayerSpriteSetX(slot, (int32_t)fx[i] << 16);
        swosPlayerSpriteSetY(slot, (int32_t)fy[i] << 16);
        swosPlayerSpriteSetDirection(slot, initialDir);
        swosPlayerSpriteSetPlayerState(slot, 0 /* PLSTATE_NORMAL */);
        swosPlayerSpriteSetImageIndex(slot, 0);
    }
}

// Same default tactics index the real production loader uses --
// TeamDataLoader.WireTeamFields's defaultTacticsIndex parameter defaults to
// 5 (4-3-3), Main.cs never overrides it. See PHASE 1 note below for why
// this now matters.
#define DEFAULT_TACTICS_INDEX 5

static void seedTeamData(bool top, int playerInfoBase)
{
    int teamBase = swosTeamDataBase(top);
    swosWriteDword(teamBase + TEAMDATA_OFF_IN_GAME_TEAM_PTR, (uint32_t)playerInfoBase);
    // playerNumber=0 on both teams -> both AI-controlled for this first
    // milestone (matches the user's own "NPC vs NPC first?" suggestion).
    // A human-controlled team is a straightforward follow-up: set this to
    // 1 or 2 and feed swosInputControlsSetJoystickState() from the pad.
    swosWriteWord(teamBase + TEAMDATA_OFF_PLAYER_NUMBER, 0);
    // PHASE 1 BOOTSTRAP FIX (2026-09-16): was never set at all (implicitly
    // 0 = tact_4_4_2 via swosMemoryInit's zero-fill) -- now matches the real
    // production default. Only meaningful now that swosTacticsLoaderLoadAll
    // Tactics() below actually populates the pool; harmless before that
    // (index into a real 370-byte struct regardless).
    swosWriteWord(teamBase + TEAMDATA_OFF_TACTICS, DEFAULT_TACTICS_INDEX);

    swosWriteDword(top ? ADDR_topTeamInGame : ADDR_bottomTeamInGame, (uint32_t)playerInfoBase);
}

static void bootstrapCommon(void)
{
    seedPlayerInfo(PLAYERINFO_TOP_BASE);
    seedPlayerInfo(PLAYERINFO_BOTTOM_BASE);

    seedTeamSprites(true, kTopFormationX, kTopFormationY);
    seedTeamSprites(false, kBottomFormationX, kBottomFormationY);

    seedTeamData(true, PLAYERINFO_TOP_BASE);
    seedTeamData(false, PLAYERINFO_BOTTOM_BASE);

    // PHASE 1 BOOTSTRAP FIX (2026-09-16): the real production
    // InitSwosVmFromMatchSetup (Main.cs) calls TacticsLoader.LoadAllTactics()
    // before Kickoff.StartingMatch() -- this synthetic bootstrap never did,
    // leaving Memory.Addr.teamTacticsPool entirely zero (confirmed via the
    // Phase 1 lockstep audit, see README.md "Status: Phase 1"). Narrow,
    // scoped fix: populate the real tactic tables now, same call point
    // (before the kickoff-prep call below). Does NOT port the rest of
    // InitSwosVmFromMatchSetup's chain (GameTime.SaveTeams/
    // InitPlayerCardChance/DetermineStartingTeamAndTeamPlayingUp,
    // Pitch.SetPitchTypeAndNumber, GameTime.InitGameVariables, the real
    // team-file-driven TeamDataLoader.WritePlayerInfos/WireTeamFields,
    // PlayerEnergy.SetMatchLength, Kickoff.StartingMatch,
    // Bench.InitBenchBeforeMatch) -- those remain a separate follow-up.
    swosTacticsLoaderLoadAllTactics();

    // Real ported logic: ball to centre spot, per-team reset (reads back
    // the sprite positions just seeded above to set restful dest_x/dest_y),
    // turn flags, camera direction.
    swosKickoffPrepareForInitialKick();
    swosCameraSetToInitialPosition();

    // ETAP 0 audit fix (2026-09-16): this function USED to force
    // gameStatePl straight to K_ST_GAME_IN_PROGRESS (100) and
    // breakCameraMode to 0 right here, skipping PrepareForInitialKick's own
    // real state (gameStatePl=101/K_ST_STOPPED, breakCameraMode=-1,
    // gameState=0). That was one confirmed cause of the broken-looking
    // kickoff: with gameStatePl already 100, `swos_update_players.c`'s
    // per-tick "STOPPAGE PATH (gameStatePl != 100)" branch -- which calls
    // setPlayerPositionsForGameBreak(), the real, mechanically-ported,
    // byte-tested walk-to-formation logic driven by OpenSWOS's own
    // kTopStartingPositions/kBottomStartingPositions tables -- never ran,
    // not even once. Every player was left exactly on this file's
    // hand-picked, UNVERIFIED kTopFormationX/Y/kBottomFormationX/Y
    // placeholder coordinates forever, immediately treated as live play.
    // The step-12 integration test proves C/C# parity for this shared,
    // synthetic setup through tick 10. It does not validate the hand-made
    // bootstrap itself or a complete kickoff-to-live-play sequence.
    //
    // The fix is to do nothing here: leave PrepareForInitialKick's own
    // gameStatePl/breakCameraMode/gameState exactly as it set them, and let
    // the real, already-ported-and-tested GameLoop tick machinery
    // (the break-camera-mode ladder, then the CPU waiting-on-player safety
    // net in swosGameLoopCoreGameUpdate) walk both AI teams into their real
    // kickoff formation and on into K_ST_GAME_IN_PROGRESS on its own, over
    // the following ticks -- exactly like a real match would.
}

void dsBootstrapMatch(void)
{
    swosMemoryInit(true);
    bootstrapCommon();
}

void dsBootstrapMatchSeeded(int seed)
{
    swosMemoryInit(true);
    // Before bootstrapCommon(): swosKickoffPrepareForInitialKick() (called
    // inside bootstrapCommon) itself draws real Rng bytes (teamPlayingUp/
    // teamStarting coin-flip, kickoff jitter), so the seed must be live
    // before that call to genuinely govern the whole match -- see this
    // function's header comment (match_bootstrap.h) for the full reasoning,
    // mirrored exactly on the C# side by Step12IntegrationGolden.Bootstrap.
    swosRngReseed(seed);
    bootstrapCommon();
}
