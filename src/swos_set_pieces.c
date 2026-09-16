#include "swos_set_pieces.h"

#include <assert.h>
#include <stddef.h>

SwosSetThrowInPlayerDestHook g_swosSetThrowInPlayerDestHook;
SwosTickThrowInHook g_swosTickThrowInHook;

void swosSetPiecesSetThrowInPlayerDestinationCoordinates(int spriteAddr)
{
    assert(g_swosSetThrowInPlayerDestHook != NULL &&
           "SetPieces.SetThrowInPlayerDestinationCoordinates requires step 10");
    if (g_swosSetThrowInPlayerDestHook)
        g_swosSetThrowInPlayerDestHook(spriteAddr);
}

void swosSetPiecesTickThrowIn(int throwerSpriteAddr, int ballSpriteAddr,
                               int teamBase)
{
    assert(g_swosTickThrowInHook != NULL &&
           "SetPieces.TickThrowIn requires step 10");
    if (g_swosTickThrowInHook)
        g_swosTickThrowInHook(throwerSpriteAddr, ballSpriteAddr, teamBase);
}
