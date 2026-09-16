// SOURCE: openswos game/scripts/Sim/Port/Camera.cs (full file, step 11 of
// the porting order -- real local dependency of GameLoop.cs).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
//
// GetCameraXWhole/GetCameraYWhole were already forward-pulled (step 7A/9)
// into swos_referee.c's own tiny static helpers; this header now becomes
// the canonical home and swos_referee.c's copies stay as-is (duplication
// is fine -- they're one-line wrappers, not logic to keep in sync).
#pragma once

#include <stdbool.h>
#include <stdint.h>

// camera.cpp:8-39 -- constants.
#define CAM_TRAINING_START_X 168
#define CAM_TRAINING_START_Y 313
#define CAM_TOP_START_LOCATION_Y    16
#define CAM_BOTTOM_START_LOCATION_Y 664
#define CAM_CENTER_X 176
#define CAM_PENALTY_SHOOTOUT_X 336
#define CAM_PENALTY_SHOOTOUT_Y 107
#define CAM_LEAVING_BENCH_DEST_X 211
#define CAM_BENCH_SLIDE_AREA_START_Y 339
#define CAM_BENCH_SLIDE_AREA_END_Y   359
#define CAM_PLAYERS_OUTSIDE_PITCH_X 590
#define CAM_TOP_GOAL_LINE 129
#define CAM_PITCH_MAX_X 352
#define CAM_PITCH_MIN_Y 16
#define CAM_PITCH_MAX_Y 664
#define CAM_TRAINING_PITCH_MIN_Y 80
#define CAM_TRAINING_PITCH_MAX_Y 616
#define CAM_PITCH_SIDE_LIMIT_DURING_BREAK 37
#define CAM_PITCH_SIDE_LIMIT_DURING_GAME  63
#define CAM_SUBSTITUTE_LIMIT 51
#define CAM_MIN_X 0
#define CAM_MAX_X CAM_PITCH_MAX_X
#define CAM_MIN_Y CAM_PITCH_MIN_Y
#define CAM_MAX_Y 680
#define CAM_PITCH_CENTER_X 336
#define CAM_PITCH_CENTER_Y 449
#define CAM_VGA_WIDTH  320
#define CAM_VGA_HEIGHT 200
#define CAM_LEAVING_BENCH_X_LIMIT 35

// camera.cpp:77-95.
int32_t swosCameraGetX(void);
int32_t swosCameraGetY(void);
int swosCameraGetXWhole(void);
int swosCameraGetYWhole(void);
void swosCameraSetX(int32_t q16_16);
void swosCameraSetY(int32_t q16_16);

// camera.cpp:97-119 -- per-tick mode select + coordinate update.
void swosCameraMoveCamera(void);

// camera.cpp:121-132.
void swosCameraSetToInitialPosition(void);

// camera.cpp:134-137.
void swosCameraSwitchToLeavingBenchMode(void);
