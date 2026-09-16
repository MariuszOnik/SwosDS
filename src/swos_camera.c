// SOURCE: openswos game/scripts/Sim/Port/Camera.cs (see swos_camera.h).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
#include "swos_camera.h"

#include <stdint.h>

#include "swos_addr.h"
#include "swos_ball_sprite.h"
#include "swos_bench.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"
#include "swos_referee.h"
#include "swos_rng.h"
#include "swos_team_data.h"

// camera.cpp:351-353.
#define CAM_VELOCITY_INCREMENT 2
#define CAM_MAX_VELOCITY       40
// camera.cpp:273.
#define CAM_MAX_MOVEMENT 5

// camera.cpp:9 (bench.cpp).
#define CAM_BENCH_X 27

// camera.cpp:316-320 helper constants.
#define CAM_TOP_GOAL_Y    129
#define CAM_BOTTOM_GOAL_Y 778

// GameState enum values used by StandardMode.
#define CAM_ST_IN_PROGRESS 100
#define CAM_ST_CORNER_LEFT  4
#define CAM_ST_CORNER_RIGHT 5
#define CAM_ST_THROW_IN_FORWARD_RIGHT 15
#define CAM_ST_THROW_IN_BACK_LEFT     20
#define CAM_ST_STARTING_GAME 21
#define CAM_ST_CAMERA_GOING_TO_SHOWERS 22
#define CAM_ST_GOING_TO_HALFTIME 23
#define CAM_ST_PLAYERS_GOING_TO_SHOWER 24
#define CAM_ST_RESULT_ON_HALFTIME 25
#define CAM_ST_RESULT_AFTER_THE_GAME 26
#define CAM_ST_FIRST_HALF_ENDED 29
#define CAM_ST_GAME_ENDED 30

typedef struct {
    int xDest, yDest, xLimit, xVelocity, yVelocity;
} SwosCameraParams;

static SwosCameraParams makeParams(int xDest, int yDest, int xLimit, int xVel, int yVel)
{
    SwosCameraParams p = { xDest, yDest, xLimit, xVel, yVel };
    return p;
}
static SwosCameraParams makeParams2(int xDest, int yDest) { return makeParams(xDest, yDest, 0, 0, 0); }
static SwosCameraParams makeParams3(int xDest, int yDest, int xLimit) { return makeParams(xDest, yDest, xLimit, 0, 0); }

int32_t swosCameraGetX(void) { return swosReadSignedDword(ADDR_cameraX); }
int32_t swosCameraGetY(void) { return swosReadSignedDword(ADDR_cameraY); }
int swosCameraGetXWhole(void) { return swosCameraGetX() >> 16; }
int swosCameraGetYWhole(void) { return swosCameraGetY() >> 16; }
void swosCameraSetX(int32_t q16_16) { swosWriteDword(ADDR_cameraX, (uint32_t)q16_16); }
void swosCameraSetY(int32_t q16_16) { swosWriteDword(ADDR_cameraY, (uint32_t)q16_16); }

static int swosRand(void) { return swosRngNextByte(); }

static bool goalsNotVisible(void)
{
    int camY = swosCameraGetYWhole();
    const int kHalfVgaHeight = 100;
    return (camY - kHalfVgaHeight > CAM_TOP_GOAL_Y) && (camY + kHalfVgaHeight < CAM_BOTTOM_GOAL_Y);
}

static int getBenchCameraXLimit(void)
{
    int limit = CAM_PITCH_SIDE_LIMIT_DURING_BREAK;
    int camYWhole = swosCameraGetYWhole();
    bool cameraAtBenchLevel = camYWhole >= CAM_BENCH_SLIDE_AREA_START_Y && camYWhole <= CAM_BENCH_SLIDE_AREA_END_Y;

    if (cameraAtBenchLevel && goalsNotVisible())
    {
        int16_t substitute = swosReadSignedWord(ADDR_g_substituteInProgress);
        limit = substitute != 0 ? CAM_SUBSTITUTE_LIMIT : CAM_MIN_X;
    }
    return limit;
}

static int benchCameraX(void) { return CAM_BENCH_X; }

static SwosCameraParams bookingPlayerMode(void)
{
    int32_t bookedPlayer = swosReadSignedDword(ADDR_bookedPlayer);
    int x = bookedPlayer == 0 ? CAM_PITCH_CENTER_X
                               : swosReadSignedWord(bookedPlayer + PLSPR_OFF_X + 2);
    int y = bookedPlayer == 0 ? CAM_PITCH_CENTER_Y
                               : swosReadSignedWord(bookedPlayer + PLSPR_OFF_Y + 2);
    return makeParams3(x, y, CAM_PITCH_SIDE_LIMIT_DURING_BREAK);
}

static SwosCameraParams penaltyShootoutMode(void)
{
    return makeParams2(CAM_PENALTY_SHOOTOUT_X, CAM_PENALTY_SHOOTOUT_Y);
}

static SwosCameraParams benchMode(bool substitutingPlayer)
{
    int limit = substitutingPlayer ? CAM_SUBSTITUTE_LIMIT : getBenchCameraXLimit();
    int x = benchCameraX();
    return makeParams3(x, CAM_PITCH_CENTER_Y, limit);
}

static SwosCameraParams leavingBenchMode(void)
{
    return makeParams3(CAM_LEAVING_BENCH_DEST_X, CAM_PITCH_CENTER_Y, CAM_PITCH_SIDE_LIMIT_DURING_BREAK);
}

static const int8_t kNextCameraDirections[16] = {
    0, -1, 1, -1, 1, 0, 1, 1, 0, 1, -1, 1, -1, 0, -1, -1,
};

static void getGameStoppedCameraDirections(int *xDirection, int *yDirection)
{
    *xDirection = 0; *yDirection = 0;

    int direction;
    bool gotPlayerDirection = false;

    int32_t lastTeam = swosReadSignedDword(ADDR_lastTeamPlayedBeforeBreak);
    int32_t controlled = lastTeam == 0 ? 0 : swosReadSignedDword(lastTeam + TEAMDATA_OFF_CONTROLLED_PLAYER);

    if (lastTeam != 0 && controlled != 0)
    {
        direction = swosReadSignedWord(controlled + PLSPR_OFF_DIRECTION);
        gotPlayerDirection = true;
    }
    else
    {
        direction = swosReadSignedWord(ADDR_cameraDirection);
    }

    if (gotPlayerDirection || direction != -1)
    {
        *xDirection = kNextCameraDirections[2 * direction];
        *yDirection = kNextCameraDirections[2 * direction + 1];
    }
}

static void getStandardModeCameraVelocity(int xDirection, int yDirection, int *xVelocity, int *yVelocity)
{
    *xVelocity = swosReadSignedWord(ADDR_cameraXVelocity);
    *yVelocity = swosReadSignedWord(ADDR_cameraYVelocity);

    if (xDirection < 0 && *xVelocity != -CAM_MAX_VELOCITY)
        *xVelocity -= CAM_VELOCITY_INCREMENT;
    else if (xDirection > 0 && *xVelocity != CAM_MAX_VELOCITY)
        *xVelocity += CAM_VELOCITY_INCREMENT;

    if (yDirection < 0 && *yVelocity != -CAM_MAX_VELOCITY)
        *yVelocity -= CAM_VELOCITY_INCREMENT;
    else if (yDirection > 0 && *yVelocity != CAM_MAX_VELOCITY)
        *yVelocity += CAM_VELOCITY_INCREMENT;
}

static SwosCameraParams waitingForPlayersToLeaveCameraLocation(int limit)
{ return makeParams3(CAM_PLAYERS_OUTSIDE_PITCH_X, CAM_PITCH_CENTER_Y, limit); }
static SwosCameraParams showResultAtCenter(int limit)
{ return makeParams3(CAM_PITCH_CENTER_X, CAM_PITCH_CENTER_Y, limit); }
static SwosCameraParams showResultAtTop(int limit)
{ return makeParams3(CAM_PITCH_CENTER_X, CAM_TOP_GOAL_LINE, limit); }
static SwosCameraParams followTheBall(int limit, int xVelocity, int yVelocity)
{
    int bx = swosReadSignedWord(BALLSPR_BASE + BALLSPR_OFF_X + 2);
    int by = swosReadSignedWord(BALLSPR_BASE + BALLSPR_OFF_Y + 2);
    return makeParams(bx, by, limit, xVelocity, yVelocity);
}

static SwosCameraParams standardMode(void)
{
    int xDirection, yDirection;
    int16_t gameStatePl = swosReadSignedWord(ADDR_gameStatePl);

    if (gameStatePl == CAM_ST_IN_PROGRESS)
    {
        xDirection = swosReadSignedDword(BALLSPR_BASE + BALLSPR_OFF_DELTA_X);
        yDirection = swosReadSignedDword(BALLSPR_BASE + BALLSPR_OFF_DELTA_Y);
    }
    else
    {
        getGameStoppedCameraDirections(&xDirection, &yDirection);
    }

    int xVelocity, yVelocity;
    getStandardModeCameraVelocity(xDirection, yDirection, &xVelocity, &yVelocity);

    int limit = CAM_PITCH_SIDE_LIMIT_DURING_GAME;

    if (gameStatePl != CAM_ST_IN_PROGRESS)
    {
        int16_t gameState = swosReadSignedWord(ADDR_gameState);
        bool cornerOrThrowIn =
            gameState == CAM_ST_CORNER_LEFT || gameState == CAM_ST_CORNER_RIGHT ||
            (gameState >= CAM_ST_THROW_IN_FORWARD_RIGHT && gameState <= CAM_ST_THROW_IN_BACK_LEFT);
        if (cornerOrThrowIn)
            limit = CAM_PITCH_SIDE_LIMIT_DURING_BREAK;
    }

    int16_t gs = swosReadSignedWord(ADDR_gameState);
    if (gs >= CAM_ST_STARTING_GAME && gs <= CAM_ST_GAME_ENDED)
    {
        if (gs == CAM_ST_STARTING_GAME || gs == CAM_ST_CAMERA_GOING_TO_SHOWERS ||
            gs == CAM_ST_GOING_TO_HALFTIME || gs == CAM_ST_PLAYERS_GOING_TO_SHOWER)
        {
            return waitingForPlayersToLeaveCameraLocation(limit);
        }
        if (gs == CAM_ST_RESULT_AFTER_THE_GAME)
        {
            return showResultAtTop(limit);
        }
        if (gs == CAM_ST_GAME_ENDED)
        {
            // camera.cpp:194-199 -- if penaltiesState < 0, ShowResultAtCenter;
            // otherwise fall through to followTheBall.
            int16_t penaltiesState = swosReadSignedWord(ADDR_penaltiesState);
            if (penaltiesState < 0)
                return showResultAtCenter(limit);
        }
        else if (gs != CAM_ST_FIRST_HALF_ENDED)
        {
            // camera.cpp:201-202 -- ST_FIRST_HALF_ENDED falls through to
            // followTheBall; every other remaining value here (25, 27, 28)
            // is the switch's `default:` -> ShowResultAtCenter.
            return showResultAtCenter(limit);
        }
    }

    return followTheBall(limit, xVelocity, yVelocity);
}

static void updateCameraLeaving(void)
{
    bool leaving = swosReadSignedWord(ADDR_leavingBenchMode) != 0;
    if (leaving)
    {
        int camXWhole = swosCameraGetXWhole();
        bool benchVisibleByX = camXWhole < CAM_LEAVING_BENCH_X_LIMIT;
        swosWriteWord(ADDR_leavingBenchMode, benchVisibleByX ? 1 : 0);
    }
}

static void clipCameraDestination(int *xDest, int *yDest, int xLimit)
{
    int xLimitQ = xLimit << 16;
    if (*xDest < xLimitQ) *xDest = xLimitQ;

    int maxXQ = (CAM_PITCH_MAX_X - xLimit) << 16;
    if (*xDest > maxXQ) *xDest = maxXQ;

    bool training = swosReadSignedWord(ADDR_g_trainingGame) != 0;
    int minY = (training ? CAM_TRAINING_PITCH_MIN_Y : CAM_PITCH_MIN_Y) << 16;
    int maxY = (training ? CAM_TRAINING_PITCH_MAX_Y : CAM_PITCH_MAX_Y) << 16;

    if (*yDest < minY) *yDest = minY;
    if (*yDest > maxY) *yDest = maxY;
}

static void clipCameraMovement(int *deltaX, int *deltaY)
{
    int maxQ = CAM_MAX_MOVEMENT << 16;
    int minQ = -maxQ;
    if (*deltaX > maxQ) *deltaX = maxQ;
    if (*deltaX < minQ) *deltaX = minQ;
    if (*deltaY > maxQ) *deltaY = maxQ;
    if (*deltaY < minQ) *deltaY = minQ;
}

static void constrainCameraToPitch(int *cameraX, int *cameraY)
{
    int minX = CAM_MIN_X << 16, maxX = CAM_MAX_X << 16;
    int minY = CAM_MIN_Y << 16, maxY = CAM_MAX_Y << 16;
    if (*cameraX < minX) *cameraX = minX;
    if (*cameraX > maxX) *cameraX = maxX;
    if (*cameraY < minY) *cameraY = minY;
    if (*cameraY > maxY) *cameraY = maxY;
}

static void updateCameraCoordinates(SwosCameraParams ps)
{
    swosWriteWord(ADDR_cameraXVelocity, (uint16_t)ps.xVelocity);
    swosWriteWord(ADDR_cameraYVelocity, (uint16_t)ps.yVelocity);

    int xDest = (ps.xDest - CAM_VGA_WIDTH  / 2) << 16;
    int yDest = (ps.yDest - CAM_VGA_HEIGHT / 2) << 16;
    xDest += ps.xVelocity << 16;
    yDest += ps.yVelocity << 16;

    clipCameraDestination(&xDest, &yDest, ps.xLimit);

    int cameraX = swosCameraGetX();
    int cameraY = swosCameraGetY();

    int deltaX = (xDest - cameraX) >> 4;
    int deltaY = (yDest - cameraY) >> 4;

    clipCameraMovement(&deltaX, &deltaY);

    cameraX += deltaX;
    cameraY += deltaY;

    constrainCameraToPitch(&cameraX, &cameraY);
    swosCameraSetX(cameraX);
    swosCameraSetY(cameraY);
}

void swosCameraSetToInitialPosition(void)
{
    int startX, startY;
    if (swosReadSignedWord(ADDR_g_trainingGame) != 0)
    {
        startX = CAM_TRAINING_START_X;
        startY = CAM_TRAINING_START_Y;
    }
    else
    {
        startX = CAM_CENTER_X;
        startY = (swosRand() & 1) != 0 ? CAM_BOTTOM_START_LOCATION_Y : CAM_TOP_START_LOCATION_Y;
    }

    swosCameraSetX(startX << 16);
    swosCameraSetY(startY << 16);
}

void swosCameraSwitchToLeavingBenchMode(void)
{
    swosWriteWord(ADDR_leavingBenchMode, 1);
}

void swosCameraMoveCamera(void)
{
    swosWriteWord(ADDR_cameraCoordinatesValid, 1);

    if (swosCameraGetX() == 0 && swosCameraGetY() == 0)
        swosCameraSetToInitialPosition();

    if (swosReadSignedWord(ADDR_showFansCounter) != 0)
        return;

    SwosCameraParams ps;

    if (swosRefereeCardHandingInProgress())
        ps = bookingPlayerMode();
    else if (swosReadWord(ADDR_playingPenalties) != 0)
        ps = penaltyShootoutMode();
    else if (swosReadSignedWord(ADDR_g_waitForPlayerToGoInTimer) != 0)
        ps = benchMode(true);
    else if (swosReadSignedWord(ADDR_leavingBenchMode) != 0)
        ps = leavingBenchMode();
    else if (swosBenchInBench())
        ps = benchMode(false);
    else
        ps = standardMode();

    updateCameraCoordinates(ps);
    updateCameraLeaving();
}
