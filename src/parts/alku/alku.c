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
 * Perform a blocking palette fade between two palettes.
 * Interpolates in 64 steps, updating video palette each frame.
 *
 * @param s Part state
 * @param pal1 Source palette (768 bytes)
 * @param pal2 Target palette (768 bytes)
 */
static void dofade(alku_state_t *s, const uint8_t *pal1, const uint8_t *pal2)
{
    uint8_t pal[ALKU_PALETTE_SIZE];
    int step, i;

    for (step = 0; step < 64 && !dis_exit(); step++) {
        for (i = 0; i < ALKU_PALETTE_SIZE; i++) {
            /* Linear interpolation: pal1*(64-step) + pal2*step >> 6 */
            pal[i] = (pal1[i] * (64 - step) + pal2[i] * step) >> 6;
        }
        video_set_palette(pal);
        dis_waitb(); /* Wait for frame */
    }
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
