// SOURCE: openswos game/scripts/Sim/Port/SpinningLogo.cs (full file, step
// 11 of the porting order -- real local dependency of GameLoop.cs).
// FIDELITY: VERIFIED_PC -- direct mechanical port, no logic changes.
#pragma once

#include <stdbool.h>

// sprites.h:70.
#define SL_BIG_S_SPRITE_START 1241

// spinningLogo.cpp:17-26 -- updateSpinningLogo.
void swosSpinningLogoUpdateSpinningLogo(void);

// spinningLogo.cpp:34-37.
bool swosSpinningLogoEnabled(void);

// spinningLogo.cpp:39-42.
void swosSpinningLogoSetEnabled(bool enabled);

// Renderer accessor -- per-tick picture index.
int swosSpinningLogoGetPictureIndex(void);
