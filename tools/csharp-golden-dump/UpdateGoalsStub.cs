// Stand-in for OpenSwos.Sim.Port.UpdateGoals, matching the EXACT scope
// decision made for its C port (swos-vm-c/src/swos_update_goals.c): the
// real UpdateGoals.GoalScored ends with an unconditional call to
// Result.RegisterScorer, which needs Result.cs (407 lines) ->
// GameTime.GameTimeAsBcd() (a later porting step, GameTime.cs) plus C#
// arrays that live entirely outside the emulated Memory buffer (the
// results-screen scorer list). None of that affects match-simulation state
// -- it's the exact "PORT_PENDING, terminal UI/stats side effect" call the
// user approved deferring for both the C port and this harness (2026-09-16).
//
// Every line below OTHER than the omitted RegisterScorer call is a
// VERBATIM copy of the real
// openswos/game/scripts/Sim/Port/UpdateGoals.cs:46-162 (BumpTeamGoals +
// GoalScored) -- the same source swos_update_goals.c independently ports
// to C with an identical omission (a NULL-defaulting hook there, simply
// not called here). Delete this file and use the real UpdateGoals.cs (plus
// Result.cs/GameTime.cs) once those are ported/verified.
namespace OpenSwos.Sim.Port;

using OpenSwos.SwosVm;

public static class UpdateGoals
{
    private const int kMaxGoals = 99;

    public enum GoalType
    {
        kRegular = 0,
        kPenalty = 1,
        kOwnGoal = 2,
    }

    public static bool BumpTeamGoals(int teamNum)
    {
        bool isSecondTeam = teamNum == 2;
        bool playingPenalties = Memory.ReadWord(Memory.Addr.playingPenalties) != 0;

        int totalAddr = isSecondTeam ? Memory.Addr.team2TotalGoals : Memory.Addr.team1TotalGoals;
        if (Memory.ReadWord(totalAddr) == kMaxGoals)
            return false;

        int bumpAddr;
        if (playingPenalties)
            bumpAddr = isSecondTeam ? Memory.Addr.team2PenaltyGoals : Memory.Addr.team1PenaltyGoals;
        else
            bumpAddr = isSecondTeam ? Memory.Addr.team2TotalGoals : Memory.Addr.team1TotalGoals;

        Memory.WriteWord(bumpAddr, Memory.ReadWord(bumpAddr) + 1);

        int digit1Addr = isSecondTeam ? Memory.Addr.team2GoalsDigit1 : Memory.Addr.team1GoalsDigit1;
        int digit2Addr = isSecondTeam ? Memory.Addr.team2GoalsDigit2 : Memory.Addr.team1GoalsDigit2;
        int statsAddr = isSecondTeam ? Memory.Addr.statsTeam2Goals : Memory.Addr.statsTeam1Goals;

        int d2 = Memory.ReadWord(digit2Addr) + 1;
        if (d2 == 10)
        {
            Memory.WriteWord(digit1Addr, Memory.ReadWord(digit1Addr) + 1);
            d2 = 0;
        }
        Memory.WriteWord(digit2Addr, d2);

        Memory.WriteWord(statsAddr, Memory.ReadWord(statsAddr) + 1);

        return true;
    }

    public static void GoalScored(int teamNum, int scorerSlot)
    {
        Memory.WriteWord(Memory.Addr.goalScored, 1);
        Memory.WriteWord(Memory.Addr.runSlower, 1);
        Memory.WriteWord(Memory.Addr.lastTeamScoredNumber, teamNum);
        Memory.WriteDword(Memory.Addr.lastPlayerScored, PlayerSprite.Base(scorerSlot));
        Memory.WriteDword(Memory.Addr.currentScorer, PlayerSprite.Base(scorerSlot));

        short scorerTeamNum = PlayerSprite.TeamNumber(scorerSlot);
        int scorerTeamGame = scorerTeamNum == 1
            ? Memory.ReadSignedDword(Memory.Addr.topTeamInGame)
            : Memory.ReadSignedDword(Memory.Addr.bottomTeamInGame);

        int topInGamePtr = Memory.ReadSignedDword(TeamData.TopBase + TeamData.OffInGameTeamPtr);
        int teamPtr = topInGamePtr == scorerTeamGame ? TeamData.TopBase : TeamData.BottomBase;

        Memory.WriteDword(Memory.Addr.lastTeamScored, teamPtr);

        bool playingPenalties = Memory.ReadWord(Memory.Addr.playingPenalties) != 0;
        if (!BumpTeamGoals(teamNum) || playingPenalties)
            return;

        GoalType goalType = GoalType.kRegular;
        Memory.WriteDword(Memory.Addr.goalTypeScored, (int)GoalType.kRegular);

        if (scorerTeamNum != teamNum)
        {
            goalType = GoalType.kOwnGoal;
            Memory.WriteDword(Memory.Addr.goalTypeScored, (int)GoalType.kOwnGoal);
        }
        else if (Memory.ReadWord(Memory.Addr.penalty) != 0)
        {
            goalType = GoalType.kPenalty;
        }
        _ = goalType;

        // PORT_PENDING(Result.RegisterScorer) -- see file header. Omitted,
        // not called with approximated behavior, matching swos_update_goals.c.
    }
}
