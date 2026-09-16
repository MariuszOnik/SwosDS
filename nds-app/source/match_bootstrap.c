// See match_bootstrap.h for what this is and why it exists.
#include "match_bootstrap.h"

#include <stdbool.h>
#include <stdint.h>

#include "swos_addr.h"
#include "swos_camera.h"
#include "swos_kickoff.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"
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

static void seedTeamData(bool top, int playerInfoBase)
{
    int teamBase = swosTeamDataBase(top);
    swosWriteDword(teamBase + TEAMDATA_OFF_IN_GAME_TEAM_PTR, (uint32_t)playerInfoBase);
    // playerNumber=0 on both teams -> both AI-controlled for this first
    // milestone (matches the user's own "NPC vs NPC first?" suggestion).
    // A human-controlled team is a straightforward follow-up: set this to
    // 1 or 2 and feed swosInputControlsSetJoystickState() from the pad.
    swosWriteWord(teamBase + TEAMDATA_OFF_PLAYER_NUMBER, 0);

    swosWriteDword(top ? ADDR_topTeamInGame : ADDR_bottomTeamInGame, (uint32_t)playerInfoBase);
}

void dsBootstrapMatch(void)
{
    swosMemoryInit(true);

    seedPlayerInfo(PLAYERINFO_TOP_BASE);
    seedPlayerInfo(PLAYERINFO_BOTTOM_BASE);

    seedTeamSprites(true, kTopFormationX, kTopFormationY);
    seedTeamSprites(false, kBottomFormationX, kBottomFormationY);

    seedTeamData(true, PLAYERINFO_TOP_BASE);
    seedTeamData(false, PLAYERINFO_BOTTOM_BASE);

    // Real ported logic: ball to centre spot, per-team reset (reads back
    // the sprite positions just seeded above to set restful dest_x/dest_y),
    // turn flags, camera direction.
    swosKickoffPrepareForInitialKick();
    swosCameraSetToInitialPosition();

    // DS-adapter simplification: skip the referee whistle / waiting-on-
    // player handshake PrepareForInitialKick sets up (gameStatePl=101,
    // breakCameraMode=-1) -- driving that state machine to a real kickoff
    // needs Main.cs-level orchestration this project hasn't ported (see
    // header comment) -- and go straight to live play instead.
    swosWriteWord(ADDR_gameState, 0);
    swosWriteWord(ADDR_gameStatePl, 100 /* K_ST_GAME_IN_PROGRESS */);
    swosWriteWord(ADDR_breakCameraMode, 0);
}
