// Step-7B differential test: compares FULL VM/Memory state against golden
// dumps produced by the REAL C# (tools/csharp-golden-dump/Step7BGolden.cs).
// Covers UpdatePlayers.cs itself, exercised through its one public entry
// point, swosUpdatePlayersUpdate(teamIndex) -- every other function in the
// C# source is `private static`, so per-function golden dumps aren't
// possible here; each scenario instead seeds full match state so the
// 11-player loop routes ONE targeted sprite through the specific state
// handler being tested (see Step7BGolden.cs's header comment for why every
// scenario keeps both teams "human" -- playerNumber != 0 -- to stay clear
// of the step-9/step-10 assert-backed hook boundaries this step
// establishes).
//
// Regenerate build/golden/s7b_*.bin with:
//   cd tools/csharp-golden-dump && dotnet run -c Release -- ../../build/golden
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "swos_addr.h"
#include "swos_ball_sprite.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"
#include "swos_player_state.h"
#include "swos_team_data.h"
#include "swos_update_players.h"

#define GOLDEN_DIR "build/golden"

static int g_failures = 0;
static int g_checked = 0;

static void compareFullBuffer(const char *label) {
    char path[256];
    snprintf(path, sizeof(path), "%s/s7b_%s.bin", GOLDEN_DIR, label);

    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "FAIL: could not open %s -- run the C# golden-dump harness first: "
                         "cd tools/csharp-golden-dump && dotnet run -c Release -- ../../build/golden\n",
                path);
        g_failures++;
        g_checked++;
        return;
    }
    uint8_t *golden = malloc(SWOS_MEM_SIZE);
    long got = (long)fread(golden, 1, SWOS_MEM_SIZE, f);
    fclose(f);
    if (got != SWOS_MEM_SIZE) {
        printf("FAIL: %s is %ld bytes, expected %d\n", path, got, SWOS_MEM_SIZE);
        g_failures++;
        g_checked++;
        free(golden);
        return;
    }

    const uint8_t *ours = swosMemoryView(0, SWOS_MEM_SIZE);
    int mismatches = 0, firstMismatch = -1;
    for (int i = 0; i < SWOS_MEM_SIZE; i++) {
        if (ours[i] != golden[i]) {
            if (firstMismatch < 0) firstMismatch = i;
            mismatches++;
            if (mismatches <= 20)
                printf("  diff @ 0x%06X: C=0x%02X golden(C#)=0x%02X\n", i, ours[i], golden[i]);
        }
    }
    free(golden);

    g_checked++;
    if (mismatches == 0) {
        printf("ok:   %s matches C# byte-for-byte (0x%X bytes)\n", label, SWOS_MEM_SIZE);
    } else {
        printf("FAIL: %s has %d mismatched byte(s), first at 0x%06X\n", label, mismatches, firstMismatch);
        g_failures++;
    }
}

// Common baseline every Step7BGolden.cs scenario applies before its own
// setup: both teams human, in-progress. Individual scenarios override
// gameStatePl/gameState afterward as needed.
static void initBaseline(void) {
    swosMemoryInit(true);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PLAYER_NUMBER, 1);
    swosWriteWord(TEAMDATA_BOTTOM_BASE + TEAMDATA_OFF_PLAYER_NUMBER, 2);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosWriteWord(ADDR_gameState, 100);
}

int main(void) {
    // ---- Full orchestration: normal in-progress tick, both teams ----
    initBaseline();
    swosTeamDataSetControlledPlayer(true, swosPlayerSpriteBase(1));
    swosUpdatePlayersUpdate(0);
    compareFullBuffer("update_normal_in_progress_top");

    initBaseline();
    swosTeamDataSetControlledPlayer(false, swosPlayerSpriteBase(12));
    swosUpdatePlayersUpdate(1);
    compareFullBuffer("update_normal_in_progress_bottom");

    // ---- Stoppage tick: setPlayerPositionsForGameBreak, both tables ----
    initBaseline();
    swosWriteWord(ADDR_gameStatePl, 0);
    swosWriteWord(ADDR_gameState, 0);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)TEAMDATA_TOP_BASE);
    swosUpdatePlayersUpdate(0);
    compareFullBuffer("update_stoppage_kickoff_top");

    initBaseline();
    swosWriteWord(ADDR_gameStatePl, 0);
    swosWriteWord(ADDR_gameState, 0);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)TEAMDATA_TOP_BASE);
    swosUpdatePlayersUpdate(1);
    compareFullBuffer("update_stoppage_kickoff_bottom");

    // ---- TickTackledPlayer ----
    initBaseline();
    swosPlayerSpriteSetPlayerState(2, PLSTATE_TACKLED);
    swosWriteByte(swosPlayerSpriteBase(2) + PLSPR_OFF_PLAYER_DOWN_TIMER, 10);
    swosWriteWord(swosPlayerSpriteBase(2) + PLSPR_OFF_SPEED, 200);
    swosPlayerSpriteSetX(2, 336 << 16); swosPlayerSpriteSetY(2, 400 << 16);
    swosUpdatePlayersUpdate(0);
    compareFullBuffer("update_tackled_player");

    // ---- TickTacklingPlayer ----
    initBaseline();
    swosPlayerSpriteSetPlayerState(2, PLSTATE_TACKLING);
    swosWriteByte(swosPlayerSpriteBase(2) + PLSPR_OFF_PLAYER_DOWN_TIMER, 10);
    swosPlayerSpriteSetTacklingTimer(2, 5);
    swosWriteWord(swosPlayerSpriteBase(2) + PLSPR_OFF_SPEED, 300);
    swosPlayerSpriteSetX(2, 336 << 16); swosPlayerSpriteSetY(2, 400 << 16);
    swosUpdatePlayersUpdate(0);
    compareFullBuffer("update_tackling_player");

    // ---- TickInjuredRollingPlayer ----
    initBaseline();
    swosPlayerSpriteSetPlayerState(2, PLSTATE_INJURED);
    swosWriteByte(swosPlayerSpriteBase(2) + PLSPR_OFF_PLAYER_DOWN_TIMER, 10);
    swosUpdatePlayersUpdate(0);
    compareFullBuffer("update_injured_rolling_player");

    // ---- TickJumpHeader ----
    initBaseline();
    swosPlayerSpriteSetPlayerState(2, PLSTATE_JUMP_HEADER);
    swosWriteByte(swosPlayerSpriteBase(2) + PLSPR_OFF_PLAYER_DOWN_TIMER, 50);
    swosWriteWord(swosPlayerSpriteBase(2) + PLSPR_OFF_SPEED, 0);
    swosPlayerSpriteSetX(2, 336 << 16); swosPlayerSpriteSetY(2, 400 << 16);
    swosUpdatePlayersUpdate(0);
    compareFullBuffer("update_jump_header_player");

    // ---- TickStaticHeader ----
    initBaseline();
    swosPlayerSpriteSetPlayerState(2, PLSTATE_STATIC_HEADER);
    swosWriteByte(swosPlayerSpriteBase(2) + PLSPR_OFF_PLAYER_DOWN_TIMER, 50);
    swosWriteWord(swosPlayerSpriteBase(2) + PLSPR_OFF_SPEED, 0);
    swosPlayerSpriteSetX(2, 336 << 16); swosPlayerSpriteSetY(2, 400 << 16);
    swosUpdatePlayersUpdate(0);
    compareFullBuffer("update_static_header_player");

    // ---- TickGoalieDiving (rise path) ----
    initBaseline();
    swosPlayerSpriteSetPlayerState(PLSPR_SLOT_GOALIE1, PLSTATE_GOALIE_DIVING_HIGH);
    swosWriteByte(swosPlayerSpriteBase(PLSPR_SLOT_GOALIE1) + PLSPR_OFF_PLAYER_DOWN_TIMER, 70);
    swosUpdatePlayersUpdate(0);
    compareFullBuffer("update_goalie_diving_rise");

    // ---- checkIfThisPlayerGettingBooked (via tickAiControlled's stoppage tail) ----
    initBaseline();
    {
        int sa = swosPlayerSpriteBase(2);
        swosWriteDword(ADDR_bookedPlayer, (uint32_t)sa);
        swosWriteWord(ADDR_foulXCoordinate, 300);
        swosWriteWord(ADDR_foulYCoordinate, 400);
        swosWriteWord(ADDR_refState, 2); // kRefWaitingPlayer
        swosPlayerSpriteSetX(2, 321 << 16); swosPlayerSpriteSetY(2, 400 << 16); // == foulX+21, foulY
        swosWriteWord(ADDR_gameStatePl, 0);
        swosWriteWord(ADDR_gameState, 0);
        swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_UPDATE_PLAYER_INDEX, 2); // round-robin -> slot 2's turn
    }
    swosUpdatePlayersUpdate(0);
    compareFullBuffer("update_booked_player_walk_to_referee");

    printf("\nchecked %d scenarios\n", g_checked);
    if (g_failures == 0) {
        printf("all %d checks passed\n", g_checked);
        return 0;
    }
    printf("%d of %d checks FAILED\n", g_failures, g_checked);
    return 1;
}
