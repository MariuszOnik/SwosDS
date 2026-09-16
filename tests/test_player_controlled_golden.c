// Step-6A differential fixtures: real PlayerControlled.cs vs generated C.
#include <stdio.h>
#include <stdlib.h>

#include "swos_addr.h"
#include "swos_ball_sprite.h"
#include "swos_memory.h"
#include "swos_player_controlled.h"
#include "swos_player_sprite.h"
#include "swos_team_data.h"

#define GOLDEN_DIR "build/golden"

static int checked, failed;

static void compare(const char *name) {
    char path[256];
    snprintf(path, sizeof(path), "%s/pc_%s.bin", GOLDEN_DIR, name);
    FILE *f = fopen(path, "rb");
    if (!f) { printf("FAIL missing %s\n", path); checked++; failed++; return; }
    uint8_t *gold = malloc(SWOS_MEM_SIZE);
    size_t got = fread(gold, 1, SWOS_MEM_SIZE, f); fclose(f);
    const uint8_t *ours = swosMemoryView(0, SWOS_MEM_SIZE);
    int count = 0, first = -1;
    if (got == SWOS_MEM_SIZE) {
        for (int i = 0; i < SWOS_MEM_SIZE; i++) if (ours[i] != gold[i]) {
            if (first < 0) first = i;
            count++;
        }
    } else count = -1;
    free(gold); checked++;
    if (count == 0) printf("ok:   %s\n", name);
    else { printf("FAIL: %s mismatches=%d first=0x%X\n", name, count, first); failed++; }
}

static void init(void) {
    swosMemoryInit(true);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PLAYER_NUMBER, 1);
}

int main(void) {
    int player = swosPlayerSpriteBase(1);
    int receiver = swosPlayerSpriteBase(2);
    int keeper = swosPlayerSpriteBase(PLSPR_SLOT_GOALIE1);

    init();
    swosWriteWord(ADDR_playingPenalties, 1); swosWriteWord(ADDR_gameState, 3);
    swosRunControlledBranch(player, true); compare("controlled_penalties_early");

    init();
    swosWriteWord(ADDR_gameStatePl, 7);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, TEAMDATA_BOTTOM_BASE);
    swosRunControlledBranch(player, true); compare("controlled_break_wrong_team");

    init();
    swosWriteWord(ADDR_gameStatePl, 7);
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, TEAMDATA_TOP_BASE);
    swosWriteWord(player + PLSPR_OFF_DIRECTION, 3);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, UINT16_MAX);
    swosRunControlledBranch(player, true); compare("controlled_stopped_no_direction");

    init();
    swosWriteWord(ADDR_gameStatePl, 100);
    swosWriteDword(TEAMDATA_TOP_BASE + TEAMDATA_OFF_CONTROLLED_PLAYER, player);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PLAYER_HAS_BALL, 1);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION, 2);
    swosWriteWord(player + PLSPR_OFF_PLAYER_ORDINAL, 2);
    swosPlayerSpriteSetX(1, 300 << 16); swosPlayerSpriteSetY(1, 400 << 16);
    swosRunControlledBranch(player, true); compare("controlled_has_ball_pin");

    init(); swosRunPassReceiptTrigger(player, true); compare("receipt_null_noop");

    init();
    swosWriteDword(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PASS_TO_PLAYER_PTR, receiver);
    swosRunPassReceiptTrigger(player, true); compare("receipt_wrong_sprite_noop");

    init();
    swosWriteDword(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PASS_TO_PLAYER_PTR, receiver);
    swosWriteByte(TEAMDATA_TOP_BASE + 64, 1);
    swosWriteByte(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PL_VERY_CLOSE_TO_BALL, 1);
    swosBallSpriteSetSpeed(900);
    swosPlayerSpriteSetX(2, 330 << 16); swosPlayerSpriteSetY(2, 440 << 16);
    swosWriteWord(receiver + PLSPR_OFF_PLAYER_ORDINAL, 3);
    swosRunPassReceiptTrigger(receiver, true); compare("receipt_outfielder_commit");

    init();
    swosWriteDword(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PASS_TO_PLAYER_PTR, keeper);
    swosWriteByte(TEAMDATA_TOP_BASE + 64, 1);
    swosWriteByte(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PL_CLOSE_TO_BALL, 1);
    swosBallSpriteSetSpeed(1800);
    swosWriteDword(ADDR_lastTeamPlayed, TEAMDATA_TOP_BASE);
    swosWriteWord(ADDR_playerHadBall, 0);
    swosPlayerSpriteSetX(PLSPR_SLOT_GOALIE1, 336 << 16);
    swosPlayerSpriteSetY(PLSPR_SLOT_GOALIE1, 160 << 16);
    swosRunPassReceiptTrigger(keeper, true); compare("receipt_keeper_backpass");

    init();
    swosPlayerSpriteSetX(2, 70 << 16); swosPlayerSpriteSetY(2, 400 << 16);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PASSING_TO_PLAYER, 1);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PASSING_BALL, 1);
    swosRunPassExpectingBranch(receiver, true); compare("expecting_outside_pitch");

    init();
    swosPlayerSpriteSetX(2, 300 << 16); swosPlayerSpriteSetY(2, 400 << 16);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_BALL_X, 360);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_BALL_Y, 450);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosRunPassExpectingBranch(receiver, true); compare("expecting_plain_chase");

    init();
    swosPlayerSpriteSetX(2, 300 << 16); swosPlayerSpriteSetY(2, 400 << 16);
    swosPlayerSpriteSetFullDirection(2, 0);
    swosBallSpriteSetSpeed(900); swosBallSpriteSetDirection(2); swosBallSpriteSetFullDirection(32);
    swosWriteDword(TEAMDATA_TOP_BASE + TEAMDATA_OFF_LONG_PASS, 1);
    swosWriteWord(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PASSING_TO_PLAYER, 1);
    swosWriteWord(ADDR_gameStatePl, 100);
    swosRunPassExpectingBranch(receiver, true); compare("expecting_long_spin");

    init();
    swosPlayerSpriteSetX(2, 330 << 16); swosPlayerSpriteSetY(2, 440 << 16);
    swosWriteDword(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PASS_TO_PLAYER_PTR, receiver);
    swosWriteByte(TEAMDATA_TOP_BASE + 64, 1);
    swosWriteByte(TEAMDATA_TOP_BASE + TEAMDATA_OFF_PL_VERY_CLOSE_TO_BALL, 1);
    swosWriteWord(ADDR_gameStatePl, 100); swosBallSpriteSetSpeed(900);
    swosRunPassExpectingBranch(receiver, true); compare("expecting_receipt_commit");

    printf("\nchecked %d scenarios\n", checked);
    if (failed) { printf("%d FAILED\n", failed); return 1; }
    printf("all %d checks passed\n", checked); return 0;
}
