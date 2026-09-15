// SOURCE OF IMPLEMENTATION/BEHAVIOUR: openswos game/scripts/SwosVm/PlayerSprite.cs
// (full file) -- every offset/constant/semantic below is OpenSWOS's,
// unchanged. FIDELITY: VERIFIED_PC.
//
// swos-port used AUXILIARY-ONLY (see swos_ball_sprite.h for the ground rule).
// Every field offset here was cross-checked against swos-port's packed
// `struct Sprite` (110 bytes) and matches exactly, including the same
// FrameIndicesTable dword-widening noted in swos_ball_sprite.h (both sprite
// kinds share the same underlying struct).
//
// 22 sprites in a contiguous pool starting at SpritePoolBase:
//   slot 0..10  -> team1 (slot 0 = goalie1, slots 1..10 = team1 outfielders)
//   slot 11..21 -> team2 (slot 11 = goalie2, slots 12..21 = team2 outfielders)
//
// Unlike ball_sprite, these accessors are parameterised by slot index --
// callers pass a slot to read/write a specific player, matching the asm
// idiom `mov esi, [team1SpritesTable + i*4]` (read a pointer-table entry,
// dereference). Our pool layout is contiguous, not pointer-table, but the
// per-slot access PATTERN is identical -- and the pointer tables themselves
// (Team1TableBase/Team2TableBase) still exist and get populated by Init(),
// because TeamData.OffPlayers points at them.
#pragma once

#include <stdbool.h>
#include <stdint.h>

// Pool sits after TeamData (which ends at 0x4FCFF). Aligned to 0x50000.
#define PLSPR_SPRITE_POOL_BASE 0x50000
#define PLSPR_SLOT_STRIDE      128   // padded from 110 for alignment
#define PLSPR_SPRITE_SIZE      110   // actual Sprite struct size

#define PLSPR_TEAM_SIZE   11         // 11 sprites per team (incl. keeper)
#define PLSPR_TOTAL_SLOTS (PLSPR_TEAM_SIZE * 2)  // 22 sprites total

#define PLSPR_SLOT_GOALIE1     0
#define PLSPR_SLOT_TEAM1_FIRST 1     // team1 outfielders 1..10 -> slots 1..10
#define PLSPR_SLOT_GOALIE2     11
#define PLSPR_SLOT_TEAM2_FIRST 12    // team2 outfielders 1..10 -> slots 12..21

// ---- Per-team sprite tables (pointer arrays) -----------------------------
// Each is 11 x 4-byte pointers to sprite slots (absolute Memory addresses).
// TeamData.OffPlayers (+20) should point at one of these -- Init() wires it.
#define PLSPR_TEAM1_TABLE_BASE 0x4FE00  // 11 x 4 = 44 bytes
#define PLSPR_TEAM2_TABLE_BASE 0x4FE2C  // 11 x 4 = 44 bytes

// ---- Slot addressing ------------------------------------------------------
// Caller must pass slot in [0, 22) -- no bounds check here beyond
// swos_memory's assert()-based address check (matches asm, where
// team1SpritesTable[12] would just trash memory).
int swosPlayerSpriteBase(int slot);

// top=true -> slots 0..10; top=false -> slots 11..21.
int swosPlayerSpriteFirstSlotForTeam(bool top);
int swosPlayerSpriteGoalieSlot(bool top);
bool swosPlayerSpriteIsGoalie(int slot);

// ---- Sprite struct field offsets (110-byte struct, pack=1) --------------
// Fields shared with ball_sprite use the same offsets.
#define PLSPR_OFF_TEAM_NUMBER          0   // word  -- 1/2 for player teams, 0=AI, 3=corner flag
#define PLSPR_OFF_PLAYER_ORDINAL       2   // word  -- 1=goalkeeper, 2..11=outfielders
#define PLSPR_OFF_FRAME_OFFSET         4   // word
#define PLSPR_OFF_ANIM_TABLE_PTR       6   // dword
#define PLSPR_OFF_STARTING_DIRECTION   10  // word
#define PLSPR_OFF_PLAYER_STATE         12  // byte -- PlayerState enum (Normal=0, GoalieCatchingBall=4, DivingHigh=6, DivingLow=7, Down=10, Claimed=11)
#define PLSPR_OFF_PLAYER_DOWN_TIMER    13  // byte -- counts down post-dive/catch animation
#define PLSPR_OFF_FRAME_INDICES_TABLE  18  // dword -- animation table (matches ball_sprite +18)
#define PLSPR_OFF_FRAME_INDEX          22  // word
#define PLSPR_OFF_FRAME_DELAY          24  // word
#define PLSPR_OFF_CYCLE_FRAMES_TIMER   26  // word
#define PLSPR_OFF_FRAME_SWITCH_COUNTER 28  // word
#define PLSPR_OFF_X                    30  // FixedPoint Q16.16
#define PLSPR_OFF_Y                    34
#define PLSPR_OFF_Z                    38
#define PLSPR_OFF_DIRECTION            42  // word -- 0..7 quantised
#define PLSPR_OFF_SPEED                44  // word Q8.8
#define PLSPR_OFF_DELTA_X              46
#define PLSPR_OFF_DELTA_Y              50
#define PLSPR_OFF_DELTA_Z              54
#define PLSPR_OFF_DEST_X               58  // word whole pixel
#define PLSPR_OFF_DEST_Y               60
#define PLSPR_OFF_VISIBLE              68  // word -- hide/show
#define PLSPR_OFF_IMAGE_INDEX          70  // word -- <0 = no image
#define PLSPR_OFF_SAVE_SPRITE          72  // word
#define PLSPR_OFF_BALL_DISTANCE        74  // dword -- PLAYER-SPECIFIC, squared dist to ball
#define PLSPR_OFF_FULL_DIRECTION       82  // word -- 0..255
#define PLSPR_OFF_ON_SCREEN            84  // word
#define PLSPR_OFF_PLAYER_DIRECTION     92  // word -- -1 for non-players
#define PLSPR_OFF_IS_MOVING            94  // word
#define PLSPR_OFF_TACKLE_STATE         96  // word
#define PLSPR_OFF_DEST_REACHED_STATE   100 // word -- 0..3
#define PLSPR_OFF_CARDS                102 // word
#define PLSPR_OFF_INJURY_LEVEL         104 // word
#define PLSPR_OFF_TACKLING_TIMER       106 // word
#define PLSPR_OFF_SENT_AWAY            108 // word

// ---- OpenSWOS extension: in-match energy/fatigue (NOT in original SWOS) --
// Stored in the 18 bytes of slot padding (110..127) that follow the
// 110-byte Sprite struct, so they live in the deterministic Memory pool and
// are NOT cleared by swosMemoryInitStub's future real Init() (which only
// zeroes bytes 0..SpriteSize-1 per slot -- see swosPlayerSpriteInit()
// below). Seeded per match by PlayerEnergy.SeedSlot (not ported yet).
#define PLSPR_OFF_ENERGY     110 // word -- current energy 0..4096
#define PLSPR_OFF_ENERGY_ACC 112 // word -- drain sub-unit accumulator
#define PLSPR_OFF_STAMINA    114 // byte -- 0..7 quantised career stamina

// ---- Per-slot accessors ---------------------------------------------------
// All reads/writes are bounds-unchecked beyond swos_memory's assert() --
// caller's responsibility to pass valid slot indices, matching the asm.

int32_t swosPlayerSpriteX(int slot);
void swosPlayerSpriteSetX(int slot, int32_t v);
int32_t swosPlayerSpriteY(int slot);
void swosPlayerSpriteSetY(int slot, int32_t v);
int32_t swosPlayerSpriteZ(int slot);
void swosPlayerSpriteSetZ(int slot, int32_t v);

int16_t swosPlayerSpriteXPixels(int slot);
void swosPlayerSpriteSetXPixels(int slot, int16_t v);
int16_t swosPlayerSpriteYPixels(int slot);
void swosPlayerSpriteSetYPixels(int slot, int16_t v);
int16_t swosPlayerSpriteZPixels(int slot);
void swosPlayerSpriteSetZPixels(int slot, int16_t v);

int32_t swosPlayerSpriteDeltaX(int slot);
void swosPlayerSpriteSetDeltaX(int slot, int32_t v);
int32_t swosPlayerSpriteDeltaY(int slot);
void swosPlayerSpriteSetDeltaY(int slot, int32_t v);
int32_t swosPlayerSpriteDeltaZ(int slot);
void swosPlayerSpriteSetDeltaZ(int slot, int32_t v);

int16_t swosPlayerSpriteDirection(int slot);
void swosPlayerSpriteSetDirection(int slot, int16_t v);
int16_t swosPlayerSpriteSpeed(int slot);
void swosPlayerSpriteSetSpeed(int slot, int16_t v);
int16_t swosPlayerSpriteDestX(int slot);
void swosPlayerSpriteSetDestX(int slot, int16_t v);
int16_t swosPlayerSpriteDestY(int slot);
void swosPlayerSpriteSetDestY(int slot, int16_t v);

int16_t swosPlayerSpriteTeamNumber(int slot);
void swosPlayerSpriteSetTeamNumber(int slot, int16_t v);
int16_t swosPlayerSpritePlayerOrdinal(int slot);
void swosPlayerSpriteSetPlayerOrdinal(int slot, int16_t v);

uint8_t swosPlayerSpritePlayerState(int slot);
void swosPlayerSpriteSetPlayerState(int slot, uint8_t v);
int8_t swosPlayerSpritePlayerDownTimer(int slot);
void swosPlayerSpriteSetPlayerDownTimer(int slot, int8_t v);

int16_t swosPlayerSpriteImageIndex(int slot);
void swosPlayerSpriteSetImageIndex(int slot, int16_t v);

int32_t swosPlayerSpriteBallDistance(int slot);
void swosPlayerSpriteSetBallDistance(int slot, int32_t v);

int16_t swosPlayerSpriteFullDirection(int slot);
void swosPlayerSpriteSetFullDirection(int slot, int16_t v);

int16_t swosPlayerSpriteTackleState(int slot);
void swosPlayerSpriteSetTackleState(int slot, int16_t v);
int16_t swosPlayerSpriteTacklingTimer(int slot);
void swosPlayerSpriteSetTacklingTimer(int slot, int16_t v);

// ---- Initialisation --------------------------------------------------------
// Populates the per-team pointer tables and clears all sprite slots. Called
// from Memory's future real Init() (not wired in yet -- see swos_memory.h).
void swosPlayerSpriteInit(void);
