// Optional per-tick CSV logging for the SDL diagnostic frontend. Reads VM
// state through the same public accessors main.c's renderer uses -- no
// gameplay logic here, this is purely an observer. Off by default (must be
// enabled with --log or the L key) so a normal run isn't slowed down or
// left with a stray file.
#pragma once

#include <stdbool.h>
#include <stdint.h>

void simLogOpen(const char *path);
void simLogClose(void);
bool simLogIsOpen(void);

// Appends one CSV record for the given tick, reading current VM state
// directly. `eventText` (may be NULL/empty) is appended as the last column
// -- gameState/gameStatePl changes, kickoff start, controlled-player
// changes, ball state transitions (see main.c's event-edge detection).
void simLogWriteTick(uint64_t tick, const char *eventText);
