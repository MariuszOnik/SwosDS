// SOURCE: OpenSWOS PlayerTackle.cs:1162-1304, step-6A dependency slice.
#include "swos_player_tackle.h"

#include <stdint.h>

#include "swos_addr.h"
#include "swos_memory.h"
#include "swos_player_actions.h"
#include "swos_player_sprite.h"
#include "swos_team_data.h"

void swosPlayerBeginTackling(int player, int team, int direction) {
    swosWriteWord(player + PLSPR_OFF_TACKLE_STATE, 0);
    swosWriteWord(team + TEAMDATA_OFF_CONTROLLED_PL_DIRECTION,
                  (uint16_t)direction);
    swosWriteWord(player + PLSPR_OFF_DIRECTION, (uint16_t)direction);
    swosSetPlayerAnimationTable(player, ADDR_kPlTacklingAnimTableAddr);
    swosWriteByte(player + PLSPR_OFF_PLAYER_STATE, 1);
    swosWriteByte(player + PLSPR_OFF_PLAYER_DOWN_TIMER,
                  (uint8_t)swosReadSignedWord(ADDR_m_playerDownTacklingInterval));

    int ordinal = swosReadSignedWord(player + PLSPR_OFF_PLAYER_ORDINAL);
    int offset = swosReadSignedWord(ADDR_inGameTeamPlayerOffsets
                                    + (int16_t)(ordinal - 1) * 2);
    int header = swosReadSignedDword(team + TEAMDATA_OFF_IN_GAME_TEAM_PTR)
                 - 42 + (uint16_t)(int16_t)offset;
    if (swosReadByte(header + 50) != 0)
        swosWriteByte(player + PLSPR_OFF_PLAYER_DOWN_TIMER, 25);

    // Verbatim OpenSWOS behavior: the following sentinel overwrites both the
    // normal interval and faster-tackle value above.
    swosWriteByte(player + PLSPR_OFF_PLAYER_DOWN_TIMER, UINT8_MAX);

    int dst = ADDR_kDefaultDestinations + ((int16_t)direction << 2);
    int vx = swosReadSignedWord(dst);
    int vy = swosReadSignedWord(dst + 2);
    int posX = swosReadSignedWord(player + PLSPR_OFF_X + 2);
    int posY = swosReadSignedWord(player + PLSPR_OFF_Y + 2);
    int travel = 1000;
    if (vx != 0) {
        int allowed = vx > 0 ? 590 - posX : posX - 81;
        if (allowed < 0) allowed = 0;
        if (allowed < travel) travel = allowed;
    }
    if (vy != 0) {
        int allowed = vy > 0 ? 769 - posY : posY - 129;
        if (allowed < 0) allowed = 0;
        if (allowed < travel) travel = allowed;
    }
    int destX = posX + (vx > 0 ? travel : vx < 0 ? -travel : 0);
    int destY = posY + (vy > 0 ? travel : vy < 0 ? -travel : 0);
    swosWriteWord(player + PLSPR_OFF_DEST_X, (uint16_t)(int16_t)destX);
    swosWriteWord(player + PLSPR_OFF_DEST_Y, (uint16_t)(int16_t)destY);
    swosWriteWord(player + PLSPR_OFF_SPEED,
                  swosReadWord(ADDR_kPlayerTacklingSpeed));
    swosWriteWord(player + PLSPR_OFF_TACKLING_TIMER, 0);
}
