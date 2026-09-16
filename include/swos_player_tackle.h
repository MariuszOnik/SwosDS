// Minimal real slice of PlayerTackle.cs required by PlayerControlled.cs
// (step 6A: PlayerBeginTackling) and UpdatePlayers.cs (step 7A: everything
// else -- PlayerTacklingTestFoul/PlayersTackledTheBallStrong plus their
// entire executed call chain: TestFoulForPenaltyAndFreeKick,
// TryBookingThePlayer, TrySendingOffThePlayer, PlayerTackled. That covers
// ALL of PlayerTackle.cs except its audio-stub wrappers (omitted,
// documented at each call site) and SwosRand (already available as
// swosRngNextByte()).
#pragma once

void swosPlayerBeginTackling(int playerSpriteAddr, int teamDataAddr,
                             int direction);

// updatePlayers.cpp:12708-13394. Checks whether a tackle draws a foul
// (proximity + tackle-state/direction gates), and on a foul: bumps
// foulsConceded, rolls for a card (yellow/red) via TryBookingThePlayer/
// TrySendingOffThePlayer, and always tail-calls TestFoulForPenaltyAndFreeKick.
void swosPlayerTacklingTestFoul(int aPlayerSprite, int aTeamData);

// updatePlayers.cpp:14960-15242. Player is tackling and hits the ball:
// adjusts ball direction/destination/speed, halves the tackler's speed,
// and (if the opponent's controlled player is far enough away) marks a
// TS_GOOD_TACKLE. NOTE: a near-duplicate of the already-ported
// PlayerActions.PlayerTackledTheBallStrong (player.cpp:1684-1966) -- see
// the .c file for why both exist (different ball-speed scaling, this one
// routes through the raw ball-sprite address like the asm did).
void swosPlayersTackledTheBallStrong(int aPlayerSprite, int aTeamData);

