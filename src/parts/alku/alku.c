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

/**
 * Copy horizon image to framebuffer with double buffering.
 * Uses the current scroll position and page for Mode X style display.
 */
static void render_horizon(alku_state_t *s)
{
    uint8_t *fb = video_get_framebuffer();
    int screen_width = VIDEO_WIDTH;
    int screen_height = VIDEO_HEIGHT_X;
    int x, y;

    /* The horizon is displayed in the lower portion of the screen */
    /* Original places it at line 50 (176 bytes per line in Mode X = 100 pixels) */
    int horizon_y_start = 100;

    /* Double buffering: page 0 or 1, each is 88 lines */
    int page_offset = s->page * 88;

    /* Calculate visible portion based on scroll position */
    int scroll_x = s->scroll_pos;

    for (y = 0; y < 88 && (y + horizon_y_start) < screen_height; y++) {
        for (x = 0; x < screen_width; x++) {
            int src_x = (x + scroll_x) % ALKU_HORIZON_WIDTH;
            int src_y = y + page_offset;
            int dst_idx = (y + horizon_y_start) * screen_width + x;
            int src_idx = src_y * ALKU_HORIZON_WIDTH + src_x;

            if (src_y < ALKU_HORIZON_TOTAL) {
                fb[dst_idx] = s->horizon[src_idx];
            }
        }
    }
}

static void alku_init(sr_part_t *part)
{
    alku_state_t *s = (alku_state_t *)part->user_data;

    /* Clear state */
    memset(s, 0, sizeof(*s));

    /* Initialize DIS */
    dis_partstart();

    /* Set video mode */
    video_set_mode(VIDEO_MODE_X);

    /* Load data files */
    alku_load_font(s);
    alku_load_horizon(s);
    alku_build_glyphs(s);
    alku_init_palettes(s);

    /* Start with black palette */
    video_set_palette(s->fade1);
}

static int alku_update(sr_part_t *part, int frame_count)
{
    alku_state_t *s = (alku_state_t *)part->user_data;
    (void)frame_count;

    /* Check for exit */
    if (dis_exit()) {
        return 1; /* Signal to advance to next part */
    }

    /* Exit when scroll reaches 320 */
    if (s->scroll_pos >= 320) {
        return 1;
    }

    return 0;
}

static void alku_render(sr_part_t *part)
{
    alku_state_t *s = (alku_state_t *)part->user_data;

    /* Render horizon background */
    render_horizon(s);
}

static void alku_cleanup(sr_part_t *part)
{
    alku_state_t *s = (alku_state_t *)part->user_data;

    /* Clear state */
    memset(s, 0, sizeof(*s));
}
