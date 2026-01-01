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
        int pixels_available = data_size;
        for (y = 0; y < ALKU_HORIZON_HEIGHT && y * 640 < pixels_available; y++) {
            for (x = 0; x < 640 && (y * 640 + x) < pixels_available; x++) {
                /* Simple 4-plane deinterleave: pixel x uses plane (x & 3) */
                int plane_offset = x & 3;
                int byte_in_plane = x >> 2;
                int plane_stride = data_size / 4;
                int src_idx = plane_offset * plane_stride + y * (640 / 4) + byte_in_plane;
                if (src_idx < data_size) {
                    state->horizon[y * ALKU_HORIZON_WIDTH + x] = planar[src_idx];
                }
            }
        }
    }

    /* Load HOI.IN1 for lower half */
    loaded = load_db_file("HOI.IN1", raw_data, 256 * 1024);
    if (loaded > ALKU_HORIZON_HEADER) {
        uint8_t *planar = raw_data + ALKU_HORIZON_HEADER;
        int data_size = loaded - ALKU_HORIZON_HEADER;
        int pixels_available = data_size;
        int y_offset = ALKU_HORIZON_HEIGHT; /* Start at line 150 */

        for (y = 0; y < ALKU_HORIZON_HEIGHT && y * 640 < pixels_available; y++) {
            for (x = 0; x < 640 && (y * 640 + x) < pixels_available; x++) {
                int plane_offset = x & 3;
                int byte_in_plane = x >> 2;
                int plane_stride = data_size / 4;
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

void alku_build_glyphs(alku_state_t *state)
{
    /* TODO: Build glyph lookup from font data */
    memset(state->glyphs, 0, sizeof(state->glyphs));
}

void alku_init_palettes(alku_state_t *state)
{
    /* TODO: Initialize derived palettes */
    memset(state->palette2, 0, sizeof(state->palette2));
    memset(state->fade1, 0, sizeof(state->fade1));
    memset(state->fade2, 0, sizeof(state->fade2));
}
