/**
 * ALKU (Opening Credits) Part Implementation
 *
 * Port of original ALKU/MAIN.C to cross-platform Sokol framework.
 */

#include "alku.h"
#include "alku_data.h"
#include "dis.h"
#include "video.h"
#include <stdio.h>
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
 * Print text at position using XOR compositing.
 * Font pixels are OR'd with existing framebuffer to overlay text.
 *
 * @param s Part state
 * @param x Left X position
 * @param y Top Y position
 * @param txt Null-terminated string
 */
static void prt(alku_state_t *s, int x, int y, const char *txt)
{
    uint8_t *fb = video_get_framebuffer();
    int screen_width = VIDEO_WIDTH;

    while (*txt) {
        unsigned char c = (unsigned char)*txt;
        alku_glyph_t *glyph = &s->glyphs[c];
        int glyph_x, glyph_y;
        int sx = glyph->start;

        for (glyph_x = 0; glyph_x < glyph->width; glyph_x++) {
            for (glyph_y = 0; glyph_y < ALKU_FONT_ROWS; glyph_y++) {
                int dst_x = x + glyph_x;
                int dst_y = y + glyph_y;
                uint8_t font_val = s->font[glyph_y][sx + glyph_x];

                if (dst_x >= 0 && dst_x < screen_width && dst_y >= 0 && dst_y < VIDEO_HEIGHT_X) {
                    int idx = dst_y * screen_width + dst_x;
                    /* OR font pixel with framebuffer for XOR-like compositing */
                    fb[idx] |= font_val;
                }
            }
        }
        x += glyph->width + 2; /* Add 2 pixels spacing between chars */
        txt++;
    }
}

/**
 * Print centered text at Y position.
 *
 * @param s Part state
 * @param center_x Center X position
 * @param y Top Y position
 * @param txt Null-terminated string
 */
static void prtc(alku_state_t *s, int center_x, int y, const char *txt)
{
    const char *t = txt;
    int width = 0;

    /* Calculate total text width */
    while (*t) {
        width += s->glyphs[(unsigned char)*t].width + 2;
        t++;
    }

    /* Draw centered */
    prt(s, center_x - width / 2, y, txt);
}

/**
 * Start a non-blocking fade between two palettes.
 * Call tick_lerp_fade() each frame to advance.
 *
 * @param s Part state
 * @param pal1 Source palette (768 bytes)
 * @param pal2 Target palette (768 bytes)
 */
static void start_lerp_fade(alku_state_t *s, const uint8_t *pal1, const uint8_t *pal2)
{
    s->fade_src = pal1;
    s->fade_dst = pal2;
    s->fade_step = 0;
}

/**
 * Tick the non-blocking lerp fade by one step.
 * Interpolates palette and updates video.
 *
 * @param s Part state
 * @return 1 if fade still active, 0 if complete
 */
static int tick_lerp_fade(alku_state_t *s)
{
    uint8_t pal[ALKU_PALETTE_SIZE];
    int i;
    int step = s->fade_step;

    if (step >= 64) {
        return 0; /* Fade complete */
    }

    /* Linear interpolation: src*(64-step) + dst*step >> 6 */
    for (i = 0; i < ALKU_PALETTE_SIZE; i++) {
        pal[i] = (s->fade_src[i] * (64 - step) + s->fade_dst[i] * step) >> 6;
    }
    video_set_palette(pal);

    s->fade_step++;
    return s->fade_step < 64;
}

/**
 * Start an incremental (non-blocking) palette fade.
 * The fade runs during update ticks while scrolling continues.
 *
 * @param s Part state
 * @param increments Fade increment table (768 int16_t values, 8.8 fixed-point)
 * @param steps Total number of steps for fade (64 or 128)
 */
static void start_incremental_fade(alku_state_t *s, const int16_t *increments, int steps)
{
    /* Copy current fadepal as starting point */
    /* (fadepal should be set before calling this) */
    s->fade_active = steps;
    s->fade_pos = 0;

    /* Fade increments are stored in state, we just remember the pointer */
    /* For simplicity, we'll apply increments directly in tick_fade */
    (void)increments; /* Handled via state->picin/textin/textout */
}

/**
 * Tick the incremental fade by one step.
 * Called each frame during scrolling to update palette smoothly.
 *
 * @param s Part state
 * @param increments Fade increment table to use
 * @return 1 if fade still active, 0 if complete
 */
static int tick_fade(alku_state_t *s, const int16_t *increments)
{
    int i;

    if (s->fade_active <= 0) {
        return 0;
    }

    /* Apply one step of increments (8.8 fixed-point) */
    for (i = 0; i < ALKU_PALETTE_SIZE; i++) {
        int val = s->fadepal[i] * 256 + increments[i];
        if (val < 0) val = 0;
        if (val > 63 * 256) val = 63 * 256;
        s->fadepal[i] = val >> 8;
    }

    video_set_palette(s->fadepal);

    s->fade_active--;
    s->fade_pos++;

    return s->fade_active > 0;
}

/**
 * Perform one scroll step.
 * Original scrolls 1 pixel every SCRLF (9) frames.
 *
 * @param s Part state
 * @return 1 if scroll position advanced, 0 if waiting
 */
static int do_scroll(alku_state_t *s)
{
    s->frame_count++;

    if (s->frame_count < ALKU_SCROLL_RATE) {
        return 0; /* Not yet time to scroll */
    }

    s->frame_count -= ALKU_SCROLL_RATE;

    /* Advance scroll position */
    s->scroll_pos++;

    /* Toggle page for double buffering */
    s->page ^= 1;

    /* Update video display offset for hardware scrolling simulation */
    video_set_start(s->scroll_pos / 4 + s->page * 88);
    video_set_hscroll((s->scroll_pos & 3) * 2);

    return 1;
}

/**
 * Add text to the overlay buffer for XOR compositing.
 * Text is rendered centered at given position into tbuf.
 *
 * @param s Part state
 * @param center_x Center X position
 * @param y Top Y position within tbuf
 * @param txt Text to render
 */
static void addtext(alku_state_t *s, int center_x, int y, const char *txt)
{
    const char *t = txt;
    int width = 0;
    int x, gx, gy;

    /* Calculate total text width */
    while (*t) {
        width += s->glyphs[(unsigned char)*t].width + 2;
        t++;
    }

    /* Draw centered into tbuf */
    x = center_x - width / 2;
    t = txt;

    while (*t) {
        unsigned char c = (unsigned char)*t;
        alku_glyph_t *glyph = &s->glyphs[c];
        int sx = glyph->start;

        for (gx = 0; gx < glyph->width; gx++) {
            for (gy = 0; gy < ALKU_FONT_ROWS; gy++) {
                int dst_x = x + gx;
                int dst_y = y + gy;
                uint8_t font_val = s->font[gy][sx + gx];

                if (dst_x >= 0 && dst_x < ALKU_TBUF_COLS &&
                    dst_y >= 0 && dst_y < ALKU_TBUF_ROWS) {
                    s->tbuf[dst_y][dst_x] = font_val;
                }
            }
        }
        x += glyph->width + 2;
        t++;
    }
}

/**
 * Apply text buffer to framebuffer using XOR compositing.
 * Called during scroll to overlay credits text on horizon.
 *
 * @param s Part state
 * @param scroll Current scroll position for offset calculation
 */
static void apply_text_overlay(alku_state_t *s, int scroll)
{
    uint8_t *fb = video_get_framebuffer();
    int screen_width = VIDEO_WIDTH;
    int x, y;
    int fb_y_start = 100; /* Matches horizon start */

    for (y = 1; y < 184 && (y + fb_y_start) < VIDEO_HEIGHT_X; y++) {
        for (x = 0; x < 320; x++) {
            int fb_x = (x + scroll) % screen_width;
            int fb_idx = (y + fb_y_start) * screen_width + fb_x;
            uint8_t text_val = s->tbuf[y][x];

            if (text_val) {
                /* XOR compositing */
                fb[fb_idx] ^= text_val;
            }
        }
    }
}

/**
 * Clear text from framebuffer by masking out text color bits.
 * Removes text overlay by ANDing with 0x3F to keep only base colors.
 *
 * @param s Part state
 */
static void fonapois(alku_state_t *s)
{
    uint8_t *fb = video_get_framebuffer();
    int screen_width = VIDEO_WIDTH;
    int x, y;
    int fb_y_start = 64;  /* Text area starts at line 64 */
    int fb_y_end = 64 + 256; /* Text area is 256 lines */

    for (y = fb_y_start; y < fb_y_end && y < VIDEO_HEIGHT_X; y++) {
        for (x = 0; x < screen_width; x++) {
            int idx = y * screen_width + x;
            /* Mask out text bits (keep only lower 6 bits = base color) */
            fb[idx] &= 0x3F;
        }
    }

    /* Also clear the text overlay buffer */
    memset(s->tbuf, 0, sizeof(s->tbuf));
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

/* Animation phase constants */
#define PHASE_WAIT_SYNC1    0   /* Wait for sync 1 */
#define PHASE_INTRO1        1   /* "A" / "Future Crew" / "Production" */
#define PHASE_WAIT_SYNC2    2   /* Wait for sync 2 */
#define PHASE_INTRO2        3   /* "First Presented" / "at Assembly 93" */
#define PHASE_WAIT_SYNC3    4   /* Wait for sync 3 */
#define PHASE_INTRO3        5   /* Special symbols display */
#define PHASE_WAIT_SYNC4    6   /* Wait for sync 4 */
#define PHASE_HORIZON       7   /* Fade in horizon + scroll */
#define PHASE_CREDITS       8   /* Scrolling credits */
#define PHASE_DONE          9   /* Exit */

/**
 * Wait helper - non-blocking frame wait.
 * @param s Part state
 * @param frames Frames to wait
 * @return 1 if still waiting, 0 if done
 */
static int wait_frames(alku_state_t *s, int frames)
{
    if (s->frame_count < frames) {
        s->frame_count++;
        dis_waitb();
        return 1;
    }
    s->frame_count = 0;
    return 0;
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
    if (alku_load_font(s) != 0) {
        fprintf(stderr, "alku: failed to load font data\n");
        part->state = SR_PART_STATE_STOPPED;
        return;
    }
    if (alku_load_horizon(s) != 0) {
        fprintf(stderr, "alku: failed to load horizon data\n");
        part->state = SR_PART_STATE_STOPPED;
        return;
    }
    alku_build_glyphs(s);
    alku_init_palettes(s);

    /* Start with black palette */
    video_set_palette(s->fade1);

    /* Start at phase 0 */
    s->phase = PHASE_WAIT_SYNC1;
    printf("ALKU: initialized, phase=%d\n", s->phase);
    fflush(stdout);
}

static int alku_update(sr_part_t *part, int frame_count)
{
    alku_state_t *s = (alku_state_t *)part->user_data;
    (void)frame_count;

    static int update_count = 0;
    update_count++;
    if (update_count <= 60 || (update_count % 100 == 0)) {
        printf("ALKU: update #%d, phase=%d, sync=%d\n", update_count, s->phase, dis_sync());
        fflush(stdout);
    }

    /* Check for exit */
    if (dis_exit()) {
        printf("ALKU: dis_exit() returned true, exiting\n");
        return 1;
    }

    switch (s->phase) {
    case PHASE_WAIT_SYNC1:
        /* Wait for music sync 1 */
        if (dis_sync() >= 1) {
            /* Display intro text 1 */
            prtc(s, 160, 120, "A");
            prtc(s, 160, 160, "Future Crew");
            prtc(s, 160, 200, "Production");
            printf("ALKU: transitioning to phase %d\n", PHASE_INTRO1);
            s->phase = PHASE_INTRO1;
            s->sub_phase = 0;
            s->frame_count = 0;
        }
        break;

    case PHASE_INTRO1:
        /* Fade in text, wait, fade out - non-blocking
         * sub_phase: 0=fade-in, 1=display, 2=fade-out */
        switch (s->sub_phase) {
        case 0: /* Fade in */
            if (s->frame_count == 0) {
                start_lerp_fade(s, s->fade1, s->fade2);
            }
            s->frame_count++;
            if (!tick_lerp_fade(s)) {
                s->sub_phase = 1;
                s->frame_count = 0;
            }
            break;
        case 1: /* Display */
            if (wait_frames(s, 200)) break;
            s->sub_phase = 2;
            start_lerp_fade(s, s->fade2, s->fade1);
            break;
        case 2: /* Fade out */
            if (!tick_lerp_fade(s)) {
                fonapois(s);
                printf("ALKU: transitioning to phase %d\n", PHASE_WAIT_SYNC2);
                s->phase = PHASE_WAIT_SYNC2;
                s->sub_phase = 0;
                s->frame_count = 0;
            }
            break;
        }
        break;

    case PHASE_WAIT_SYNC2:
        if (dis_sync() >= 2) {
            prtc(s, 160, 160, "First Presented");
            prtc(s, 160, 200, "at Assembly 93");
            printf("ALKU: transitioning to phase %d\n", PHASE_INTRO2);
            s->phase = PHASE_INTRO2;
            s->sub_phase = 0;
            s->frame_count = 0;
        }
        break;

    case PHASE_INTRO2:
        /* Fade in text, wait, fade out - non-blocking
         * sub_phase: 0=fade-in, 1=display, 2=fade-out */
        switch (s->sub_phase) {
        case 0: /* Fade in */
            if (s->frame_count == 0) {
                start_lerp_fade(s, s->fade1, s->fade2);
            }
            s->frame_count++;
            if (!tick_lerp_fade(s)) {
                s->sub_phase = 1;
                s->frame_count = 0;
            }
            break;
        case 1: /* Display */
            if (wait_frames(s, 200)) break;
            s->sub_phase = 2;
            start_lerp_fade(s, s->fade2, s->fade1);
            break;
        case 2: /* Fade out */
            if (!tick_lerp_fade(s)) {
                fonapois(s);
                printf("ALKU: transitioning to phase %d\n", PHASE_WAIT_SYNC3);
                s->phase = PHASE_WAIT_SYNC3;
                s->sub_phase = 0;
                s->frame_count = 0;
            }
            break;
        }
        break;

    case PHASE_WAIT_SYNC3:
        if (dis_sync() >= 3) {
            /* Note: Original shows special symbols, we use placeholder */
            prtc(s, 160, 120, "in");
            prtc(s, 160, 160, "Second");
            prtc(s, 160, 200, "Reality");
            printf("ALKU: transitioning to phase %d\n", PHASE_INTRO3);
            s->phase = PHASE_INTRO3;
            s->sub_phase = 0;
            s->frame_count = 0;
        }
        break;

    case PHASE_INTRO3:
        /* Fade in text, wait, fade out - non-blocking
         * sub_phase: 0=fade-in, 1=display, 2=fade-out */
        switch (s->sub_phase) {
        case 0: /* Fade in */
            if (s->frame_count == 0) {
                start_lerp_fade(s, s->fade1, s->fade2);
            }
            s->frame_count++;
            if (!tick_lerp_fade(s)) {
                s->sub_phase = 1;
                s->frame_count = 0;
            }
            break;
        case 1: /* Display */
            if (wait_frames(s, 200)) break;
            s->sub_phase = 2;
            start_lerp_fade(s, s->fade2, s->fade1);
            break;
        case 2: /* Fade out */
            if (!tick_lerp_fade(s)) {
                fonapois(s);
                printf("ALKU: transitioning to phase %d\n", PHASE_WAIT_SYNC4);
                s->phase = PHASE_WAIT_SYNC4;
                s->sub_phase = 0;
                s->frame_count = 0;
            }
            break;
        }
        break;

    case PHASE_WAIT_SYNC4:
        if (dis_sync() >= 4) {
            /* Start horizon fade-in while scrolling */
            memcpy(s->fadepal, s->fade1, ALKU_PALETTE_SIZE);
            start_incremental_fade(s, s->picin, 128);
            printf("ALKU: transitioning to phase %d\n", PHASE_HORIZON);
            s->phase = PHASE_HORIZON;
            s->scroll_pos = 1;
            s->page = 1;
            s->frame_count = 0;
        }
        break;

    case PHASE_HORIZON:
        /* Fade in horizon while scrolling */
        tick_fade(s, s->picin);
        do_scroll(s);
        if (s->fade_active <= 0) {
            /* Fade complete, start credits */
            printf("ALKU: transitioning to phase %d\n", PHASE_CREDITS);
            s->phase = PHASE_CREDITS;
            s->credits_index = 0;
            s->frame_count = 60; /* Start with delay before first credit */
        }
        break;

    case PHASE_CREDITS:
        /* Continue scrolling, show credits at intervals */
        do_scroll(s);

        /* Check if time to show next credit */
        if (s->frame_count == 0 && s->credits_index < 5) {
            /* Clear previous text and prepare new */
            memset(s->tbuf, 0, sizeof(s->tbuf));

            switch (s->credits_index) {
            case 0:
                addtext(s, 160, 50, "Graphics");
                addtext(s, 160, 90, "Marvel");
                addtext(s, 160, 130, "Pixel");
                break;
            case 1:
                addtext(s, 160, 50, "Music");
                addtext(s, 160, 90, "Purple Motion");
                addtext(s, 160, 130, "Skaven");
                break;
            case 2:
                addtext(s, 160, 30, "Code");
                addtext(s, 160, 70, "Psi");
                addtext(s, 160, 110, "Trug");
                addtext(s, 160, 148, "Wildfire");
                break;
            case 3:
                addtext(s, 160, 50, "Additional Design");
                addtext(s, 160, 90, "Abyss");
                addtext(s, 160, 130, "Gore");
                break;
            case 4:
                /* Empty - just continue scrolling */
                break;
            }

            /* Start text fade in */
            memcpy(s->fadepal, s->palette, ALKU_PALETTE_SIZE);
            start_incremental_fade(s, s->textin, 64);
            s->credits_index++;
        }

        /* Tick text fade */
        if (s->fade_active > 0) {
            tick_fade(s, s->textin);
        }

        /* Advance frame counter for credit timing */
        if (s->frame_count > 0) {
            s->frame_count--;
        } else if (s->credits_index <= 5) {
            /* Time for next credit after sync with scroll */
            if ((s->scroll_pos & 1) == 0 && dis_sync() >= 4 + s->credits_index) {
                s->frame_count = 0; /* Trigger next credit */
            }
        }

        /* Check for exit condition */
        if (s->scroll_pos >= 320) {
            printf("ALKU: transitioning to phase %d\n", PHASE_DONE);
            s->phase = PHASE_DONE;
        }
        break;

    case PHASE_DONE:
        printf("ALKU: PHASE_DONE, returning exit\n");
        return 1; /* Signal exit */
    }

    return 0;
}

static void alku_render(sr_part_t *part)
{
    alku_state_t *s = (alku_state_t *)part->user_data;

    /* Only render horizon during horizon/credits phases */
    if (s->phase >= PHASE_HORIZON) {
        render_horizon(s);
    }

    /* Apply text overlay during credits phase */
    if (s->phase == PHASE_CREDITS || s->phase == PHASE_HORIZON) {
        apply_text_overlay(s, s->scroll_pos);
    }
}

static void alku_cleanup(sr_part_t *part)
{
    alku_state_t *s = (alku_state_t *)part->user_data;

    /* Clear state */
    memset(s, 0, sizeof(*s));
}
