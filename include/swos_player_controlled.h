// SOURCE: OpenSWOS PlayerControlled.cs (step 6A).
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SWOS_PC_UPDATE_SPEED,
    SWOS_PC_STOP_PLAYER,
} SwosPlayerControlledExit;

typedef struct {
    int fireFallbackTop, fireFallbackBottom;
    int enterCseg80FFD, reachedCwbCall;
    int skillDuelOwnWin, skillDuelOppWin;
    int pinControlledPerTick;
} SwosPlayerControlledTelemetry;

typedef void (*SwosAiSetControlsDirectionHook)(int teamBase);
typedef void (*SwosAiKickHook)(int spriteAddr, int teamBase);

extern SwosPlayerControlledTelemetry g_swosPcTelemetry;
extern SwosAiSetControlsDirectionHook g_swosAiSetControlsDirectionHook;
extern SwosAiKickHook g_swosAiKickHook;
extern bool g_swosFaithfulBallControl;

void swosPlayerControlledResetTelemetry(void);
void swosPlayerControlledIncSkillDuelOwnWin(void);
void swosPlayerControlledIncSkillDuelOppWin(void);

SwosPlayerControlledExit swosRunControlledBranch(int spriteAddr, bool topTeam);
void swosRunPassReceiptTrigger(int spriteAddr, bool topTeam);
void swosRunPassExpectingBranch(int spriteAddr, bool topTeam);
