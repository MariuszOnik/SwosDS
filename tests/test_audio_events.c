// Tests for swos_audio_events.h's hook mechanism, plus one real
// integration check (swosResultRegisterScorer -> SWOS_AUDIO_EVENT_GOAL)
// confirming the mechanism actually fires from the real call site, not
// just in isolation.
#include <stdio.h>

#include "swos_addr.h"
#include "swos_audio_events.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"
#include "swos_result.h"

static int g_failures = 0;

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            g_failures++; \
        } else { \
            printf("ok:   %s\n", msg); \
        } \
    } while (0)

static int s_fireCount;
static SwosAudioEvent s_lastEvent;

static void testHook(SwosAudioEvent event) {
    s_fireCount++;
    s_lastEvent = event;
}

static void test_fire_event_is_a_noop_when_unset(void) {
    swosAudioSetEventHook(0);
    swosAudioFireEvent(SWOS_AUDIO_EVENT_GOAL); // must not crash
    CHECK(1, "fire event with no hook set: does not crash (desktop tests have no audio subsystem)");
}

static void test_fire_event_calls_the_registered_hook(void) {
    s_fireCount = 0;
    swosAudioSetEventHook(testHook);

    swosAudioFireEvent(SWOS_AUDIO_EVENT_BALL_BOUNCE);
    CHECK(s_fireCount == 1 && s_lastEvent == SWOS_AUDIO_EVENT_BALL_BOUNCE,
          "fire event: registered hook receives SWOS_AUDIO_EVENT_BALL_BOUNCE");

    swosAudioFireEvent(SWOS_AUDIO_EVENT_FOUL_WHISTLE);
    CHECK(s_fireCount == 2 && s_lastEvent == SWOS_AUDIO_EVENT_FOUL_WHISTLE,
          "fire event: registered hook receives SWOS_AUDIO_EVENT_FOUL_WHISTLE");

    swosAudioSetEventHook(0);
    swosAudioFireEvent(SWOS_AUDIO_EVENT_RESTART_WHISTLE);
    CHECK(s_fireCount == 2, "fire event: unregistering the hook stops delivery");
}

// Real call-site check: swosResultRegisterScorer (src/swos_result.c) fires
// SWOS_AUDIO_EVENT_GOAL exactly once per real goal -- the single real
// place "a goal was scored" is known, per swos_update_goals.c's
// swosRegisterScorerHook call site.
static void test_register_scorer_fires_goal_event(void) {
    swosMemoryInit(true);
    swosResultReset("", "");

    const int K_TOP_TEAM_IN_GAME = 0x4FE60;
    const int K_BOT_TEAM_IN_GAME = 0x4FEA0;
    swosWriteDword(ADDR_topTeamInGame, K_TOP_TEAM_IN_GAME);
    swosWriteDword(ADDR_bottomTeamInGame, K_BOT_TEAM_IN_GAME);
    swosPlayerSpriteSetPlayerOrdinal(1, 1);
    swosWriteByte(K_TOP_TEAM_IN_GAME + 3, 7);

    s_fireCount = 0;
    swosAudioSetEventHook(testHook);

    int scorerSpriteAddr = swosPlayerSpriteBase(1);
    swosResultRegisterScorer(scorerSpriteAddr, 1, 0);

    CHECK(s_fireCount == 1 && s_lastEvent == SWOS_AUDIO_EVENT_GOAL,
          "swosResultRegisterScorer fires SWOS_AUDIO_EVENT_GOAL exactly once");

    swosAudioSetEventHook(0);
}

int main(void) {
    test_fire_event_is_a_noop_when_unset();
    test_fire_event_calls_the_registered_hook();
    test_register_scorer_fires_goal_event();

    if (g_failures) {
        printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    printf("\nall checks passed\n");
    return 0;
}
