// SOURCE: openswos game/scripts/Sim/Port/GameLoop.cs:1860-1888 (see
// swos_game_loop.h for the exact slice ported).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
#include "swos_game_loop.h"

#include "swos_addr.h"
#include "swos_memory.h"
#include "swos_team_data.h"
#include "swos_team_port.h"

// game.cpp:711 -- GameState::kPlayersGoingToShower (swos.h). GameLoop.cs:1679.
#define K_ST_PLAYERS_GOING_TO_SHOWER 24
// game.cpp:713 -- GameState::kStopped. GameLoop.cs:1681.
#define K_ST_STOPPED 101

void swosGameLoopPlayersLeavingPitch(void)
{
    // game.cpp:709 -- mov hideBall, 0.
    swosWriteWord(ADDR_hideBall, 0);
    // game.cpp:710 -- mov stoppageEventTimer, 275.
    swosWriteWord(ADDR_stoppageEventTimer, 275);
    // game.cpp:711 -- mov gameState, ST_PLAYERS_GOING_TO_SHOWER (24).
    swosWriteWord(ADDR_gameState, K_ST_PLAYERS_GOING_TO_SHOWER);
    // game.cpp:712 -- mov breakCameraMode, -1.
    swosWriteWord(ADDR_breakCameraMode, (uint16_t)-1);
    // game.cpp:713 -- mov gameStatePl, ST_STOPPED (101).
    swosWriteWord(ADDR_gameStatePl, K_ST_STOPPED);
    // game.cpp:714 -- mov gameNotInProgressCounterWriteOnly, 0.
    swosWriteWord(ADDR_gameNotInProgressCounterWriteOnly, 0);
    // game.cpp:715 -- mov cameraDirection, -1.
    swosWriteWord(ADDR_cameraDirection, (uint16_t)-1);
    // game.cpp:716 -- mov lastTeamPlayedBeforeBreak, offset topTeamData.
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)TEAMDATA_TOP_BASE);
    // game.cpp:717-718 -- clear stoppage timers.
    swosWriteWord(ADDR_stoppageTimerTotal, 0);
    swosWriteWord(ADDR_stoppageTimerActive, 0);
    // game.cpp:719 -- call stopAllPlayers().
    swosTeamPortStopAllPlayers();
    // game.cpp:720-721 -- zero camera velocities.
    swosWriteWord(ADDR_cameraXVelocity, 0);
    swosWriteWord(ADDR_cameraYVelocity, 0);
    // game.cpp:722 -- mov stateGoal, 0.
    swosWriteWord(ADDR_stateGoal, 0);
}
