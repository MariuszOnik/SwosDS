// SOURCE: openswos game/scripts/Sim/Port/BallVariables.cs (full file).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
#include "swos_ball_variables.h"
#include "swos_addr.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"
#include "swos_team_data.h"

#include <stdint.h>

// Emulates `xchg ax, word ptr Dn+2` followed by `mov word ptr Dn, ax`:
// swap the high and low 16-bit halves of a 32-bit register.
static int32_t swapWords(int32_t v) {
    uint32_t u = (uint32_t)v;
    return (int32_t)((u >> 16) | (u << 16));
}

// ============================================================================
// updateBallVariables -- updatePlayers.cpp:11198-12486
// ============================================================================
void swosUpdateBallVariables(int aPlayerSprite, int aBallSprite, int aTeamData) {
    int A1 = aPlayerSprite;
    int A2 = aBallSprite;
    int A6 = aTeamData;
    int esi;

    // updatePlayers.cpp:11205-11219 -- load ball pos/delta + gravity.
    esi = A2;
    int32_t D1 = swosReadSignedDword(esi + PLSPR_OFF_X);       // ball.x  Q16.16
    int32_t D2 = swosReadSignedDword(esi + PLSPR_OFF_Y);       // ball.y  Q16.16
    int32_t D3 = swosReadSignedDword(esi + PLSPR_OFF_Z);       // ball.z  Q16.16
    int32_t D4 = swosReadSignedDword(esi + PLSPR_OFF_DELTA_X); // deltaX  Q16.16
    int32_t D5 = swosReadSignedDword(esi + PLSPR_OFF_DELTA_Y); // deltaY  Q16.16
    int32_t D6 = swosReadSignedDword(esi + PLSPR_OFF_DELTA_Z); // deltaZ  Q16.16
    int32_t D7 = swosReadSignedDword(ADDR_kGravityConstant);   // gravity (dword)

    // updatePlayers.cpp:11220-11268 -- branch dispatch on delta magnitude.
    if (D5 < -65536) goto l_ball_going_up;
    if (D5 > 65536) goto l_ball_going_down;
    if (D4 < -65536) goto l_ball_going_left;
    if (D4 > 65536) goto l_ball_going_right;

    goto l_ball_not_moving;

l_ball_going_up:;
    // updatePlayers.cpp:11270-11317 -- only continue if team is topTeamData.
    if (A6 != TEAMDATA_TOP_BASE) goto l_ball_not_moving;

    esi = A1;
    D2 = D2 - swosReadSignedDword(esi + PLSPR_OFF_Y);
    if (D2 >= 0) goto l_set_ball_y_to_0;

    esi = A2;
    swosWriteWord(ADDR_ballDefensiveX, swosReadWord(esi + PLSPR_OFF_X + 2));
    swosWriteWord(ADDR_ballDefensiveY, swosReadWord(esi + PLSPR_OFF_Y + 2));
    swosWriteWord(ADDR_ballDefensiveZ, swosReadWord(esi + PLSPR_OFF_Z + 2));

    esi = A1;
    D2 = D2 + swosReadSignedDword(esi + PLSPR_OFF_Y);
    goto l_ball_above_player_check;

l_set_ball_y_to_0:;
    // updatePlayers.cpp:11319-11353 -- gravity loop body.
    D6 = D6 - D7;
    D3 = D3 + D6;
    D1 = D1 + D4;
    D2 = D2 + D5;
    if (D2 >= 0) goto l_set_ball_y_to_0;

    // updatePlayers.cpp:11355-11442 -- undo last step, swap halves, write defensives.
    D1 = D1 - D4;
    D2 = D2 - D5;
    D3 = D3 - D6;
    esi = A1;
    D2 = D2 + swosReadSignedDword(esi + PLSPR_OFF_Y);

    D1 = swapWords(D1);
    D2 = swapWords(D2);
    D3 = swapWords(D3);

    swosWriteWord(ADDR_ballDefensiveX, (uint16_t)(int16_t)D1);
    swosWriteWord(ADDR_ballDefensiveY, (uint16_t)(int16_t)D2);
    if (((int16_t)D3) < 0) {
        D3 = (int32_t)((uint32_t)D3 & 0xFFFF0000u);
    }
    swosWriteWord(ADDR_ballDefensiveZ, (uint16_t)(int16_t)D3);

    D1 = swapWords(D1);
    D2 = swapWords(D2);
    D3 = swapWords(D3);

l_ball_above_player_check:;
    // updatePlayers.cpp:11444-11474.
    D2 = D2 - 8847360;
    if (D2 >= 0) goto l_ball_above_135;

    swosWriteWord(ADDR_ballNotHighX, swosReadWord(ADDR_ballDefensiveX));
    swosWriteWord(ADDR_ballNotHighY, swosReadWord(ADDR_ballDefensiveY));
    swosWriteWord(ADDR_ballNotHighZ, swosReadWord(ADDR_ballDefensiveZ));
    D2 = D2 + 8847360;
    goto cseg_779F5;

l_ball_above_135:;
    // updatePlayers.cpp:11476-11586 -- gravity loop, target Y == 135 (Q16.16).
    D6 = D6 - D7;
    D3 = D3 + D6;
    D1 = D1 + D4;
    D2 = D2 + D5;
    if (D2 >= 0) goto l_ball_above_135;

    D1 = D1 - D4;
    D3 = D3 - D6;
    D2 = 8847360;

    D1 = swapWords(D1);
    D2 = swapWords(D2);
    D3 = swapWords(D3);

    swosWriteWord(ADDR_ballNotHighX, (uint16_t)(int16_t)D1);
    swosWriteWord(ADDR_ballNotHighY, (uint16_t)(int16_t)D2);
    if (((int16_t)D3) < 0) {
        D3 = (int32_t)((uint32_t)D3 & 0xFFFF0000u);
    }
    swosWriteWord(ADDR_ballNotHighZ, (uint16_t)(int16_t)D3);

    D1 = swapWords(D1);
    D2 = swapWords(D2);
    D3 = swapWords(D3);

cseg_779F5:;
    // updatePlayers.cpp:11587-11614.
    D2 = D2 - 8454144;
    if (D2 >= 0) goto l_ball_between_129_135;

    swosWriteWord(ADDR_strikeDestX, 0);
    swosWriteWord(ADDR_ballStrikeY, 0);
    swosWriteWord(ADDR_ballStrikeZ, 0);
    D2 = D2 + 8454144;
    goto l_out;

l_ball_between_129_135:;
    // updatePlayers.cpp:11616-11726 -- gravity loop, target Y == 129 (Q16.16).
    D6 = D6 - D7;
    D3 = D3 + D6;
    D1 = D1 + D4;
    D2 = D2 + D5;
    if (D2 >= 0) goto l_ball_between_129_135;

    D1 = D1 - D4;
    D3 = D3 - D6;
    D2 = 8454144;

    D1 = swapWords(D1);
    D2 = swapWords(D2);
    D3 = swapWords(D3);

    swosWriteWord(ADDR_strikeDestX, (uint16_t)(int16_t)D1);
    swosWriteWord(ADDR_ballStrikeY, (uint16_t)(int16_t)D2);
    if (((int16_t)D3) < 0) {
        D3 = (int32_t)((uint32_t)D3 & 0xFFFF0000u);
    }
    swosWriteWord(ADDR_ballStrikeZ, (uint16_t)(int16_t)D3);

    D1 = swapWords(D1);
    D2 = swapWords(D2);
    D3 = swapWords(D3);
    goto l_out;

l_ball_going_down:;
    // updatePlayers.cpp:11728-12184 -- symmetric to ball_going_up.
    if (A6 != TEAMDATA_BOTTOM_BASE) goto l_ball_not_moving;

    esi = A1;
    D2 = D2 - swosReadSignedDword(esi + PLSPR_OFF_Y);
    if (D2 < 0) goto l_ball_above_player;

    esi = A2;
    swosWriteWord(ADDR_ballDefensiveX, swosReadWord(esi + PLSPR_OFF_X + 2));
    swosWriteWord(ADDR_ballDefensiveY, swosReadWord(esi + PLSPR_OFF_Y + 2));
    swosWriteWord(ADDR_ballDefensiveZ, swosReadWord(esi + PLSPR_OFF_Z + 2));

    esi = A1;
    D2 = D2 + swosReadSignedDword(esi + PLSPR_OFF_Y);
    goto cseg_77C95;

l_ball_above_player:;
    // updatePlayers.cpp:11777-11900 -- gravity loop, target = player.y.
    D6 = D6 - D7;
    D3 = D3 + D6;
    D1 = D1 + D4;
    D2 = D2 + D5;
    if (D2 < 0) goto l_ball_above_player;

    D1 = D1 - D4;
    D2 = D2 - D5;
    D3 = D3 - D6;
    esi = A1;
    D2 = D2 + swosReadSignedDword(esi + PLSPR_OFF_Y);

    D1 = swapWords(D1);
    D2 = swapWords(D2);
    D3 = swapWords(D3);

    swosWriteWord(ADDR_ballDefensiveX, (uint16_t)(int16_t)D1);
    swosWriteWord(ADDR_ballDefensiveY, (uint16_t)(int16_t)D2);
    if (((int16_t)D3) < 0) {
        D3 = (int32_t)((uint32_t)D3 & 0xFFFF0000u);
    }
    swosWriteWord(ADDR_ballDefensiveZ, (uint16_t)(int16_t)D3);

    D1 = swapWords(D1);
    D2 = swapWords(D2);
    D3 = swapWords(D3);

cseg_77C95:;
    // updatePlayers.cpp:11902-11932.
    D2 = D2 - 50003968;
    if (D2 < 0) goto cseg_77CDD;

    swosWriteWord(ADDR_ballNotHighX, swosReadWord(ADDR_ballDefensiveX));
    swosWriteWord(ADDR_ballNotHighY, swosReadWord(ADDR_ballDefensiveY));
    swosWriteWord(ADDR_ballNotHighZ, swosReadWord(ADDR_ballDefensiveZ));
    D2 = D2 + 50003968;
    goto cseg_77DD5;

cseg_77CDD:;
    // updatePlayers.cpp:11934-12043 -- gravity loop, target Y == 50003968 (Q16.16).
    D6 = D6 - D7;
    D3 = D3 + D6;
    D1 = D1 + D4;
    D2 = D2 + D5;
    if (D2 < 0) goto cseg_77CDD;

    D1 = D1 - D4;
    D3 = D3 - D6;
    D2 = 50003968;

    D1 = swapWords(D1);
    D2 = swapWords(D2);
    D3 = swapWords(D3);

    swosWriteWord(ADDR_ballNotHighX, (uint16_t)(int16_t)D1);
    swosWriteWord(ADDR_ballNotHighY, (uint16_t)(int16_t)D2);
    if (((int16_t)D3) < 0) {
        D3 = (int32_t)((uint32_t)D3 & 0xFFFF0000u);
    }
    swosWriteWord(ADDR_ballNotHighZ, (uint16_t)(int16_t)D3);

    D1 = swapWords(D1);
    D2 = swapWords(D2);
    D3 = swapWords(D3);

cseg_77DD5:;
    // updatePlayers.cpp:12045-12072.
    D2 = D2 - 50397184;
    if (D2 < 0) goto cseg_77E0B;

    swosWriteWord(ADDR_strikeDestX, 0);
    swosWriteWord(ADDR_ballStrikeY, 0);
    swosWriteWord(ADDR_ballStrikeZ, 0);
    D2 = D2 + 50397184;
    goto l_out;

cseg_77E0B:;
    // updatePlayers.cpp:12074-12184 -- gravity loop, target Y == 50397184 (Q16.16).
    D6 = D6 - D7;
    D3 = D3 + D6;
    D1 = D1 + D4;
    D2 = D2 + D5;
    if (D2 < 0) goto cseg_77E0B;

    D1 = D1 - D4;
    D3 = D3 - D6;
    D2 = 50397184;

    D1 = swapWords(D1);
    D2 = swapWords(D2);
    D3 = swapWords(D3);

    swosWriteWord(ADDR_strikeDestX, (uint16_t)(int16_t)D1);
    swosWriteWord(ADDR_ballStrikeY, (uint16_t)(int16_t)D2);
    if (((int16_t)D3) < 0) {
        D3 = (int32_t)((uint32_t)D3 & 0xFFFF0000u);
    }
    swosWriteWord(ADDR_ballStrikeZ, (uint16_t)(int16_t)D3);

    D1 = swapWords(D1);
    D2 = swapWords(D2);
    D3 = swapWords(D3);
    goto l_out;

l_ball_going_left:;
    // updatePlayers.cpp:12186-12317.
    esi = A1;
    D1 = D1 - swosReadSignedDword(esi + PLSPR_OFF_X);
    if (D1 >= 0) goto l_ball_right_of_player;

    D1 = D1 + swosReadSignedDword(esi + PLSPR_OFF_X);
    goto l_ball_not_moving;

l_ball_right_of_player:;
    // updatePlayers.cpp:12215-12317.
    D6 = D6 - D7;
    D3 = D3 + D6;
    D2 = D2 + D5;
    D1 = D1 + D4;
    if (D1 >= 0) goto l_ball_right_of_player;

    // updatePlayers.cpp:12251-12317 -- the original's sub uses the stale
    // `eax` value (D4 from the prior load); mechanically preserve: subtract D4.
    D1 = D1 - D4;
    D2 = D2 - D5;
    D3 = D3 - D6;
    esi = A1;
    D1 = D1 + swosReadSignedDword(esi + PLSPR_OFF_X);

    D1 = swapWords(D1);
    D2 = swapWords(D2);
    D3 = swapWords(D3);

    swosWriteWord(ADDR_ballDefensiveX, (uint16_t)(int16_t)D1);
    swosWriteWord(ADDR_ballDefensiveY, (uint16_t)(int16_t)D2);
    if (((int16_t)D3) < 0) {
        D3 = (int32_t)((uint32_t)D3 & 0xFFFF0000u);
    }
    swosWriteWord(ADDR_ballDefensiveZ, (uint16_t)(int16_t)D3);
    goto cseg_78136;

l_ball_going_right:;
    // updatePlayers.cpp:12319-12454 -- mirror of ball_going_left.
    esi = A1;
    D1 = D1 - swosReadSignedDword(esi + PLSPR_OFF_X);
    if (D1 < 0) goto l_ball_left_of_player;

    D1 = D1 + swosReadSignedDword(esi + PLSPR_OFF_X);
    goto l_ball_not_moving;

l_ball_left_of_player:;
    D6 = D6 - D7;
    D3 = D3 + D6;
    D2 = D2 + D5;
    D1 = D1 + D4;
    if (D1 < 0) goto l_ball_left_of_player;

    D1 = D1 - D4;
    D2 = D2 - D5;
    D3 = D3 - D6;
    esi = A1;
    D1 = D1 + swosReadSignedDword(esi + PLSPR_OFF_X);

    D1 = swapWords(D1);
    D2 = swapWords(D2);
    D3 = swapWords(D3);

    swosWriteWord(ADDR_ballDefensiveX, (uint16_t)(int16_t)D1);
    swosWriteWord(ADDR_ballDefensiveY, (uint16_t)(int16_t)D2);
    if (((int16_t)D3) < 0) {
        D3 = (int32_t)((uint32_t)D3 & 0xFFFF0000u);
    }
    swosWriteWord(ADDR_ballDefensiveZ, (uint16_t)(int16_t)D3);
    goto cseg_78136;

l_ball_not_moving:;
    // updatePlayers.cpp:12456-12464 -- ball static: snapshot whole-pixel x/y/z.
    esi = A2;
    swosWriteWord(ADDR_ballDefensiveX, swosReadWord(esi + PLSPR_OFF_X + 2));
    swosWriteWord(ADDR_ballDefensiveY, swosReadWord(esi + PLSPR_OFF_Y + 2));
    swosWriteWord(ADDR_ballDefensiveZ, swosReadWord(esi + PLSPR_OFF_Z + 2));

cseg_78136:;
    // updatePlayers.cpp:12465-12478 -- copy defensive -> notHigh, clear strike vars.
    swosWriteWord(ADDR_ballNotHighX, swosReadWord(ADDR_ballDefensiveX));
    swosWriteWord(ADDR_ballNotHighY, swosReadWord(ADDR_ballDefensiveY));
    swosWriteWord(ADDR_ballNotHighZ, swosReadWord(ADDR_ballDefensiveZ));
    swosWriteWord(ADDR_strikeDestX, 0);
    swosWriteWord(ADDR_ballStrikeY, 0);
    swosWriteWord(ADDR_ballStrikeZ, 0);

l_out:;
    return;
}

// ============================================================================
// calculateBallNextGroundXYPositions -- updatePlayers.cpp:12491-12697
// ============================================================================
void swosCalculateBallNextGroundXYPositions(int aBallSprite) {
    int A2 = aBallSprite;
    int esi = A2;

    int32_t D1 = swosReadSignedDword(esi + PLSPR_OFF_X);
    int32_t D2 = swosReadSignedDword(esi + PLSPR_OFF_Y);
    int32_t D3 = swosReadSignedDword(esi + PLSPR_OFF_Z);
    int32_t D4 = swosReadSignedDword(esi + PLSPR_OFF_DELTA_X);
    int32_t D5 = swosReadSignedDword(esi + PLSPR_OFF_DELTA_Y);

    if (D5 < -65536) goto l_ball_moving;
    if (D5 > 65536) goto l_ball_moving;
    if (D4 < -65536) goto l_ball_moving;
    if (D4 > 65536) goto l_ball_moving;

    goto l_ball_standing;

l_ball_moving:;
    {
    int32_t D6 = swosReadSignedDword(esi + PLSPR_OFF_DELTA_Z);
    int32_t D7 = swosReadSignedDword(ADDR_kGravityConstant);
    int32_t D0 = 1310720; // 0x140000

l_subtract_z:;
    D3 = D3 - D0;
    if (D3 >= 0) goto l_z_in_range;

    if (D0 == 851968) goto l_ball_standing; // 0x0D0000

    D3 = D3 + D0;
    D0 = D0 - 65536;
    goto l_subtract_z;

l_z_in_range:;
    D1 = D1 + D4;
    D2 = D2 + D5;
    D6 = D6 - D7;
    D3 = D3 + D6;
    if (D3 >= 0) goto l_z_in_range;

    D3 = D3 + D0;

    D1 = swapWords(D1);
    D2 = swapWords(D2);
    D3 = swapWords(D3);

    swosWriteWord(ADDR_ballNextGroundX, (uint16_t)(int16_t)D1);
    swosWriteWord(ADDR_ballNextGroundY, (uint16_t)(int16_t)D2);
    // updatePlayers.cpp:12685 -- ballNextGroundZDead is hardcoded to 0
    // regardless of D3.
    swosWriteWord(ADDR_ballNextGroundZDead, 0);
    goto l_out;
    }

l_ball_standing:;
    // updatePlayers.cpp:12688-12689 -- sentinel: ballNextGroundX = -1.
    swosWriteWord(ADDR_ballNextGroundX, (uint16_t)-1);

l_out:;
    return;
}
