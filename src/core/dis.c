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
     * Problem: The S3M orders 0-3 contain pattern jump commands that
     * immediately jump to order 28 (not sequential playback). This was
     * handled by DIS in the original demo but breaks standalone playback.
     *
     * Solution: Use time-based sync for intro phases. Each text screen
     * takes ~5 seconds (1 fade frame + 300 wait frames + 1 fade frame
     * = ~302 frames at 60fps). The intro music (subsong 4) is 21.9 seconds.
     *
     * Timing: sync triggers start of each phase, then code displays text
     * for ~5 seconds before checking next sync. Sync points spaced to allow
     * text display to complete:
     *   sync 0: t < 0.3s (initial black - brief pause)
     *   sync 1: t >= 0.3s (triggers "Future Crew" display, takes ~5s)
     *   sync 2: t >= 5.5s (triggers "Assembly 93" display, takes ~5s)
     *   sync 3: t >= 10.5s (triggers "Second Reality" display, takes ~5s)
     *   sync 4+: t >= 15.5s (horizon + credits)
     */
    if (music_is_playing()) {
        double pos = music_get_position_seconds();
        int order = music_get_current_order();

        /*
         * Use time-based sync for intro phases (sync 0-3).
         * Each text screen needs ~5 seconds (300 frame wait + instant fades).
         * Total intro: ~15 seconds for 3 text screens.
         *
         * After intro completes (t >= 15.5s), use order-based sync for credits.
         * Subsong 4 plays orders 4-13 with ~2.2s per order.
         */

        /* Intro phases: pure time-based (orders 0-3 don't play in S3M) */
        if (pos < 0.3) return 0;     /* Initial black - brief pause */
        if (pos < 5.5) return 1;     /* "A Future Crew Production" ~5s */
        if (pos < 10.5) return 2;    /* "First Presented at Assembly 93" ~5s */
        if (pos < 15.5) return 3;    /* "in Second Reality" ~5s */

        /* Credits phase: order-based sync, offset from subsong 4 start order */
        /* Subsong 4 starts at order 4, so offset = order - 4 + 4 = order */
        /* But we want sync 4 at order 4, sync 5 at order 5, etc. */
        /* Since subsong 4 is ~22s and loops, order progression is reliable */
        if (order >= 8) return 8;    /* Exit after credits */
        if (order >= 7) return 7;    /* Additional credits */
        if (order >= 6) return 6;    /* Code credits */
        if (order >= 5) return 5;    /* Music credits */
        return 4;                    /* Horizon + Graphics credits */
    }

    /* Fallback: frame-based timing (60fps) */
    int frames = dis_state.total_frames;
    if (frames < SYNC_FRAMES_FIRST) return 0;
    int sync_point = 1 + (frames - SYNC_FRAMES_FIRST) / SYNC_FRAMES_PER_POINT;
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
