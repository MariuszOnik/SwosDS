// ARM/BlocksDS checkpoint for the swos-vm-c mechanical port (2026-09-16,
// after step 4: Memory/Flags/Rng/Tables, BallSprite/PlayerSprite/TeamData,
// AnimationTablesData, SpriteUpdate, BallUpdate -- 137/137 on desktop
// against real C#). NOT the swos-ds game: no rendering, no input. This
// re-runs a slice of already host-verified checks (exact expected values
// come from tests/test_*.c and tools/csharp-golden-dump's golden vectors,
// already proven correct against real OpenSWOS C#) on the real optimizing
// ARM toolchain (arm946e-s+nofp, -O2) to catch anything that only shows up
// there: struct layout/alignment surprises, UB that happened to behave on
// x86_64 but not ARM, stack depth through the goto-heavy Section4 state
// machine, etc. See ../README.md "Status: ARM checkpoint" for the result.
#include <nds.h>
#include <stdio.h>

#include "swos_addr.h"
#include "swos_ball_sprite.h"
#include "swos_ball_update.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"
#include "swos_rng.h"
#include "swos_sprite_update.h"
#include "swos_team_data.h"

static int g_checked = 0;
static int g_failed = 0;

#define CHECK(cond, msg) \
    do { \
        g_checked++; \
        if (!(cond)) { \
            g_failed++; \
            printf("FAIL: %s\n", msg); \
        } \
    } while (0)

int main(void) {
    consoleDemoInit();
    printf("swos-vm-c ARM checkpoint\n");
    printf("arm946e-s+nofp -O2 -- step 4\n\n");

    // ---- Memory.Init(pcMode) golden constants (already byte-exact vs
    // ---- real C# on desktop -- test_golden_dump.c) ----
    swosMemoryInit(true);
    CHECK(swosReadWord(ADDR_kBallKickingSpeed) == 2208, "Init(true): kBallKickingSpeed");
    CHECK(swosReadSignedWord(ADDR_kBallGroundConstant) == 13, "Init(true): kBallGroundConstant (PC)");

    swosMemoryInit(false);
    CHECK(swosReadSignedWord(ADDR_kBallGroundConstant) == 16, "Init(false): kBallGroundConstant (Amiga)");

    swosMemoryInit(true); // back to PC mode baseline for the rest

    // ---- RNG: seed=0 reproduces the original's natural zero-start byte ----
    swosRngReseed(0);
    CHECK(swosRngNextByte() == 230, "Rng: seed=0 first byte == 230 (== original zero-start state)");

    // ---- CalculateDeltaXAndY: exact golden vectors from
    // ---- test_sprite_update_golden.c (23/23 verified vs real C#) ----
    {
        SwosDeltasAndAngle r = swosCalculateDeltaXAndY(2048, 0, 0, 100, 0);
        CHECK(r.deltaX == 167933 && r.deltaY == 0 && r.direction == 64,
              "CalculateDeltaXAndY(2048,0,0,100,0) golden vector");
    }
    {
        SwosDeltasAndAngle r = swosCalculateDeltaXAndY(2048, 0, 0, -100, -100);
        CHECK(r.deltaX == -118745 && r.deltaY == -118745 && r.direction == 224,
              "CalculateDeltaXAndY(2048,0,0,-100,-100) golden vector");
    }

    // ---- MoveSprite: overshoot-on-both-axes golden vector ----
    swosMemoryInitStub();
    swosBallSpriteSetX(99 << 16);
    swosBallSpriteSetDestX(100);
    swosBallSpriteSetDeltaX(0x00020000);
    swosBallSpriteSetY(49 << 16);
    swosBallSpriteSetDestY(50);
    swosBallSpriteSetDeltaY(0x00020000);
    swosMoveSprite(BALLSPR_BASE);
    CHECK(swosBallSpriteX() == (100 << 16) && swosBallSpriteDeltaX() == 0
              && swosBallSpriteY() == (50 << 16) && swosBallSpriteDeltaY() == 0,
          "MoveSprite: overshoot on both axes snaps to dest, golden vector");

    // ---- PlayerSprite/TeamData Init structural invariants ----
    swosMemoryInit(true);
    CHECK(swosPlayerSpritePlayerOrdinal(0) == 1 && swosPlayerSpriteTeamNumber(0) == 1,
          "PlayerSpriteInit: slot 0 is team1 goalkeeper");
    CHECK(swosPlayerSpritePlayerOrdinal(11) == 1 && swosPlayerSpriteTeamNumber(11) == 2,
          "PlayerSpriteInit: slot 11 is team2 goalkeeper");
    CHECK(swosTeamDataOpponentsTeam(true) == TEAMDATA_BOTTOM_BASE,
          "TeamDataInit: top.opponentsTeam == bottom base");

    // ---- BallUpdate: friction clamps to zero (hand-verified: speed=2 <
    // ---- kBallGroundConstant=13, no movement -> friction clamps to 0) ----
    swosMemoryInit(true);
    swosBallSpriteSetSpeed(2);
    swosBallSpriteSetDestX(336); swosBallSpriteSetDestY(449);
    swosBallSpriteSetXPixels(336); swosBallSpriteSetYPixels(449);
    swosBallUpdateTick();
    CHECK(swosBallSpriteSpeed() == 0, "BallUpdate: friction clamps speed to 0");

    // ---- BallUpdate: ground bounce settles (hand-verified: Z=0, small
    // ---- downward delta -> post-bounce dz math yields <= 40960 -> settle) ----
    swosMemoryInit(true);
    swosBallSpriteSetXPixels(336); swosBallSpriteSetYPixels(449);
    swosBallSpriteSetZ(0);
    swosBallSpriteSetDeltaZ(-1000);
    swosBallSpriteSetSpeed(300);
    swosBallUpdateTick();
    CHECK(swosBallSpriteZPixels() == 0 && swosBallSpriteDeltaZ() == 0,
          "BallUpdate: small ground bounce settles (z=0, deltaZ=0)");

    // ---- BallUpdate: goal scored (same setup as the desktop golden
    // ---- scenario "goal_scored_lower_net", already byte-exact vs C#) ----
    swosMemoryInit(true);
    swosBallSpriteSetXPixels(336);
    swosBallSpriteSetYPixels(780);
    swosBallSpriteSetZPixels(10);
    swosBallSpriteSetDeltaY(30000);
    swosWriteDword(ADDR_lastPlayerPlayed, (uint32_t)swosPlayerSpriteBase(1));
    swosBallUpdateTick();
    CHECK(swosReadWord(ADDR_team1TotalGoals) == 1,
          "BallUpdate: lower-net goal bumps team1TotalGoals to 1");

    printf("\nchecked %d, failed %d\n", g_checked, g_failed);
    printf(g_failed == 0 ? "\nALL CHECKS PASSED\n" : "\nSOME CHECKS FAILED\n");

    while (1) {
        swiWaitForVBlank();
        scanKeys();
        if (keysDown() & KEY_START) break;
    }
    return 0;
}
