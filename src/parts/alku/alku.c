/**
 * ALKU (Opening Credits) Part Implementation
 *
 * Port of original ALKU/MAIN.C to cross-platform Sokol framework.
 */

#include "alku.h"
#include "alku_data.h"
#include "dis.h"
#include "video.h"
#include <string.h>

/* Static state for the part */
static alku_state_t state;

/* Forward declarations */
static void alku_init(sr_part_t *part);
static int alku_update(sr_part_t *part, int frame_count);
static void alku_render(sr_part_t *part);
static void alku_cleanup(sr_part_t *part);

/* Part definition */
static sr_part_t alku_part = {
    .name = "ALKU",
    .description = "Opening Credits",
    .id = SR_PART_ALKU,
    .state = SR_PART_STATE_STOPPED,
    .init = alku_init,
    .update = alku_update,
    .render = alku_render,
    .cleanup = alku_cleanup,
    .user_data = &state
};

sr_part_t *alku_get_part(void)
{
    return &alku_part;
}

static void alku_init(sr_part_t *part)
{
    alku_state_t *s = (alku_state_t *)part->user_data;
    (void)s;

    /* Initialize DIS */
    dis_partstart();

    /* TODO: Load data and initialize */
}

static int alku_update(sr_part_t *part, int frame_count)
{
    alku_state_t *s = (alku_state_t *)part->user_data;
    (void)s;
    (void)frame_count;

    /* Check for exit */
    if (dis_exit()) {
        return 1; /* Signal to advance to next part */
    }

    /* TODO: Implement animation logic */

    return 0;
}

static void alku_render(sr_part_t *part)
{
    alku_state_t *s = (alku_state_t *)part->user_data;
    (void)s;

    /* TODO: Render frame */
}

static void alku_cleanup(sr_part_t *part)
{
    alku_state_t *s = (alku_state_t *)part->user_data;
    (void)s;

    /* Clear state */
    memset(s, 0, sizeof(*s));
}
