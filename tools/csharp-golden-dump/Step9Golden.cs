// Generates the step-9 differential-test golden dumps: AiHelpers.cs (the
// small AI helpers) and AiBrain.SetControlsDirection (the largest single AI
// function in SWOS, ~3333 LOC of original asm), plus a few scenarios that
// exercise the CPU-team branches in PlayerControlled.cs/UpdatePlayers.cs
// that were previously gated behind the step-6A/7B assert-backed hooks and
// are now real (g_swosAiSetControlsDirectionHook/g_swosAiKickHook wired to
// the real implementations as of this step).
//
// AiBrain.SetControlsDirection draws exactly one Rng byte per call
// (AI_rand, right at the top) that drives many downstream branch choices
// (aiRand & 0xF, & 7, & 3, & 1, & 0x18, etc.). Rng state is a SEPARATE set
// of C#-side statics from Memory (see swos_rng.h's own note) -- NOT reset
// by Memory.Init() beyond the single deterministic
// `Rng.Reseed(ReadWord(Addr.currentGameTick))` call at the very end of
// Init() itself (currentGameTick == 0 on a fresh Init, so that reseed is
// itself deterministic and reproducible -- see swos_memory_init.c's mirror
// of the same call). Every scenario below still explicitly re-seeds with
// its own chosen seed right after Memory.Init() when the specific AI_rand
// value matters for which branch gets exercised, rather than relying on
// "whatever Init's own reseed happened to produce" -- same pattern
// Step7AGolden.cs established for PlayerTackle's card rolls. Seeds were
// picked by probing `Rng.Reseed(N); Rng.NextByte()` for the low-bit
// patterns each branch gate needs; see the comment on each scenario.
//
// AiBrain's `s_amigaPreventDirectionFlip` file-static is NOT reset by
// Memory.Init() either, but GameTime.AmigaModeActive() is hardcoded false
// in this port (PC-only), so checkForAmigaModeDirectionFlipBan always
// leaves it false and writeAmigaModeDirectionFlip's write branch is
// unreachable in every scenario here -- verified by reading both helpers'
// bodies, not assumed. No cross-scenario contamination risk exists for it
// in this configuration.
using OpenSwos.Sim.Port;
using OpenSwos.SwosVm;

public static class Step9Golden
{
    private const int kMemSize = 0x60000;

    public static void Run(string outDir)
    {
        Directory.CreateDirectory(outDir);
        int count = 0;

        void Scenario(string name, Action setup, Action act)
        {
            Memory.Init(pcMode: true);
            setup();
            act();
            byte[] dump = Memory.View(0, kMemSize).ToArray();
            File.WriteAllBytes(Path.Combine(outDir, $"s9_{name}.bin"), dump);
            count++;
        }

        int top = TeamData.TopBase, bot = TeamData.BottomBase;
        int t1_1 = PlayerSprite.Base(1);
        int t1_2 = PlayerSprite.Base(2);
        int t2_1 = PlayerSprite.Base(12);

        // Common two-controlled-player baseline several scenarios build on.
        void BaseSetup()
        {
            TeamData.SetControlledPlayer(true, t1_1);
            TeamData.SetControlledPlayer(false, t2_1);
            PlayerSprite.SetX(1, 300 << 16); PlayerSprite.SetY(1, 400 << 16);
            PlayerSprite.SetX(12, 350 << 16); PlayerSprite.SetY(12, 450 << 16);
            BallSprite.XPixels = 320; BallSprite.YPixels = 420;
        }

        // ==================================================================
        // AiBrain.SetControlsDirection -- top-level gates
        // ==================================================================
        Scenario("reset_controls_gate", () =>
        {
            Memory.WriteWord(top + TeamData.OffResetControls, 1);
        }, () => AiBrain.SetControlsDirection(top));

        Scenario("subs_menu_gate", () =>
        {
            Memory.WriteWord(Memory.Addr.g_inSubstitutesMenu, 1);
        }, () => AiBrain.SetControlsDirection(top));

        Scenario("not_in_progress_not_last_team", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 0);
            Memory.WriteDword(Memory.Addr.lastTeamPlayedBeforeBreak, bot);
        }, () => AiBrain.SetControlsDirection(top));

        // ==================================================================
        // Game-over (halftime/full-time result) auto-fire branch
        // ==================================================================
        Scenario("game_over_result_halftime", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 0);
            Memory.WriteDword(Memory.Addr.lastTeamPlayedBeforeBreak, top);
            Memory.WriteWord(Memory.Addr.gameState, 25); // ST_RESULT_ON_HALFTIME
            Memory.WriteWord(Memory.Addr.team1Computer, 1);
            Memory.WriteWord(Memory.Addr.team2Computer, 1);
            Memory.WriteWord(Memory.Addr.stoppageTimerTotal, 500);
            Memory.WriteWord(Memory.Addr.m_clearResultInterval, 100);
        }, () => AiBrain.SetControlsDirection(top));

        // ==================================================================
        // Game-not-over: stoppage set-piece direction dispatch by gameState
        // ==================================================================
        Scenario("game_not_over_keeper_holds_ball", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 0);
            Memory.WriteDword(Memory.Addr.lastTeamPlayedBeforeBreak, top);
            Memory.WriteWord(Memory.Addr.gameState, 3); // ST_KEEPER_HOLDS_BALL
            Memory.WriteWord(Memory.Addr.stoppageTimerActive, 0);
            BaseSetup();
            Rng.Reseed(20); // aiRand=1 -> (aiRand&1)!=0
        }, () => AiBrain.SetControlsDirection(top));

        Scenario("game_not_over_goal_scored", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 0);
            Memory.WriteDword(Memory.Addr.lastTeamPlayedBeforeBreak, top);
            Memory.WriteWord(Memory.Addr.gameState, 0); // ST_PLAYERS_TO_INITIAL_POSITIONS
            Memory.WriteWord(Memory.Addr.stoppageTimerActive, 200);
            BaseSetup();
        }, () => AiBrain.SetControlsDirection(top));

        Scenario("game_not_over_throw_in", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 0);
            Memory.WriteDword(Memory.Addr.lastTeamPlayedBeforeBreak, top);
            Memory.WriteWord(Memory.Addr.gameState, 15); // ST_THROW_IN_FORWARD_RIGHT
            Memory.WriteWord(Memory.Addr.stoppageTimerActive, 0);
            BaseSetup();
            Rng.Reseed(20); // aiRand=1 -> aiRand&0xF != 0
        }, () => AiBrain.SetControlsDirection(top));

        Scenario("game_not_over_free_kick", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 0);
            Memory.WriteDword(Memory.Addr.lastTeamPlayedBeforeBreak, top);
            Memory.WriteWord(Memory.Addr.gameState, 6); // ST_FREE_KICK_LEFT1
            Memory.WriteWord(Memory.Addr.stoppageTimerActive, 0);
            BaseSetup();
            Rng.Reseed(24); // aiRand=192 -> aiRand&0xF == 0
        }, () => AiBrain.SetControlsDirection(top));

        Scenario("game_not_over_penalties", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 0);
            Memory.WriteDword(Memory.Addr.lastTeamPlayedBeforeBreak, top);
            Memory.WriteWord(Memory.Addr.playingPenalties, 1);
            Memory.WriteWord(Memory.Addr.stoppageTimerActive, 0);
            BaseSetup();
            Rng.Reseed(20);
        }, () => AiBrain.SetControlsDirection(top));

        // ==================================================================
        // Game-in-progress: penalty/spin-timer fast path into ball-after-touch
        // ==================================================================
        Scenario("game_in_progress_penalty_spin_timer", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            Memory.WriteWord(Memory.Addr.penalty, 1);
            TeamData.SetSpinTimer(true, 3);
            BaseSetup();
            Rng.Reseed(20);
        }, () => AiBrain.SetControlsDirection(top));

        Scenario("game_in_progress_no_controlled_player", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            BallSprite.XPixels = 320; BallSprite.YPixels = 420;
            Rng.Reseed(20);
        }, () => AiBrain.SetControlsDirection(top));

        // ==================================================================
        // Game-in-progress: player-near fire decision (both outcomes)
        // ==================================================================
        Scenario("game_in_progress_player_near_fires", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            BaseSetup();
            Memory.WriteByte(top + TeamData.OffPlVeryCloseToBall, 1);
            PlayerSprite.SetDirection(1, 4); // top team: valid facing dirs 3/4/5
            PlayerSprite.SetBallDistance(1, 100);
            BallSprite.DeltaZ = 1000;
            BallSprite.ZPixels = 10;
            Rng.Reseed(20);
        }, () => AiBrain.SetControlsDirection(top));

        Scenario("game_in_progress_player_near_no_fire_chase", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            BaseSetup();
            Memory.WriteByte(top + TeamData.OffPlVeryCloseToBall, 1);
            PlayerSprite.SetDirection(1, 0); // not facing goal -> no fire -> chase logic
            Rng.Reseed(20);
        }, () => AiBrain.SetControlsDirection(top));

        // ==================================================================
        // Game-in-progress: chase success (l_our_player_closest)
        // ==================================================================
        Scenario("our_player_closest_chase_success", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            BaseSetup();
            Memory.WriteByte(top + TeamData.OffPlCloseToBall, 1);
            PlayerSprite.SetDirection(1, 2); // facing L/R -> d6-bucket path
            Memory.WriteWord(top + TeamData.OffPassKickTimer, 5); // <13 -> AI_ResumeGameDelay carry
            Memory.WriteWord(Memory.Addr.AI_resumePlayTimer, 0);
            Rng.Reseed(20);
        }, () => AiBrain.SetControlsDirection(top));

        // ==================================================================
        // Game-in-progress: no-one near -> pass-target reassignment / random
        // flip / use-current-direction
        // ==================================================================
        Scenario("game_in_progress_noone_near_reassign", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            TeamData.SetControlledPlayer(true, t1_1);
            Memory.WriteDword(top + TeamData.OffPassToPlayerPtr, t1_2);
            PlayerSprite.SetX(1, 300 << 16); PlayerSprite.SetY(1, 400 << 16);
            PlayerSprite.SetBallDistance(1, 5000);
            PlayerSprite.SetX(2, 305 << 16); PlayerSprite.SetY(2, 405 << 16);
            PlayerSprite.SetBallDistance(2, 100);
            BallSprite.XPixels = 305; BallSprite.YPixels = 405;
            Rng.Reseed(20);
        }, () => AiBrain.SetControlsDirection(top));

        Scenario("game_in_progress_noone_near_random_flip", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            TeamData.SetControlledPlayer(true, t1_1);
            Memory.WriteWord(top + TeamData.OffPlayerNumber, 0);
            Memory.WriteWord(bot + TeamData.OffPlayerNumber, 0);
            PlayerSprite.SetX(1, 300 << 16); PlayerSprite.SetY(1, 400 << 16);
            BallSprite.XPixels = 320; BallSprite.YPixels = 420;
            Rng.Reseed(20); // aiRand=1 -> gameTick&0x18 gate taken; aiRand&2 selects rotate table entry
        }, () => AiBrain.SetControlsDirection(top));

        Scenario("game_in_progress_noone_near_use_current_direction", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            TeamData.SetControlledPlayer(true, t1_1);
            Memory.WriteWord(top + TeamData.OffPlayerNumber, 1); // human present -> skip random flip
            PlayerSprite.SetX(1, 300 << 16); PlayerSprite.SetY(1, 400 << 16);
            PlayerSprite.SetDirection(1, 3);
            BallSprite.XPixels = 320; BallSprite.YPixels = 420;
            Rng.Reseed(20);
        }, () => AiBrain.SetControlsDirection(top));

        // ==================================================================
        // l_ball_after_touch_allowed -- spin/after-touch-strength selection
        // ==================================================================
        Scenario("ball_after_touch_left_spin", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            BaseSetup();
            TeamData.SetSpinTimer(true, 3);
            Memory.WriteWord(top + TeamData.OffAiBallSpinDirection, -1);
            Memory.WriteWord(top + TeamData.OffControlledPlDirection, 3);
            Rng.Reseed(20); // aiRand&1 == 1
        }, () => AiBrain.SetControlsDirection(top));

        Scenario("ball_after_touch_right_spin", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            BaseSetup();
            TeamData.SetSpinTimer(true, 3);
            Memory.WriteWord(top + TeamData.OffAiBallSpinDirection, 1);
            Memory.WriteWord(top + TeamData.OffControlledPlDirection, 3);
            Rng.Reseed(20);
        }, () => AiBrain.SetControlsDirection(top));

        Scenario("ball_after_touch_no_spin_medium_strength", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            BaseSetup();
            TeamData.SetSpinTimer(true, 3);
            Memory.WriteWord(top + TeamData.OffAiBallSpinDirection, 0);
            Memory.WriteWord(top + TeamData.OffAiAfterTouchStrength, 1);
            Memory.WriteWord(top + TeamData.OffControlledPlDirection, 3);
            Rng.Reseed(24); // aiRand&1 == 0 -> l_no_ball_after_touch_local
        }, () => AiBrain.SetControlsDirection(top));

        // ==================================================================
        // AiHelpers.cs -- standalone (public) functions
        // ==================================================================
        Scenario("ai_helpers_kick_direct", () =>
        {
            Memory.WriteDword(top + TeamData.OffOpponentsTeam, bot);
            Memory.WriteWord(bot + TeamData.OffPlayerHasBall, 1);
            Memory.WriteDword(bot + TeamData.OffControlledPlayer, t2_1);
            PlayerSprite.SetBallDistance(1, 50);
            PlayerSprite.SetDirection(1, 0);
            // Verbatim quirk (see swos_ai_helpers.c's header note): the asm
            // reads a TeamData-shaped offset off the opponent's controlled
            // PLAYER sprite address, not off a TeamData base.
            Memory.WriteWord(t2_1 + TeamData.OffAllowedDirections, 0);
        }, () => AiHelpers.AI_Kick(t1_1, top));

        Scenario("ai_helpers_set_direction_toward_goal", () =>
        {
            Memory.WriteWord(Memory.Addr.AI_counter, 5);
            Memory.WriteWord(Memory.Addr.AI_attackHalf, 2);
            BallSprite.XPixels = 200;
        }, () => AiHelpers.AI_SetDirectionTowardOpponentsGoal(top));

        Scenario("ai_helpers_decide_fire_true", () =>
        {
            PlayerSprite.SetPlayerOrdinal(1, 4);
            PlayerSprite.SetBallDistance(1, 100);
            BallSprite.DeltaZ = 1000; BallSprite.ZPixels = 10;
        }, () => AiHelpers.AI_DecideWhetherToTriggerFire(4, t1_1, top));

        Scenario("ai_helpers_decide_fire_false", () =>
        {
            PlayerSprite.SetPlayerOrdinal(1, 4);
            PlayerSprite.SetBallDistance(1, 5000);
        }, () => AiHelpers.AI_DecideWhetherToTriggerFire(4, t1_1, top));

        // ==================================================================
        // CPU-team integration: previously-blocked branches in
        // PlayerControlled.cs / UpdatePlayers.cs now run real AI.
        // ==================================================================
        Scenario("player_controlled_cpu_team_uses_real_ai", () =>
        {
            Memory.WriteWord(top + TeamData.OffPlayerNumber, 0); // CPU
            TeamData.SetControlledPlayer(true, t1_1);
            BaseSetup();
            Rng.Reseed(20);
        }, () => PlayerControlled.RunControlledBranch(t1_1, true));

        Scenario("update_players_cpu_off_ball_real_ai", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 100);
            Memory.WriteWord(Memory.Addr.gameState, 100);
            Memory.WriteWord(top + TeamData.OffPlayerNumber, 0);
            Memory.WriteWord(bot + TeamData.OffPlayerNumber, 0);
            // controlledPlayer == 0 -> TickAiControlled calls AI_SetControlsDirection for real.
            BallSprite.XPixels = 320; BallSprite.YPixels = 420;
            Rng.Reseed(20);
        }, () => UpdatePlayers.Update(0));

        Scenario("update_players_cpu_stoppage_ai_kick", () =>
        {
            Memory.WriteWord(Memory.Addr.gameStatePl, 0);
            Memory.WriteWord(Memory.Addr.gameState, 0);
            Memory.WriteWord(top + TeamData.OffPlayerNumber, 0);
            Memory.WriteDword(Memory.Addr.lastTeamPlayedBeforeBreak, top);
            Memory.WriteDword(top + TeamData.OffPassToPlayerPtr, t1_2);
            PlayerSprite.SetX(2, 305 << 16); PlayerSprite.SetY(2, 405 << 16);
            BallSprite.XPixels = 305; BallSprite.YPixels = 405;
            Memory.WriteWord(top + TeamData.OffUpdatePlayerIndex, 1); // slot 2's round-robin turn
            Rng.Reseed(20);
        }, () => UpdatePlayers.Update(0));

        Console.WriteLine($"wrote {count} Step9 scenario dumps ({kMemSize} bytes each) to {outDir}");
    }
}
