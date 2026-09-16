#include "grit_texture.h"

static uint8_t scale5to8(unsigned v) {
    return (uint8_t)((v * 255 + 15) / 31);
}

void gritDecodeToRgba(const unsigned int *bitmap, const unsigned short *pal,
                       int width, int height, uint8_t *outRgba) {
    const uint8_t *indices = (const uint8_t *)bitmap;
    for (int i = 0; i < width * height; i++) {
        uint8_t idx = indices[i];
        uint8_t *px = &outRgba[i * 4];
        if (idx == 0) {
            px[0] = px[1] = px[2] = px[3] = 0;
            continue;
        }
        unsigned short c = pal[idx];
        px[0] = scale5to8(c & 0x1F);
        px[1] = scale5to8((c >> 5) & 0x1F);
        px[2] = scale5to8((c >> 10) & 0x1F);
        px[3] = 255;
    }
}
