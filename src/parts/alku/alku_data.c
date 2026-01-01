/**
 * ALKU Data Loading Implementation
 *
 * Parses original ALKU data files (assembly format) into runtime structures.
 */

#include "alku_data.h"
#include <string.h>

int alku_load_font(alku_state_t *state)
{
    /* TODO: Parse FONA.INC */
    memset(state->font, 0, sizeof(state->font));
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
