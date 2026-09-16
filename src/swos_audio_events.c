#include "swos_audio_events.h"

SwosAudioEventHook g_swosAudioEventHook = 0;

void swosAudioSetEventHook(SwosAudioEventHook hook) {
    g_swosAudioEventHook = hook;
}

void swosAudioFireEvent(SwosAudioEvent event) {
    if (g_swosAudioEventHook)
        g_swosAudioEventHook(event);
}
