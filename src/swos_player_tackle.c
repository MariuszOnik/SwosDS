// SOURCE: openswos game/scripts/Sim/Port/PlayerTackle.cs (see
// swos_player_tackle.h for the exact slice ported: PlayerBeginTackling
// (step 6A) plus PlayerTacklingTestFoul/PlayersTackledTheBallStrong and
// their whole executed call chain (step 7A)).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
//
// This file mirrors the C# source's own super-literal register-emulation
// style (esi/eax/A0..A6 as local int variables, raw struct-field-offset
// integer literals rather than named PLSPR_OFF_*/TEAMDATA_OFF_* macros) --
// the same offset value means a different field depending on what a given
// register currently points at (e.g. +32 is TeamData.OffControlledPlayer
// when esi holds a team address, but PlayerSprite.OffX+2 when esi holds a
// player address, exactly like the original asm's register reuse). Named
// macros are used only at the handful of sites where the C# source itself
// uses one (e.g. PlayerSprite.OffInjuryLevel in PlayerTackled).
#include "swos_player_tackle.h"

#include <stdbool.h>
#include <stdint.h>

#include "swos_addr.h"
#include "swos_audio_events.h"
#include "swos_ball_sprite.h"
#include "swos_ball_update.h"
#include "swos_memory.h"
#include "swos_player_actions.h"
#include "swos_player_energy.h"
#include "swos_player_sprite.h"
#include "swos_referee.h"
#include "swos_rng.h"
#include "swos_team_data.h"
#include "swos_team_port.h"

// player.cpp:1961 -- TS_GOOD_TACKLE.
#define PT_TS_GOOD_TACKLE 2
// swos.h:296-317 -- offsetof(TeamGame, players). See PlayerTackle.cs's own
// file-header comment for the full derivation of why every PlayerGameHeader
// site below subtracts this from TeamData.OffInGameTeamPtr.
#define PT_TEAM_GAME_HEADER_SIZE 42

static void testFoulForPenaltyAndFreeKick(int aBallOrFouledSprite, int aTeamData);
static int tryBookingThePlayer(int aPlayerSprite, int aTeamData);
static int trySendingOffThePlayer(int aPlayerSprite, int aTeamData);
static void playerTackled(int playerSpriteAddr, int teamDataAddr);

void swosPlayerBeginTackling(int player, int team, int direction) {
    swosWriteWord(player + PLSPR_OFF_TACKLE_STATE, 0);
    swosWriteWord(team + TEAMDATA_OFF_CONTROLLED_PL_DIRECTION,
                  (uint16_t)direction);
    swosWriteWord(player + PLSPR_OFF_DIRECTION, (uint16_t)direction);
    swosSetPlayerAnimationTable(player, ADDR_kPlTacklingAnimTableAddr);
    swosWriteByte(player + PLSPR_OFF_PLAYER_STATE, 1);
    swosWriteByte(player + PLSPR_OFF_PLAYER_DOWN_TIMER,
                  (uint8_t)swosReadSignedWord(ADDR_m_playerDownTacklingInterval));

    int ordinal = swosReadSignedWord(player + PLSPR_OFF_PLAYER_ORDINAL);
    int offset = swosReadSignedWord(ADDR_inGameTeamPlayerOffsets
                                    + (int16_t)(ordinal - 1) * 2);
    int header = swosReadSignedDword(team + TEAMDATA_OFF_IN_GAME_TEAM_PTR)
                 - 42 + (uint16_t)(int16_t)offset;
    if (swosReadByte(header + 50) != 0)
        swosWriteByte(player + PLSPR_OFF_PLAYER_DOWN_TIMER, 25);

    // Verbatim OpenSWOS behavior: the following sentinel overwrites both the
    // normal interval and faster-tackle value above.
    swosWriteByte(player + PLSPR_OFF_PLAYER_DOWN_TIMER, UINT8_MAX);

    int dst = ADDR_kDefaultDestinations + ((int16_t)direction << 2);
    int vx = swosReadSignedWord(dst);
    int vy = swosReadSignedWord(dst + 2);
    int posX = swosReadSignedWord(player + PLSPR_OFF_X + 2);
    int posY = swosReadSignedWord(player + PLSPR_OFF_Y + 2);
    int travel = 1000;
    if (vx != 0) {
        int allowed = vx > 0 ? 590 - posX : posX - 81;
        if (allowed < 0) allowed = 0;
        if (allowed < travel) travel = allowed;
    }
    if (vy != 0) {
        int allowed = vy > 0 ? 769 - posY : posY - 129;
        if (allowed < 0) allowed = 0;
        if (allowed < travel) travel = allowed;
    }
    int destX = posX + (vx > 0 ? travel : vx < 0 ? -travel : 0);
    int destY = posY + (vy > 0 ? travel : vy < 0 ? -travel : 0);
    swosWriteWord(player + PLSPR_OFF_DEST_X, (uint16_t)(int16_t)destX);
    swosWriteWord(player + PLSPR_OFF_DEST_Y, (uint16_t)(int16_t)destY);
    swosWriteWord(player + PLSPR_OFF_SPEED,
                  swosReadWord(ADDR_kPlayerTacklingSpeed));
    swosWriteWord(player + PLSPR_OFF_TACKLING_TIMER, 0);
}


// ============================================================================
// playerTacklingTestFoul -- updatePlayers.cpp:12708-13394
// ============================================================================
void swosPlayerTacklingTestFoul(int aPlayerSprite, int aTeamData) {
    int A1 = aPlayerSprite;
    int A6 = aTeamData;
    int A2;
    int A0;
    int A4;
    int A5 = 0;
    int esi;
    int32_t eax;

    // 12710-12715 -- A2 = opponentsTeam.controlledPlayer.
    esi = A6;
    eax = swosReadSignedDword(esi + 0); // [A6+OffOpponentsTeam]
    A2 = eax;
    esi = A2;
    eax = swosReadSignedDword(esi + 32); // [opponent + OffControlledPlayer]
    A2 = eax;

    // 12716-12726 -- cmp A2, 0 ; jz @@out.
    if (A2 == 0) return;

    // 12728-12747 -- D0 = (player.x.whole - opponent.x.whole)^2.
    esi = A1;
    int16_t axS = swosReadSignedWord(esi + 32); // player.x +2
    int32_t D0_w = axS;
    esi = A2;
    axS = swosReadSignedWord(esi + 32); // opponent.x +2
    D0_w = (int16_t)(D0_w - axS);
    int32_t D0 = (int16_t)D0_w * (int16_t)D0_w; // imul bx, signed

    // 12748-12767 -- D1 = (player.y.whole - opponent.y.whole)^2.
    esi = A1;
    axS = swosReadSignedWord(esi + 36); // player.y +2
    int32_t D1_w = axS;
    esi = A2;
    axS = swosReadSignedWord(esi + 36); // opponent.y +2
    D1_w = (int16_t)(D1_w - axS);
    int32_t D1 = (int16_t)D1_w * (int16_t)D1_w;

    // 12768-12774 -- D0 += D1 (squared distance).
    D0 = D0 + D1;

    // 12775-12785 -- cmp D0, 32 ; jbe @@players_very_close (unsigned).
    if ((uint32_t)D0 <= 32u) goto l_players_very_close;

    return;

l_play_foul_comment_and_return:;
    // StubPlayDangerousPlayComment() omitted (audio).
    return;

l_opponents_player_goalkeeper:;
    // 12794-12817 -- shr speed, 1 twice (speed >>= 2); or speed, 1.
    esi = A1;
    {
        uint16_t src = swosReadWord(esi + 44); // player.speed
        src = (uint16_t)(src >> 1);
        swosWriteWord(esi + 44, src);
    }
    {
        uint16_t src = swosReadWord(esi + 44);
        src = (uint16_t)(src >> 1);
        swosWriteWord(esi + 44, src);
    }
    {
        uint16_t src = swosReadWord(esi + 44);
        src = (uint16_t)(src | 1);
        swosWriteWord(esi + 44, src);
    }
    return;

l_players_very_close:;
    // 12819-12832 -- cmp opponent.playerOrdinal, 1 ; jz l_opponents_player_goalkeeper.
    // (asm has TWO consecutive identical compares; preserve mechanically.)
    esi = A2;
    if ((int16_t)swosReadWord(esi + 2) == 1) goto l_opponents_player_goalkeeper;

    // 12834-12845 -- second identical compare; same branch, opposite sense.
    if ((int16_t)swosReadWord(esi + 2) == 1) return;

    // 12847-12858 -- cmp opponent.x +2, 81 ; jl out.
    if ((int16_t)swosReadWord(esi + 32) < 81) return;

    // 12860-12871 -- cmp opponent.y +2, 129 ; jl out.
    if ((int16_t)swosReadWord(esi + 36) < 129) return;

    // 12873-12884 -- cmp opponent.x +2, 590 ; jg out.
    if ((int16_t)swosReadWord(esi + 32) > 590) return;

    // 12886-12897 -- cmp opponent.y +2, 769 ; jg out.
    if ((int16_t)swosReadWord(esi + 36) > 769) return;

    // 12899-12920 -- tackler.speed >>= 2; tackler.speed |= 1.
    esi = A1;
    {
        uint16_t src = swosReadWord(esi + 44);
        src = (uint16_t)(src >> 1);
        swosWriteWord(esi + 44, src);
    }
    {
        uint16_t src = swosReadWord(esi + 44);
        src = (uint16_t)(src >> 1);
        swosWriteWord(esi + 44, src);
    }
    {
        uint16_t src = swosReadWord(esi + 44);
        src = (uint16_t)(src | 1);
        swosWriteWord(esi + 44, src);
    }

    // 12921-12930 -- push A1; push A6; call playerTackled; pop A6; pop A1.
    // A1 becomes opponent, A6 becomes opponent's team.
    {
        int savedA1 = A1;
        int savedA6 = A6;
        A1 = A2; // mov A1, A2
        esi = A6;
        eax = swosReadSignedDword(esi + 0); // [A6+OffOpponentsTeam]
        A6 = eax;
        playerTackled(A1, A6);
        A6 = savedA6;
        A1 = savedA1;
    }

    // 12931-12943 -- cmp opponent.ballDistance, 800 ; ja out.
    esi = A2;
    {
        uint32_t src = swosReadDword(esi + 74);
        if (src > 800u) return;
    }

    {
    // 12945-12952 -- load tackler.tackleState; or ax,ax ; jz @@foul_conceded.
    esi = A1;
    int16_t tackleState = (int16_t)swosReadWord(esi + 96);
    if (tackleState == 0) goto l_foul_conceeded;

    // 12954-12965 -- cmp tackler.tackleState, TS_GOOD_TACKLE (2) ;
    // jz @@play_foul_comment_and_return.
    if (tackleState == PT_TS_GOOD_TACKLE) goto l_play_foul_comment_and_return;

    // 12967-12987 -- D0 = tackler.direction; D0 -= opponent.direction;
    // cmp D0,-1 ; jl out.
    int16_t tacklerDir = (int16_t)swosReadWord(esi + 42);
    int16_t D0_dir = tacklerDir;
    esi = A2;
    int16_t oppDir = (int16_t)swosReadWord(esi + 42);
    D0_dir = (int16_t)(D0_dir - oppDir);
    if (D0_dir < -1) return;

    // 12989-12999 -- cmp D0, 1 ; jg out.
    if (D0_dir > 1) return;
    }

l_foul_conceeded:;
    {
    // 13002-13013 -- A0 = team.teamStatsPtr; add [A0+4], 1 (foulsConceded).
    esi = A6;
    eax = swosReadSignedDword(esi + 14); // [A6+OffTeamStatsPtr]
    A0 = eax;
    esi = A0;
    {
        uint16_t src = swosReadWord(esi + 4);
        src = (uint16_t)(src + 1);
        swosWriteWord(esi + 4, src);
    }

    // 13014-13020 -- or ax, cardsDisallowed ; jnz @@no_cards_given.
    int16_t ax = (int16_t)swosReadWord(ADDR_cardsDisallowed);
    if (ax != 0) goto l_no_cards_given;

    // 13022-13028 -- or ax, g_trainingGame ; jnz @@no_cards_given.
    ax = (int16_t)swosReadWord(ADDR_g_trainingGame);
    if (ax != 0) goto l_no_cards_given;

    // 13030-13045 -- D1 = opponent.x +2; D2 = opponent.y +2.
    esi = A2;
    int16_t D1s = (int16_t)swosReadWord(esi + 32);
    int16_t D2s = (int16_t)swosReadWord(esi + 36);
    if (D1s < 193) goto l_not_in_penalty_area;
    if (D1s > 478) goto l_not_in_penalty_area;
    if (D2s < 129) goto l_not_in_penalty_area;
    if (D2s > 769) goto l_not_in_penalty_area;

    // 13083-13093 -- cmp A6, offset topTeamData ; jnz @@right_team.
    if (A6 != TEAMDATA_TOP_BASE) goto l_right_team;

    // 13095-13105 -- cmp D2, 216 ; jle @@in_penalty_area.
    if (D2s <= 216) goto l_in_penalty_area;
    goto l_not_in_penalty_area;

l_right_team:;
    // 13110-13120 -- cmp D2, 682 ; jge @@in_penalty_area.
    if (D2s >= 682) goto l_in_penalty_area;
    // falls through to l_not_in_penalty_area below.

l_not_in_penalty_area:;
    // 13123-13137 -- D1 = 336, D2 = 129 (if top) or 769 (if bottom).
    D1s = 336;
    D2s = 129;
    if (A6 != TEAMDATA_TOP_BASE)
        D2s = 769;

    // 13140-13189 -- A5 = A2; D3 = (opp.x - D1)^2 + (opp.y - D2)^2. Then
    // iterate the team's spritesTable to find the closest non-sent-off,
    // non-keeper teammate to (D1, D2). A5 ends up as the closest such player.
    A5 = A2;
    esi = A2;
    int16_t D6_w = (int16_t)swosReadWord(esi + 32);
    D6_w = (int16_t)(D6_w - D1s);
    int32_t D6 = D6_w * D6_w;
    int16_t D3_w = (int16_t)swosReadWord(esi + 36);
    D3_w = (int16_t)(D3_w - D2s);
    int32_t D3 = D3_w * D3_w;
    D3 = D3 + D6;

    esi = A6;
    eax = swosReadSignedDword(esi + 20); // [A6+OffPlayers]
    A0 = eax;
    int D0_loop = 10;

l_players_loop:;
    esi = A0;
    eax = swosReadSignedDword(esi + 0);
    A0 = A0 + 4;
    A4 = eax;
    esi = A4;
    {
    int16_t sentAway = (int16_t)swosReadWord(esi + 108);
    if (sentAway != 0) goto l_next_player;

    if ((int16_t)swosReadWord(esi + 2) == 1) goto l_next_player; // ordinal == 1 (goalkeeper)
    if (A4 == A1) goto l_next_player;                             // skip self

    D6_w = (int16_t)swosReadWord(esi + 32);
    D6_w = (int16_t)(D6_w - D1s);
    D6 = D6_w * D6_w;
    int16_t D7_w = (int16_t)swosReadWord(esi + 36);
    D7_w = (int16_t)(D7_w - D2s);
    int32_t D7 = D7_w * D7_w;
    D6 = D6 + D7;

    // cmp D6, D3 ; ja next_player (D6 is unsigned via `ja`).
    if ((uint32_t)D6 > (uint32_t)D3) goto l_next_player;
    D3 = D6;
    A5 = A4;
    }

l_next_player:;
    D0_loop--;
    // jns players_loop -- repeat while non-negative.
    if (D0_loop >= 0) goto l_players_loop;

    // 13305-13317 -- dseg_114EC2 = A5; cmp A2, A5 ; jz cseg_79444.
    swosWriteDword(ADDR_lastTackleNearestTeammate, (uint32_t)A5);
    if (A2 == A5) goto cseg_79444;

l_in_penalty_area:;
    {
    // 13320-13341 -- D0 = (currentGameTick & 0x1E) >> 1.
    // cmp D0, playerCardChance ; jnb @@no_cards_given.
    int16_t tick = (int16_t)swosReadWord(ADDR_currentGameTick);
    int D0_tick = tick & 30;
    D0_tick = D0_tick >> 1;
    int16_t cardChance = (int16_t)swosReadWord(ADDR_playerCardChance);
    // jnb = unsigned >=.
    if ((uint16_t)D0_tick >= (uint16_t)cardChance) goto l_no_cards_given;

    // 13343-13354 -- Rand(); cmp D0, 32 ; jb @@direct_red_card.
    int D0_rand = swosRngNextByte();
    if ((uint16_t)D0_rand < 32u) goto l_direct_red_card;
    }

l_yellow_card:;
    // 13356-13365 -- call TryBookingThePlayer; if !zero (no card given), goto
    // no_cards. Else TestFoulForPenaltyAndFreeKick + ActivateReferee.
    {
        int bookOut = tryBookingThePlayer(A1, A6);
        if (bookOut != 0) goto l_no_cards_given;
        testFoulForPenaltyAndFreeKick(A2, A6);
        swosRefereeActivate();
        return;
    }

cseg_79444:;
    {
    // 13367-13379 -- Rand() ; cmp D0, 32 ; jb @@yellow_card.
    int D0_rand = swosRngNextByte();
    if ((uint16_t)D0_rand < 32u) goto l_yellow_card;
    }

l_direct_red_card:;
    // 13381-13390 -- call TrySendingOffThePlayer; if !zero, no_cards. Else
    // TestFoulForPenaltyAndFreeKick + ActivateReferee.
    {
        int sendOut = trySendingOffThePlayer(A1, A6);
        if (sendOut != 0) goto l_no_cards_given;
        testFoulForPenaltyAndFreeKick(A2, A6);
        swosRefereeActivate();
        return;
    }

l_no_cards_given:;
    // 13392-13393 -- jmp TestFoulForPenaltyAndFreeKick (tail call).
    testFoulForPenaltyAndFreeKick(A2, A6);
    }
}


// ============================================================================
// testFoulForPenaltyAndFreeKick -- updatePlayers.cpp:13404-13868
// ============================================================================
static void testFoulForPenaltyAndFreeKick(int aBallOrFouledSprite, int aTeamData) {
    int A2 = aBallOrFouledSprite;
    int A6 = aTeamData;
    int esi;

    // 13406-13417 -- cmp gameStatePl, 101 ; jz @@out.
    if ((int16_t)swosReadWord(ADDR_gameStatePl) == 101) return;

    // StubPlayFoulWhistleSample() now wired to a real sound (see
    // swos_audio_events.h).
    swosAudioFireEvent(SWOS_AUDIO_EVENT_FOUL_WHISTLE);

    // 13420-13424 -- D1 = foul.x +2, D2 = foul.y +2.
    esi = A2;
    int16_t D1 = (int16_t)swosReadWord(esi + 32);
    int16_t D2 = (int16_t)swosReadWord(esi + 36);

    // 13425-13435 -- cmp A6, offset topTeamData ; jz @@left_team.
    if (A6 == TEAMDATA_TOP_BASE) goto l_left_team;

    // 13437 -- cameraDirection = 4.
    swosWriteWord(ADDR_cameraDirection, 4);
    // 13438-13472 -- penalty area test for bottom team (Y>=682, 193<=X<=478).
    if (D2 < 682) goto l_not_in_lower_penalty_area;
    if (D1 < 193) goto l_not_in_lower_penalty_area;
    if (D1 > 478) goto l_not_in_lower_penalty_area;

    // 13474-13509 -- penalty.
    swosWriteWord(ADDR_gameState, 14);
    swosWriteWord(ADDR_breakCameraMode, (uint16_t)-1);
    swosWriteByte(ADDR_playerTurnFlags, 56);
    swosWriteWord(ADDR_foulXCoordinate, 336);
    swosWriteWord(ADDR_foulYCoordinate, 711);
    // StubPlayPenaltyComment() omitted (audio).
    goto l_continue_after_penalty;

l_left_team:;
    // 13513 -- cameraDirection = 0.
    swosWriteWord(ADDR_cameraDirection, 0);
    // 13514-13548 -- penalty area test for top team (Y<=216, 193<=X<=478).
    if (D2 > 216) goto l_not_in_upper_penalty_area;
    if (D1 < 193) goto l_not_in_upper_penalty_area;
    if (D1 > 478) goto l_not_in_upper_penalty_area;

    // 13550-13585 -- penalty (upper).
    swosWriteWord(ADDR_gameState, 14);
    swosWriteWord(ADDR_breakCameraMode, (uint16_t)-1);
    swosWriteByte(ADDR_playerTurnFlags, 131);
    swosWriteWord(ADDR_foulXCoordinate, 336);
    swosWriteWord(ADDR_foulYCoordinate, 187);
    // StubPlayPenaltyComment() omitted (audio).
    goto l_continue_after_penalty;

l_not_in_upper_penalty_area:;
    // StubPlayFoulComment() omitted (audio).
    // 13591-13600 -- cmp D2, 216 ; jl @@ordinary_foul.
    if (D2 < 216) goto l_ordinary_foul;
    // 13602-13612 -- cmp D2, 331 ; jl @@its_a_free_kick.
    if (D2 < 331) goto l_its_a_free_kick;
    goto l_ordinary_foul;

l_not_in_lower_penalty_area:;
    // StubPlayFoulComment() omitted (audio).
    // 13619-13628 -- cmp D2, 567 ; jl @@ordinary_foul.
    if (D2 < 567) goto l_ordinary_foul;
    // 13630-13640 -- cmp D2, 682 ; jg @@ordinary_foul.
    if (D2 > 682) goto l_ordinary_foul;

l_its_a_free_kick:;
    // 13643-13653 -- cmp A6, offset bottomTeamData ; jz @@right_team_made_foul.
    if (A6 == TEAMDATA_BOTTOM_BASE) goto l_right_team_made_foul;

    // 13655-13727 -- top-team-made-foul (foul taken by bottom team): map X
    // to one of 7 free-kick zones.
    if (D1 < 153) goto l_free_kick_left_1;
    if (D1 < 261) goto l_free_kick_left_2;
    if (D1 < 309) goto l_free_kick_left_3;
    if (D1 < 362) goto l_free_kick_center;
    if (D1 < 410) goto l_free_kick_right_1;
    if (D1 < 518) goto l_free_kick_right_2;
    goto l_free_kick_right_3;

l_right_team_made_foul:;
    // 13730-13800 -- bottom-team-made-foul: mirror mapping.
    if (D1 < 153) goto l_free_kick_right_3;
    if (D1 < 261) goto l_free_kick_right_2;
    if (D1 < 309) goto l_free_kick_right_1;
    if (D1 < 362) goto l_free_kick_center;
    if (D1 < 410) goto l_free_kick_left_3;
    if (D1 < 518) goto l_free_kick_left_2;
    // fall through to l_free_kick_left_1.

l_free_kick_left_1:;
    swosWriteWord(ADDR_gameState, 6); // ST_FREE_KICK_LEFT1
    goto l_save_foul_coordinates;
l_free_kick_left_2:;
    swosWriteWord(ADDR_gameState, 7);
    goto l_save_foul_coordinates;
l_free_kick_left_3:;
    swosWriteWord(ADDR_gameState, 8);
    goto l_save_foul_coordinates;
l_free_kick_center:;
    swosWriteWord(ADDR_gameState, 9);
    goto l_save_foul_coordinates;
l_free_kick_right_1:;
    swosWriteWord(ADDR_gameState, 10);
    goto l_save_foul_coordinates;
l_free_kick_right_2:;
    swosWriteWord(ADDR_gameState, 11);
    goto l_save_foul_coordinates;
l_free_kick_right_3:;
    swosWriteWord(ADDR_gameState, 12);
    goto l_save_foul_coordinates;

l_ordinary_foul:;
    swosWriteWord(ADDR_gameState, 13); // ST_FOUL

l_save_foul_coordinates:;
    swosWriteWord(ADDR_breakCameraMode, (uint16_t)-1);
    swosWriteByte(ADDR_playerTurnFlags, -1);
    swosWriteWord(ADDR_foulXCoordinate, (uint16_t)D1);
    swosWriteWord(ADDR_foulYCoordinate, (uint16_t)D2);

l_continue_after_penalty:;
    // 13842-13855 -- `cmp forceLeftTeam, 1 / jnz @@jump_here / mov A6,
    // offset bottomTeamData`: the jnz SKIPS the assignment, so A6 becomes
    // bottomTeamData only when forceLeftTeam == 1; otherwise A6 stays the
    // FOULING team.
    if ((int16_t)swosReadWord(ADDR_forceLeftTeam) == 1)
        A6 = TEAMDATA_BOTTOM_BASE;

    // 13858-13867 -- stoppage transition.
    swosWriteWord(ADDR_gameStatePl, 101);
    swosWriteWord(ADDR_gameNotInProgressCounterWriteOnly, 0);
    esi = A6;
    int32_t teamOpp = swosReadSignedDword(esi + 0); // [A6+OffOpponentsTeam]
    swosWriteDword(ADDR_lastTeamPlayedBeforeBreak, (uint32_t)teamOpp);
    swosWriteWord(ADDR_stoppageTimerTotal, 0);
    swosWriteWord(ADDR_stoppageTimerActive, 0);
    swosTeamPortStopAllPlayers();
    swosWriteWord(ADDR_cameraXVelocity, 0);
    swosWriteWord(ADDR_cameraYVelocity, 0);
}


// ============================================================================
// tryBookingThePlayer -- updatePlayers.cpp:13877-14174
// returns: 0 = card given, 1 = card NOT given.
// ============================================================================
static int tryBookingThePlayer(int aPlayerSprite, int aTeamData) {
    int A1 = aPlayerSprite;
    int A6 = aTeamData;
    int A0;
    int A5;
    int esi;
    int32_t eax;
    int16_t D0;
    uint8_t D1b;
    (void)D1b; // D1 is read but its value is never used downstream (matches C#).

    // 13879-13884 -- or ax, plg_D3_param ; jnz cseg_794A5.
    int16_t ax = (int16_t)swosReadWord(ADDR_plg_D3_param);
    if (ax != 0) goto cseg_794A5;

    // 13886-13899 -- AI-only path: check team.playerNumber + playerCoachNumber.
    esi = A6;
    ax = (int16_t)swosReadWord(esi + 4); // playerNumber
    if (ax != 0) goto l_player_controls_team;

    ax = (int16_t)swosReadWord(esi + 6); // playerCoachNumber
    if (ax != 0) goto l_player_controls_team;

    goto l_no_card_out;

cseg_794A5:;
    // 13904-13917 -- same playerNumber + playerCoachNumber checks.
    esi = A6;
    ax = (int16_t)swosReadWord(esi + 4);
    if (ax != 0) goto l_player_controls_team;
    ax = (int16_t)swosReadWord(esi + 6);
    if (ax != 0) goto l_player_controls_team;

    // 13919-13925 -- cmp player.cards, 0 ; jnz l_no_card_out.
    esi = A1;
    ax = (int16_t)swosReadWord(esi + 102);
    if (ax != 0) goto l_no_card_out;

l_player_controls_team:;
    // 13927-13934 -- cmp player.cards, 0 ; jz l_player_has_no_cards.
    esi = A1;
    ax = (int16_t)swosReadWord(esi + 102);
    if (ax == 0) goto l_player_has_no_cards;

    // 13936-13947 -- cmp team.teamNumber, 1 ; jnz l_second_team.
    esi = A6;
    if ((int16_t)swosReadWord(esi + 18) != 1) goto l_second_team;

    // 13949-13954 -- or ax, team1NumAllowedInjuries ; jz l_no_card_out.
    ax = (int16_t)swosReadWord(ADDR_team1NumAllowedInjuries);
    if (ax == 0) goto l_no_card_out;
    goto l_player_has_no_cards;

l_second_team:;
    // 13959-13964 -- or ax, team2NumAllowedInjuries ; jz l_no_card_out.
    ax = (int16_t)swosReadWord(ADDR_team2NumAllowedInjuries);
    if (ax == 0) goto l_no_card_out;

l_player_has_no_cards:;
    // 13966-13996 -- A5 = team.inGameTeamPtr + inGameTeamPlayerOffsets[(ordinal-1)*2].
    // Port fix: inGameTeamPtr is players[0]; the +51/+52 field literals below
    // are TeamGame-header-relative, so rebase to the TeamGame start (-42).
    esi = A6;
    eax = swosReadSignedDword(esi + 10) - PT_TEAM_GAME_HEADER_SIZE; // OffInGameTeamPtr - header
    A5 = eax;
    esi = A1;
    D0 = (int16_t)swosReadWord(esi + 2); // ordinal
    D0 = (int16_t)(D0 - 1);
    D0 = (int16_t)(D0 << 1); // *2
    A0 = ADDR_inGameTeamPlayerOffsets;
    esi = A0;
    {
        int ebx = (uint16_t)D0;
        int16_t axS = (int16_t)swosReadWord(esi + ebx);
        D0 = axS;
    }
    eax = A5;
    {
        int ebx = (uint16_t)D0;
        eax = eax + ebx;
    }
    A5 = eax;

    // 13997-14005 -- D0 = player.cards + 1.
    esi = A1;
    ax = (int16_t)swosReadWord(esi + 102);
    D0 = ax;
    D0 = (int16_t)(D0 + 1);

    // 14006-14011 -- or ax, plg_D3_param ; jnz cseg_795E0.
    ax = (int16_t)swosReadWord(ADDR_plg_D3_param);
    if (ax != 0) goto cseg_795E0;

    // 14013-14022 -- A0 = dseg_17E3EE; if PlayerGameHeader.previousCards != 0
    // then A0 = dseg_17E3F3.
    A0 = ADDR_dseg_17E3EE;
    esi = A5;
    {
        uint8_t al = swosReadByte(esi + 51); // previousCards
        if (al != 0)
            A0 = ADDR_dseg_17E3F3;
    }

    // cseg_795B7:
    // 14025-14033 -- D1 = PlayerGameHeader.cards; newCards =
    // dseg_17E3xx[D0 (ordinal byte)]; write to PlayerGameHeader.cards.
    esi = A5;
    D1b = swosReadByte(esi + 52); // cards
    esi = A0;
    {
        int ebx = (uint16_t)D0;
        uint8_t al = swosReadByte(esi + ebx);
        esi = A5;
        swosWriteByte(esi + 52, al);
    }
    goto l_give_yellow_card_to_player;

cseg_795E0:;
    {
    // 14036-14050 -- D1 = PlayerGameHeader.cards; if cards == 0: cards = 1
    // (yellow). Else: cards = 3 (second yellow).
    esi = A5;
    uint8_t al = swosReadByte(esi + 52);
    D1b = al;
    if (al != 0) goto cseg_79603;
    swosWriteByte(esi + 52, 1);
    goto l_give_yellow_card_to_player;
    }

cseg_79603:;
    esi = A5;
    swosWriteByte(esi + 52, 3);
    // l_jmp_give_yellow_card -- fall through.

l_no_card_out:;
    return 1;

l_give_yellow_card_to_player:;
    // 14063-14077 -- player.cards++; lastTeamBooked = A6; bookedPlayer = A1; refTimer = 0.
    esi = A1;
    {
        uint16_t src = swosReadWord(esi + 102);
        src = (uint16_t)(src + 1);
        swosWriteWord(esi + 102, src);
    }
    swosWriteDword(ADDR_lastTeamBooked, (uint32_t)A6);
    swosWriteDword(ADDR_bookedPlayer, (uint32_t)A1);
    swosWriteWord(ADDR_refTimer, 0);

    // 14078-14088 -- cmp player.cards, 2 ; jz @@second_yellow_card.
    if ((int16_t)swosReadWord(esi + 102) == 2) goto l_second_yellow_card;

    // 14090-14107 -- whichCard = CARD_YELLOW (1); team.teamStatsPtr.bookings++.
    swosWriteWord(ADDR_whichCard, 1);
    esi = A6;
    eax = swosReadSignedDword(esi + 14); // teamStatsPtr
    A0 = eax;
    esi = A0;
    {
        uint16_t src = swosReadWord(esi + 6);
        src = (uint16_t)(src + 1);
        swosWriteWord(esi + 6, src);
    }
    return 0;

l_second_yellow_card:;
    // 14111-14123 -- whichCard = CARD_SECOND_YELLOW (3); bookings--.
    swosWriteWord(ADDR_whichCard, 3);
    esi = A6;
    eax = swosReadSignedDword(esi + 14);
    A0 = eax;
    esi = A0;
    {
        uint16_t src = swosReadWord(esi + 6);
        src = (uint16_t)(src - 1);
        swosWriteWord(esi + 6, src);
    }
    // 14124-14131 -- sendingsOff++.
    {
        uint16_t src = swosReadWord(esi + 8);
        src = (uint16_t)(src + 1);
        swosWriteWord(esi + 8, src);
    }

    // 14132-14143 -- cmp team.teamNumber, 1 ; jnz l_team2_red_card.
    esi = A6;
    if ((int16_t)swosReadWord(esi + 18) != 1) goto l_team2_red_card;

    // 14145-14156 -- team1NumAllowedInjuries--.
    {
        uint16_t src = swosReadWord(ADDR_team1NumAllowedInjuries);
        src = (uint16_t)(src - 1);
        swosWriteWord(ADDR_team1NumAllowedInjuries, src);
    }
    goto l_given_card_ok;

l_team2_red_card:;
    // 14159-14166 -- team2NumAllowedInjuries--.
    {
        uint16_t src = swosReadWord(ADDR_team2NumAllowedInjuries);
        src = (uint16_t)(src - 1);
        swosWriteWord(ADDR_team2NumAllowedInjuries, src);
    }

l_given_card_ok:;
    return 0;
}


// ============================================================================
// trySendingOffThePlayer -- updatePlayers.cpp:14180-14400
// returns: 0 = red card given, 1 = no red card given.
// ============================================================================
static int trySendingOffThePlayer(int aPlayerSprite, int aTeamData) {
    int A1 = aPlayerSprite;
    int A6 = aTeamData;
    int A0;
    int A5;
    int esi;
    int32_t eax;
    int16_t D0;
    uint8_t D1b;
    (void)D1b; // read but never used downstream (matches C#).

    // 14182-14195 -- playerNumber check; else playerCoachNumber.
    esi = A6;
    int16_t ax = (int16_t)swosReadWord(esi + 4);
    if (ax != 0) goto l_player;

    ax = (int16_t)swosReadWord(esi + 6);
    if (ax == 0) goto l_computer_team_no_red_card;

l_player:;
    // 14197-14209 -- cmp team.teamNumber, 1.
    esi = A6;
    if ((int16_t)swosReadWord(esi + 18) != 1) goto l_team2;

    // 14211-14218 -- or ax, team1NumAllowedInjuries ; jz @@computer_team_no_red_card.
    ax = (int16_t)swosReadWord(ADDR_team1NumAllowedInjuries);
    if (ax == 0) goto l_computer_team_no_red_card;
    goto cseg_7972E;

l_team2:;
    // 14221-14226 -- or ax, team2NumAllowedInjuries ; jz @@computer_team_no_red_card.
    ax = (int16_t)swosReadWord(ADDR_team2NumAllowedInjuries);
    if (ax == 0) goto l_computer_team_no_red_card;

cseg_7972E:;
    // 14229-14258 -- A5 = team.inGameTeamPtr + offsets[(ordinal-1)*2].
    // Port fix: rebase inGameTeamPtr(players[0]) to the TeamGame start (-42).
    esi = A6;
    eax = swosReadSignedDword(esi + 10) - PT_TEAM_GAME_HEADER_SIZE;
    A5 = eax;
    esi = A1;
    D0 = (int16_t)swosReadWord(esi + 2);
    D0 = (int16_t)(D0 - 1);
    D0 = (int16_t)(D0 << 1);
    A0 = ADDR_inGameTeamPlayerOffsets;
    esi = A0;
    {
        int ebx = (uint16_t)D0;
        int16_t axS = (int16_t)swosReadWord(esi + ebx);
        D0 = axS;
    }
    eax = A5;
    {
        int ebx = (uint16_t)D0;
        eax = eax + ebx;
    }
    A5 = eax;

    // 14259-14268 -- D0 = (player.cards XOR 1) + 3.
    esi = A1;
    ax = (int16_t)swosReadWord(esi + 102);
    D0 = ax;
    D0 = (int16_t)(D0 ^ 1);
    D0 = (int16_t)(D0 + 3);

    // 14269-14274 -- or ax, plg_D3_param ; jnz cseg_79804.
    ax = (int16_t)swosReadWord(ADDR_plg_D3_param);
    if (ax != 0) goto cseg_79804;

    // 14276-14285 -- A0 = dseg_17E3EE; if previousCards != 0, A0 = dseg_17E3F3.
    A0 = ADDR_dseg_17E3EE;
    esi = A5;
    {
        uint8_t al = swosReadByte(esi + 51);
        if (al != 0)
            A0 = ADDR_dseg_17E3F3;
    }

    // cseg_797DB:
    // 14288-14296 -- D1 = PlayerGameHeader.cards; new = table[D0]; write back.
    esi = A5;
    D1b = swosReadByte(esi + 52);
    esi = A0;
    {
        int ebx = (uint16_t)D0;
        uint8_t al = swosReadByte(esi + ebx);
        esi = A5;
        swosWriteByte(esi + 52, al);
    }
    goto l_update_statistics_with_red_card;

cseg_79804:;
    // 14299-14302 -- D1 = PlayerGameHeader.cards; cards = 3.
    esi = A5;
    D1b = swosReadByte(esi + 52);
    swosWriteByte(esi + 52, 3);
    goto l_update_statistics_with_red_card;

l_computer_team_no_red_card:;
    return 1;

l_update_statistics_with_red_card:;
    // 14316-14321 -- lastTeamBooked = A6; bookedPlayer = A1; refTimer = 0;
    // whichCard = CARD_RED (2).
    swosWriteDword(ADDR_lastTeamBooked, (uint32_t)A6);
    swosWriteDword(ADDR_bookedPlayer, (uint32_t)A1);
    swosWriteWord(ADDR_refTimer, 0);
    swosWriteWord(ADDR_whichCard, 2);

    // 14322-14336 -- A0 = team.teamStatsPtr; if player.cards == 1, A0.bookings--.
    esi = A6;
    eax = swosReadSignedDword(esi + 14);
    A0 = eax;
    esi = A1;
    if ((int16_t)swosReadWord(esi + 102) != 1) goto l_no_yellow_card;

    esi = A0;
    {
        uint16_t src = swosReadWord(esi + 6);
        src = (uint16_t)(src - 1);
        swosWriteWord(esi + 6, src);
    }

l_no_yellow_card:;
    // 14349-14357 -- A0.sendingsOff++.
    esi = A0;
    {
        uint16_t src = swosReadWord(esi + 8);
        src = (uint16_t)(src + 1);
        swosWriteWord(esi + 8, src);
    }

    // 14358-14369 -- cmp team.teamNumber, 1 ; jnz l_second_team_player.
    esi = A6;
    if ((int16_t)swosReadWord(esi + 18) != 1) goto l_second_team_player;

    // 14371-14381 -- team1NumAllowedInjuries--.
    {
        uint16_t src = swosReadWord(ADDR_team1NumAllowedInjuries);
        src = (uint16_t)(src - 1);
        swosWriteWord(ADDR_team1NumAllowedInjuries, src);
    }
    goto l_red_card_given_out;

l_second_team_player:;
    // 14385-14392 -- team2NumAllowedInjuries--.
    {
        uint16_t src = swosReadWord(ADDR_team2NumAllowedInjuries);
        src = (uint16_t)(src - 1);
        swosWriteWord(ADDR_team2NumAllowedInjuries, src);
    }

l_red_card_given_out:;
    return 0;
}


// ============================================================================
// playerTackled -- updatePlayers.cpp:14410-14831
// ============================================================================
// Marks the fouled player down (common tail at l_set_tackled_anim_table)
// and conditionally writes PlayerGameHeader.isInjured/.injuriesBitfield +
// Sprite.injuryLevel when the injury-probability roll lands.
static void playerTackled(int playerSpriteAddr, int teamDataAddr) {
    int A1 = playerSpriteAddr;
    int A6 = teamDataAddr;
    int A0 = 0;   // PlayerGameHeader ptr (set in the injury-allowed path).
    int A5 = 0;   // table-walking pointer.
    int16_t D0 = 0;
    int16_t D1 = 0; // injury bitfield accumulator.
    bool doInjury = false;
    bool injuryAlreadyInjured = false;
    (void)doInjury;
    (void)injuryAlreadyInjured;

    // 14412-14417 -- training game check: 3/4 of the time the tackle is
    // harmless in training.
    int16_t trainingGame = swosReadSignedWord(ADDR_g_trainingGame);
    if (trainingGame != 0) {
        int trRnd = swosRngNextByte() & 3;
        if (trRnd != 0) goto l_set_tackled_anim_table;
    }

    {
    // l_not_a_training_game: check teamN allowed-injuries; bail if 0.
    int16_t teamNumber = swosReadSignedWord(A6 + 18);
    if (teamNumber == 1) {
        int16_t ali = swosReadSignedWord(ADDR_team1NumAllowedInjuries);
        if (ali == 0) goto l_set_tackled_anim_table;
    } else {
        int16_t ali = swosReadSignedWord(ADDR_team2NumAllowedInjuries);
        if (ali == 0) goto l_set_tackled_anim_table;
    }

    // l_injury_allowed: resolve PlayerGameHeader.
    int16_t ordinal = swosReadSignedWord(A1 + 2);
    int idx2 = (ordinal - 1) << 1;
    int byteOff = swosReadSignedWord(ADDR_inGameTeamPlayerOffsets + (uint16_t)idx2);
    // Port fix: inGameTeamPtr is players[0]; the +48/+77 field literals
    // below are TeamGame-header-relative, so rebase to the TeamGame start
    // (-42) -- A0+48 then lands on PlayerInfo+6 (isInjured), A0+77 on +35
    // (injuriesBits).
    int32_t inGameTeamPtr = swosReadSignedDword(A6 + 10) - PT_TEAM_GAME_HEADER_SIZE;
    A0 = inGameTeamPtr + (uint16_t)byteOff;

    // 14490-14491 -- D1 = gameLengthInGame (probability table index).
    D1 = swosReadSignedWord(ADDR_gameLengthInGame);

    // 14492-14507 -- choose probability table.
    A5 = ADDR_kTackleInjuryProbability;
    {
        uint8_t alIb = swosReadByte(A0 + 77);
        int16_t tmp = (int16_t)(alIb & 0xE0);
        if (tmp == 32) {
            A5 = ADDR_kTackleInjuryProbabilityAlreadyInjured;
            injuryAlreadyInjured = true;
        }
    }

    // OpenSWOS fatigue: being tackled costs the fouled player (A1) a random
    // 1..5% of their current energy -- fires once here.
    swosPlayerEnergyDrainOnTackle(A1);

    // l_not_injured: roll Rand against A5[D1]. Carry-clear (Rand >= thr) -> skip.
    D0 = (int16_t)swosRngNextByte();
    {
    int threshold = swosReadByte(A5 + (uint16_t)D1);
    // OpenSWOS fatigue: an exhausted (<20% energy) tackled player has
    // DOUBLE the injury chance.
    if (swosPlayerEnergyInjuryRiskDoubled(A1)) {
        threshold = threshold * 2;
        if (threshold > 255) threshold = 255;
    }
    // jnb = jump if not below, i.e. Rand >= threshold -> no injury.
    if ((uint8_t)D0 >= (uint8_t)threshold) goto l_set_tackled_anim_table;
    }

    doInjury = true;

    // StubPlayInjuryComment() omitted (audio).

    // 14530-14549 -- choose injury-level table.
    A5 = ADDR_kInjuryLevels;
    {
        uint8_t alIb = swosReadByte(A0 + 77);
        int16_t tmp = (int16_t)(alIb & 0xE0);
        if (tmp == 32)
            A5 = ADDR_kInjuryLevelAlreadyInjured;
    }

    // cseg_79E4D: injury-severity rolling walk.
    D0 = (int16_t)(swosRngNextByte() & 0x3F);
    D1 = 0x20;
    for (int step = 0; step < 7; step++) {
        uint8_t limit = swosReadByte(A5);
        A5++;
        // cmp byte ptr D0, al ; jb @@set_injury_level (break).
        if ((uint8_t)D0 < limit)
            goto l_set_injury_level;
        // sub byte ptr D0, al
        D0 = (int16_t)((uint8_t)((uint8_t)D0 - limit));
        // add word ptr D1, 32
        D1 = (int16_t)(D1 + 32);
    }
    // No explicit break -- fall through with D1 accumulated (the asm falls
    // through too; the bitfield write below truncates via & 0xE0).

l_set_injury_level:;
    // 14751 -- PlayerGameHeader.isInjured = 1.
    swosWriteByte(A0 + 48, 1);

    // 14752-14765 -- if (D1 > injuriesBitfield) injuriesBitfield = D1.
    {
    uint8_t existingIb = swosReadByte(A0 + 77);
    if ((uint8_t)D1 > existingIb) {
        swosWriteByte(A0 + 77, (uint8_t)D1);
    }
    }

    // cseg_79FD7: Sprite.injuryLevel = dseg_17E2EC[(D1 & 0xE0) >> 5].
    {
    int16_t d1Idx = (int16_t)((D1 & 0xE0) >> 5);
    int16_t injLvl = swosReadSignedWord(ADDR_dseg_17E2EC + (uint16_t)(d1Idx << 1));
    swosWriteWord(A1 + PLSPR_OFF_INJURY_LEVEL, (uint16_t)injLvl);
    }

    // 14786-14823 -- decrement the appropriate teamN allowed-injuries counter.
    if (teamNumber == 1) {
        uint16_t src = swosReadWord(ADDR_team1NumAllowedInjuries);
        src = (uint16_t)(src - 1);
        swosWriteWord(ADDR_team1NumAllowedInjuries, src);
    } else {
        uint16_t src = swosReadWord(ADDR_team2NumAllowedInjuries);
        src = (uint16_t)(src - 1);
        swosWriteWord(ADDR_team2NumAllowedInjuries, src);
    }
    }

l_set_tackled_anim_table:;
    // 14825-14830 -- common tail. Mark down + animation.
    swosWriteByte(A1 + PLSPR_OFF_PLAYER_STATE, 3); // PL_TACKLED
    swosWriteByte(A1 + PLSPR_OFF_PLAYER_DOWN_TIMER, 50);
    swosSetPlayerAnimationTable(A1, ADDR_kPlayerTackledAnimTableAddr);
}


// ============================================================================
// playersTackledTheBallStrong -- updatePlayers.cpp:14960-15242
// ============================================================================
// NOTE: there is a sibling/duplicate routine (singular) in player.cpp:
// 1684-1966, already ported as swosPlayerTackledTheBallStrong (step 5,
// swos_player_actions.c). The two have nearly identical logic but slightly
// different ball-speed scaling and route ball access differently (this one
// through the raw ball-sprite address, matching the asm exactly, per the
// C# source's own header comment). Ported as written -- not merged or
// "de-duplicated" with the step-5 version.
void swosPlayersTackledTheBallStrong(int aPlayerSprite, int aTeamData) {
    int A1 = aPlayerSprite;
    int A6 = aTeamData;
    int A0;
    int A2;
    int esi;
    int ebx;
    int32_t eax;

    // 14962-14969 -- D1 = team.currentAllowedDirection; if sign-bit set
    // (== -1) then D1 = sprite.direction.
    esi = A6;
    int16_t axS = (int16_t)swosReadWord(esi + 44); // currentAllowedDirection
    int16_t D1 = axS;
    if (axS >= 0) goto l_current_direction_allowed;

    esi = A1;
    axS = (int16_t)swosReadWord(esi + 42); // Sprite.direction
    D1 = axS;

l_current_direction_allowed:;
    // 14975-14979 -- A2 = offset ballSprite.
    A2 = BALLSPR_BASE;

    // 14977-14989 -- D0 = sprite.direction; D0 -= D1.
    esi = A1;
    int16_t pDir = (int16_t)swosReadWord(esi + 42);
    int16_t D0 = pDir;
    D0 = (int16_t)(D0 - D1);

    // 14990-14991 -- jz @@set_controlled_player_direction.
    if (D0 == 0) goto l_set_controlled_player_direction;

    // 14993-14996 -- D0 &= 7.
    D0 = (int16_t)(D0 & 7);

    // 14997-15006 -- cmp D0, 4 ; jz @@set_controlled_player_direction.
    bool cmp4Carry = (uint16_t)D0 < 4u;
    if (D0 == 4) goto l_set_controlled_player_direction;

    // 15008-15009 -- jb @@controls_leaning_leftward.
    if (cmp4Carry) goto l_controls_leaning_leftward;

    // 15011-15022 -- D0 = sprite.direction + 1.
    esi = A1;
    axS = (int16_t)swosReadWord(esi + 42);
    D0 = axS;
    D0 = (int16_t)(D0 + 1);
    goto l_set_new_direction;

l_controls_leaning_leftward:;
    // 15024-15037 -- D0 = sprite.direction - 1.
    esi = A1;
    axS = (int16_t)swosReadWord(esi + 42);
    D0 = axS;
    D0 = (int16_t)(D0 - 1);
    goto l_set_new_direction;

l_set_controlled_player_direction:;
    // 15039-15042 -- D0 = sprite.direction.
    esi = A1;
    axS = (int16_t)swosReadWord(esi + 42);
    D0 = axS;

l_set_new_direction:;
    // 15044-15051 -- D0 &= 7; team.controlledPlDirection = D0.
    D0 = (int16_t)(D0 & 7);
    esi = A6;
    swosWriteWord(esi + 56, (uint16_t)D0); // TeamGeneralInfo.controlledPlDirection

    // 15052-15060 -- A0 = kDefaultDestinations; D1 = [A0 + (D0 << 2)].
    A0 = ADDR_kDefaultDestinations;
    int16_t D0_shl2 = (int16_t)(D0 << 2);
    esi = A0;
    ebx = (uint16_t)D0_shl2;
    axS = (int16_t)swosReadWord(esi + ebx);
    D1 = axS;

    // 15061-15070 -- esi = A2 (ballSprite); D1 += ball.x +2; ball.destX = D1.
    esi = A2;
    axS = (int16_t)swosReadWord(esi + 32); // ball.x +2 (whole)
    D1 = (int16_t)(D1 + axS);
    swosWriteWord(esi + 58, (uint16_t)D1); // ball.destX

    // 15071-15083 -- D1 = ball.y +2; D1 += [A0 + ebx + 2]; ball.destY = D1.
    axS = (int16_t)swosReadWord(esi + 36); // ball.y +2 (whole)
    D1 = axS;
    esi = A0;
    axS = (int16_t)swosReadWord(esi + ebx + 2);
    D1 = (int16_t)(D1 + axS);
    esi = A2;
    swosWriteWord(esi + 60, (uint16_t)D1); // ball.destY

    // 15084-15090 -- D0 = team.playerNumber; if != 0 -> @@player_not_cpu.
    esi = A6;
    int16_t pNum = (int16_t)swosReadWord(esi + 4);
    if (pNum != 0) goto l_player_not_cpu;

    // 15092-15100 -- CPU path: ball.speed = sprite.speed.
    esi = A1;
    int16_t spSpeed = (int16_t)swosReadWord(esi + 44);
    D0 = spSpeed;
    esi = A2;
    swosWriteWord(esi + 44, (uint16_t)spSpeed); // ball.speed
    goto l_halve_player_speed;

l_player_not_cpu:;
    // 15102-15119 -- first pass: D0 = sprite.speed; D0 >>= 2; D0 += sprite.speed;
    // ball.speed = D0.
    esi = A1;
    spSpeed = (int16_t)swosReadWord(esi + 44);
    D0 = spSpeed;
    D0 = (int16_t)((uint16_t)D0 >> 2);
    spSpeed = (int16_t)swosReadWord(esi + 44);
    D0 = (int16_t)(D0 + spSpeed);
    esi = A2;
    swosWriteWord(esi + 44, (uint16_t)D0);

    // 15120-15136 -- second pass, same shape.
    esi = A1;
    spSpeed = (int16_t)swosReadWord(esi + 44);
    D0 = spSpeed;
    D0 = (int16_t)((uint16_t)D0 >> 2);
    spSpeed = (int16_t)swosReadWord(esi + 44);
    D0 = (int16_t)(D0 + spSpeed);
    esi = A2;
    swosWriteWord(esi + 44, (uint16_t)D0);

l_halve_player_speed:;
    // 15138-15146 -- sprite.speed >>= 1; sprite.tackleState = 1.
    esi = A1;
    {
        uint16_t src = swosReadWord(esi + 44);
        src = (uint16_t)(src >> 1);
        swosWriteWord(esi + 44, src);
    }
    swosWriteWord(esi + 96, 1); // Sprite.tackleState

    // 15147-15162 -- A0 = team.opponentsTeam; A0 = A0.controlledPlayer;
    // if A0 == 0 -> @@out.
    esi = A6;
    eax = swosReadSignedDword(esi + 0); // TeamGeneralInfo.opponentsTeam
    A0 = eax;
    esi = A0;
    eax = swosReadSignedDword(esi + 32); // TeamGeneralInfo.controlledPlayer
    A0 = eax;
    if (A0 == 0) goto l_out;

    // 15164-15175 -- cmp [A0+Sprite.ballDistance], 9 ; jb @@out (unsigned).
    esi = A0;
    {
        uint32_t src = swosReadDword(esi + 74);
        if (src < 9u) goto l_out;
    }

    {
    // 15177-15196 -- D0 = (sprite.x+2 - opp.x+2); D0 = (int16)D0 * (int16)D0.
    esi = A1;
    int16_t pxS = (int16_t)swosReadWord(esi + 32);
    D0 = pxS;
    esi = A0;
    int16_t oxS = (int16_t)swosReadWord(esi + 32);
    D0 = (int16_t)(D0 - oxS);
    int32_t dxSq = (int16_t)D0 * (int16_t)D0; // imul bx.
    int32_t D0_32 = dxSq;

    // 15197-15216 -- D1 = (sprite.y+2 - opp.y+2); D1 = (int16)D1 * (int16)D1.
    esi = A1;
    int16_t pyS = (int16_t)swosReadWord(esi + 36);
    D1 = pyS;
    esi = A0;
    int16_t oyS = (int16_t)swosReadWord(esi + 36);
    D1 = (int16_t)(D1 - oyS);
    int32_t dySq = (int16_t)D1 * (int16_t)D1;
    int32_t D1_32 = dySq;

    // 15217-15223 -- D0 += D1 (32-bit).
    D0_32 = D0_32 + D1_32;

    // 15224-15233 -- cmp D0, 32 ; jbe @@out (unsigned compare).
    if ((uint32_t)D0_32 <= 32u) goto l_out;

    // 15235-15237 -- PlayGoodTackleComment (omitted, audio); tackleState = TS_GOOD_TACKLE.
    esi = A1;
    swosWriteWord(esi + 96, PT_TS_GOOD_TACKLE);
    }

l_out:;
    // 15240-15241 -- PlayKickSample now wired to a real sound (see
    // swos_audio_events.h); resetBothTeamSpinTimers.
    swosAudioFireEvent(SWOS_AUDIO_EVENT_KICK);
    swosResetBothTeamSpinTimers();
}
