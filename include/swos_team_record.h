// PORT-ONLY data-transport type -- NOT a mechanical port of any single
// OpenSWOS file, and NOT a Memory-mapped structure (it exists only to be
// consumed by swosTeamDataLoaderWritePlayerInfos()/...WireTeamFields()
// before anything is written to Memory).
//
// Mirrors the FIELDS of OpenSwos.Assets.TeamRecord/PlayerRecord
// (../../openswos/game/scripts/Assets/TeamRecord.cs) that
// TeamDataLoader.WritePlayerInfos/WireTeamFields and SkillScaling.cs's
// ComputePlayerPrice actually read (comment-filtered-grep verified against
// both files) -- not the full C# type, which also carries fields (Nation,
// GlobalId, Coach, kit bytes, ...) those functions never touch.
//
// ONE deliberate representation change, not a logic change: `Position` is
// stored here as the already-mapped PlayerPosition enum value (0..7, see
// swos_team_data_loader.h's TDL_POS_* -- to be added alongside this file)
// instead of a display string ("G"/"RB"/.../"A"). The C# TeamRecord carries
// a string because it comes straight out of a text-based team-file
// decoder; TeamDataLoader.MapPositionString()/SkillScaling's own call to it
// exist purely to turn that string back into the same enum value before
// using it. Skipping the string round-trip for a struct that is 100%
// synthetic input in this repo (no team-file parser is ported here) avoids
// pulling in C string-matching for zero behavioural difference -- the enum
// values below are copied verbatim from TeamDataLoader.cs's own Pos* consts.
#pragma once

#include <stdint.h>

#define TDL_POS_GOALKEEPER  0
#define TDL_POS_RIGHT_BACK  1
#define TDL_POS_LEFT_BACK   2
#define TDL_POS_DEFENDER    3
#define TDL_POS_RIGHT_WING  4
#define TDL_POS_LEFT_WING   5
#define TDL_POS_MIDFIELDER  6
#define TDL_POS_ATTACKER    7

typedef struct {
    uint8_t shirtNumber;
    const char *name;      // ASCII, NUL-terminated; truncated to 14/22 chars on write, same as the C# port
    int position;           // TDL_POS_* -- see header note above
    int passing, shooting, heading, tackling, control, speed, finishing; // 0..7 raw file nibbles
    int valueCode;           // 0..47ish price CODE (TEAM.* byte +0x20); read as a SIGNED byte downstream
    int stamina;             // 0..7, career stat; non-career records pass 7 (full)
    int fatigueCarry;        // 0..100, carried between-match fatigue; non-career records pass 0
    int injurySeverity;      // 0..7, carried "knock"; non-career records pass 0
} SwosPlayerRecord;

typedef struct {
    const char *name;                  // ASCII, NUL-terminated; truncated to 17 chars on write
    uint8_t tactics;                    // team FILE tactics byte -- indexes swosTacticsLoaderBuiltinTactics() for the price calc (SkillScaling.ComputePlayerPrice), separate from TeamData.OffTactics (the IN-GAME tactics slot WireTeamFields sets)
    const SwosPlayerRecord *players;    // roster, in FILE order (not in-game slot order)
    int playerCount;                    // 1..16
    const uint8_t *lineupOrder;         // NULL, or 16 bytes mapping in-game slot -> roster index (TeamRecord.LineupOrder); NULL means identity (in-game slot i <- roster index i)
} SwosTeamRecord;
