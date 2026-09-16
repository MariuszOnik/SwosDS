// PHASE 3. See swos_render_frames.h for the full scope note.
#include "swos_render_frames.h"

#include <stdio.h>

#include "generated/swos_render_frames_data.h"

bool swosRenderFramesLookup(int32_t globalImageIndex, SwosRenderFrameInfo *out) {
    if (globalImageIndex < 0 || globalImageIndex >= SWOS_RENDER_FRAME_COUNT ||
        !RENDER_FRAMES[globalImageIndex].valid) {
        printf("MISSING IMAGE: global index %d has no known sprite mapping\n", (int)globalImageIndex);
        return false;
    }
    *out = RENDER_FRAMES[globalImageIndex];
    return true;
}

int swosRenderFramesUsedIndexCount(void) {
    return RENDER_FRAMES_USED_COUNT;
}

int32_t swosRenderFramesUsedIndexAt(int i) {
    return RENDER_FRAMES_USED_INDICES[i];
}
