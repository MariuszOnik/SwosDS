// Exact dependency slices required to compile and exercise the real
// PlayerControlled.cs during step 6A. AI entry points deliberately throw:
// step 6A scenarios are human-team paths; step 9 replaces these with AiBrain.
namespace OpenSwos.Sim.Port;

using OpenSwos.SwosVm;

public static class AiBrain
{
    public static void SetControlsDirection(int teamBase) =>
        throw new InvalidOperationException("AiBrain belongs to step 9");
}

public static class AiHelpers
{
    public static void AI_Kick(int spriteAddr, int teamBase) =>
        throw new InvalidOperationException("AiHelpers belongs to step 9");
}

public static class PlayerHeader
{
    private static int SlotFromAddr(int addr) =>
        (addr - PlayerSprite.SpritePoolBase) / PlayerSprite.SlotStride;

    private static short Clamp(int value, int lo, int hi) =>
        (short)Math.Clamp(value, lo, hi);

    public static void PlayerAttemptingJumpHeader(int spriteAddr, int direction)
    {
        Memory.WriteWord(spriteAddr + 98, 0);
        Memory.WriteWord(spriteAddr + PlayerSprite.OffDirection, direction);
        PlayerActions.SetPlayerAnimationTable(spriteAddr,
            Memory.Addr.kJumpHeaderAttemptAnimTableAddr);
        Memory.WriteByte(spriteAddr + PlayerSprite.OffPlayerDownTimer,
            Memory.ReadSignedWord(Memory.Addr.m_playerDownHeadingInterval));
        Memory.WriteByte(spriteAddr + PlayerSprite.OffPlayerState, 2);
        int dst = Memory.Addr.kDefaultDestinations + (direction << 2);
        int slot = SlotFromAddr(spriteAddr);
        Memory.WriteWord(spriteAddr + PlayerSprite.OffDestX,
            Clamp(PlayerSprite.XPixels(slot) + Memory.ReadSignedWord(dst), 81, 590));
        Memory.WriteWord(spriteAddr + PlayerSprite.OffDestY,
            Clamp(PlayerSprite.YPixels(slot) + Memory.ReadSignedWord(dst + 2), 129, 769));
        Memory.WriteWord(spriteAddr + PlayerSprite.OffSpeed,
            Memory.ReadWord(Memory.Addr.kJumpHeaderSpeed));
    }

    public static void AttemptStaticHeader(int spriteAddr, int direction)
    {
        Memory.WriteWord(spriteAddr + 98, 0);
        Memory.WriteWord(spriteAddr + PlayerSprite.OffDirection, direction);
        int dst = Memory.Addr.kDefaultDestinations + (direction << 2);
        int slot = SlotFromAddr(spriteAddr);
        Memory.WriteWord(spriteAddr + PlayerSprite.OffDestX,
            Clamp(PlayerSprite.XPixels(slot) + Memory.ReadSignedWord(dst), 81, 590));
        Memory.WriteWord(spriteAddr + PlayerSprite.OffDestY,
            Clamp(PlayerSprite.YPixels(slot) + Memory.ReadSignedWord(dst + 2), 129, 769));
        Memory.WriteWord(spriteAddr + PlayerSprite.OffSpeed,
            Memory.ReadWord(Memory.Addr.kStaticHeaderPlayerSpeed));
        PlayerActions.SetPlayerAnimationTable(spriteAddr,
            Memory.Addr.kStaticHeaderAttemptAnimTableAddr);
        Memory.WriteByte(spriteAddr + PlayerSprite.OffPlayerState, 8);
        Memory.WriteByte(spriteAddr + PlayerSprite.OffPlayerDownTimer, 20);
    }
}

public static class PlayerTackle
{
    public static void PlayerBeginTackling(int player, int team, int direction)
    {
        Memory.WriteWord(player + 96, 0);
        Memory.WriteWord(team + 56, direction);
        Memory.WriteWord(player + 42, direction);
        PlayerActions.SetPlayerAnimationTable(player, Memory.Addr.kPlTacklingAnimTableAddr);
        Memory.WriteByte(player + 12, 1);
        Memory.WriteByte(player + 13,
            Memory.ReadSignedWord(Memory.Addr.m_playerDownTacklingInterval));
        int ordinal = Memory.ReadSignedWord(player + 2);
        int offset = Memory.ReadSignedWord(Memory.Addr.inGameTeamPlayerOffsets
                                           + (short)(ordinal - 1) * 2);
        int header = Memory.ReadSignedDword(team + 10) - 42 + (ushort)(short)offset;
        if (Memory.ReadByte(header + 50) != 0) Memory.WriteByte(player + 13, 25);
        Memory.WriteByte(player + 13, -1);
        int dst = Memory.Addr.kDefaultDestinations + ((short)direction << 2);
        int vx = Memory.ReadSignedWord(dst), vy = Memory.ReadSignedWord(dst + 2);
        int px = Memory.ReadSignedWord(player + 32), py = Memory.ReadSignedWord(player + 36);
        int travel = 1000;
        if (vx != 0) { int a = vx > 0 ? 590 - px : px - 81; if (a < 0) a = 0; if (a < travel) travel = a; }
        if (vy != 0) { int a = vy > 0 ? 769 - py : py - 129; if (a < 0) a = 0; if (a < travel) travel = a; }
        Memory.WriteWord(player + 58, (short)(px + (vx > 0 ? travel : vx < 0 ? -travel : 0)));
        Memory.WriteWord(player + 60, (short)(py + (vy > 0 ? travel : vy < 0 ? -travel : 0)));
        Memory.WriteWord(player + 44, Memory.ReadWord(Memory.Addr.kPlayerTacklingSpeed));
        Memory.WriteWord(player + 106, 0);
    }
}
