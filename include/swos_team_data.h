// SOURCE OF IMPLEMENTATION/BEHAVIOUR: openswos game/scripts/SwosVm/TeamData.cs
// (full file) -- every offset/constant/semantic below is OpenSWOS's,
// unchanged. FIDELITY: VERIFIED_PC.
//
// swos-port used AUXILIARY-ONLY (see swos_ball_sprite.h for the ground rule
// on how far that goes -- read for understanding/discrepancy-reporting
// only, never to override OpenSWOS's translation). Every offset here was
// cross-checked against swos-port's packed `struct TeamGeneralInfo`
// (../swos-port/src/swos/swos.h:345-418, 145 bytes, #pragma pack(1)) and
// matches exactly. Two things noted, NEITHER changed here:
//   - OffOfs108 is OpenSWOS's placeholder name for an unidentified field;
//     swos-port has since named the same offset `ballDirectionChangeTimer`.
//     Cosmetic only (same offset, same semantics either way) -- kept as
//     OpenSWOS names it.
//   - swos-port's struct has a `wonTheBallTimer` field (offset 138, right
//     after OffAiBallSpinDirection/136) that TeamData.cs simply never
//     exposes as a named accessor. Confirmed (by grep) this is a gap in
//     TeamData.cs's semantic API ONLY, not in OpenSWOS's VM: PlayerActions.cs,
//     PlayerControlled.cs, UpdatePlayers.cs and Kickoff.cs all read/write
//     the raw `+ 138` offset directly (e.g. UpdatePlayers.cs:3620
//     `Memory.WriteWord(tackleOppBase + 138, 12)`), so the mechanic fully
//     works today in OpenSWOS -- just via a hardcoded offset instead of a
//     TeamData accessor at those call sites. When those files are ported
//     (steps 5-8), each site's local `+ 138` literal gets ported as
//     written, matching OpenSWOS's own inconsistency rather than
//     introducing a `swosTeamDataWonTheBallTimer()` accessor OpenSWOS
//     itself doesn't have.
//
// Per-team runtime data -- mirrors swos-port `swos.topTeamData` /
// `swos.bottomTeamData`. Full TeamGeneralInfo struct has 100+ fields; only
// the subset OpenSWOS's TeamData.cs itself exposes is ported here.
#pragma once

#include <stdbool.h>
#include <stdint.h>

// Allocate 512 bytes per team. swos-port TeamGeneralInfo is 145 bytes.
// Top and bottom team data live consecutively in memory.
#define TEAMDATA_TOP_BASE    0x4F900
#define TEAMDATA_BOTTOM_BASE 0x4FB00

// ---- Field offsets within TeamGeneralInfo --------------------------------
#define TEAMDATA_OFF_OPPONENTS_TEAM                 0    // dword -- absolute address of opponent's TeamData
#define TEAMDATA_OFF_PLAYER_NUMBER                  4    // word  -- set to 1/2 for human-controlled team
#define TEAMDATA_OFF_PLAYER_COACH_NUMBER            6    // word
#define TEAMDATA_OFF_IS_PL_COACH                    8    // word
#define TEAMDATA_OFF_IN_GAME_TEAM_PTR                10   // dword
#define TEAMDATA_OFF_TEAM_STATS_PTR                  14   // dword
#define TEAMDATA_OFF_TEAM_NUMBER                     18   // word
#define TEAMDATA_OFF_PLAYERS                         20   // dword -- pointer to per-team SpritesTable (11 x 4 bytes)
#define TEAMDATA_OFF_SHOT_CHANCE_TABLE                24   // dword -- pointer to per-team chance table
#define TEAMDATA_OFF_TACTICS                         28   // word
#define TEAMDATA_OFF_UPDATE_PLAYER_INDEX             30   // word
#define TEAMDATA_OFF_CONTROLLED_PLAYER               32   // dword -- pointer to controlled player Sprite
#define TEAMDATA_OFF_PASS_TO_PLAYER_PTR              36   // dword
#define TEAMDATA_OFF_PLAYER_HAS_BALL                 40   // word -- non-zero = this team's player controls ball
#define TEAMDATA_OFF_ALLOWED_DIRECTIONS              42   // word
#define TEAMDATA_OFF_CURRENT_ALLOWED_DIRECTION       44   // word -- LIVE held/joystick direction this frame (or -1 = no spin allowed)
#define TEAMDATA_OFF_CONTROLLED_PL_DIRECTION         56   // word -- direction recorded AT kick time (0..7)
#define TEAMDATA_OFF_DIRECTION                       46   // word
#define TEAMDATA_OFF_QUICK_FIRE                      48   // byte
#define TEAMDATA_OFF_NORMAL_FIRE                     49   // byte
#define TEAMDATA_OFF_FIRE_PRESSED                    50   // byte
#define TEAMDATA_OFF_FIRE_THIS_FRAME                 51   // byte
#define TEAMDATA_OFF_HEADER_OR_TACKLE                52   // word
#define TEAMDATA_OFF_FIRE_COUNTER                    54   // word
#define TEAMDATA_OFF_SHOOTING                        58   // word
#define TEAMDATA_OFF_GOALKEEPER_SAVED_COMMENT_TIMER  76   // word -- < 0 disables spin
#define TEAMDATA_OFF_LAST_HEADING_PLAYER             72   // dword
#define TEAMDATA_OFF_OFS78                           78   // word
#define TEAMDATA_OFF_GOALKEEPER_DIVING_RIGHT         80   // word -- set by goalkeeperJumping
#define TEAMDATA_OFF_GOALKEEPER_DIVING_LEFT          82   // word -- set by goalkeeperJumping
#define TEAMDATA_OFF_BALL_OUT_OF_PLAY_OR_KEEPER      84   // word -- set by goalkeeperClaimedTheBall
#define TEAMDATA_OFF_GOALIE_PLAYING_OR_OUT           86   // word
#define TEAMDATA_OFF_PASSING_BALL                    88   // word
#define TEAMDATA_OFF_PASSING_TO_PLAYER               90   // word
#define TEAMDATA_OFF_PLAYER_SWITCH_TIMER             92   // word
#define TEAMDATA_OFF_BALL_IN_PLAY                    94   // word
#define TEAMDATA_OFF_BALL_OUT_OF_PLAY                96   // word
#define TEAMDATA_OFF_BALL_X                          98   // word
#define TEAMDATA_OFF_BALL_Y                          100  // word
#define TEAMDATA_OFF_PASS_KICK_TIMER                 102  // word
#define TEAMDATA_OFF_PASSING_KICKING_PLAYER          104  // dword
#define TEAMDATA_OFF_OFS108                          108  // word -- see file header note (swos-port: ballDirectionChangeTimer)
#define TEAMDATA_OFF_BALL_CAN_BE_CONTROLLED          110  // word
#define TEAMDATA_OFF_BALL_CONTROLLING_PLAYER_DIRECTION 112 // word
// ball.cpp:2294 `readMemory(esi+118, 2)` -- spinTimer.
#define TEAMDATA_OFF_SPIN_TIMER                      118  // word -- -1 = no spin, else 0..10 ticks since touch
#define TEAMDATA_OFF_LEFT_SPIN                       120  // word -- 1 if left-spinning
#define TEAMDATA_OFF_RIGHT_SPIN                      122  // word -- 1 if right-spinning
#define TEAMDATA_OFF_LONG_PASS                       124  // word -- 1 if long pass active
#define TEAMDATA_OFF_LONG_SPIN_PASS                  126  // word -- 1 if long+spin pass
#define TEAMDATA_OFF_PASS_IN_PROGRESS                128  // word -- non-zero -> take passing path in applyBallAfterTouch
#define TEAMDATA_OFF_AI_TIMER                        130  // word
// updatePlayers.cpp:15980+ (AI_SetControlsDirection). field_84 -- chase/regroup counter.
#define TEAMDATA_OFF_AI_FIELD_84                     132  // word
// updatePlayers.cpp:18210/18223/18675 -- AI after-touch strength (0/1/2).
#define TEAMDATA_OFF_AI_AFTER_TOUCH_STRENGTH         134  // word
// updatePlayers.cpp:18431/18506/18823 -- AI ball-spin direction (-1/0/+1).
#define TEAMDATA_OFF_AI_BALL_SPIN_DIRECTION          136  // word
#define TEAMDATA_OFF_GOALKEEPER_PLAYING              140  // word
#define TEAMDATA_OFF_RESET_CONTROLS                  142  // word
#define TEAMDATA_OFF_SECONDARY_FIRE                  144  // byte (last field; total struct size = 145)

// updatePlayers.cpp:17289 -- plVeryCloseToBall byte flag (offset +61).
#define TEAMDATA_OFF_PL_VERY_CLOSE_TO_BALL           61   // byte
// updatePlayers.cpp:17297 -- plCloseToBall byte (offset +62).
#define TEAMDATA_OFF_PL_CLOSE_TO_BALL                62   // byte
// updatePlayers.cpp:2832 -- ball12To17 byte (offset +67): ball height in (12..17] px.
#define TEAMDATA_OFF_BALL_12_TO_17                   67   // byte
// updatePlayers.cpp:2809 -- ballAbove17 byte (offset +68): ball height > 17 px.
#define TEAMDATA_OFF_BALL_ABOVE_17                   68   // byte

// ---- Per-team accessors ---------------------------------------------------

int swosTeamDataBase(bool top);

int32_t swosTeamDataOpponentsTeam(bool top);

int32_t swosTeamDataControlledPlayer(bool top);
void swosTeamDataSetControlledPlayer(bool top, int32_t spritePtr);

// Read controlledPlayer pointer from a TeamData address (used when the port
// branches via lastTeamPlayedBeforeBreak -- pointer to either top or bottom
// TeamData).
int32_t swosTeamDataControlledPlayerFromBase(int teamDataBase);

int16_t swosTeamDataPlayerHasBall(bool top);
void swosTeamDataSetPlayerHasBall(bool top, int16_t v);

int16_t swosTeamDataCurrentAllowedDirection(bool top);
void swosTeamDataSetCurrentAllowedDirection(bool top, int16_t v);

int16_t swosTeamDataControlledPlDirection(bool top);
void swosTeamDataSetControlledPlDirection(bool top, int16_t v);

int16_t swosTeamDataGoalkeeperSavedCommentTimer(bool top);
void swosTeamDataSetGoalkeeperSavedCommentTimer(bool top, int16_t v);

int16_t swosTeamDataGetSpinTimer(bool top);
void swosTeamDataSetSpinTimer(bool top, int16_t v);

int16_t swosTeamDataLeftSpin(bool top);
void swosTeamDataSetLeftSpin(bool top, int16_t v);

int16_t swosTeamDataRightSpin(bool top);
void swosTeamDataSetRightSpin(bool top, int16_t v);

int16_t swosTeamDataLongPass(bool top);
void swosTeamDataSetLongPass(bool top, int16_t v);

int16_t swosTeamDataLongSpinPass(bool top);
void swosTeamDataSetLongSpinPass(bool top, int16_t v);

int16_t swosTeamDataPassInProgress(bool top);
void swosTeamDataSetPassInProgress(bool top, int16_t v);

// Goalkeeper diving state -- set by goalkeeperJumping (updatePlayers.cpp:10829+).
int16_t swosTeamDataGoalkeeperDivingRight(bool top);
void swosTeamDataSetGoalkeeperDivingRight(bool top, int16_t v);

int16_t swosTeamDataGoalkeeperDivingLeft(bool top);
void swosTeamDataSetGoalkeeperDivingLeft(bool top, int16_t v);

int16_t swosTeamDataBallOutOfPlayOrKeeper(bool top);
void swosTeamDataSetBallOutOfPlayOrKeeper(bool top, int16_t v);

// Pointer to per-team SpritesTable (11 x 4-byte sprite slot addresses).
// Initialised by swosTeamDataInit() to point at PLSPR_TEAM1_TABLE_BASE or
// PLSPR_TEAM2_TABLE_BASE.
int32_t swosTeamDataPlayersTable(bool top);
void swosTeamDataSetPlayersTable(bool top, int32_t v);

// Get the sprite slot address for slot index 0..10 of this team. Reads via
// the players table -- matches asm idiom `mov esi, [A1+i*4]`.
int32_t swosTeamDataGetTeamSpriteAddr(bool top, int slotInTeam);

// ---- Initialisation (called from Memory's future real Init()) -----------
// Sets up opponentsTeam cross-pointers + per-team sprite tables.
void swosTeamDataInit(void);
