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

int alku_load_horizon(alku_state_t *state)
{
    /* TODO: Parse HOI.IN0 and HOI.IN1 */
    memset(state->horizon, 0, sizeof(state->horizon));
    memset(state->palette, 0, sizeof(state->palette));
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
