// SOURCE: openswos game/scripts/Sim/Port/PlayerNameDisplay.cs (full file
// minus pure-text rendering, step 11 of the porting order -- real local
// dependency of GameLoop.cs).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
//
// EXCLUDED (matches the C#'s own exclusion): drawPlayerName,
// getPlayerNumberAndSurname, the string-composition half of
// showCurrentPlayerName -- pure text rendering that depends on
// PlayerInfo.shortName + an asm ExtractSurname helper, zero Memory effect
// beyond the state this header DOES port (pnd_topTeam/pnd_playerOrdinal,
// which a future renderer reads).
#pragma once

#include <stdbool.h>

// playerNameDisplay.cpp:20-55 -- main per-tick entry.
void swosPlayerNameDisplayUpdateCurrentPlayerName(void);

// playerNameDisplay.cpp:92-98.
void swosPlayerNameDisplayShowCurrentPlayerName(int lastPlayerSpriteAddr, int lastTeamAddr);

// playerNameDisplay.cpp:100-104.
void swosPlayerNameDisplayHideCurrentPlayerName(void);

// playerNameDisplay.cpp:106-110.
void swosPlayerNameDisplayShowNameBlinking(int lastPlayerSpriteAddr, int lastTeamAddr);

// playerNameDisplay.cpp:112-120.
void swosPlayerNameDisplayProlongLastPlayersName(int lastPlayerSpriteAddr, int lastTeamAddr);

// playerNameDisplay.cpp:122-128.
void swosPlayerNameDisplayResetNobodysBallTimer(void);

// playerNameDisplay.cpp:74-77 -- renderer accessor.
void swosPlayerNameDisplayGetDisplayedPlayerNumberAndTeam(bool *topTeam, int *playerOrdinal);
