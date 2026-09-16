// SOURCE: openswos game/scripts/Sim/Port/Bench.cs (see swos_bench.h for
// the full file-header note).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
#include "swos_bench.h"

#include <stdint.h>

#include "swos_addr.h"
#include "swos_camera.h"
#include "swos_game_sprites.h"
#include "swos_input_controls.h"
#include "swos_memory.h"
#include "swos_player_actions.h"
#include "swos_player_sprite.h"
#include "swos_player_update.h"
#include "swos_referee.h"
#include "swos_team_data.h"
#include "swos_team_data_loader.h"

int g_swosBenchGameMinSubstitutes = 2;
int g_swosBenchGameMaxSubstitutes = 5;

// swos.asm:207972-208017 -- positionsTable. Maps a bench-menu row ordinal
// (0..10, goalkeeper first) to the 1-based players[] index for the team's
// current tactics.
static const uint8_t kPositionsTable[18][11] = {
    { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 },   // 0  default (4-4-2)
    { 1, 2, 3, 4, 7, 5, 6, 8, 10, 9, 11 },   // 1  5-4-1
    { 1, 2, 3, 4, 5, 6, 7, 8, 10, 9, 11 },   // 2  4-5-1
    { 1, 2, 3, 4, 7, 5, 6, 8, 9, 10, 11 },   // 3  5-3-2
    { 1, 2, 3, 5, 6, 4, 7, 8, 9, 10, 11 },   // 4  3-5-2
    { 1, 2, 3, 4, 5, 6, 7, 9, 8, 10, 11 },   // 5  4-3-3
    { 1, 2, 3, 4, 5, 7, 8, 6, 10, 11, 9 },   // 6  4-2-4
    { 1, 2, 3, 5, 6, 4, 7, 9, 8, 10, 11 },   // 7  3-4-3
    { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 },   // 8  sweep
    { 1, 2, 3, 4, 7, 5, 8, 10, 6, 11, 9 },   // 9  5-2-3
    { 1, 2, 3, 5, 4, 7, 6, 8, 10, 11, 9 },   // 10 attack
    { 1, 6, 2, 3, 4, 5, 9, 7, 8, 10, 11 },   // 11 defend
    { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 },   // 12 USER_A -> defaultPositions
    { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 },   // 13 USER_B
    { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 },   // 14 USER_C
    { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 },   // 15 USER_D
    { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 },   // 16 USER_E
    { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 },   // 17 USER_F
};
#define K_POSITIONS_TABLE_ROWS 18

// ---- Module state -- updateBench.cpp:23-80 file-statics -------------------
typedef struct {
    int tapCount;
    int tapTimeoutCounter;
    int previousDirection;
    bool blockWhileHoldingDirection;
} SwosBenchTapCounterState;

static void tapStateInit(SwosBenchTapCounterState *s)
{
    s->tapCount = 0; s->tapTimeoutCounter = 0; s->previousDirection = 0;
    s->blockWhileHoldingDirection = false;
}
static void tapStateReset(SwosBenchTapCounterState *s)
{
    s->tapCount = 0; s->tapTimeoutCounter = 0; s->previousDirection = 0;
}
static bool tapStateGotLastTapDirection(const SwosBenchTapCounterState *s)
{
    return (s->previousDirection & IC_EVENT_MOVEMENT_MASK) != 0;
}
static bool tapStateHoldingSameDirectionAsLastTap(const SwosBenchTapCounterState *s, int controls)
{
    return s->previousDirection == (controls & IC_EVENT_MOVEMENT_MASK);
}

static SwosBenchTapCounterState s_pl1TapState;
static SwosBenchTapCounterState s_pl2TapState;
static int s_controls;

static int s_teamBase;
static int s_teamGameBase;
static int s_teamNumber;

static int s_goToBenchTimer;

static bool s_bench1Called;
static bool s_bench2Called;

static bool s_blockDirections;
static int s_fireTimer;
static bool s_blockFire;

static int s_lastDirection;
static int s_movementDelayTimer;

static bool s_trainingTopTeam;
static bool s_teamsSwapped;
static int s_alternateTeamsTimer;

static int s_arrowPlayerIndex;
static int s_selectedMenuPlayerIndex;
static int s_playerToEnterGameIndex;

static int s_playerToBeSubstitutedPos;
static int s_playerToBeSubstitutedOrd;

static uint8_t s_shirtNumberTable[2][16];

static int s_selectedFormationEntry;

static int16_t s_substitutedPlDestX;
static int16_t s_substitutedPlDestY;
static int16_t s_plSubstitutedX;
static int16_t s_plSubstitutedY;

// ---- Forward declarations of static helpers --------------------------------
static void setBenchState(int state);
static void initBench(void);
static void handleMenuControls(void);
static void initBenchVars(int teamBase);
static void handleBenchArrowSelection(void);
static void selectPlayerToSubstituteMenuHandler(void);
static void handleFormationMenuMovement(void);
static void handleFormationMenuControls(void);
static void markPlayersMenuHandler(void);
static void updateSelectedMenuPlayer(void);
static void showFormationMenu(void);
static void firePressedInSubsMenu(void);
static void maintainMarkedPlayer(void);
static void swapMarkedPlayer(void);
static void selectOrSwapPlayers(void);
static void updatePlayerToBeSubstitutedPosition(void);
static void initiateSubstitution(void);
static void substitutePlayer(void);
static void changeTactics(int newTactics);
static bool leavingBenchMotion(void);
static void leaveBench(void);
static void leaveBenchFromMenu(void);
static bool benchBlocked(void);
static bool benchUnavailable(void);
static int getNonBenchControlsTeam(void);
static bool updateNonBenchControls(int teamBase);
static void updateBenchControls(void);
static bool bumpGoToBenchTimer(void);
static bool filterControls(void);
static bool benchInvoked(int teamBase);
static void findInitialPlayerToBeSubstituted(void);
static bool isDefencePosition(int p);
static bool isMidfieldPosition(int p);
static int getBenchPlayerInfoAddrByOrd(int ord);
static void markPlayer(void);
static void swapPlayerShirtNumbers(int ord1, int ord2);
static void increasePlayerIndex(void);
static void decreasePlayerIndex(bool allowBenchSwitch);
static void increasePlayerToSubstitute(void);
static void decreasePlayerToSubstitute(void);
static bool isPlayerOkToSelect(void);
static bool isPlayerOkToSubstitute(void);
static void trainingSwapBenchTeams(void);
static bool playerWasSubstituted(int infoAddr);
static void updateSubstitutedPlayerWalk(void);
static void newPlayerAboutToGoInTransition(int spriteAddr, int teamBase);
static void setSubstitutedPlayerDestination(int spriteAddr, int teamBase);
static void swapPlayerInfoRecords(int idxA, int idxB);
static void swapSpriteContentsKeepingOrdinals(int posA, int posB);
static void initializePlayerSpriteFrameIndices(void);
static void checkForThrowInAndKeepersBall(void);
static void stopAllPlayers(void);

// ====================================================================
// Public state queries -- bench.cpp:42-65 + updateBench.cpp:194-294
// ====================================================================

bool swosBenchInBench(void) { return swosReadSignedWord(ADDR_g_inSubstitutesMenu) != 0; }

int swosBenchGetBenchState(void) { return swosReadSignedWord(ADDR_m_benchState); }
static void setBenchState(int state) { swosWriteWord(ADDR_m_benchState, (uint16_t)state); }

bool swosBenchInBenchMenus(void)
{
    return swosBenchInBench() && swosBenchGetBenchState() == BENCH_STATE_INITIAL;
}

int swosBenchGetBenchY(void) { return swosReadSignedWord(ADDR_m_benchY); }
int swosBenchGetOpponentBenchY(void) { return swosReadSignedWord(ADDR_m_opponentBenchY); }

bool swosBenchTrainingTopTeam(void) { return s_trainingTopTeam; }
void swosBenchSetTrainingTopTeam(bool value) { s_trainingTopTeam = value; }

void swosBenchRequestBench1(void) { s_bench1Called = true; }
void swosBenchRequestBench2(void) { s_bench2Called = true; }

int swosBenchGetBenchPlayerIndex(void)        { return s_arrowPlayerIndex; }
int swosBenchGetBenchMenuSelectedPlayer(void) { return s_selectedMenuPlayerIndex; }
int swosBenchGetSelectedFormationEntry(void)  { return s_selectedFormationEntry; }
int swosBenchPlayerToEnterGameIndex(void)     { return s_playerToEnterGameIndex; }
int swosBenchPlayerToBeSubstitutedIndex(void) { return s_playerToBeSubstitutedOrd; }
int swosBenchPlayerToBeSubstitutedPos(void)   { return s_playerToBeSubstitutedPos; }

int swosBenchGetBenchPlayerShirtNumber(bool topTeam, int index)
{
    return s_shirtNumberTable[topTeam ? 1 : 0][index];
}

bool swosBenchInBenchOrGoingTo(void) { return s_goToBenchTimer == 0 && swosBenchInBench(); }
bool swosBenchGoingToBenchDelay(void) { return s_goToBenchTimer != 0; }

bool swosBenchSubstituteInProgress(void) { return swosReadSignedWord(ADDR_g_substituteInProgress) != 0; }
bool swosBenchNewPlayerAboutToGoIn(void) { return swosReadSignedWord(ADDR_g_substituteInProgress) < 0; }
void swosBenchSetSubstituteInProgress(void) { swosWriteWord(ADDR_g_substituteInProgress, 1); }

int swosBenchGetBenchTeamBase(void)
{
    if (swosReadSignedWord(ADDR_g_trainingGame) == 0)
    {
        int16_t actualNumber = swosReadSignedWord(s_teamBase + TEAMDATA_OFF_TEAM_NUMBER);
        if (s_teamNumber != actualNumber)
        {
            s_teamBase = s_teamNumber == 1 ? TEAMDATA_BOTTOM_BASE : TEAMDATA_TOP_BASE;
            s_teamNumber = swosReadSignedWord(s_teamBase + TEAMDATA_OFF_TEAM_NUMBER);
        }
    }
    return s_teamBase;
}

int swosBenchGetBenchTeamGameBase(void) { return s_teamGameBase; }
bool swosBenchTeamIsTop(void) { return s_teamBase == TEAMDATA_TOP_BASE; }

int swosBenchGetBenchPlayerInfoAddr(int index)
{
    return s_teamGameBase + swosBenchGetBenchPlayerPosition(index) * TDL_PLAYER_INFO_SIZE;
}

int swosBenchGetBenchPlayerPosition(int index)
{
    int16_t tactics = swosReadSignedWord(swosBenchGetBenchTeamBase() + TEAMDATA_OFF_TACTICS);
    if (tactics < 0 || tactics >= K_POSITIONS_TABLE_ROWS)
        tactics = 0;
    return kPositionsTable[tactics][index] - 1;
}

// ====================================================================
// initBenchBeforeMatch -- bench.cpp:26-33
// ====================================================================
void swosBenchInitBenchBeforeMatch(void)
{
    swosWriteWord(ADDR_g_inSubstitutesMenu, 0);
    swosWriteWord(ADDR_g_cameraLeavingSubsTimer, 0);
    swosBenchInitBenchControls();
    initBench();
}

// ====================================================================
// initBenchControls -- updateBench.cpp:118-166
// ====================================================================
void swosBenchInitBenchControls(void)
{
    s_teamsSwapped = false;
    s_trainingTopTeam = false;

    s_alternateTeamsTimer = 0;
    s_teamNumber = 0;

    tapStateInit(&s_pl1TapState);
    tapStateInit(&s_pl2TapState);

    s_goToBenchTimer = 0;

    s_blockDirections = false;
    s_blockFire = false;
    s_fireTimer = 0;

    s_lastDirection = 0;
    s_movementDelayTimer = 0;

    s_controls = 0;

    setBenchState(BENCH_STATE_INITIAL);

    s_arrowPlayerIndex = 0;
    s_playerToEnterGameIndex = 0;
    s_selectedMenuPlayerIndex = 0;

    s_playerToBeSubstitutedPos = 0;
    s_playerToBeSubstitutedOrd = 0;

    s_selectedFormationEntry = 0;

    s_bench1Called = false;
    s_bench2Called = false;

    for (int i = 0; i < 16; i++)
    {
        s_shirtNumberTable[0][i] = (uint8_t)i;
        s_shirtNumberTable[1][i] = (uint8_t)i;
    }

    if (swosReadSignedWord(ADDR_teamPlayingUp) == 1)
    {
        s_teamBase = TEAMDATA_TOP_BASE;
        s_teamGameBase = swosReadSignedDword(ADDR_topTeamInGame);
    }
    else
    {
        s_teamBase = TEAMDATA_BOTTOM_BASE;
        s_teamGameBase = swosReadSignedDword(ADDR_bottomTeamInGame);
    }

    s_substitutedPlDestX = 0;
    s_substitutedPlDestY = 0;
    s_plSubstitutedX = 0;
    s_plSubstitutedY = 0;
}

// ====================================================================
// initBench -- bench.cpp:96-118 (static)
// ====================================================================
static void initBench(void)
{
    swosWriteWord(ADDR_stateGoal, 0);

    int32_t resultTimer = swosReadSignedDword(ADDR_resultTimer);
    swosWriteDword(ADDR_resultTimer, (uint32_t)(-resultTimer));
    int16_t statsTimer = swosReadSignedWord(ADDR_statsTimer);
    swosWriteWord(ADDR_statsTimer, (uint16_t)(-statsTimer));

    int benchY = BENCH_TOP_BENCH_Y;
    int oppBenchY = BENCH_BOTTOM_BENCH_Y;
    bool topTeam = s_teamGameBase != 0
        && s_teamGameBase == swosReadSignedDword(ADDR_topTeamInGame);

    if (swosReadSignedWord(ADDR_g_trainingGame) != 0)
    {
        benchY = BENCH_TRAINING_PITCH_BENCH_Y;
        oppBenchY = BENCH_TRAINING_PITCH_BENCH_Y;
    }
    else
    {
        int32_t topInGame = swosReadSignedDword(ADDR_topTeamInGame);
        if (topInGame != 0)
        {
            uint8_t ch1 = swosReadByte(topInGame - 20 + 1);
            if ((ch1 & 2) != 0)
            {
                int t = benchY; benchY = oppBenchY; oppBenchY = t;
            }
        }

        if (topTeam)
        {
            int t = benchY; benchY = oppBenchY; oppBenchY = t;
        }
    }

    swosWriteWord(ADDR_m_benchY, (uint16_t)benchY);
    swosWriteWord(ADDR_m_opponentBenchY, (uint16_t)oppBenchY);

    swosWriteWord(ADDR_plComingX, BENCH_PLAYER_GOING_IN_X);
    swosWriteWord(ADDR_plComingY, BENCH_PLAYER_GOING_IN_Y);
}

// ====================================================================
// invokeBench -- bench.cpp:121-127 (static)
// ====================================================================
void swosBenchInvokeBench(void)
{
    initBench();
    swosWriteWord(ADDR_g_inSubstitutesMenu, 1);
    checkForThrowInAndKeepersBall();
}

// ====================================================================
// setBenchOff -- bench.cpp:67-71
// ====================================================================
void swosBenchSetBenchOff(void)
{
    swosBenchCheckIfGoalkeeperClaimedTheBall();
    swosWriteWord(ADDR_g_inSubstitutesMenu, 0);
}

// ====================================================================
// swapBenchWithOpponent -- bench.cpp:62-65
// ====================================================================
void swosBenchSwapBenchWithOpponent(void)
{
    int b = swosReadSignedWord(ADDR_m_benchY);
    int o = swosReadSignedWord(ADDR_m_opponentBenchY);
    swosWriteWord(ADDR_m_benchY, (uint16_t)o);
    swosWriteWord(ADDR_m_opponentBenchY, (uint16_t)b);
}

// ====================================================================
// updateBench -- bench.cpp:36-40 (main entry, gameLoop.cpp:315)
// ====================================================================
void swosBenchUpdateBench(void)
{
    updateSubstitutedPlayerWalk();

    if (swosBenchCheckControls())
        swosBenchInvokeBench();
}

// ====================================================================
// benchCheckControls -- updateBench.cpp:169-192
// ====================================================================
bool swosBenchCheckControls(void)
{
    if (swosBenchInBench())
    {
        updateBenchControls();
        if (!bumpGoToBenchTimer())
        {
            if (swosBenchNewPlayerAboutToGoIn())
                substitutePlayer();
            else if (!filterControls())
                handleMenuControls();
        }
    }
    else
    {
        bool blocked = benchBlocked();
        if (!blocked)
        {
            bool unavailable = benchUnavailable();
            if (!unavailable)
            {
                int teamBase = getNonBenchControlsTeam();

                bool benchCalled = s_bench1Called || s_bench2Called;
                if (benchCalled || (updateNonBenchControls(teamBase) && benchInvoked(teamBase)))
                {
                    initBenchVars(teamBase);
                    return true;
                }
            }
        }
    }

    return false;
}

// ====================================================================
// handleMenuControls -- updateBench.cpp:296-315
// ====================================================================
static void handleMenuControls(void)
{
    if (swosBenchSubstituteInProgress())
        return;

    switch (swosBenchGetBenchState())
    {
        case BENCH_STATE_INITIAL:
            handleBenchArrowSelection();
            break;
        case BENCH_STATE_ABOUT_TO_SUBSTITUTE:
            selectPlayerToSubstituteMenuHandler();
            break;
        case BENCH_STATE_FORMATION_MENU:
            handleFormationMenuControls();
            break;
        case BENCH_STATE_MARKING_PLAYERS:
            markPlayersMenuHandler();
            break;
        default:
            break;
    }
}

// ====================================================================
// initBenchVars -- updateBench.cpp:317-335
// ====================================================================
static void initBenchVars(int teamBase)
{
    s_bench1Called = false;
    s_bench2Called = false;

    s_teamBase = teamBase;
    int16_t teamNumber = swosReadSignedWord(teamBase + TEAMDATA_OFF_TEAM_NUMBER);
    s_teamGameBase = swosReadSignedDword(teamNumber == 2 ? ADDR_bottomTeamInGame : ADDR_topTeamInGame);
    s_trainingTopTeam = s_teamGameBase != swosReadSignedDword(ADDR_topTeamInGame);
    s_teamNumber = teamNumber;

    s_playerToBeSubstitutedPos = -1;
    s_arrowPlayerIndex = 0;

    setBenchState(BENCH_STATE_INITIAL);
    s_goToBenchTimer = BENCH_ENTER_BENCH_DELAY;

    s_blockFire = true;
    s_blockDirections = true;
}

// ====================================================================
// handleBenchArrowSelection -- updateBench.cpp:337-372
// ====================================================================
static void handleBenchArrowSelection(void)
{
    bool benchRecalled = s_teamBase == TEAMDATA_TOP_BASE ? s_bench1Called : s_bench2Called;
    int movementFlags = s_controls & IC_EVENT_MOVEMENT_MASK;

    if (benchRecalled || leavingBenchMotion())
    {
        leaveBench();
    }
    else if (movementFlags == IC_EVENT_UP || movementFlags == IC_EVENT_DOWN)
    {
        bool increaseIndex = true;
        bool allowIfKeeperHolds = false;

        if (s_controls == IC_EVENT_UP)
        {
            increaseIndex = false;
            if (swosReadSignedWord(ADDR_g_trainingGame) != 0)
            {
                if (s_trainingTopTeam)
                    increaseIndex = true;
                else
                    allowIfKeeperHolds = true;
            }
        }
        else if (swosReadSignedWord(ADDR_g_trainingGame) != 0)
        {
            if (s_trainingTopTeam)
            {
                increaseIndex = false;
                allowIfKeeperHolds = true;
            }
        }

        // updateBench.cpp:363-364 -- GameState::kKeeperHoldsTheBall == 3.
        if (!allowIfKeeperHolds && swosReadSignedWord(ADDR_gameState) == 3)
            return;

        s_blockDirections = true;

        if (increaseIndex) increasePlayerIndex();
        else                decreasePlayerIndex(allowIfKeeperHolds);
    }
    else if ((s_controls & IC_EVENT_KICK) != 0)
    {
        firePressedInSubsMenu();
    }
}

// updateBench.cpp:374-380 -- selectPlayerToSubstituteMenuHandler.
static void selectPlayerToSubstituteMenuHandler(void)
{
    if ((s_controls & IC_EVENT_KICK) != 0)
        initiateSubstitution();
    else
        updateSelectedMenuPlayer();
}

// updateBench.cpp:382-397 -- handleFormationMenuMovement.
static void handleFormationMenuMovement(void)
{
    if (leavingBenchMotion())
    {
        leaveBenchFromMenu();
    }
    else if (s_controls == IC_EVENT_UP)
    {
        s_blockDirections = true;
        if (s_selectedFormationEntry > 0)
            s_selectedFormationEntry--;
    }
    else if (s_controls == IC_EVENT_DOWN)
    {
        s_blockDirections = true;
        if (s_selectedFormationEntry < 0)
            s_selectedFormationEntry = 0;
        else if (s_selectedFormationEntry < BENCH_NUM_FORMATION_ENTRIES - 1)
            s_selectedFormationEntry++;
    }
}

// updateBench.cpp:399-407 -- handleFormationMenuControls.
static void handleFormationMenuControls(void)
{
    if ((s_controls & IC_EVENT_KICK) != 0)
        changeTactics(s_selectedFormationEntry);
    else
        handleFormationMenuMovement();
}

// updateBench.cpp:409-430 -- markPlayersMenuHandler.
static void markPlayersMenuHandler(void)
{
    int playerInfoAddr = 0;
    if (s_playerToBeSubstitutedPos >= 0 && s_playerToBeSubstitutedPos <= 10)
        playerInfoAddr = s_teamGameBase + s_playerToBeSubstitutedPos * TDL_PLAYER_INFO_SIZE;

    uint8_t cards = playerInfoAddr != 0 ? swosReadByte(playerInfoAddr + TDL_OFF_CARDS) : 0;

    if (playerInfoAddr == 0 || cards >= 2 || s_fireTimer != BENCH_SUBSTITUTE_FIRE_TICKS)
    {
        if ((s_controls & IC_EVENT_KICK) != 0)
        {
            if (s_playerToBeSubstitutedOrd < 0)
                showFormationMenu();
            else if (s_playerToBeSubstitutedOrd > 0)
                selectOrSwapPlayers();
        }
        else
        {
            updateSelectedMenuPlayer();
        }
    }
    else
    {
        markPlayer();
    }
}

// updateBench.cpp:432-457 -- updateSelectedMenuPlayer.
static void updateSelectedMenuPlayer(void)
{
    if (leavingBenchMotion())
    {
        leaveBenchFromMenu();
    }
    else if (swosBenchGetBenchState() == BENCH_STATE_MARKING_PLAYERS
             || s_playerToEnterGameIndex != 11
             || g_swosBenchGameMaxSubstitutes <= 5)
    {
        if (s_controls == IC_EVENT_UP)
        {
            s_blockDirections = true;
            if (swosBenchGetBenchState() == BENCH_STATE_MARKING_PLAYERS
                && (s_playerToBeSubstitutedOrd < 0 || s_playerToBeSubstitutedOrd == 1))
            {
                s_playerToBeSubstitutedOrd = -1;
                s_playerToBeSubstitutedPos = -1;
            }
            else if (s_playerToBeSubstitutedOrd != 0)
            {
                decreasePlayerToSubstitute();
            }
        }
        else if (s_controls == IC_EVENT_DOWN)
        {
            s_blockDirections = true;
            if (swosBenchGetBenchState() == BENCH_STATE_MARKING_PLAYERS && s_playerToBeSubstitutedOrd < 0)
            {
                s_playerToBeSubstitutedOrd = 1;
                updatePlayerToBeSubstitutedPosition();
            }
            else if (s_playerToBeSubstitutedOrd != 10)
            {
                increasePlayerToSubstitute();
            }
        }
    }
}

// updateBench.cpp:459-464 -- showFormationMenu.
static void showFormationMenu(void)
{
    setBenchState(BENCH_STATE_FORMATION_MENU);
    s_blockFire = true;
    s_selectedFormationEntry = swosReadSignedWord(s_teamBase + TEAMDATA_OFF_TACTICS);
}

// updateBench.cpp:466-486 -- firePressedInSubsMenu.
static void firePressedInSubsMenu(void)
{
    if (s_arrowPlayerIndex == 0)
    {
        setBenchState(BENCH_STATE_MARKING_PLAYERS);
        s_blockFire = true;
        s_playerToBeSubstitutedOrd = -1;
        s_playerToBeSubstitutedPos = -1;
        s_selectedMenuPlayerIndex = -1;
    }
    else
    {
        int playerInfoAddr = s_teamGameBase + (s_arrowPlayerIndex + 10) * TDL_PLAYER_INFO_SIZE;
        if (!playerWasSubstituted(playerInfoAddr))
        {
            setBenchState(BENCH_STATE_ABOUT_TO_SUBSTITUTE);
            s_blockFire = true;
            s_playerToEnterGameIndex = s_arrowPlayerIndex + 10;
            if (g_swosBenchGameMaxSubstitutes <= BENCH_MAX_SUBSTITUTES || s_playerToEnterGameIndex != 11)
                findInitialPlayerToBeSubstituted();
            else
                s_playerToBeSubstitutedPos = 0;
        }
    }
}

// updateBench.cpp:488-499 -- maintainMarkedPlayer.
static void maintainMarkedPlayer(void)
{
    int markedAddr = s_teamGameBase - 22;
    int16_t marked = swosReadSignedWord(markedAddr);

    if (marked == s_playerToBeSubstitutedPos)
    {
        swosWriteWord(markedAddr, (uint16_t)-1);
        marked = -1;
    }
    if (marked == s_playerToEnterGameIndex)
    {
        swosWriteWord(markedAddr, (uint16_t)-1);
        if (s_playerToBeSubstitutedPos != 0)
            swosWriteWord(markedAddr, (uint16_t)s_playerToBeSubstitutedPos);
    }
}

// updateBench.cpp:501-508 -- swapMarkedPlayer.
static void swapMarkedPlayer(void)
{
    int markedAddr = s_teamGameBase - 22;
    int16_t marked = swosReadSignedWord(markedAddr);
    if (marked == s_playerToBeSubstitutedPos)
        swosWriteWord(markedAddr, (uint16_t)s_selectedMenuPlayerIndex);
    else if (marked == s_selectedMenuPlayerIndex)
        swosWriteWord(markedAddr, (uint16_t)s_playerToBeSubstitutedPos);
}

// updateBench.cpp:510-539 -- selectOrSwapPlayers.
static void selectOrSwapPlayers(void)
{
    if (s_selectedMenuPlayerIndex < 0)
    {
        if (s_fireTimer == 1)
            s_selectedMenuPlayerIndex = s_playerToBeSubstitutedPos;
    }
    else
    {
        if (s_selectedMenuPlayerIndex == s_playerToBeSubstitutedPos)
        {
            if (s_fireTimer == 1)
                s_selectedMenuPlayerIndex = -1;
        }
        else
        {
            swapMarkedPlayer();
            swapPlayerShirtNumbers(s_playerToBeSubstitutedPos, s_selectedMenuPlayerIndex);
            swapPlayerInfoRecords(s_playerToBeSubstitutedPos, s_selectedMenuPlayerIndex);
            swapSpriteContentsKeepingOrdinals(s_playerToBeSubstitutedPos, s_selectedMenuPlayerIndex);

            initializePlayerSpriteFrameIndices();
            // ApplyTeamTactics() -- PORT-DIVERGENCE, see header note: the
            // per-tick break repositioning re-derives destinations from
            // team.tactics every stoppage, replacing the one-shot call.

            s_selectedMenuPlayerIndex = -1;

            leaveBenchFromMenu();
        }
    }
}

// updateBench.cpp:541-544 -- updatePlayerToBeSubstitutedPosition.
static void updatePlayerToBeSubstitutedPosition(void)
{
    s_playerToBeSubstitutedPos = swosBenchGetBenchPlayerPosition(s_playerToBeSubstitutedOrd);
}

// ====================================================================
// initiateSubstitution -- updateBench.cpp:546-568
// ====================================================================
static void initiateSubstitution(void)
{
    if (s_playerToBeSubstitutedPos < 0 || s_playerToBeSubstitutedPos > 10)
        return;

    swosBenchSetSubstituteInProgress();

    int numSubsAddr = s_teamNumber == 1 ? ADDR_team1NumSubs : ADDR_team2NumSubs;
    swosWriteWord(numSubsAddr, (uint16_t)(swosReadSignedWord(numSubsAddr) + 1));

    s_blockFire = true;
    setBenchState(BENCH_STATE_INITIAL);

    bool top = s_teamBase == TEAMDATA_TOP_BASE;
    int spriteAddr = swosTeamDataGetTeamSpriteAddr(top, s_playerToBeSubstitutedPos);
    swosWriteDword(ADDR_substitutedPlSprite, (uint32_t)spriteAddr);
    swosWriteDword(ADDR_teamThatSubstitutes, (uint32_t)s_teamBase);

    swosWriteWord(spriteAddr + PLSPR_OFF_CARDS, 0);
    s_substitutedPlDestX = BENCH_SUBSTITUTED_PLAYER_X;
    s_substitutedPlDestY = BENCH_SUBSTITUTED_PLAYER_Y;
    s_plSubstitutedX = BENCH_SUBSTITUTED_PLAYER_X;
    s_plSubstitutedY = BENCH_SUBSTITUTED_PLAYER_Y;
}

// ====================================================================
// substitutePlayer -- updateBench.cpp:570-591
// ====================================================================
static void substitutePlayer(void)
{
    int32_t spriteAddr = swosReadSignedDword(ADDR_substitutedPlSprite);
    if (spriteAddr != 0)
    {
        swosWriteWord(spriteAddr + PLSPR_OFF_INJURY_LEVEL, 0);
        swosWriteWord(spriteAddr + PLSPR_OFF_SENT_AWAY, 0);
    }

    maintainMarkedPlayer();
    swapPlayerShirtNumbers(s_playerToEnterGameIndex, s_playerToBeSubstitutedPos);

    int posInfoAddr = s_teamGameBase + s_playerToBeSubstitutedPos * TDL_PLAYER_INFO_SIZE;
    swosWriteByte(posInfoAddr + TDL_OFF_POSITION, (uint8_t)(int8_t)-1);

    swapPlayerInfoRecords(s_playerToEnterGameIndex, s_playerToBeSubstitutedPos);

    initializePlayerSpriteFrameIndices();
    // ApplyTeamTactics() -- PORT-DIVERGENCE, see selectOrSwapPlayers note.

    swosWriteWord(ADDR_g_waitForPlayerToGoInTimer, BENCH_PLAYER_GOING_IN_DELAY);
    s_blockFire = true;

    // MatchAudio.EnqueueSubstitute() omitted -- audio, see header.
    leaveBench();
}

// ====================================================================
// changeTactics -- updateBench.cpp:593-606
// ====================================================================
static void changeTactics(int newTactics)
{
    if (newTactics < 0 || newTactics >= 19)
        return;

    swosWriteWord(s_teamBase + TEAMDATA_OFF_TACTICS, (uint16_t)newTactics);
    // ApplyTeamTactics() -- PORT-DIVERGENCE, see selectOrSwapPlayers note.

    // MatchAudio.EnqueueTactics() omitted -- audio, see header.
    leaveBenchFromMenu();
}

// updateBench.cpp:608-612 -- leavingBenchMotion.
static bool leavingBenchMotion(void)
{
    return s_controls == IC_EVENT_LEFT || s_controls == IC_EVENT_RIGHT;
}

// updateBench.cpp:614-630 -- leaveBench.
static void leaveBench(void)
{
    swosBenchSetBenchOff();
    swosCameraSwitchToLeavingBenchMode();

    swosWriteWord(ADDR_g_cameraLeavingSubsTimer, BENCH_LEAVING_SUBS_DELAY);

    s_bench1Called = false;
    s_bench2Called = false;

    s_teamsSwapped = false;
    setBenchState(BENCH_STATE_INITIAL);
    s_playerToBeSubstitutedPos = -1;

    tapStateReset(&s_pl1TapState);
    tapStateReset(&s_pl2TapState);
}

// updateBench.cpp:632-637 -- leaveBenchFromMenu.
static void leaveBenchFromMenu(void)
{
    setBenchState(BENCH_STATE_INITIAL);
    s_blockDirections = true;
    s_blockFire = true;
}

// ====================================================================
// benchBlocked -- updateBench.cpp:639-650
// ====================================================================
static bool benchBlocked(void)
{
    int16_t waitTimer = swosReadSignedWord(ADDR_g_waitForPlayerToGoInTimer);
    if (waitTimer != 0)
    {
        swosWriteWord(ADDR_g_waitForPlayerToGoInTimer, (uint16_t)(waitTimer - 1));
        return true;
    }

    int16_t leavingTimer = swosReadSignedWord(ADDR_g_cameraLeavingSubsTimer);
    if (leavingTimer != 0)
    {
        swosWriteWord(ADDR_g_cameraLeavingSubsTimer, (uint16_t)(leavingTimer - 1));
        return true;
    }

    return swosBenchSubstituteInProgress()
        || swosRefereeActive()
        || swosReadSignedWord(ADDR_statsTimer) != 0;
}

// ====================================================================
// benchUnavailable -- updateBench.cpp:652-664
// ====================================================================
static bool benchUnavailable(void)
{
    int16_t gameStatePl = swosReadSignedWord(ADDR_gameStatePl);
    int16_t gameState   = swosReadSignedWord(ADDR_gameState);

    if (gameStatePl == 100
        || swosRefereeCardHandingInProgress()
        || swosReadSignedWord(ADDR_playingPenalties) != 0
        || (gameState >= 21 && gameState <= 30))
    {
        tapStateReset(&s_pl1TapState);
        tapStateReset(&s_pl2TapState);
        s_bench1Called = false;
        s_bench2Called = false;
        return true;
    }

    return false;
}

// updateBench.cpp:666-674 -- getNonBenchControlsTeam.
static int getNonBenchControlsTeam(void)
{
    if (s_bench1Called) return TEAMDATA_TOP_BASE;
    if (s_bench2Called) return TEAMDATA_BOTTOM_BASE;
    return (++s_alternateTeamsTimer & 1) != 0 ? TEAMDATA_TOP_BASE : TEAMDATA_BOTTOM_BASE;
}

// ====================================================================
// updateNonBenchControls -- updateBench.cpp:676-701
// ====================================================================
static bool updateNonBenchControls(int teamBase)
{
    int16_t playerNumber = swosReadSignedWord(teamBase + TEAMDATA_OFF_PLAYER_NUMBER);
    if (playerNumber != 0)
    {
        int16_t direction = swosReadSignedWord(teamBase + TEAMDATA_OFF_DIRECTION);
        s_controls = swosDirectionToEvents(direction);
        if (swosReadByte(teamBase + TEAMDATA_OFF_FIRE_PRESSED) != 0)
            s_controls |= IC_EVENT_KICK;
        if (swosReadByte(teamBase + TEAMDATA_OFF_SECONDARY_FIRE) != 0)
            s_controls |= IC_EVENT_BENCH;
    }
    else
    {
        int16_t playerCoachNumber = swosReadSignedWord(teamBase + TEAMDATA_OFF_PLAYER_COACH_NUMBER);
        if (playerCoachNumber == 1)
        {
            s_controls = swosGetPlayerEvents(IC_PLAYER1);
        }
        else if (playerCoachNumber == 2)
        {
            s_controls = swosGetPlayerEvents(IC_PLAYER2);
        }
        else
        {
            // updateBench.cpp:692-696 -- assert(false) + fallthrough to
            // case 0 -> return false (pure-AI team never calls a bench).
            return false;
        }
    }

    return true;
}

// ====================================================================
// updateBenchControls -- updateBench.cpp:703-714
// ====================================================================
static void updateBenchControls(void)
{
    int teamBase = s_teamsSwapped
        ? swosReadSignedDword(s_teamBase + TEAMDATA_OFF_OPPONENTS_TEAM)
        : s_teamBase;
    if (teamBase == 0) teamBase = s_teamBase;

    int16_t playerNumber      = swosReadSignedWord(teamBase + TEAMDATA_OFF_PLAYER_NUMBER);
    int16_t playerCoachNumber = swosReadSignedWord(teamBase + TEAMDATA_OFF_PLAYER_COACH_NUMBER);
    int player = (playerNumber == 1 || playerCoachNumber == 1) ? IC_PLAYER1 : IC_PLAYER2;
    s_controls = swosGetPlayerEvents(player);
}

// updateBench.cpp:716-723 -- bumpGoToBenchTimer.
static bool bumpGoToBenchTimer(void)
{
    if (s_goToBenchTimer <= 0)
        return false;
    s_goToBenchTimer--;
    return true;
}

// ====================================================================
// filterControls -- updateBench.cpp:725-758
// ====================================================================
static bool filterControls(void)
{
    const int kMovementDelay = 4;

    int currentDirection = s_controls & IC_EVENT_MOVEMENT_MASK;
    bool sameDirectionHeld = currentDirection != 0
        && s_blockDirections
        && s_lastDirection == currentDirection;

    if (sameDirectionHeld &&
        (swosBenchGetBenchState() == BENCH_STATE_INITIAL ||
         (s_lastDirection != IC_EVENT_UP && s_lastDirection != IC_EVENT_DOWN) ||
         ++s_movementDelayTimer != kMovementDelay))
        return true;

    s_blockDirections = false;

    s_movementDelayTimer = 0;
    s_lastDirection = currentDirection;

    bool firing = (s_controls & IC_EVENT_KICK) != 0;

    if (firing)
        s_fireTimer++;
    else
        s_fireTimer = 0;

    if (s_blockFire && firing)
        return true;
    s_blockFire = false;

    return false;
}

// ====================================================================
// benchInvoked -- updateBench.cpp:760-794
// ====================================================================
static bool benchInvoked(int teamBase)
{
    if ((s_controls & IC_EVENT_BENCH) != 0)
        return true;

    bool topTeam = teamBase == TEAMDATA_TOP_BASE;
    SwosBenchTapCounterState *state = topTeam ? &s_pl1TapState : &s_pl2TapState;

    if ((s_controls & IC_EVENT_MOVEMENT_MASK) == 0)
    {
        state->blockWhileHoldingDirection = false;
        if (++state->tapTimeoutCounter == BENCH_TAP_TIMEOUT_TICKS)
            tapStateReset(state);
    }
    else if (!state->blockWhileHoldingDirection)
    {
        if (tapStateGotLastTapDirection(state))
        {
            state->tapTimeoutCounter = 0;
            if (tapStateHoldingSameDirectionAsLastTap(state, s_controls))
            {
                if (++state->tapCount >= BENCH_NUM_TAPS_FOR_BENCH)
                    return true;
                state->blockWhileHoldingDirection = true;
            }
            else
            {
                state->previousDirection = 0;
                state->tapCount = 0;
            }
        }
        else
        {
            state->previousDirection = s_controls & IC_EVENT_MOVEMENT_MASK;
            state->blockWhileHoldingDirection = true;
        }
    }

    return false;
}

// ====================================================================
// findInitialPlayerToBeSubstituted -- updateBench.cpp:796-838
// ====================================================================
static void findInitialPlayerToBeSubstituted(void)
{
    int enterInfoAddr = s_teamGameBase + s_playerToEnterGameIndex * TDL_PLAYER_INFO_SIZE;
    int8_t position = (int8_t)swosReadByte(enterInfoAddr + TDL_OFF_POSITION);

    int exactMatch = -1;
    int approximateMatch = -1;
    int firstAvailablePlayer = -1;

    for (int i = 0; i < 11; i++)
    {
        int infoAddr = getBenchPlayerInfoAddrByOrd(i);
        uint8_t cards = swosReadByte(infoAddr + TDL_OFF_CARDS);
        if (cards >= 2)
            continue;

        if (firstAvailablePlayer < 0)
            firstAvailablePlayer = i;

        int8_t playerPosition = (int8_t)swosReadByte(infoAddr + TDL_OFF_POSITION);

        if (approximateMatch < 0)
        {
            bool bothDefence  = isDefencePosition(position)  && isDefencePosition(playerPosition);
            bool bothMidfield = isMidfieldPosition(position) && isMidfieldPosition(playerPosition);
            if (bothDefence || bothMidfield)
                approximateMatch = i;
        }

        if (exactMatch < 0 && position == playerPosition)
            exactMatch = i;
    }

    if (exactMatch >= 0)
        s_playerToBeSubstitutedOrd = exactMatch;
    else if (approximateMatch >= 0)
        s_playerToBeSubstitutedOrd = approximateMatch;
    else
        s_playerToBeSubstitutedOrd = firstAvailablePlayer >= 0 ? firstAvailablePlayer : 0;

    s_playerToBeSubstitutedPos = swosBenchGetBenchPlayerPosition(s_playerToBeSubstitutedOrd);
}

static bool isDefencePosition(int p)  { return p == 3 || p == 1 || p == 2; }
static bool isMidfieldPosition(int p) { return p == 6 || p == 4 || p == 5; }

static int getBenchPlayerInfoAddrByOrd(int ord)
{
    return s_teamGameBase + swosBenchGetBenchPlayerPosition(ord) * TDL_PLAYER_INFO_SIZE;
}

// updateBench.cpp:840-845 -- markPlayer.
static void markPlayer(void)
{
    int markedAddr = s_teamGameBase - 22;
    int16_t marked = swosReadSignedWord(markedAddr);
    bool alreadyMarked = marked == s_playerToBeSubstitutedPos;
    swosWriteWord(markedAddr, alreadyMarked ? (uint16_t)-1 : (uint16_t)s_playerToBeSubstitutedPos);
    s_selectedMenuPlayerIndex = -1;
}

// updateBench.cpp:847-851 -- swapPlayerShirtNumbers.
static void swapPlayerShirtNumbers(int ord1, int ord2)
{
    int row = s_teamGameBase == swosReadSignedDword(ADDR_topTeamInGame) ? 1 : 0;
    uint8_t t = s_shirtNumberTable[row][ord1];
    s_shirtNumberTable[row][ord1] = s_shirtNumberTable[row][ord2];
    s_shirtNumberTable[row][ord2] = t;
}

// updateBench.cpp:853-860 -- increasePlayerIndex.
static void increasePlayerIndex(void)
{
    while (++s_arrowPlayerIndex <= BENCH_MAX_SUBSTITUTES && !isPlayerOkToSelect())
    {
    }

    if (s_arrowPlayerIndex > BENCH_MAX_SUBSTITUTES)
        decreasePlayerIndex(false);
}

// updateBench.cpp:862-870 -- decreasePlayerIndex.
static void decreasePlayerIndex(bool allowBenchSwitch)
{
    if (s_arrowPlayerIndex != 0)
    {
        while (--s_arrowPlayerIndex > 0 && !isPlayerOkToSelect())
        {
        }
    }
    else if (allowBenchSwitch)
    {
        trainingSwapBenchTeams();
    }
}

// updateBench.cpp:872-888 -- increase/decreasePlayerToSubstitute. PORT-GUARD
// (matches the C#'s own bounded-loop note): a 32-step guard replaces the
// original's mutual recursion so pathological data can't hang the tick.
static void increasePlayerToSubstitute(void)
{
    int guard = 32;
    int step = +1;
    do
    {
        s_playerToBeSubstitutedOrd += step;
        if (s_playerToBeSubstitutedOrd >= 11) { s_playerToBeSubstitutedOrd = 10; step = -1; }
        if (s_playerToBeSubstitutedOrd < 0)   { s_playerToBeSubstitutedOrd = 0;  step = +1; }
        updatePlayerToBeSubstitutedPosition();
    } while (!isPlayerOkToSubstitute() && --guard > 0);
}

static void decreasePlayerToSubstitute(void)
{
    int guard = 32;
    int step = -1;
    do
    {
        s_playerToBeSubstitutedOrd += step;
        if (s_playerToBeSubstitutedOrd < 0)   { s_playerToBeSubstitutedOrd = 0;  step = +1; }
        if (s_playerToBeSubstitutedOrd >= 11) { s_playerToBeSubstitutedOrd = 10; step = -1; }
        updatePlayerToBeSubstitutedPosition();
    } while (!isPlayerOkToSubstitute() && --guard > 0);
}

// updateBench.cpp:890-905 -- isPlayerOkToSelect.
static bool isPlayerOkToSelect(void)
{
    int16_t numSubs = swosReadSignedWord(s_teamNumber == 1 ? ADDR_team1NumSubs : ADDR_team2NumSubs);

    if (g_swosBenchGameMinSubstitutes == numSubs)
        return false;

    int infoAddr = s_teamGameBase + (s_arrowPlayerIndex + 10) * TDL_PLAYER_INFO_SIZE;
    if (playerWasSubstituted(infoAddr))
        return false;

    return true;
}

// updateBench.cpp:907-913 -- isPlayerOkToSubstitute.
static bool isPlayerOkToSubstitute(void)
{
    if (swosBenchGetBenchState() == BENCH_STATE_MARKING_PLAYERS)
        return s_playerToBeSubstitutedOrd != 0;

    if (s_playerToBeSubstitutedPos < 0 || s_playerToBeSubstitutedPos > 10)
        return false;
    int infoAddr = s_teamGameBase + s_playerToBeSubstitutedPos * TDL_PLAYER_INFO_SIZE;
    return swosReadByte(infoAddr + TDL_OFF_CARDS) < 2;
}

// updateBench.cpp:915-922 -- trainingSwapBenchTeams.
static void trainingSwapBenchTeams(void)
{
    s_teamsSwapped = !s_teamsSwapped;
    s_trainingTopTeam = !s_trainingTopTeam;
    swosBenchSwapBenchWithOpponent();
    s_teamBase = swosReadSignedDword(s_teamBase + TEAMDATA_OFF_OPPONENTS_TEAM);
    int16_t teamNumber = swosReadSignedWord(s_teamBase + TEAMDATA_OFF_TEAM_NUMBER);
    s_teamGameBase = swosReadSignedDword(teamNumber == 2 ? ADDR_bottomTeamInGame : ADDR_topTeamInGame);
}

// PlayerInfo.wasSubstituted() -- swos.h:202-204.
static bool playerWasSubstituted(int infoAddr)
{
    if (swosReadByte(infoAddr + TDL_OFF_SUBSTITUTED) != 0)
        return true;
    if ((int8_t)swosReadByte(infoAddr + TDL_OFF_POSITION) == -1)
        return true;
    return swosReadByte(infoAddr + TDL_OFF_CARDS) >= 2;
}

// ====================================================================
// updateSubstitutedPlayerWalk -- updatePlayers.cpp:9051-9197
// ====================================================================
static void updateSubstitutedPlayerWalk(void)
{
    int16_t sip = swosReadSignedWord(ADDR_g_substituteInProgress);
    if (sip == 0)
        return;

    int32_t spriteAddr = swosReadSignedDword(ADDR_substitutedPlSprite);
    if (spriteAddr == 0)
        return;

    int32_t teamBase = swosReadSignedDword(ADDR_teamThatSubstitutes);
    if (teamBase == 0)
        teamBase = TEAMDATA_TOP_BASE;

    if (sip < 0)
    {
        // updatePlayers.cpp:9154-9197 -- l_set_player_going_in_speed.
        swosWriteWord(spriteAddr + PLSPR_OFF_SPEED, BENCH_SUBSTITUTED_PLAYER_SPEED);
        int32_t deltaX = swosReadSignedDword(spriteAddr + PLSPR_OFF_DELTA_X);
        int32_t deltaY = swosReadSignedDword(spriteAddr + PLSPR_OFF_DELTA_Y);
        if (deltaX == 0 && deltaY == 0)
            swosWriteWord(ADDR_g_substituteInProgress, 0);
        return;
    }

    int16_t injuryLevel = swosReadSignedWord(spriteAddr + PLSPR_OFF_INJURY_LEVEL);
    if (injuryLevel == -2)
    {
        newPlayerAboutToGoInTransition(spriteAddr, teamBase);
        return;
    }

    int16_t xWhole = swosReadSignedWord(spriteAddr + PLSPR_OFF_X + 2);
    int16_t yWhole = swosReadSignedWord(spriteAddr + PLSPR_OFF_Y + 2);

    if (xWhole != s_substitutedPlDestX || yWhole != s_substitutedPlDestY)
    {
        setSubstitutedPlayerDestination(spriteAddr, teamBase);
        return;
    }

    if (sip == 2)
    {
        newPlayerAboutToGoInTransition(spriteAddr, teamBase);
        return;
    }

    swosWriteWord(ADDR_g_substituteInProgress, 2);
    s_substitutedPlDestX = swosReadSignedWord(ADDR_plComingX);
    s_substitutedPlDestY = swosReadSignedWord(ADDR_plComingY);
    setSubstitutedPlayerDestination(spriteAddr, teamBase);
}

// updatePlayers.cpp:9140-9152 -- l_new_player_about_to_go_in.
static void newPlayerAboutToGoInTransition(int spriteAddr, int teamBase)
{
    swosWriteWord(ADDR_g_substituteInProgress, (uint16_t)-1);
    swosWriteWord(spriteAddr + PLSPR_OFF_X + 2, (uint16_t)s_plSubstitutedX);
    swosWriteWord(spriteAddr + PLSPR_OFF_Y + 2, (uint16_t)s_plSubstitutedY);
    swosWriteWord(spriteAddr + PLSPR_OFF_SPEED, BENCH_SUBSTITUTED_PLAYER_SPEED);
    swosWriteWord(spriteAddr + PLSPR_OFF_SENT_AWAY, 0);
    swosUpdatePlayerSpeedAndFrameDelay(teamBase, spriteAddr);
}

// updatePlayers.cpp:9154-9170 -- l_set_substituted_player_destination.
static void setSubstitutedPlayerDestination(int spriteAddr, int teamBase)
{
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_X, (uint16_t)s_substitutedPlDestX);
    swosWriteWord(spriteAddr + PLSPR_OFF_DEST_Y, (uint16_t)s_substitutedPlDestY);
    swosWriteWord(spriteAddr + PLSPR_OFF_SPEED, BENCH_SUBSTITUTED_PLAYER_SPEED);
    swosUpdatePlayerSpeedAndFrameDelay(teamBase, spriteAddr);
}

// ====================================================================
// Data-swap helpers (substitutePlayer / selectOrSwapPlayers)
// ====================================================================

static void swapPlayerInfoRecords(int idxA, int idxB)
{
    int a = s_teamGameBase + idxA * TDL_PLAYER_INFO_SIZE;
    int b = s_teamGameBase + idxB * TDL_PLAYER_INFO_SIZE;
    for (int i = 0; i < TDL_PLAYER_INFO_SIZE; i++)
    {
        uint8_t t = swosReadByte(a + i);
        swosWriteByte(a + i, swosReadByte(b + i));
        swosWriteByte(b + i, t);
    }
}

static void swapSpriteContentsKeepingOrdinals(int posA, int posB)
{
    bool top = s_teamBase == TEAMDATA_TOP_BASE;
    int a = swosTeamDataGetTeamSpriteAddr(top, posA);
    int b = swosTeamDataGetTeamSpriteAddr(top, posB);
    if (a == 0 || b == 0) return;

    for (int i = 0; i < PLSPR_SPRITE_SIZE; i++)
    {
        uint8_t t = swosReadByte(a + i);
        swosWriteByte(a + i, swosReadByte(b + i));
        swosWriteByte(b + i, t);
    }

    int16_t ordA = swosReadSignedWord(a + PLSPR_OFF_PLAYER_ORDINAL);
    int16_t ordB = swosReadSignedWord(b + PLSPR_OFF_PLAYER_ORDINAL);
    swosWriteWord(a + PLSPR_OFF_PLAYER_ORDINAL, (uint16_t)ordB);
    swosWriteWord(b + PLSPR_OFF_PLAYER_ORDINAL, (uint16_t)ordA);
}

// ====================================================================
// initializePlayerSpriteFrameIndices -- gameSprites.cpp:113-134
// ====================================================================
static void initializePlayerSpriteFrameIndices(void)
{
    for (int t = 0; t < 2; t++)
    {
        bool top = t == 0;
        int32_t teamGame = swosReadSignedDword(top ? ADDR_topTeamInGame : ADDR_bottomTeamInGame);
        if (teamGame == 0)
            continue;

        int keeperAddr = swosTeamDataGetTeamSpriteAddr(top, 0);
        if (keeperAddr != 0)
        {
            uint8_t keeperFace = swosReadByte(teamGame + TDL_OFF_FACE);
            swosWriteWord(keeperAddr + PLSPR_OFF_FRAME_OFFSET,
                (uint16_t)swosGameSpritesGetGoalkeeperSpriteOffset(top, keeperFace));
        }

        for (int i = 1; i < PLSPR_TEAM_SIZE; i++)
        {
            int spriteAddr = swosTeamDataGetTeamSpriteAddr(top, i);
            if (spriteAddr == 0) continue;
            uint8_t face = swosReadByte(teamGame + i * TDL_PLAYER_INFO_SIZE + TDL_OFF_FACE);
            swosWriteWord(spriteAddr + PLSPR_OFF_FRAME_OFFSET,
                (uint16_t)swosGameSpritesGetPlayerSpriteOffsetFromFace(face));
        }
    }
}

// ====================================================================
// checkForThrowInAndKeepersBall -- bench.cpp:131-143 (static)
// ====================================================================
static void checkForThrowInAndKeepersBall(void)
{
    int32_t lastTeamBase = swosReadSignedDword(ADDR_lastTeamPlayedBeforeBreak);
    int32_t playerAddr = 0;
    if (lastTeamBase != 0)
        playerAddr = swosTeamDataControlledPlayerFromBase(lastTeamBase);

    if (playerAddr != 0)
    {
        uint8_t state = swosReadByte(playerAddr + PLSPR_OFF_PLAYER_STATE);
        if (state == 5) // kPlayerStateThrowIn
        {
            swosWriteWord(ADDR_hideBall, 0);
            swosWriteByte(playerAddr + PLSPR_OFF_PLAYER_STATE, 0); // kPlayerStateNormal
            swosSetPlayerAnimationTable(playerAddr, ADDR_playerNormalStandingAnimTable);
        }
    }

    swosBenchCheckIfGoalkeeperClaimedTheBall();
}

// ====================================================================
// checkIfGoalkeeperClaimedTheBall -- game.cpp:1218-1235
// ====================================================================
void swosBenchCheckIfGoalkeeperClaimedTheBall(void)
{
    int16_t gs = swosReadSignedWord(ADDR_gameState);
    if (gs == 3) // kGameStateKeeperHoldsTheBall
    {
        int32_t teamBase = swosReadSignedDword(ADDR_lastTeamPlayedBeforeBreak);
        if (teamBase == 0) return;

        int keeperAddr = swosTeamDataGetTeamSpriteAddr(teamBase == TEAMDATA_TOP_BASE, 0);
        if (keeperAddr == 0) return;

        swosGoalkeeperClaimedTheBall(keeperAddr, teamBase == TEAMDATA_TOP_BASE);
    }
    else
    {
        swosWriteWord(ADDR_breakCameraMode, (uint16_t)-1);
        swosWriteWord(ADDR_gameStatePl, 101);
        swosWriteWord(ADDR_stoppageTimerTotal, 0);
        swosWriteWord(ADDR_stoppageTimerActive, 0);
        stopAllPlayers();
        swosWriteWord(ADDR_cameraXVelocity, 0);
        swosWriteWord(ADDR_cameraYVelocity, 0);
    }
}

// ====================================================================
// stopAllPlayers -- team.cpp:26-44 (Bench.cs's OWN copy -- NOT
// swosTeamPortStopAllPlayers(). Deliberately different: this one preserves
// the original SWOS bug where the top team's goalkeeperPlaying is NEVER
// cleared (team.cpp:39-43), while TeamPort.StopAllPlayers (used elsewhere
// in this port) always clears it for both teams -- see swos_team_port.c's
// own header note. Two distinct functions in the C# source; ported as two
// distinct functions here.)
// ====================================================================
static void stopAllPlayers(void)
{
    for (int t = 0; t < 2; t++)
    {
        bool top = (t == 0);
        int teamBase = swosTeamDataBase(top);

        for (int slot = 0; slot < PLSPR_TEAM_SIZE; slot++)
        {
            int spriteAddr = swosTeamDataGetTeamSpriteAddr(top, slot);
            if (spriteAddr == 0) continue;

            uint8_t state = swosReadByte(spriteAddr + PLSPR_OFF_PLAYER_STATE);
            int16_t sentAway = swosReadSignedWord(spriteAddr + PLSPR_OFF_SENT_AWAY);
            if (state == 0 && sentAway == 0)
            {
                int16_t xWhole = swosReadSignedWord(spriteAddr + PLSPR_OFF_X + 2);
                int16_t yWhole = swosReadSignedWord(spriteAddr + PLSPR_OFF_Y + 2);
                swosWriteWord(spriteAddr + PLSPR_OFF_DEST_X, (uint16_t)xWhole);
                swosWriteWord(spriteAddr + PLSPR_OFF_DEST_Y, (uint16_t)yWhole);
            }
        }

        swosWriteWord(teamBase + TEAMDATA_OFF_BALL_IN_PLAY, 0);
        swosWriteWord(teamBase + TEAMDATA_OFF_BALL_OUT_OF_PLAY, 0);
        swosWriteDword(teamBase + TEAMDATA_OFF_CONTROLLED_PLAYER, 0);
        swosWriteDword(teamBase + TEAMDATA_OFF_PASS_TO_PLAYER_PTR, 0);
        swosWriteWord(teamBase + TEAMDATA_OFF_PASSING_BALL, 0);
        swosWriteWord(teamBase + TEAMDATA_OFF_PASSING_TO_PLAYER, 0);
        swosWriteWord(teamBase + TEAMDATA_OFF_PLAYER_SWITCH_TIMER, 0);
        swosWriteDword(teamBase + TEAMDATA_OFF_PASSING_KICKING_PLAYER, 0);

        // team.cpp:39-43 -- original SWOS bug: top team's goalkeeperPlaying
        // is NEVER cleared. Preserved per the project's 1:1 charter.
        if (!top)
        {
            swosWriteWord(teamBase + TEAMDATA_OFF_GOALKEEPER_PLAYING, 0);
        }
    }
}
