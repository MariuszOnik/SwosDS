// PHASE 4 sprite laboratory: decodes BlocksDS's grit tool output (a
// palette-indexed bitmap packed 4 bytes/uint32 + a 16-bit BGR555 palette --
// the exact format nds-app's own GL2D loop consumes via glLoadSpriteSet)
// into a plain RGBA byte buffer, for SDL2 to turn into a texture. Desktop-
// only, no libnds/GL2D dependency -- this is a debug-tool concern, not
// part of the portable VM.
#pragma once

#include <stdint.h>

// bitmap: packed 8bpp indices, 4 per uint32 (grit's -gB8 -gx format).
// pal: 256 entries, BGR555 (bits 0-4 R, 5-9 G, 10-14 B), same convention
// nds-app's glLoadSpriteSet consumes. Index 0 is always treated as fully
// transparent (matches GL_TEXTURE_COLOR0_TRANSPARENT, the flag nds-app's
// main.c passes to glLoadSpriteSet -- see that file's glLoadSpriteSet calls).
// outRgba must be width*height*4 bytes.
void gritDecodeToRgba(const unsigned int *bitmap, const unsigned short *pal,
                       int width, int height, uint8_t *outRgba);
