// SOURCE: openswos game/scripts/Sim/Port/GameSprites.cs (see
// swos_game_sprites.h).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
#include "swos_game_sprites.h"

#include <stdint.h>

#include "swos_addr.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"
#include "swos_team_data.h"

// gameSprites.cpp:264-266 -- 32-element 0..3 bit-1 sliding window.
static const uint8_t kCornerFlagFrameOffsets[32] = {
    0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3,
    0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3,
};

// TeamGame / PlayerInfo struct offsets. Source: swos.h:296-315 (TeamGame).
#define GS_PLAYERINFO_SIZE 61
#define GS_PLAYERINFO_OFF_SHIRT_NUMBER 3

void swosGameSpritesUpdateCornerFlags(void)
{
    static const int xs[4] = { GS_LEFT_CORNER_FLAG_X, GS_RIGHT_CORNER_FLAG_X, GS_LEFT_CORNER_FLAG_X, GS_RIGHT_CORNER_FLAG_X };
    static const int ys[4] = { GS_TOP_CORNER_FLAG_Y, GS_TOP_CORNER_FLAG_Y, GS_BOTTOM_CORNER_FLAG_Y, GS_BOTTOM_CORNER_FLAG_Y };

    int32_t frameCount = swosReadSignedDword(ADDR_frameCounter);
    int frame = (frameCount >> 1) & 0x1f;
    int imageIndex = GS_CORNER_FLAG_SPRITE_START + kCornerFlagFrameOffsets[frame];

    for (int i = 0; i < 4; i++)
    {
        int baseAddr = ADDR_cornerFlags + i * GS_CORNER_FLAG_STRIDE;
        swosWriteWord(baseAddr + GS_CORNER_FLAG_OFF_X, (uint16_t)xs[i]);
        swosWriteWord(baseAddr + GS_CORNER_FLAG_OFF_Y, (uint16_t)ys[i]);
        swosWriteWord(baseAddr + GS_CORNER_FLAG_OFF_IMAGE_INDEX, (uint16_t)imageIndex);
    }
}

// Debounce state for the (omitted) out-of-range-shirt diagnostic -- kept so
// the guarded control-flow branch structure matches the C# 1:1 even though
// the print itself is omitted (see header).
static int s_lastBadShirtOrdinal = -1;

void swosGameSpritesUpdateControlledPlayerNumbers(void)
{
    uint16_t tick = swosReadWord(ADDR_currentGameTick);
    int16_t trainingGame = swosReadSignedWord(ADDR_g_trainingGame);

    for (int i = 0; i < 2; i++)
    {
        bool top = (i == 0);
        int teamBase = swosTeamDataBase(top);
        int numSpriteAddr = ADDR_curPlayerNumSprites + i * GS_CUR_PLAYER_NUM_STRIDE;

        int32_t controlledPlayerPtr = swosTeamDataControlledPlayer(top);
        int16_t playerNumber = swosReadSignedWord(teamBase + TEAMDATA_OFF_PLAYER_NUMBER);
        int16_t isPlCoach    = swosReadSignedWord(teamBase + TEAMDATA_OFF_IS_PL_COACH);

        bool tickHas16 = (tick & 0x10) != 0;
        bool shouldShowMark = (i > 0) == tickHas16;

        bool gateOk =
            (isPlCoach != 0 || playerNumber != 0 || trainingGame != 0) &&
            controlledPlayerPtr != 0;
        if (!gateOk)
        {
            swosWriteWord(numSpriteAddr + GS_CUR_PLAYER_NUM_OFF_IMAGE_INDEX, (uint16_t)-1);
            continue;
        }

        int16_t playerOrdinal = swosReadSignedWord(controlledPlayerPtr + PLSPR_OFF_PLAYER_ORDINAL);

        int32_t inGameTeamPtr = swosReadSignedDword(teamBase + TEAMDATA_OFF_IN_GAME_TEAM_PTR);
        int16_t markedPlayer = inGameTeamPtr == 0
            ? (int16_t)-1
            : swosReadSignedWord(inGameTeamPtr - 22);
        bool ordinalDiffersFromMarked = (playerOrdinal - 1) != markedPlayer;

        bool drawPlayerNumber = ordinalDiffersFromMarked || !shouldShowMark;

        if (!drawPlayerNumber)
        {
            swosWriteWord(numSpriteAddr + GS_CUR_PLAYER_NUM_OFF_IMAGE_INDEX, (uint16_t)-1);
            continue;
        }

        int shirtNumber = 1;
        if (inGameTeamPtr != 0 && playerOrdinal >= 1 && playerOrdinal <= 16)
        {
            int playerInfoAddr = inGameTeamPtr + (playerOrdinal - 1) * GS_PLAYERINFO_SIZE;
            shirtNumber = swosReadByte(playerInfoAddr + GS_PLAYERINFO_OFF_SHIRT_NUMBER);

            if (shirtNumber < 1 || shirtNumber > 16)
            {
                // Diagnostic print omitted (zero Memory effect); debounce
                // state kept for control-flow parity -- see header note.
                if (s_lastBadShirtOrdinal != playerOrdinal)
                    s_lastBadShirtOrdinal = playerOrdinal;
                swosWriteWord(numSpriteAddr + GS_CUR_PLAYER_NUM_OFF_IMAGE_INDEX, (uint16_t)-1);
                continue;
            }
            s_lastBadShirtOrdinal = -1;
        }

        int imageIndex = GS_SMALL_DIGIT1 + shirtNumber - 1;
        swosWriteWord(numSpriteAddr + GS_CUR_PLAYER_NUM_OFF_IMAGE_INDEX, (uint16_t)imageIndex);

        int16_t playerX = swosReadSignedWord(controlledPlayerPtr + PLSPR_OFF_X + 2);
        int16_t playerY = swosReadSignedWord(controlledPlayerPtr + PLSPR_OFF_Y + 2);
        int16_t playerZ = swosReadSignedWord(controlledPlayerPtr + PLSPR_OFF_Z + 2);
        swosWriteWord(numSpriteAddr + GS_CUR_PLAYER_NUM_OFF_X, (uint16_t)playerX);
        swosWriteWord(numSpriteAddr + GS_CUR_PLAYER_NUM_OFF_Y, (uint16_t)playerY);
        swosWriteWord(numSpriteAddr + GS_CUR_PLAYER_NUM_OFF_Z, (uint16_t)(playerZ + GS_PLAYER_NUMBER_OFFSET));
    }
}

int swosGameSpritesGetPlayerSpriteOffsetFromFace(int face)
{
    if (face < 0) face = 0;
    if (face > 3) face = 3;
    return face * 101; // kNumPlayerSprites
}

int swosGameSpritesGetGoalkeeperSpriteOffset(bool topTeam, int face)
{
    (void)topTeam;
    if (face < 0) face = 0;
    if (face > 3) face = 3;
    int goalie = face / 2;
    return goalie * 58; // kNumGoalkeeperSprites
}

void swosGameSpritesGetCornerFlag(int index, int *x, int *y, int *imageIndex)
{
    if (index < 0 || index > 3) { *x = 0; *y = 0; *imageIndex = 0; return; }
    int baseAddr = ADDR_cornerFlags + index * GS_CORNER_FLAG_STRIDE;
    *x = swosReadSignedWord(baseAddr + GS_CORNER_FLAG_OFF_X);
    *y = swosReadSignedWord(baseAddr + GS_CORNER_FLAG_OFF_Y);
    *imageIndex = swosReadSignedWord(baseAddr + GS_CORNER_FLAG_OFF_IMAGE_INDEX);
}

void swosGameSpritesGetCurPlayerNumSprite(int teamIndex, int *x, int *y, int *z, int *imageIndex)
{
    if (teamIndex < 0 || teamIndex > 1) { *x = 0; *y = 0; *z = 0; *imageIndex = -1; return; }
    int baseAddr = ADDR_curPlayerNumSprites + teamIndex * GS_CUR_PLAYER_NUM_STRIDE;
    *x = swosReadSignedWord(baseAddr + GS_CUR_PLAYER_NUM_OFF_X);
    *y = swosReadSignedWord(baseAddr + GS_CUR_PLAYER_NUM_OFF_Y);
    *z = swosReadSignedWord(baseAddr + GS_CUR_PLAYER_NUM_OFF_Z);
    *imageIndex = swosReadSignedWord(baseAddr + GS_CUR_PLAYER_NUM_OFF_IMAGE_INDEX);
}
