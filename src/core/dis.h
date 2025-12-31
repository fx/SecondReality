/**
 * Demo Interrupt Server (DIS) - Cross-platform reimplementation
 *
 * Original DIS was an interrupt-driven server for DOS that provided:
 * - Frame synchronization (dis_waitb)
 * - Inter-part communication (dis_msgarea)
 * - Music synchronization (dis_muscode, dis_musrow, dis_musplus)
 * - Copper-like raster interrupts (dis_setcopper)
 *
 * This implementation provides the same API using Sokol's frame callbacks.
 */

#ifndef DIS_H
#define DIS_H

#include "sokol_app.h"
#include <stdint.h>

/* DIS version - 0x100 = V1.0 */
#define DIS_VERSION 0x100

/* Message area constants */
#define DIS_MSG_AREA_SIZE  64
#define DIS_MSG_AREA_COUNT 4

/* Copper callback count (0=top, 1=bottom, 2=retrace) */
#define DIS_COPPER_COUNT 3

/* Copper callback function type */
typedef void (*dis_copper_fn)(void);

/**
 * Initialize DIS. Must be called at start of each demo part.
 * Clears the exit flag and initializes internal state.
 * @return Version number (0x100 for V1.0), 0 if not installed
 */
int dis_version(void);

/**
 * Initialize DIS with error handling.
 * Calls dis_version() internally.
 * In cross-platform version, logs warning if called without proper init.
 */
void dis_partstart(void);

/**
 * Wait for vertical blank (frame sync).
 * @return Number of frames since last call
 */
int dis_waitb(void);

/**
 * Check if part should exit.
 * @return 1 if exit requested (ESC pressed), 0 otherwise
 */
int dis_exit(void);

/**
 * Check if running inside the demo vs standalone.
 * @return 1 if inside demo, 0 if run from DOS/standalone
 */
int dis_indemo(void);

/**
 * Get music synchronization code.
 * @param code The code you are waiting for
 * @return Current music code
 */
int dis_muscode(int code);

/**
 * Get music plus value for synchronization.
 * @return Music plus value
 */
int dis_musplus(void);

/**
 * Get music row synchronization.
 * @param row The row you are waiting for
 * @return Current music row
 */
int dis_musrow(int row);

/**
 * Get pointer to inter-part communication area.
 * @param areanumber Area index (0-3)
 * @return Pointer to 64-byte message area, NULL if invalid index
 */
void *dis_msgarea(int areanumber);

/**
 * Set copper interrupt routine.
 * @param routine_number 0=top of screen, 1=bottom of screen, 2=retrace
 * @param routine Callback function (NULL to remove)
 */
void dis_setcopper(int routine_number, dis_copper_fn routine);

/**
 * Set music frame counter.
 * @param frame Frame value to set
 */
void dis_setmframe(int frame);

/**
 * Get music frame counter.
 * @return Current music frame
 */
int dis_getmframe(void);

/**
 * Get sync point value.
 * @return Sync point value
 */
int dis_sync(void);

/* Internal functions for Sokol integration */

/**
 * Called each frame by Sokol frame callback.
 * Increments frame counter and manages timing.
 */
void dis_frame_tick(void);

/**
 * Handle Sokol events (key presses, etc).
 * @param e Sokol event
 */
void dis_handle_event(const sapp_event *e);

/**
 * Reset transient state for part transitions.
 * Clears copper callbacks and resets counters.
 */
void dis_reset(void);

#endif /* DIS_H */
