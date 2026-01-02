/**
 * ALKU Data Loading Implementation
 *
 * Parses original ALKU data files (assembly format) into runtime structures.
 */

#include "alku_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Embedded font data parsed from FONA.INC at build time */
/* This will be generated or we embed directly */

/**
 * Parse a line of assembly db values into a byte buffer.
 * Format: "db val1,val2,val3,..." or "\tdb\tval1,val2,..."
 *
 * @param line Input line
 * @param buf Output buffer
 * @param max_bytes Maximum bytes to parse
 * @return Number of bytes parsed
 */
static int parse_db_line(const char *line, uint8_t *buf, int max_bytes)
{
    const char *p = line;
    int count = 0;

    /* Skip whitespace and find 'db' */
    while (*p && isspace((unsigned char)*p)) p++;
    if (strncmp(p, "db", 2) != 0) return 0;
    p += 2;

    /* Parse comma-separated values */
    while (*p && count < max_bytes) {
        /* Skip whitespace */
        while (*p && (isspace((unsigned char)*p) || *p == '\t')) p++;
        if (!*p || *p == '\r' || *p == '\n') break;

        /* Parse number */
        int val = 0;
        while (*p >= '0' && *p <= '9') {
            val = val * 10 + (*p - '0');
            p++;
        }
        /* Clamp to uint8_t range to prevent overflow */
        if (val > 255) val = 255;
        buf[count++] = (uint8_t)val;

        /* Skip comma */
        while (*p && (isspace((unsigned char)*p) || *p == ',')) {
            if (*p == ',') {
                p++;
                break;
            }
            p++;
        }
    }

    return count;
}

int alku_load_font(alku_state_t *state)
{
    FILE *f;
    char line[4096];
    uint8_t values[256];
    int row = 0;
    int col = 0;
    int total = 0;

    memset(state->font, 0, sizeof(state->font));

    f = fopen("ALKU/FONA.INC", "r");
    if (!f) {
        /* Try alternate path */
        f = fopen("../ALKU/FONA.INC", "r");
    }
    if (!f) {
        fprintf(stderr, "alku: failed to open FONA.INC\n");
        return -1;
    }

    /* Parse each line and fill font array */
    while (fgets(line, sizeof(line), f) && total < ALKU_FONT_SIZE) {
        int count = parse_db_line(line, values, sizeof(values));
        for (int i = 0; i < count && total < ALKU_FONT_SIZE; i++) {
            /* Store 2-bit value, will be converted later */
            state->font[row][col] = values[i];
            col++;
            if (col >= ALKU_FONT_COLS) {
                col = 0;
                row++;
            }
            total++;
        }
    }

    fclose(f);

    /* Convert 2-bit values to palette indices (0, 0x40, 0x80, 0xC0) */
    /* This matches the original init() preprocessing */
    for (row = 0; row < ALKU_FONT_ROWS; row++) {
        for (col = 0; col < ALKU_FONT_COLS; col++) {
            uint8_t val = state->font[row][col] & 3;
            switch (val) {
                case 1: state->font[row][col] = 0x40; break;
                case 2: state->font[row][col] = 0x80; break;
                case 3: state->font[row][col] = 0xC0; break;
                default: state->font[row][col] = 0; break;
            }
        }
    }

    return 0;
}

/**
 * Load raw bytes from an assembly db file into a buffer.
 *
 * @param filename File to load
 * @param buf Output buffer
 * @param max_bytes Maximum bytes to load
 * @return Number of bytes loaded, or -1 on error
 */
static int load_db_file(const char *filename, uint8_t *buf, int max_bytes)
{
    FILE *f;
    char line[4096];
    uint8_t values[256];
    int total = 0;

    f = fopen(filename, "r");
    if (!f) {
        /* Try with ALKU/ prefix */
        char path[256];
        snprintf(path, sizeof(path), "ALKU/%s", filename);
        f = fopen(path, "r");
    }
    if (!f) {
        /* Try with ../ALKU/ prefix */
        char path[256];
        snprintf(path, sizeof(path), "../ALKU/%s", filename);
        f = fopen(path, "r");
    }
    if (!f) {
        return -1;
    }

    while (fgets(line, sizeof(line), f) && total < max_bytes) {
        int count = parse_db_line(line, values, sizeof(values));
        for (int i = 0; i < count && total < max_bytes; i++) {
            buf[total++] = values[i];
        }
    }

    fclose(f);
    return total;
}

int alku_load_horizon(alku_state_t *state)
{
    uint8_t *raw_data;
    int loaded;
    int x, y;

    memset(state->horizon, 0, sizeof(state->horizon));
    memset(state->palette, 0, sizeof(state->palette));

    /* Allocate buffer for raw planar data */
    /* Header (16) + Palette (768) + Planar image data */
    raw_data = (uint8_t *)malloc(256 * 1024);
    if (!raw_data) {
        fprintf(stderr, "alku: failed to allocate horizon buffer\n");
        return -1;
    }

    /* Load HOI.IN0 */
    loaded = load_db_file("HOI.IN0", raw_data, 256 * 1024);
    if (loaded < ALKU_HORIZON_HEADER) {
        fprintf(stderr, "alku: failed to load HOI.IN0 (got %d bytes)\n", loaded);
        free(raw_data);
        return -1;
    }

    /* Extract palette from first file (offset 16, 768 bytes) */
    memcpy(state->palette, raw_data + 16, ALKU_PALETTE_SIZE);

    /*
     * The horizon image is stored in Mode X planar format.
     * Each scanline is 176 bytes per plane (704 pixels / 4 planes).
     * The image is 88 lines tall, replicated to 176 lines.
     *
     * We convert from planar to linear format for our framebuffer.
     * In the original: outline() copies 4 bytes at position a*4+784
     * which means data starts at offset 784 and each scanline's 4 planes
     * are stored sequentially (4 bytes = one pixel column across 4 planes).
     */

    /* For simplicity, convert planar to linear 640-pixel wide scanlines */
    /* The data layout in HOI files: each group of 4 bytes represents
     * 4 pixels (one from each plane) at the same X position */
    {
        uint8_t *planar = raw_data + ALKU_HORIZON_HEADER;
        int data_size = loaded - ALKU_HORIZON_HEADER;

        /* Simpler approach: just copy data linearly and let rendering handle it */
        /* The original uses a custom outline() function for Mode X */
        /* For our linear framebuffer, we copy 640 pixels per line */

        /* Validate data_size is sufficient for plane calculation */
        if (data_size < 4) {
            fprintf(stderr, "alku: HOI.IN0 data too small (%d bytes)\n", data_size);
            free(raw_data);
            return -1;
        }

        int pixels_available = data_size;
        int plane_stride = data_size / 4;
        for (y = 0; y < ALKU_HORIZON_HEIGHT && y * 640 < pixels_available; y++) {
            for (x = 0; x < 640 && (y * 640 + x) < pixels_available; x++) {
                /* Simple 4-plane deinterleave: pixel x uses plane (x & 3) */
                int plane_offset = x & 3;
                int byte_in_plane = x >> 2;
                int src_idx = plane_offset * plane_stride + y * (640 / 4) + byte_in_plane;
                if (src_idx < data_size) {
                    state->horizon[y * ALKU_HORIZON_WIDTH + x] = planar[src_idx];
                }
            }
        }
    }

    /* Load HOI.IN1 for lower half */
    loaded = load_db_file("HOI.IN1", raw_data, 256 * 1024);
    if (loaded < ALKU_HORIZON_HEADER) {
        fprintf(stderr, "alku: failed to load HOI.IN1 (got %d bytes)\n", loaded);
        free(raw_data);
        return -1;
    }
    if (loaded > ALKU_HORIZON_HEADER) {
        uint8_t *planar = raw_data + ALKU_HORIZON_HEADER;
        int data_size = loaded - ALKU_HORIZON_HEADER;
        int pixels_available = data_size;
        int y_offset = ALKU_HORIZON_HEIGHT; /* Start at line 150 */
        int plane_stride = data_size / 4;

        for (y = 0; y < ALKU_HORIZON_HEIGHT && y * 640 < pixels_available; y++) {
            for (x = 0; x < 640 && (y * 640 + x) < pixels_available; x++) {
                int plane_offset = x & 3;
                int byte_in_plane = x >> 2;
                int src_idx = plane_offset * plane_stride + y * (640 / 4) + byte_in_plane;
                if (src_idx < data_size) {
                    state->horizon[(y + y_offset) * ALKU_HORIZON_WIDTH + x] = planar[src_idx];
                }
            }
        }
    }

    free(raw_data);
    return 0;
}

/* Character order in the font sheet - matches original */
static const char *fonaorder =
    "ABCDEFGHIJKLMNOPQRSTUVWXabcdefghijklmnopqrstuvwxyz0123456789!?,.:()+-*='";

void alku_build_glyphs(alku_state_t *state)
{
    const char *order = fonaorder;
    int x = 0;
    int y, start, end;

    memset(state->glyphs, 0, sizeof(state->glyphs));

    /*
     * Scan through font columns to find character boundaries.
     * A character starts when we hit a non-zero column and ends
     * when we hit an all-zero column again.
     */
    while (x < ALKU_FONT_COLS && *order) {
        /* Skip empty columns to find start of next glyph */
        while (x < ALKU_FONT_COLS) {
            int has_pixels = 0;
            for (y = 0; y < ALKU_FONT_ROWS; y++) {
                if (state->font[y][x]) {
                    has_pixels = 1;
                    break;
                }
            }
            if (has_pixels) break;
            x++;
        }
        if (x >= ALKU_FONT_COLS) break;

        start = x;

        /* Scan to find end of glyph (next all-zero column) */
        while (x < ALKU_FONT_COLS) {
            int has_pixels = 0;
            for (y = 0; y < ALKU_FONT_ROWS; y++) {
                if (state->font[y][x]) {
                    has_pixels = 1;
                    break;
                }
            }
            if (!has_pixels) break;
            x++;
        }

        end = x;

        /* Record glyph position and width */
        state->glyphs[(unsigned char)*order].start = start;
        state->glyphs[(unsigned char)*order].width = end - start;
        order++;
    }

    /* Space character - fixed width at end of font */
    state->glyphs[(unsigned char)' '].start = ALKU_FONT_COLS - 20;
    state->glyphs[(unsigned char)' '].width = 16;
}

void alku_init_palettes(alku_state_t *state)
{
    int i;

    /* fade1 = black (all zeros) - start of fades */
    memset(state->fade1, 0, sizeof(state->fade1));

    /*
     * Build palette2 (horizon + text colors) and fade2 (text colors).
     *
     * Original layout uses palette ranges:
     * - 0-63: Base horizon colors (copied as-is)
     * - 64-127: Text shade 1 (blended with palette color 1)
     * - 128-191: Text shade 2 (blended with palette color 2)
     * - 192-255: Text shade 3 (blended with palette color 3)
     *
     * The text colors in fade2 are the blend factors.
     */
    for (i = 0; i < ALKU_PALETTE_SIZE; i += 3) {
        int idx = i / 3; /* Color index 0-255 */

        if (idx < 64) {
            /* Base horizon colors - copy directly */
            state->palette2[i + 0] = state->palette[i + 0];
            state->palette2[i + 1] = state->palette[i + 1];
            state->palette2[i + 2] = state->palette[i + 2];
            state->fade2[i + 0] = 0;
            state->fade2[i + 1] = 0;
            state->fade2[i + 2] = 0;
        } else {
            /* Text colors: blend base color with text shade */
            int shade_idx = (idx >= 192) ? 3 : (idx >= 128) ? 2 : 1;
            int base_idx = idx % 64;
            int shade_offset = shade_idx * 3;
            int base_offset = base_idx * 3;

            /* fade2 = text color (shade color) */
            state->fade2[i + 0] = state->palette[shade_offset + 0];
            state->fade2[i + 1] = state->palette[shade_offset + 1];
            state->fade2[i + 2] = state->palette[shade_offset + 2];

            /* palette2 = blended color for text over horizon */
            /* Formula: text_color * 63 + base_color * (63 - text_color) >> 6 */
            int tr = state->palette[shade_offset + 0];
            int tg = state->palette[shade_offset + 1];
            int tb = state->palette[shade_offset + 2];
            int br = state->palette[base_offset + 0];
            int bg = state->palette[base_offset + 1];
            int bb = state->palette[base_offset + 2];

            state->palette2[i + 0] = (tr * 63 + br * (63 - tr)) >> 6;
            state->palette2[i + 1] = (tg * 63 + bg * (63 - tg)) >> 6;
            state->palette2[i + 2] = (tb * 63 + bb * (63 - tb)) >> 6;
        }
    }

    /* Extend palette: colors 192-255 = copy of 0-63 (for scrolling) */
    for (i = 192 * 3; i < ALKU_PALETTE_SIZE; i++) {
        state->palette[i] = state->palette[i - 192 * 3];
    }

    /* Calculate fade increments (fixed-point 8.8 format) */
    for (i = 0; i < ALKU_PALETTE_SIZE; i++) {
        /* textin: fade from palette to palette2 in 64 steps */
        state->textin[i] = ((int)state->palette2[i] - (int)state->palette[i]) * 256 / 64;

        /* textout: fade from palette2 to palette in 64 steps */
        state->textout[i] = ((int)state->palette[i] - (int)state->palette2[i]) * 256 / 64;

        /* picin: fade from black to palette in 128 steps */
        state->picin[i] = ((int)state->palette[i] - (int)state->fade1[i]) * 256 / 128;
    }
}
