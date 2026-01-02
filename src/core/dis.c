/**
 * Demo Interrupt Server (DIS) - Cross-platform implementation
 *
 * Reimplements the original DOS interrupt-driven DIS server using
 * Sokol's frame-based callback system.
 */

#include "dis.h"
#include "audio/music.h"
#include <string.h>
#include <stdio.h>

/* Sync point timing (frames at 60fps) */
#define SYNC_FRAMES_PER_POINT 600  /* ~10 seconds per sync point */
#define SYNC_FRAMES_FIRST 1080     /* ~18 seconds until first sync point */
#define SYNC_POINT_MAX 8

/* Internal DIS state */
static struct {
    int initialized;
    int exit_flag;
    int frame_counter;
    int total_frames;           /* Total frames since part start (for sync) */
    int music_frame;
    int music_code;
    int music_row;
    int music_plus;
    uint8_t msg_areas[DIS_MSG_AREA_COUNT][DIS_MSG_AREA_SIZE];
    dis_copper_fn copper[DIS_COPPER_COUNT];
} dis_state;

int dis_version(void) {
    /* Clear transient state */
    dis_state.exit_flag = 0;
    dis_state.frame_counter = 0;
    dis_state.total_frames = 0;
    dis_state.music_code = 0;
    dis_state.music_row = 0;
    dis_state.music_plus = 0;

    /* Mark as initialized */
    dis_state.initialized = 1;

    return DIS_VERSION;
}

void dis_partstart(void) {
    dis_version();
}

int dis_waitb(void) {
    int frames;

    /* Execute copper callbacks in order: top, bottom, retrace */
    for (int i = 0; i < DIS_COPPER_COUNT; i++) {
        if (dis_state.copper[i]) {
            dis_state.copper[i]();
        }
    }

    /* Return frame count and reset */
    frames = dis_state.frame_counter;
    if (frames == 0) {
        frames = 1; /* Always return at least 1 */
    }
    /* NOTE: Not thread-safe. Must be called from the same thread as dis_frame_tick() */
    dis_state.frame_counter = 0;

    return frames;
}

int dis_exit(void) {
    return dis_state.exit_flag;
}

int dis_indemo(void) {
    /* Always return 1 - cross-platform version is always "in demo" */
    return 1;
}

int dis_muscode(int code) {
    (void)code; /* Parameter reserved for skip logic */
    /* Return current order from music subsystem */
    return music_get_current_order();
}

int dis_musplus(void) {
    /* Returns order*64 + row for sync calculations.
     * In tracker terms: order is position in pattern sequence. */
    int order = music_get_current_order();
    int row = music_get_current_row();
    return order * 64 + row;
}

int dis_musrow(int row) {
    (void)row; /* Parameter reserved for skip logic */
    /* Return current row from music subsystem */
    return music_get_current_row();
}

void *dis_msgarea(int areanumber) {
    if (areanumber < 0 || areanumber >= DIS_MSG_AREA_COUNT) {
        fprintf(stderr, "DIS: ERROR Invalid message area %d (valid: 0-%d)\n",
                areanumber, DIS_MSG_AREA_COUNT - 1);
        return NULL;
    }
    return dis_state.msg_areas[areanumber];
}

void dis_setcopper(int routine_number, dis_copper_fn routine) {
    if (routine_number < 0 || routine_number >= DIS_COPPER_COUNT) {
        fprintf(stderr, "DIS: ERROR Invalid copper routine %d (valid: 0-%d)\n",
                routine_number, DIS_COPPER_COUNT - 1);
        return;
    }
    dis_state.copper[routine_number] = routine;
}

void dis_setmframe(int frame) {
    dis_state.music_frame = frame;
}

int dis_getmframe(void) {
    return dis_state.music_frame;
}

int dis_sync(void) {
    /*
     * Sync points for ALKU (from original DIS/DISINT.ASM ordersync1 table):
     *
     * Original mapping (order*256 + row -> sync):
     *   0x0000 (order 0) -> sync 0
     *   0x0200 (order 2) -> sync 1  "A Future Crew Production"
     *   0x0300 (order 3) -> sync 2  "First Presented at Assembly 93"
     *   0x032f (order 3, row 47) -> sync 3  "in Second Reality"
     *   0x042f (order 4, row 47) -> sync 4  Graphics credits
     *   0x052f (order 5, row 47) -> sync 5  Music credits
     *   0x062f (order 6, row 47) -> sync 6  Code credits
     *   0x072f (order 7, row 47) -> sync 7  Additional credits
     *   0x082f (order 8, row 47) -> sync 8  Exit
     *
     * Problem: The S3M orders 0-3 contain pattern jump commands (Bxx) that
     * immediately jump to order 28. The original DIS handled this specially,
     * but we can't replicate that with libopenmpt.
     *
     * Solution: Use ELAPSED FRAME TIME (not music position) for sync.
     * Based on user testing, the original timing is:
     *   - ~15 seconds of intro music before first text
     *   - ~5 seconds per text screen (300 frame wait)
     *   - Total intro: ~30 seconds before credits
     *
     * At 60fps:
     *   sync 0: frames < 900 (0-15s intro music)
     *   sync 1: frames >= 900 (15s - Future Crew, ~5s display)
     *   sync 2: frames >= 1200 (20s - Assembly 93, ~5s display)
     *   sync 3: frames >= 1500 (25s - Second Reality, ~5s display)
     *   sync 4+: frames >= 1800 (30s+ - credits, ~5s each)
     */
    int frames = dis_state.total_frames;

    /* Time-based sync using elapsed frames at 60fps */
    if (frames < 900) return 0;      /* 0-15s: intro music, black screen */
    if (frames < 1200) return 1;     /* 15-20s: "A Future Crew Production" */
    if (frames < 1500) return 2;     /* 20-25s: "First Presented at Assembly 93" */
    if (frames < 1800) return 3;     /* 25-30s: "in Second Reality" */
    if (frames < 2100) return 4;     /* 30-35s: horizon + graphics credits */
    if (frames < 2400) return 5;     /* 35-40s: music credits */
    if (frames < 2700) return 6;     /* 40-45s: code credits */
    if (frames < 3000) return 7;     /* 45-50s: additional credits */
    return 8;                        /* 50s+: exit */
}

/* Internal Sokol integration functions */

void dis_frame_tick(void) {
    dis_state.frame_counter++;
    dis_state.total_frames++;
}

void dis_handle_event(const sapp_event *e) {
    if (!e) {
        return;
    }

    /* ESC key sets exit flag */
    if (e->type == SAPP_EVENTTYPE_KEY_DOWN && e->key_code == SAPP_KEYCODE_ESCAPE) {
        dis_state.exit_flag = 1;
    }
}

/**
 * Reset DIS state for part transitions.
 * Call this before loading a new demo part to clear transient state
 * (exit flag, frame counter, copper callbacks) while preserving
 * persistent state (message areas, music position).
 */
void dis_reset(void) {
    /* Clear transient state for part transitions */
    dis_state.exit_flag = 0;
    dis_state.frame_counter = 0;
    dis_state.total_frames = 0;
    dis_state.music_code = 0;
    dis_state.music_row = 0;
    dis_state.music_plus = 0;

    /* Clear copper callbacks */
    for (int i = 0; i < DIS_COPPER_COUNT; i++) {
        dis_state.copper[i] = NULL;
    }

    /* Note: message areas are NOT cleared - they persist across parts */
}
