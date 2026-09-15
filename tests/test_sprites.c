// Step-2 test: BallSprite/PlayerSprite/TeamData. Not a port of anything in
// OpenSWOS -- new code written for this repo. Verifies: every offset
// against swos-port's independently-computed struct layout (see the
// SOURCE/FIDELITY comments in the headers for the full cross-check),
// round-trips every ported field, and checks swosPlayerSpriteInit()'s
// 22-slot clear + pointer-table population + swosTeamDataInit()'s
// cross-pointers.
#include <stdio.h>

#include "swos_addr.h"
#include "swos_ball_sprite.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"
#include "swos_team_data.h"

static int g_failures = 0;

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            g_failures++; \
        } else { \
            printf("ok:   %s\n", msg); \
        } \
    } while (0)

// ---- Offset checks, independently re-derived from swos-port's packed
// ---- struct Sprite (110 bytes) / struct TeamGeneralInfo (145 bytes) --
// See header comments for the full derivation; this only pins the numbers
// so a future accidental edit gets caught.
static void test_offsets(void) {
    CHECK(PLSPR_SPRITE_SIZE == 110, "PlayerSprite: struct size is 110 (swos-port static_assert)");
    CHECK(PLSPR_OFF_FRAME_INDEX - PLSPR_OFF_FRAME_INDICES_TABLE == 4,
          "PlayerSprite: frameIndex sits right after the widened 4-byte frameIndicesTable");
    CHECK(PLSPR_OFF_PLAYER_STATE == 12 && PLSPR_OFF_PLAYER_DOWN_TIMER == 13,
          "PlayerSprite: state(byte)+playerDownTimer(byte) are adjacent, matching swos-port");
    CHECK(PLSPR_OFF_X == 30 && PLSPR_OFF_Y == 34 && PLSPR_OFF_Z == 38,
          "PlayerSprite: x/y/z are 4 bytes apart (FixedPoint Q16.16)");
    CHECK(PLSPR_OFF_SENT_AWAY == 108, "PlayerSprite: last struct field (sentAway) at 108, size 110 total");

    CHECK(BALLSPR_OFF_X == PLSPR_OFF_X && BALLSPR_OFF_SPEED == PLSPR_OFF_SPEED,
          "BallSprite and PlayerSprite share the same Sprite struct layout");

    CHECK(TEAMDATA_OFF_CONTROLLED_PLAYER == 32, "TeamData: controlledPlayer at +32 (swos.h:344)");
    CHECK(TEAMDATA_OFF_PL_VERY_CLOSE_TO_BALL == 61, "TeamData: plVeryCloseToBall at +61 (swos-port named field)");
    CHECK(TEAMDATA_OFF_SECONDARY_FIRE == 144, "TeamData: last field (secondaryFire) at 144, size 145 total");
    // AI_field_84's own name literally encodes its offset in hex (0x84 = 132) --
    // confirms 132 against swos-port's OWN internal "ofs134" label being stale
    // (see swos_team_data.h note; swos-port's sequential field sizes compute to
    // 132 too, so "ofs134" is swos-port's own drift, not a real disagreement).
    CHECK(TEAMDATA_OFF_AI_FIELD_84 == 132, "TeamData: AI field_84 at +132 (name encodes offset in hex: 0x84)");
}

static void test_ball_sprite_roundtrip(void) {
    swosMemoryInitStub();

    swosBallSpriteSetX(0x00450000);  // 69.0 in Q16.16
    CHECK(swosBallSpriteX() == 0x00450000, "BallSprite X round-trip");
    CHECK(swosBallSpriteXPixels() == 69, "BallSprite XPixels reads the Q16.16 high word");

    swosBallSpriteSetY(-1);
    CHECK(swosBallSpriteY() == -1, "BallSprite Y round-trip (negative)");

    swosBallSpriteSetSpeed(-2048);
    CHECK(swosBallSpriteSpeed() == -2048, "BallSprite Speed round-trip (negative Q8.8)");

    swosBallSpriteSetDirection(5);
    CHECK(swosBallSpriteDirection() == 5, "BallSprite Direction round-trip");

    swosBallSpriteSetDestX(-30000);
    swosBallSpriteSetDestY(30000);
    CHECK(swosBallSpriteDestX() == -30000 && swosBallSpriteDestY() == 30000, "BallSprite DestX/DestY round-trip");

    swosBallSpriteSetDeltaX(1234);
    swosBallSpriteSetDeltaY(-1234);
    swosBallSpriteSetDeltaZ(0x10000);
    CHECK(swosBallSpriteDeltaX() == 1234 && swosBallSpriteDeltaY() == -1234 && swosBallSpriteDeltaZ() == 0x10000,
          "BallSprite DeltaX/Y/Z round-trip");

    swosBallSpriteSetFrameIndicesTable(0x12345678);
    CHECK(swosBallSpriteFrameIndicesTable() == 0x12345678, "BallSprite FrameIndicesTable round-trip (widened dword)");
    swosBallSpriteSetFrameIndex(-1);
    swosBallSpriteSetFrameDelay(5);
    swosBallSpriteSetCycleFramesTimer(1);
    swosBallSpriteSetImageIndex(-1);
    swosBallSpriteSetFullDirection(200);
    CHECK(swosBallSpriteFrameIndex() == -1 && swosBallSpriteFrameDelay() == 5 &&
          swosBallSpriteCycleFramesTimer() == 1 && swosBallSpriteImageIndex() == -1 &&
          swosBallSpriteFullDirection() == 200, "BallSprite animation fields round-trip");
}

static void test_player_sprite_roundtrip(void) {
    swosMemoryInitStub();
    int slot = 7;  // arbitrary mid-range outfielder slot

    swosPlayerSpriteSetX(slot, 0x00320000);
    swosPlayerSpriteSetY(slot, 0x00640000);
    swosPlayerSpriteSetZ(slot, 0);
    CHECK(swosPlayerSpriteX(slot) == 0x00320000 && swosPlayerSpriteY(slot) == 0x00640000,
          "PlayerSprite X/Y round-trip (arbitrary slot)");

    swosPlayerSpriteSetTeamNumber(slot, 2);
    swosPlayerSpriteSetPlayerOrdinal(slot, 5);
    CHECK(swosPlayerSpriteTeamNumber(slot) == 2 && swosPlayerSpritePlayerOrdinal(slot) == 5,
          "PlayerSprite TeamNumber/PlayerOrdinal round-trip");

    swosPlayerSpriteSetPlayerState(slot, 6);  // GoalieDivingHigh
    swosPlayerSpriteSetPlayerDownTimer(slot, -12);
    CHECK(swosPlayerSpritePlayerState(slot) == 6 && swosPlayerSpritePlayerDownTimer(slot) == -12,
          "PlayerSprite PlayerState(byte)/PlayerDownTimer(signed byte) round-trip");

    swosPlayerSpriteSetBallDistance(slot, 123456);
    CHECK(swosPlayerSpriteBallDistance(slot) == 123456, "PlayerSprite BallDistance round-trip (dword)");

    swosPlayerSpriteSetTackleState(slot, 2);
    swosPlayerSpriteSetTacklingTimer(slot, 30);
    CHECK(swosPlayerSpriteTackleState(slot) == 2 && swosPlayerSpriteTacklingTimer(slot) == 30,
          "PlayerSprite TackleState/TacklingTimer round-trip");

    // Writing slot 7 must not disturb neighbouring slots 6/8 -- catches a
    // wrong SlotStride or an offset that overflows into the next slot.
    swosPlayerSpriteSetX(6, 111);
    swosPlayerSpriteSetX(8, 222);
    CHECK(swosPlayerSpriteX(6) == 111 && swosPlayerSpriteX(7) == 0x00320000 && swosPlayerSpriteX(8) == 222,
          "PlayerSprite slots are independent (no stride/offset overlap)");
}

static void test_player_sprite_init(void) {
    swosMemoryInitStub();
    swosPlayerSpriteInit();

    // All 22 slots got the documented defaults.
    int allSlotsOk = 1;
    for (int slot = 0; slot < PLSPR_TOTAL_SLOTS; slot++) {
        if (swosReadSignedWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_PLAYER_DIRECTION) != -1) allSlotsOk = 0;
        if (swosReadSignedWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_ON_SCREEN) != 1) allSlotsOk = 0;
        if (swosPlayerSpriteImageIndex(slot) != -1) allSlotsOk = 0;
        if (swosReadSignedWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_FRAME_INDEX) != -1) allSlotsOk = 0;
        if (swosReadSignedWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_FRAME_DELAY) != 5) allSlotsOk = 0;
        if (swosReadSignedWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_CYCLE_FRAMES_TIMER) != 1) allSlotsOk = 0;
        if (swosReadSignedWord(swosPlayerSpriteBase(slot) + PLSPR_OFF_FRAME_SWITCH_COUNTER) != -1) allSlotsOk = 0;
    }
    CHECK(allSlotsOk, "swosPlayerSpriteInit: all 22 slots get the documented per-slot defaults");

    // Ordinals/team numbers: slot 0 = team1 keeper (ordinal 1), slots 1..10 =
    // team1 outfielders (ordinal 2..11), slot 11 = team2 keeper, 12..21 =
    // team2 outfielders.
    CHECK(swosPlayerSpritePlayerOrdinal(0) == 1 && swosPlayerSpriteTeamNumber(0) == 1,
          "swosPlayerSpriteInit: slot 0 is team1 goalkeeper (ordinal 1, team 1)");
    CHECK(swosPlayerSpritePlayerOrdinal(1) == 2 && swosPlayerSpriteTeamNumber(1) == 1,
          "swosPlayerSpriteInit: slot 1 is team1's first outfielder (ordinal 2)");
    CHECK(swosPlayerSpritePlayerOrdinal(10) == 11 && swosPlayerSpriteTeamNumber(10) == 1,
          "swosPlayerSpriteInit: slot 10 is team1's last outfielder (ordinal 11)");
    CHECK(swosPlayerSpritePlayerOrdinal(11) == 1 && swosPlayerSpriteTeamNumber(11) == 2,
          "swosPlayerSpriteInit: slot 11 is team2 goalkeeper (ordinal 1, team 2)");
    CHECK(swosPlayerSpritePlayerOrdinal(21) == 11 && swosPlayerSpriteTeamNumber(21) == 2,
          "swosPlayerSpriteInit: slot 21 is team2's last outfielder (ordinal 11)");

    // Pointer tables: each entry is the absolute address of its slot.
    int tablesOk = 1;
    for (int i = 0; i < PLSPR_TEAM_SIZE; i++) {
        if (swosReadSignedDword(PLSPR_TEAM1_TABLE_BASE + i * 4) != swosPlayerSpriteBase(i)) tablesOk = 0;
        if (swosReadSignedDword(PLSPR_TEAM2_TABLE_BASE + i * 4) != swosPlayerSpriteBase(PLSPR_TEAM_SIZE + i)) tablesOk = 0;
    }
    CHECK(tablesOk, "swosPlayerSpriteInit: Team1/Team2TableBase pointer tables point at the right slots");

    // Bytes 0..SpriteSize-1 got cleared; the OpenSWOS-extension padding
    // region (110..127) is documented as NOT cleared by this function.
    swosPlayerSpriteSetX(3, 999);  // dirty a field first
    swosWriteWord(swosPlayerSpriteBase(3) + PLSPR_OFF_ENERGY, 4096);
    swosPlayerSpriteInit();
    CHECK(swosPlayerSpriteX(3) == 0, "swosPlayerSpriteInit: re-running clears bytes 0..109 (e.g. X back to 0)");
    CHECK(swosReadSignedWord(swosPlayerSpriteBase(3) + PLSPR_OFF_ENERGY) == 4096,
          "swosPlayerSpriteInit: does NOT clear the energy padding region (110+), matching the documented contract");
}

static void test_team_data_roundtrip(void) {
    swosMemoryInitStub();

    swosTeamDataSetPlayerHasBall(true, 1);
    swosTeamDataSetPlayerHasBall(false, 0);
    CHECK(swosTeamDataPlayerHasBall(true) == 1 && swosTeamDataPlayerHasBall(false) == 0,
          "TeamData PlayerHasBall round-trip, top/bottom independent");

    swosTeamDataSetControlledPlayer(true, 0x50380);
    CHECK(swosTeamDataControlledPlayer(true) == 0x50380, "TeamData ControlledPlayer round-trip");
    CHECK(swosTeamDataControlledPlayerFromBase(TEAMDATA_TOP_BASE) == 0x50380,
          "TeamData ControlledPlayerFromBase matches the per-team accessor");

    swosTeamDataSetSpinTimer(true, -1);
    swosTeamDataSetLeftSpin(true, 1);
    swosTeamDataSetRightSpin(true, 0);
    CHECK(swosTeamDataGetSpinTimer(true) == -1 && swosTeamDataLeftSpin(true) == 1 && swosTeamDataRightSpin(true) == 0,
          "TeamData SpinTimer/LeftSpin/RightSpin round-trip");

    swosTeamDataSetGoalkeeperDivingRight(false, 1);
    swosTeamDataSetGoalkeeperDivingLeft(false, 0);
    CHECK(swosTeamDataGoalkeeperDivingRight(false) == 1 && swosTeamDataGoalkeeperDivingLeft(false) == 0,
          "TeamData GoalkeeperDivingRight/Left round-trip (bottom team)");
}

static void test_team_data_init(void) {
    swosMemoryInitStub();
    swosPlayerSpriteInit();  // TeamData.Init reads PlayerSprite's table bases
    swosTeamDataInit();

    CHECK(swosTeamDataOpponentsTeam(true) == TEAMDATA_BOTTOM_BASE, "swosTeamDataInit: top.opponentsTeam == bottom base");
    CHECK(swosTeamDataOpponentsTeam(false) == TEAMDATA_TOP_BASE, "swosTeamDataInit: bottom.opponentsTeam == top base");

    CHECK(swosTeamDataPlayersTable(true) == PLSPR_TEAM1_TABLE_BASE, "swosTeamDataInit: top.players -> Team1TableBase");
    CHECK(swosTeamDataPlayersTable(false) == PLSPR_TEAM2_TABLE_BASE, "swosTeamDataInit: bottom.players -> Team2TableBase");

    // GetTeamSpriteAddr should resolve through the players table to the same
    // addresses swosPlayerSpriteBase() computes directly.
    int ok = 1;
    for (int i = 0; i < PLSPR_TEAM_SIZE; i++) {
        if (swosTeamDataGetTeamSpriteAddr(true, i) != swosPlayerSpriteBase(i)) ok = 0;
        if (swosTeamDataGetTeamSpriteAddr(false, i) != swosPlayerSpriteBase(PLSPR_TEAM_SIZE + i)) ok = 0;
    }
    CHECK(ok, "swosTeamDataInit: GetTeamSpriteAddr resolves through the players table correctly for both teams");

    CHECK(swosReadSignedDword(TEAMDATA_TOP_BASE + TEAMDATA_OFF_TEAM_STATS_PTR) == ADDR_topTeamStatsData,
          "swosTeamDataInit: top.teamStatsPtr -> ADDR_topTeamStatsData");
    CHECK(swosReadSignedDword(TEAMDATA_BOTTOM_BASE + TEAMDATA_OFF_TEAM_STATS_PTR) == ADDR_bottomTeamStatsData,
          "swosTeamDataInit: bottom.teamStatsPtr -> ADDR_bottomTeamStatsData");

    CHECK(swosTeamDataGetSpinTimer(true) == -1 && swosTeamDataGetSpinTimer(false) == -1,
          "swosTeamDataInit: spinTimer defaults to -1 for both teams");
    CHECK(swosTeamDataCurrentAllowedDirection(true) == -1 && swosTeamDataCurrentAllowedDirection(false) == -1,
          "swosTeamDataInit: currentAllowedDirection defaults to -1 for both teams");
}

int main(void) {
    test_offsets();
    test_ball_sprite_roundtrip();
    test_player_sprite_roundtrip();
    test_player_sprite_init();
    test_team_data_roundtrip();
    test_team_data_init();

    if (g_failures) {
        printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    printf("\nall checks passed\n");
    return 0;
}
