// SOURCE: openswos game/scripts/Sim/Port/TeamPort.cs:54-107 (StopAllPlayers
// + StopPlayers ONLY -- see swos_team_port.h).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
#include "swos_team_port.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"
#include "swos_team_data.h"

// team.cpp:46-55 -- stopPlayers(team).
static void stopPlayers(bool top) {
    for (int slotInTeam = 0; slotInTeam < PLSPR_TEAM_SIZE; slotInTeam++) {
        int32_t spriteAddr = swosTeamDataGetTeamSpriteAddr(top, slotInTeam);
        if (spriteAddr == 0) continue;

        uint8_t plState = swosReadByte(spriteAddr + PLSPR_OFF_PLAYER_STATE);
        int16_t sentAway = swosReadSignedWord(spriteAddr + PLSPR_OFF_SENT_AWAY);
        if (plState == 0 && sentAway == 0) {
            int16_t curX = swosReadSignedWord(spriteAddr + PLSPR_OFF_X + 2);
            int16_t curY = swosReadSignedWord(spriteAddr + PLSPR_OFF_Y + 2);
            swosWriteWord(spriteAddr + PLSPR_OFF_DEST_X, (uint16_t)curX);
            swosWriteWord(spriteAddr + PLSPR_OFF_DEST_Y, (uint16_t)curY);
        }
    }
}

// team.cpp:26-44 -- stopAllPlayers.
void swosTeamPortStopAllPlayers(void) {
    for (int t = 0; t < 2; t++) {
        bool top = (t == 0);
        int teamBase = swosTeamDataBase(top);
        stopPlayers(top);

        swosWriteWord(teamBase + TEAMDATA_OFF_BALL_IN_PLAY, 0);
        swosWriteWord(teamBase + TEAMDATA_OFF_BALL_OUT_OF_PLAY, 0);
        swosWriteDword(teamBase + TEAMDATA_OFF_CONTROLLED_PLAYER, 0);
        swosWriteDword(teamBase + TEAMDATA_OFF_PASS_TO_PLAYER_PTR, 0);
        swosWriteWord(teamBase + TEAMDATA_OFF_PASSING_BALL, 0);
        swosWriteWord(teamBase + TEAMDATA_OFF_PASSING_TO_PLAYER, 0);
        swosWriteWord(teamBase + TEAMDATA_OFF_PLAYER_SWITCH_TIMER, 0);
        swosWriteDword(teamBase + TEAMDATA_OFF_PASSING_KICKING_PLAYER, 0);

        // team.cpp:38-43 -- SWOS_TEST guard not wired in OpenSWOS; always resets.
        swosWriteWord(teamBase + TEAMDATA_OFF_GOALKEEPER_PLAYING, 0);
    }
}
