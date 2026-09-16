// SOURCE: openswos game/scripts/Sim/Port/PlayerNameDisplay.cs (see
// swos_player_name_display.h).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
#include "swos_player_name_display.h"

#include <stdbool.h>
#include <stdint.h>

#include "swos_addr.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"
#include "swos_referee.h"
#include "swos_team_data.h"

#define K_FRAMES_BEFORE_NOBODYS_BALL 50
#define K_GAME_STATE_IN_PROGRESS 100

void swosPlayerNameDisplayUpdateCurrentPlayerName(void)
{
    swosWriteWord(ADDR_pnd_visible, 1);

    int32_t currentScorer = swosReadSignedDword(ADDR_currentScorer);
    if (currentScorer != 0)
    {
        int32_t lastTeamScored = swosReadSignedDword(ADDR_lastTeamScored);
        swosPlayerNameDisplayShowNameBlinking(currentScorer, lastTeamScored);
        return;
    }

    if (swosRefereeCardHandingInProgress())
    {
        int32_t bookedPlayer = swosReadSignedDword(ADDR_bookedPlayer);
        int32_t lastTeamBooked = swosReadSignedDword(ADDR_lastTeamBooked);
        swosPlayerNameDisplayShowNameBlinking(bookedPlayer, lastTeamBooked);
        return;
    }

    int32_t lpbgk = swosReadSignedDword(ADDR_lastPlayerBeforeGoalkeeper);
    if (lpbgk != 0)
    {
        int32_t lastTeamScored = swosReadSignedDword(ADDR_lastTeamScored);
        swosPlayerNameDisplayProlongLastPlayersName(lpbgk, lastTeamScored);
        return;
    }

    int32_t lastPlayer = swosReadSignedDword(ADDR_lastPlayerPlayed);
    int32_t lastTeam   = swosReadSignedDword(ADDR_lastTeamPlayed);

    int16_t gameStatePl = swosReadSignedWord(ADDR_gameStatePl);
    if (gameStatePl == K_GAME_STATE_IN_PROGRESS)
    {
        if (lastPlayer != 0)
        {
            if (lastTeam != 0 && swosReadSignedWord(lastTeam + TEAMDATA_OFF_PLAYER_HAS_BALL) != 0)
                swosPlayerNameDisplayResetNobodysBallTimer();
            swosPlayerNameDisplayProlongLastPlayersName(lastPlayer, lastTeam);
        }
        else
        {
            swosPlayerNameDisplayHideCurrentPlayerName();
        }
    }
    else
    {
        int32_t teamControlled = 0;
        if (lastTeam != 0)
            teamControlled = swosReadSignedDword(lastTeam + TEAMDATA_OFF_CONTROLLED_PLAYER);

        if (lastPlayer == 0 || teamControlled == 0)
        {
            swosWriteWord(ADDR_pnd_nobodysBallLastFrame, 1);
            swosPlayerNameDisplayHideCurrentPlayerName();
        }
        else
        {
            int16_t lastFrame = swosReadSignedWord(ADDR_pnd_nobodysBallLastFrame);
            if (lastFrame != 0)
            {
                swosWriteWord(ADDR_pnd_nobodysBallLastFrame, (uint16_t)(lastFrame - 1));
                swosPlayerNameDisplayHideCurrentPlayerName();
            }
            else
            {
                swosPlayerNameDisplayResetNobodysBallTimer();
                swosPlayerNameDisplayProlongLastPlayersName(lastPlayer, lastTeam);
            }
        }
    }
}

void swosPlayerNameDisplayShowCurrentPlayerName(int lastPlayerSpriteAddr, int lastTeamAddr)
{
    if (lastPlayerSpriteAddr == 0 || lastTeamAddr == 0)
    {
        swosPlayerNameDisplayHideCurrentPlayerName();
        return;
    }

    int32_t topInGame  = swosReadSignedDword(ADDR_topTeamInGame);
    int32_t teamInGame = swosReadSignedDword(lastTeamAddr + TEAMDATA_OFF_IN_GAME_TEAM_PTR);
    bool topTeam = (teamInGame == topInGame);

    int16_t playerOrdinal = swosReadSignedWord(lastPlayerSpriteAddr + PLSPR_OFF_PLAYER_ORDINAL);

    swosWriteWord(ADDR_pnd_topTeam, topTeam ? 1 : 0);
    swosWriteWord(ADDR_pnd_playerOrdinal, (uint16_t)(playerOrdinal - 1));
}

void swosPlayerNameDisplayHideCurrentPlayerName(void)
{
    swosWriteWord(ADDR_pnd_playerOrdinal, (uint16_t)-1);
    swosWriteDword(ADDR_lastPlayerBeforeGoalkeeper, 0);
}

void swosPlayerNameDisplayShowNameBlinking(int lastPlayerSpriteAddr, int lastTeamAddr)
{
    uint16_t tick = swosReadWord(ADDR_currentGameTick);
    bool showName = (tick & 8) != 0;
    if (showName)
        swosPlayerNameDisplayShowCurrentPlayerName(lastPlayerSpriteAddr, lastTeamAddr);
    else
        swosPlayerNameDisplayHideCurrentPlayerName();
}

void swosPlayerNameDisplayProlongLastPlayersName(int lastPlayerSpriteAddr, int lastTeamAddr)
{
    int16_t timer = swosReadSignedWord(ADDR_nobodysBallTimer);
    if (timer == 0)
    {
        swosPlayerNameDisplayHideCurrentPlayerName();
    }
    else
    {
        swosWriteWord(ADDR_nobodysBallTimer, (uint16_t)(timer - 1));
        swosPlayerNameDisplayShowCurrentPlayerName(lastPlayerSpriteAddr, lastTeamAddr);
    }
}

void swosPlayerNameDisplayResetNobodysBallTimer(void)
{
    swosWriteWord(ADDR_nobodysBallTimer, K_FRAMES_BEFORE_NOBODYS_BALL);
}

void swosPlayerNameDisplayGetDisplayedPlayerNumberAndTeam(bool *topTeam, int *playerOrdinal)
{
    *topTeam = swosReadSignedWord(ADDR_pnd_topTeam) != 0;
    *playerOrdinal = swosReadSignedWord(ADDR_pnd_playerOrdinal);
}
