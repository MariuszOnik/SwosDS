// SOURCE: openswos game/scripts/Sim/Port/Stats.cs (full file, step 11 of
// the porting order -- real local dependency of GameLoop.cs).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
//
// DrawStatsIfNeeded is ALREADY a no-op in the C# source (`// TODO -- skip,
// Godot draws this`) -- ported as the same no-op, not omitted, matching
// the StubDrawMenuSprite precedent from step 10.
#pragma once

#include <stdbool.h>
#include <stdint.h>

// swos.h:212-221 -- TeamStatsData, 7 word fields.
#define STATS_OFF_BALL_POSSESSION 0
#define STATS_OFF_CORNERS_WON     2
#define STATS_OFF_FOULS_CONCEDED  4
#define STATS_OFF_BOOKINGS        6
#define STATS_OFF_SENDINGS_OFF    8
#define STATS_OFF_GOAL_ATTEMPTS   10
#define STATS_OFF_ON_TARGET       12

// stats.cpp:29-33.
void swosStatsInitStats(void);

// stats.cpp:35-44.
void swosStatsToggleStats(void);

// stats.cpp:46-51.
void swosStatsHideStats(void);

// stats.cpp:53-56.
bool swosStatsEnqueued(void);

// stats.cpp:58-67.
bool swosStatsShowingUserRequestedStats(void);

// stats.cpp:69-72.
bool swosStatsShowingPostGameStats(void);

// stats.cpp:74-112 -- main per-tick entry.
void swosStatsUpdateStatistics(void);

// stats.cpp:114-125 -- getStats, as an out-struct.
typedef struct {
    uint16_t ballPossession;
    uint16_t cornersWon;
    uint16_t foulsConceded;
    uint16_t bookings;
    uint16_t sendingsOff;
    uint16_t goalAttempts;
    uint16_t onTarget;
} SwosStatsTeamStats;

typedef struct {
    SwosStatsTeamStats team1;
    SwosStatsTeamStats team2;
} SwosStatsGameStats;

void swosStatsGetStats(SwosStatsGameStats *out);

// stats.cpp:134-138 -- pure UI no-op (see header note).
void swosStatsDrawStatsIfNeeded(void);
