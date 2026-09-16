// Minimal stand-in for Godot.GD, needed so PlayerUpdate.cs (which calls
// Godot.GD.Print("[PORT-SAFETY] ...") in TickGoalkeeperHoldAutoRelease) can
// compile here without a Godot engine reference. This is a debug log
// statement with zero Memory/game-state effect (confirmed by reading the
// call site) -- the C port omits it entirely (see swos_player_update.h) --
// so a no-op stand-in changes nothing observable on either side of the
// differential test.
namespace Godot;

public static class GD
{
    public static void Print(object? message) { }

    // Added for step 11 (GameSprites.cs's out-of-range-shirt diagnostic) --
    // same rationale as Print above: a debounced debug print, zero Memory
    // effect (confirmed by reading the call site; the C port omits the
    // print but keeps the debounce-state control flow -- see
    // swos_game_sprites.c).
    public static void PrintErr(object? message) { }
}
