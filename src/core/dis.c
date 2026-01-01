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
#define SYNC_FRAMES_PER_POINT 300  /* ~5 seconds per sync point */
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
     * Sync points for ALKU (from original MAIN.C comments):
     * 0 = initial (black)
     * 1 = fc_pres ("A Future Crew Production")
     * 2 = first ("First Presented at Assembly 93")
     * 3 = maisema ("in Second Reality" / horizon)
     * 4 = gfx (graphics credits)
     * 5 = music (music credits)
     * 6 = code (code credits)
     * 7 = addi (additional credits)
     * 8 = exit
     *
     * If music is playing, map position to sync points.
     * Otherwise, use frame-based timing (~5 seconds per point).
     */
    if (music_is_playing()) {
        /* Map music position to sync points 0-8 */
        double pos = music_get_position_seconds();
        if (pos < 3.0) return 0;
        if (pos < 8.0) return 1;
        if (pos < 13.0) return 2;
        if (pos < 18.0) return 3;
        if (pos < 23.0) return 4;
        if (pos < 28.0) return 5;
        if (pos < 33.0) return 6;
        if (pos < 38.0) return 7;
        return 8;
    }

    /* Fallback: frame-based timing */
    int sync_point = dis_state.total_frames / SYNC_FRAMES_PER_POINT;
    if (sync_point > SYNC_POINT_MAX) {
        sync_point = SYNC_POINT_MAX;
    }
    return sync_point;
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
