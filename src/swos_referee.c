// SOURCE: openswos game/scripts/Sim/Port/Referee.cs (see swos_referee.h
// for the exact slice ported / omitted).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
#include "swos_referee.h"
#include "swos_addr.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"
#include "swos_rng.h"
#include "swos_sprite_update.h"
#include "swos_team_data.h"
#include "swos_util.h"

#include <stdint.h>

// pitchConstants.h:3.
#define REF_PITCH_CENTER_X 336
#define REF_PITCH_CENTER_Y 449

#define REF_SPEED 1024

// referee.cpp:15-16.
#define REF_LEAVING_TOP_DEST_Y    129
#define REF_LEAVING_BOTTOM_DEST_Y 770

// referee.cpp:18-19.
#define REF_SENT_OFF_PLAYER_X (-20)
#define REF_SENT_OFF_PLAYER_Y 449

// referee.cpp:21.
#define REF_PLAYER_NUMBER_OFFSET 20

// swos.h:139 -- Direction enum.
#define REF_FACING_TOP  0
#define REF_FACING_LEFT 6

// Sprite.h:29 -- PlayerState::kBooked.
#define REF_PLAYER_STATE_BOOKED 12

// GameState::kInProgress (swos.h:592).
#define REF_GAME_STATE_IN_PROGRESS 100

// sprites.h:64 -- kSmallDigit1, base sprite index for player-number 1.
#define REF_SMALL_DIGIT1 1188

// referee.cpp:107-110 -- player-number blink table (30 bytes). Indices 0-11
// alternate 0/9 (sprite frame off/on); 12-28 are 0 (hidden); entry 29 is -1
// (sentinel = leave).
static const int8_t kPlayerNumberBlinkTable[30] = {
    0, 9, 0, 9, 0, 9, 0, 9, 0, 9, 0, 9, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1,
};

// camera.cpp:82, referee.cpp:60 -- Camera.GetCameraYWhole(). Minimal
// forward-pull (not the rest of Camera.cs, 573 lines, not called from
// anything ported so far): read cameraY (Q16.16) and take the whole-pixel
// part via an arithmetic shift (swosAsr32, not a plain >>, since cameraY
// is a signed dword and C11 leaves >> on a negative signed value
// implementation-defined -- see swos_util.h).
static int cameraGetYWhole(void) {
    int32_t cameraY = swosReadSignedDword(ADDR_cameraY);
    return swosAsr32(cameraY, 16);
}

// camera.cpp:78-81, swos_referee.c's UpdateRefereeOnScreenFlag (referee.cpp
// via swos.asm:100209-100221). Same forward-pull as cameraGetYWhole, X axis.
static int cameraGetXWhole(void) {
    int32_t cameraX = swosReadSignedDword(ADDR_cameraX);
    return swosAsr32(cameraX, 16);
}

// referee.cpp:277-286 -- initRefereeAnimationTable.
static void initRefereeAnimationTable(int animTableAddr) {
    int16_t delay = swosReadSignedWord(animTableAddr);
    int16_t direction = swosReadSignedWord(REFSPR_BASE + PLSPR_OFF_DIRECTION);

    int frameTablePtr = swosReadSignedDword(animTableAddr + 2 + direction * 4);

    swosWriteWord(REFSPR_BASE + PLSPR_OFF_FRAME_DELAY, (uint16_t)delay);
    swosWriteDword(REFSPR_BASE + PLSPR_OFF_FRAME_INDICES_TABLE, (uint32_t)frameTablePtr);

    swosWriteWord(REFSPR_BASE + PLSPR_OFF_FRAME_SWITCH_COUNTER, (uint16_t)-1);
    swosWriteWord(REFSPR_BASE + PLSPR_OFF_FRAME_INDEX, (uint16_t)-1);
    swosWriteWord(REFSPR_BASE + PLSPR_OFF_CYCLE_FRAMES_TIMER, 1);
}

// gameSprites.cpp:101-111 -- markDisplaySpritesDirty (see Referee.cs's own
// comment: tells the host renderer the sprite set changed).
static void markDisplaySpritesDirty(void) {
    swosWriteWord(ADDR_displaySpritesDirtyFlag, 1);
}

// referee.cpp:50-75 -- activateReferee.
void swosRefereeActivate(void) {
    // Telemetry omitted (DbgActivations/DbgYellowCards/DbgRedCards/
    // DbgSecondYellowCards -- zero Memory effect, see header).

    int16_t foulX = swosReadSignedWord(ADDR_foulXCoordinate);
    int16_t foulY = swosReadSignedWord(ADDR_foulYCoordinate);

    swosWriteWord(REFSPR_BASE + PLSPR_OFF_DEST_X, (uint16_t)(foulX + 28));
    swosWriteWord(REFSPR_BASE + PLSPR_OFF_DEST_Y, (uint16_t)(foulY + 5));

    // referee.cpp:55-58 -- random horizontal offset for starting position.
    int xOffset = swosRngNextByte() / 8;
    if (foulX >= REF_PITCH_CENTER_X)
        xOffset = -xOffset;

    // referee.cpp:60-64 -- starting Y depends on which half the foul was on.
    int cameraY = cameraGetYWhole();
    int refStartY = cameraY - 20;
    if (foulY <= REF_PITCH_CENTER_Y)
        refStartY = cameraY + 215;

    int destX = swosReadSignedWord(REFSPR_BASE + PLSPR_OFF_DEST_X);
    swosWriteWord(REFSPR_BASE + PLSPR_OFF_X + 2, (uint16_t)(destX + xOffset));
    swosWriteWord(REFSPR_BASE + PLSPR_OFF_Y + 2, (uint16_t)refStartY);

    swosWriteWord(REFSPR_BASE + PLSPR_OFF_SPEED, REF_SPEED);
    swosWriteWord(REFSPR_BASE + PLSPR_OFF_VISIBLE, 1);

    markDisplaySpritesDirty();
    initRefereeAnimationTable(ADDR_refComingAnimTable);

    swosWriteWord(ADDR_refState, REF_ST_INCOMING);
    // Telemetry omitted (DbgEnteredIncoming -- zero Memory effect, see header).
}

// referee.cpp:77-80 -- refereeActive.
bool swosRefereeActive(void) {
    return swosReadSignedWord(ADDR_refState) != REF_ST_OFF_SCREEN;
}

// referee.cpp:82-85 -- cardHandingInProgress.
bool swosRefereeCardHandingInProgress(void) {
    return swosReadSignedWord(ADDR_whichCard) != REF_CARD_NONE;
}

// ---- Read-only render accessors (task #181) --------------------------------
int  swosRefereeState(void)     { return swosReadSignedWord(ADDR_refState); }
int  swosRefereeWhichCard(void) { return swosReadSignedWord(ADDR_whichCard); }
bool swosRefereeVisible(void)   { return swosReadSignedWord(REFSPR_BASE + PLSPR_OFF_VISIBLE) != 0; }
int  swosRefereeImageIndex(void){ return swosReadSignedWord(REFSPR_BASE + PLSPR_OFF_IMAGE_INDEX); }
int  swosRefereeWorldX(void)    { return swosAsr32(swosReadSignedDword(REFSPR_BASE + PLSPR_OFF_X), 16); }
int  swosRefereeWorldY(void)    { return swosAsr32(swosReadSignedDword(REFSPR_BASE + PLSPR_OFF_Y), 16); }
int  swosRefereeWorldZ(void)    { return swosAsr32(swosReadSignedDword(REFSPR_BASE + PLSPR_OFF_Z), 16); }

// ====================================================================
// Referee sprite onScreen maintenance -- DrawSprites clip test
// (swos.asm:100200-100317, DrawSprites @@sprites_loop body). See
// Referee.cs's own comment for the full rationale (no DrawSprites render
// pass in this port -- the host renderer draws -- so onScreen would
// otherwise freeze at its init value of 1 and deadlock the state machine).
// ====================================================================
#define REF_SPR_CENTER_X  8   // SpriteGraphics.centerX (nominal)
#define REF_SPR_CENTER_Y  16  // SpriteGraphics.centerY (nominal)
#define REF_SPR_PIX_WIDTH 16  // SpriteGraphics.pixWidth (nominal)
#define REF_SPR_NLINES    32  // SpriteGraphics.nlines (nominal)

static void updateRefereeOnScreenFlag(void) {
    int16_t x = swosReadSignedWord(REFSPR_BASE + PLSPR_OFF_X + 2);
    int16_t y = swosReadSignedWord(REFSPR_BASE + PLSPR_OFF_Y + 2);
    int16_t z = swosReadSignedWord(REFSPR_BASE + PLSPR_OFF_Z + 2);

    // swos.asm:100209-100221 -- D1/D2 camera-relative top-left corner.
    int d1 = x - REF_SPR_CENTER_X - cameraGetXWhole();
    int d2 = y - REF_SPR_CENTER_Y - cameraGetYWhole() - z;

    // swos.asm:100223-100241 -- the four clip comparisons (jge/jle @@clipped).
    bool clipped = d1 >= 336 || d2 >= 200
                || d1 <= -REF_SPR_PIX_WIDTH || d2 <= -REF_SPR_NLINES;

    // swos.asm:100260 (drawn) / :100315 (clipped).
    swosWriteWord(REFSPR_BASE + PLSPR_OFF_ON_SCREEN, clipped ? 0 : 1);
}

// Forward declarations of the state-machine helpers, so updateReferee and
// updateRefereeState can appear in the same order as the C# file.
static void updateRefereeState(void);
static void putRefereeToLeavingState(void);
static void sendPlayerAway(void);
static void stubEnqueueRedCardSample(void) { /* audio, omitted -- see header */ }
static void stubEnqueueYellowCardSample(void) { /* audio, omitted -- see header */ }

// referee.cpp:87-99 -- updateReferee. Main per-tick entry point.
void swosRefereeUpdateReferee(void) {
    if (!(swosRefereeActive() && swosReadSignedWord(REFSPR_BASE + PLSPR_OFF_VISIBLE) != 0))
        return;

    // Port-side stand-in for the DrawSprites per-frame onScreen update.
    updateRefereeOnScreenFlag();

    int16_t onScreen = swosReadSignedWord(REFSPR_BASE + PLSPR_OFF_ON_SCREEN);
    if (onScreen != 0) {
        // referee.cpp:91-92 -- already on screen: animate + state machine.
        swosUpdateSpriteAnimation(REFSPR_BASE);
        updateRefereeState();
    } else {
        int16_t gameStatePl = swosReadSignedWord(ADDR_gameStatePl);
        if (gameStatePl == REF_GAME_STATE_IN_PROGRESS) {
            // referee.cpp:94 -- game in progress, walking on/off -- move sprite.
            swosMoveSprite(REFSPR_BASE);
        } else {
            // referee.cpp:96 -- game stopped, advance state machine only.
            updateRefereeState();
        }
    }
}

// Reads players[index].shirtNumber from a TeamGame (in-game team) struct.
// PlayerInfo struct (swos.h:162) has shirtNumber at offset +3. TeamGame
// struct layout (swos.h:296-315): players[] starts at +42, 61 bytes per
// PlayerInfo.
#define REF_TEAMGAME_OFF_PLAYERS      42
#define REF_PLAYERINFO_SIZE           61
#define REF_PLAYERINFO_OFF_SHIRT_NUM  3
static int getTeamGamePlayerShirtNumber(int teamGameAddr, int slotInTeam) {
    if (teamGameAddr == 0) return 1;
    return swosReadByte(teamGameAddr + REF_TEAMGAME_OFF_PLAYERS
                         + slotInTeam * REF_PLAYERINFO_SIZE + REF_PLAYERINFO_OFF_SHIRT_NUM);
}

// TeamGame.markedPlayer (signed word at offset +20). Layout from
// swos.h:296-315 -- 10 word fields (prShirtType..secSocksCol) precede
// markedPlayer at byte offset 20.
#define REF_TEAMGAME_OFF_MARKED_PLAYER 20
static int getTeamGameMarkedPlayer(int teamGameAddr) {
    return teamGameAddr == 0 ? -1 : swosReadSignedWord(teamGameAddr + REF_TEAMGAME_OFF_MARKED_PLAYER);
}
static void setTeamGameMarkedPlayer(int teamGameAddr, int v) {
    if (teamGameAddr != 0)
        swosWriteWord(teamGameAddr + REF_TEAMGAME_OFF_MARKED_PLAYER, (uint16_t)v);
}

// referee.cpp:101-151 -- updateBookedPlayerNumberSprite.
void swosRefereeUpdateBookedPlayerNumberSprite(void) {
    // referee.cpp:103 -- clear the previous frame's image.
    swosWriteWord(BKPLSPR_BASE + PLSPR_OFF_IMAGE_INDEX, (uint16_t)-1);

    int16_t whichCard = swosReadSignedWord(ADDR_whichCard);
    int16_t refState  = swosReadSignedWord(ADDR_refState);
    if (whichCard == 0 || refState != REF_ST_BOOKING) return;

    int32_t bookedPlayer = swosReadSignedDword(ADDR_bookedPlayer);
    if (bookedPlayer == 0) return;

    // referee.cpp:106 -- only blink while booked-state animation is running.
    uint8_t plState = swosReadByte(bookedPlayer + PLSPR_OFF_PLAYER_STATE);
    if (plState != REF_PLAYER_STATE_BOOKED) return;

    // referee.cpp:111 -- refTimer += lastFrameTicks.
    int16_t timer = (int16_t)(swosReadSignedWord(ADDR_refTimer) + swosReadSignedWord(ADDR_lastFrameTicks));
    swosWriteWord(ADDR_refTimer, (uint16_t)timer);

    // referee.cpp:113-115 -- index = refTimer >> 3 (one frame per 8 ticks).
    int index = swosAsr32(timer, 3);
    if (index < 0 || index >= 30) return;
    int action = kPlayerNumberBlinkTable[index];

    if (action > 0) {
        // referee.cpp:127-139 -- paint the player-number digit over the head.
        int32_t lastTeamBooked = swosReadSignedDword(ADDR_lastTeamBooked);
        if (lastTeamBooked == 0) return;

        int32_t teamGame = swosReadSignedDword(lastTeamBooked + TEAMDATA_OFF_IN_GAME_TEAM_PTR);
        int16_t playerOrdinal = swosReadSignedWord(bookedPlayer + PLSPR_OFF_PLAYER_ORDINAL);
        int shirtNumber = getTeamGamePlayerShirtNumber(teamGame, playerOrdinal - 1);
        int imageIndex = REF_SMALL_DIGIT1 + shirtNumber - 1;

        swosWriteWord(BKPLSPR_BASE + PLSPR_OFF_IMAGE_INDEX, (uint16_t)imageIndex);

        // referee.cpp:135-138 -- copy booked player position to the digit sprite.
        int32_t bpX = swosReadSignedDword(bookedPlayer + PLSPR_OFF_X);
        int32_t bpY = swosReadSignedDword(bookedPlayer + PLSPR_OFF_Y);
        swosWriteDword(BKPLSPR_BASE + PLSPR_OFF_X, (uint32_t)bpX);
        swosWriteDword(BKPLSPR_BASE + PLSPR_OFF_Y, (uint32_t)bpY);

        // referee.cpp:139 -- z = kPlayerNumberOffset (digit floats above head).
        swosWriteDword(BKPLSPR_BASE + PLSPR_OFF_Z, (uint32_t)(REF_PLAYER_NUMBER_OFFSET << 16));
    } else if (action < 0) {
        // referee.cpp:140-148 -- sentinel: transition to leaving + red-card off.
        putRefereeToLeavingState();

        // referee.cpp:143-144 -- kRedCard bit (also matches kSecondYellowCard=3).
        if ((whichCard & REF_CARD_RED) != 0)
            sendPlayerAway();

        swosWriteWord(ADDR_whichCard, 0);
        swosWriteDword(ADDR_bookedPlayer, 0);
    }
}

// referee.cpp:163-185 -- removeReferee.
void swosRefereeRemoveReferee(void) {
    swosWriteWord(ADDR_refState, REF_ST_OFF_SCREEN);
    swosWriteWord(REFSPR_BASE + PLSPR_OFF_VISIBLE, 0);

    // referee.cpp:163-185 -- reset to kRefereeHidingPlaceX/Y (276, 439).
    swosWriteDword(REFSPR_BASE + PLSPR_OFF_X, (uint32_t)(276 << 16));
    swosWriteDword(REFSPR_BASE + PLSPR_OFF_Y, (uint32_t)(439 << 16));

    swosWriteDword(REFSPR_BASE + PLSPR_OFF_Z, 0);
    swosWriteWord(REFSPR_BASE + PLSPR_OFF_DEST_X, 276);
    swosWriteWord(REFSPR_BASE + PLSPR_OFF_DEST_Y, 439);
    swosWriteWord(REFSPR_BASE + PLSPR_OFF_SPEED, 0);
    swosWriteByte(REFSPR_BASE + PLSPR_OFF_PLAYER_DOWN_TIMER, 0);
    swosWriteWord(REFSPR_BASE + PLSPR_OFF_FRAME_INDEX, (uint16_t)-1);
    swosWriteWord(REFSPR_BASE + PLSPR_OFF_CYCLE_FRAMES_TIMER, 1);
    // clearImage().
    swosWriteWord(REFSPR_BASE + PLSPR_OFF_IMAGE_INDEX, (uint16_t)-1);
    swosWriteWord(REFSPR_BASE + PLSPR_OFF_DIRECTION, REF_FACING_TOP);
    swosWriteWord(REFSPR_BASE + PLSPR_OFF_ON_SCREEN, 1);

    initRefereeAnimationTable(ADDR_refWaitingAnimTable);
}

// referee.cpp:192-245 -- updateRefereeState.
static void updateRefereeState(void) {
    int16_t refState = swosReadSignedWord(ADDR_refState);
    int16_t whichCard = swosReadSignedWord(ADDR_whichCard);

    switch (refState) {
        case REF_ST_ABOUT_TO_GIVE_CARD:
            // referee.cpp:195-213.
            swosWriteWord(ADDR_refState, REF_ST_BOOKING);
            switch (whichCard) {
                case REF_CARD_RED:
                    initRefereeAnimationTable(ADDR_refRedCardAnimTable);
                    stubEnqueueRedCardSample();
                    break;
                case REF_CARD_YELLOW:
                    initRefereeAnimationTable(ADDR_refYellowCardAnimTable);
                    stubEnqueueYellowCardSample();
                    break;
                case REF_CARD_SECOND_YELLOW:
                    initRefereeAnimationTable(ADDR_refSecondYellowAnimTable);
                    stubEnqueueRedCardSample();
                    break;
                default:
                    break;
            }
            break;

        case REF_ST_LEAVING:
            // referee.cpp:215-221 -- off-screen check; either transition off
            // or fall through to the shared Incoming/Leaving movement path.
            if (swosReadSignedWord(REFSPR_BASE + PLSPR_OFF_ON_SCREEN) == 0) {
                swosWriteWord(ADDR_refState, REF_ST_OFF_SCREEN);
                swosWriteWord(REFSPR_BASE + PLSPR_OFF_VISIBLE, 0);
                markDisplaySpritesDirty();
                break;
            }
            // referee.cpp:222 -- fall-through (C# `goto case kRefIncoming`).
            /* fallthrough */

        case REF_ST_INCOMING: {
            // referee.cpp:224-242 -- recompute direction, move, transition on arrival.
            swosWriteWord(REFSPR_BASE + PLSPR_OFF_SPEED, REF_SPEED);
            int16_t oldDirection = swosReadSignedWord(REFSPR_BASE + PLSPR_OFF_DIRECTION);

            swosUpdateSpriteDirectionAndDeltas(REFSPR_BASE);

            int16_t newDirection = swosReadSignedWord(REFSPR_BASE + PLSPR_OFF_DIRECTION);
            if (oldDirection != newDirection)
                initRefereeAnimationTable(ADDR_refComingAnimTable);

            // referee.cpp:236 -- move toward player and stop on arrival.
            swosMoveSprite(REFSPR_BASE);

            // referee.cpp:238 -- stationary() check.
            int32_t dx = swosReadSignedDword(REFSPR_BASE + PLSPR_OFF_DELTA_X);
            int32_t dy = swosReadSignedDword(REFSPR_BASE + PLSPR_OFF_DELTA_Y);
            if (dx == 0 && dy == 0) {
                swosWriteWord(ADDR_refState, REF_ST_WAITING_PLAYER);
                swosWriteWord(REFSPR_BASE + PLSPR_OFF_DIRECTION, REF_FACING_LEFT);
                initRefereeAnimationTable(ADDR_refWaitingAnimTable);
            }
            break;
        }

        default:
            break;
    }
}

// referee.cpp:247-258 -- putRefereeToLeavingState.
static void putRefereeToLeavingState(void) {
    // referee.cpp:249 -- rand() / 4 - 32 gives a value in [-32, 31].
    int xOffset = (swosRngNextByte() / 4) - 32;

    int16_t refX = swosReadSignedWord(REFSPR_BASE + PLSPR_OFF_X + 2);
    swosWriteWord(REFSPR_BASE + PLSPR_OFF_DEST_X, (uint16_t)(refX + xOffset));

    int16_t foulY = swosReadSignedWord(ADDR_foulYCoordinate);
    int destY = foulY > REF_PITCH_CENTER_Y ? REF_LEAVING_TOP_DEST_Y : REF_LEAVING_BOTTOM_DEST_Y;
    swosWriteWord(REFSPR_BASE + PLSPR_OFF_DEST_Y, (uint16_t)destY);

    initRefereeAnimationTable(ADDR_refComingAnimTable);

    swosWriteWord(ADDR_refState, REF_ST_LEAVING);
}

// referee.cpp:260-273 -- sendPlayerAway.
static void sendPlayerAway(void) {
    int32_t bookedPlayer = swosReadSignedDword(ADDR_bookedPlayer);
    int32_t lastTeamBooked = swosReadSignedDword(ADDR_lastTeamBooked);
    if (bookedPlayer == 0 || lastTeamBooked == 0) return;

    int32_t teamGame = swosReadSignedDword(lastTeamBooked + TEAMDATA_OFF_IN_GAME_TEAM_PTR);
    int16_t playerOrdinal = swosReadSignedWord(bookedPlayer + PLSPR_OFF_PLAYER_ORDINAL);

    // referee.cpp:266-267 -- clear markedPlayer if it was this player.
    int markedPlayer = getTeamGameMarkedPlayer(teamGame);
    if (markedPlayer == playerOrdinal - 1)
        setTeamGameMarkedPlayer(teamGame, -1);

    // referee.cpp:269-272.
    swosWriteWord(bookedPlayer + PLSPR_OFF_CARDS, (uint16_t)-1);
    swosWriteWord(bookedPlayer + PLSPR_OFF_SENT_AWAY, 1);
    swosWriteWord(bookedPlayer + PLSPR_OFF_DEST_X, (uint16_t)REF_SENT_OFF_PLAYER_X);
    swosWriteWord(bookedPlayer + PLSPR_OFF_DEST_Y, REF_SENT_OFF_PLAYER_Y);
}
