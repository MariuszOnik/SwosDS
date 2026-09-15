// SOURCE: openswos game/scripts/SwosVm/Tables.cs (full file)
// FIDELITY: VERIFIED_PC -- data extracted mechanically (tools/extract_table.py),
// not retyped by hand, to rule out transcription errors in ~1300 constants.
#pragma once

#include <stdint.h>

#include "generated/swos_tables_data.h"

// kSineCosineTable[angle] -- angle is 0..255 representing a full circle.
// Output is approx 32767 * sin(angle / 256 * 2*pi), SWOS fixed-point format.
// cosine(angle) = swos_sineCosineTable[(angle + 64) & 0xFF].
#define swosSine(angle)   (swos_sineCosineTable[(uint8_t)(angle)])
#define swosCosine(angle) (swos_sineCosineTable[(uint8_t)((angle) + 64)])

// swos_angleTangent[deltaY][deltaX] returns an angle 0..64 representing the
// direction (deltaX, deltaY) for deltaX >= deltaY. For deltaX < deltaY, the
// caller swaps and uses 64 - result. -1 = no movement (deltaX=deltaY=0).
