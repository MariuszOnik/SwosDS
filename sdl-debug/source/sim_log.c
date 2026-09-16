#include "sim_log.h"

#include <stdio.h>
#include <string.h>

#include "swos_addr.h"
#include "swos_ball_sprite.h"
#include "swos_camera.h"
#include "swos_memory.h"
#include "swos_player_sprite.h"
#include "swos_result.h"
#include "swos_team_data.h"

static FILE *s_file = NULL;

void simLogOpen(const char *path)
{
    if (s_file) fclose(s_file);
    s_file = fopen(path, "w");
    if (!s_file)
    {
        fprintf(stderr, "sim_log: could not open %s for writing\n", path);
        return;
    }

    fprintf(s_file, "tick\tgameState\tgameStatePl\tbreakCameraMode\t"
                     "ballX\tballY\tballZ\tcamX\tcamY\t"
                     "ctrlTopSlot\tctrlBotSlot\tscoreTop\tscoreBot");
    for (int slot = 0; slot < PLSPR_TOTAL_SLOTS; slot++)
        fprintf(s_file, "\tslot%d_team\tslot%d_ord\tslot%d_x\tslot%d_y\tslot%d_state",
                slot, slot, slot, slot, slot);
    fprintf(s_file, "\tevent\n");
}

void simLogClose(void)
{
    if (s_file)
    {
        fclose(s_file);
        s_file = NULL;
    }
}

bool simLogIsOpen(void)
{
    return s_file != NULL;
}

static int findControlledSlot(int32_t controlledAddr)
{
    if (controlledAddr == 0) return -1;
    for (int slot = 0; slot < PLSPR_TOTAL_SLOTS; slot++)
        if (swosPlayerSpriteBase(slot) == controlledAddr)
            return slot;
    return -1;
}

static int sumGoals(const SwosResultScorerInfo *scorers)
{
    int total = 0;
    for (int i = 0; i < SWOS_RESULT_MAX_SCORERS; i++)
        if (scorers[i].shirtNum != 0)
            total += scorers[i].numGoals;
    return total;
}

void simLogWriteTick(uint64_t tick, const char *eventText)
{
    if (!s_file) return;

    int32_t ctrlTop = swosTeamDataControlledPlayer(true);
    int32_t ctrlBot = swosTeamDataControlledPlayer(false);
    int scoreTop = sumGoals(swosResultGetTeam1Scorers());
    int scoreBot = sumGoals(swosResultGetTeam2Scorers());

    fprintf(s_file, "%llu\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d",
            (unsigned long long)tick,
            swosReadSignedWord(ADDR_gameState),
            swosReadSignedWord(ADDR_gameStatePl),
            swosReadSignedWord(ADDR_breakCameraMode),
            swosBallSpriteXPixels(), swosBallSpriteYPixels(), swosBallSpriteZPixels(),
            swosCameraGetXWhole(), swosCameraGetYWhole(),
            findControlledSlot(ctrlTop), findControlledSlot(ctrlBot),
            scoreTop, scoreBot);

    for (int slot = 0; slot < PLSPR_TOTAL_SLOTS; slot++)
    {
        fprintf(s_file, "\t%d\t%d\t%d\t%d\t%d",
                swosPlayerSpriteTeamNumber(slot),
                swosPlayerSpritePlayerOrdinal(slot),
                swosPlayerSpriteXPixels(slot),
                swosPlayerSpriteYPixels(slot),
                swosPlayerSpritePlayerState(slot));
    }

    fprintf(s_file, "\t%s\n", eventText ? eventText : "");
    fflush(s_file);
}
