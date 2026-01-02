/**
 * ALKU Data Structures and Loading
 *
 * Handles parsing of original ALKU data files:
 * - FONA.INC: Font data (32x1500 bytes, 2-bit per pixel)
 * - HOI.IN0/HOI.IN1: Horizon images (640x150 each, stacked to 640x300)
 */

#ifndef ALKU_DATA_H
#define ALKU_DATA_H

#include <stdint.h>

/* Font constants from original */
#define ALKU_FONT_ROWS      32      /* 32 rows per glyph */
#define ALKU_FONT_COLS      1500    /* Total pixel columns in font sheet */
#define ALKU_FONT_SIZE      (ALKU_FONT_ROWS * ALKU_FONT_COLS)

/* Horizon image constants */
#define ALKU_HORIZON_WIDTH  640     /* Width of each horizon image */
#define ALKU_HORIZON_HEIGHT 150     /* Height of each horizon image file */
#define ALKU_HORIZON_TOTAL  300     /* Combined height (IN0 + IN1) */

/* Palette size (256 colors * 3 RGB) */
#define ALKU_PALETTE_SIZE   768

/* Horizon header size (16 bytes info + 768 palette) */
#define ALKU_HORIZON_HEADER 784

/* Text buffer dimensions for credits overlay */
#define ALKU_TBUF_ROWS      186
#define ALKU_TBUF_COLS      352

/* Scroll rate: 1 pixel per SCRLF frames */
#define ALKU_SCROLL_RATE    9

/**
 * Glyph information for a single character
 */
typedef struct {
    int start;      /* Starting X position in font sheet */
    int width;      /* Width in pixels */
} alku_glyph_t;

/**
 * ALKU part state
 */
typedef struct {
    /* Font data - 32 rows x 1500 columns, preprocessed to 0/0x40/0x80/0xC0 */
    uint8_t font[ALKU_FONT_ROWS][ALKU_FONT_COLS];

    /* Glyph lookup table (indexed by ASCII) */
    alku_glyph_t glyphs[256];

    /* Horizon image (640x300 pixels, indexed color) */
    uint8_t horizon[ALKU_HORIZON_WIDTH * ALKU_HORIZON_TOTAL];

    /* Palettes */
    uint8_t palette[ALKU_PALETTE_SIZE];     /* Base horizon palette */
    uint8_t palette2[ALKU_PALETTE_SIZE];    /* Palette with text colors */
    uint8_t fade1[ALKU_PALETTE_SIZE];       /* Black (start) */
    uint8_t fade2[ALKU_PALETTE_SIZE];       /* Text colors */

    /* Fade increments (fixed-point 8.8) */
    int16_t picin[ALKU_PALETTE_SIZE];       /* Fade in picture */
    int16_t textin[ALKU_PALETTE_SIZE];      /* Fade in text */
    int16_t textout[ALKU_PALETTE_SIZE];     /* Fade out text */

    /* Text overlay buffer (186x352) */
    uint8_t tbuf[ALKU_TBUF_ROWS][ALKU_TBUF_COLS];

    /* Scroll state */
    int scroll_pos;         /* Current scroll position (0-320) */
    int page;               /* Double buffer page (0 or 1) */
    int frame_count;        /* Frame counter for timing */

    /* Fade state */
    int fade_pos;           /* Current fade position (0-64 or 0-128) */
    int fade_active;        /* Non-zero if fade in progress */
    uint8_t fadepal[ALKU_PALETTE_SIZE];  /* Current fade palette */

    /* Sequence state */
    int credits_index;      /* Current credits group (0-4) */
    int phase;              /* Current animation phase */
    int sub_phase;          /* Sub-phase: 0=fade-in, 1=display, 2=fade-out */
    int fade_step;          /* Current step in fade (0-63) */
    const uint8_t *fade_src;  /* Source palette for current fade */
    const uint8_t *fade_dst;  /* Destination palette for current fade */
} alku_state_t;

/**
 * Load font data from FONA.INC assembly file.
 * Parses db directives and extracts 2-bit pixel values.
 *
 * @param state State structure to populate font data
 * @return 0 on success, -1 on error
 */
int alku_load_font(alku_state_t *state);

/**
 * Load horizon images from HOI.IN0 and HOI.IN1.
 * Parses assembly db directives, extracts palette and pixel data.
 *
 * @param state State structure to populate horizon and palette
 * @return 0 on success, -1 on error
 */
int alku_load_horizon(alku_state_t *state);

/**
 * Build glyph lookup table from font data.
 * Maps characters using fonaorder to their positions in the font sheet.
 *
 * @param state State structure with font data loaded
 */
void alku_build_glyphs(alku_state_t *state);

/**
 * Initialize palettes for fading effects.
 * Sets up palette2, fade1, fade2, and fade increments.
 *
 * @param state State structure with base palette loaded
 */
void alku_init_palettes(alku_state_t *state);

#endif /* ALKU_DATA_H */
