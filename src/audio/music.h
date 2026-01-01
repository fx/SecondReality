/**
 * Music subsystem for Second Reality cross-platform refactor
 *
 * Provides S3M/MOD music playback using libopenmpt with Sokol Audio backend.
 * Thread-safe position queries for DIS music synchronization.
 */

#ifndef MUSIC_H
#define MUSIC_H

#include <stddef.h>
#include <stdbool.h>

/**
 * Initialize the music subsystem.
 * Must be called before any other music functions.
 * Initializes Sokol Audio with appropriate settings for music playback.
 * @return true on success, false on failure
 */
bool music_init(void);

/**
 * Shutdown the music subsystem.
 * Stops playback and releases all resources.
 * Safe to call even if not initialized.
 */
void music_shutdown(void);

/**
 * Load a module from memory.
 * @param data Pointer to module data (S3M, MOD, XM, IT, etc.)
 * @param size Size of module data in bytes
 * @return true on success, false on failure
 */
bool music_load(const void *data, size_t size);

/**
 * Load a module from file.
 * @param path Path to module file
 * @return true on success, false on failure
 */
bool music_load_file(const char *path);

/**
 * Unload the currently loaded module.
 * Stops playback if playing.
 */
void music_unload(void);

/**
 * Start or resume playback.
 */
void music_play(void);

/**
 * Pause playback (can be resumed with music_play).
 */
void music_pause(void);

/**
 * Stop playback and reset position to beginning.
 */
void music_stop(void);

/**
 * Check if music is currently playing.
 * @return true if playing, false if paused or stopped
 */
bool music_is_playing(void);

/**
 * Get current playback position in seconds.
 * Thread-safe for use from any thread.
 * @return Position in seconds, or 0.0 if not playing
 */
double music_get_position_seconds(void);

/**
 * Get current order (pattern sequence position).
 * Thread-safe for DIS synchronization.
 * @return Current order number (0-based)
 */
int music_get_current_order(void);

/**
 * Get current pattern number.
 * Thread-safe for DIS synchronization.
 * @return Current pattern number
 */
int music_get_current_pattern(void);

/**
 * Get current row within the pattern.
 * Thread-safe for DIS synchronization.
 * @return Current row number (0-63 typically for S3M)
 */
int music_get_current_row(void);

/**
 * Set playback position by order and row.
 * @param order Order number to seek to
 * @param row Row within the pattern (0-63)
 */
void music_set_position(int order, int row);

/**
 * Get total duration of the module in seconds.
 * @return Duration in seconds, or 0.0 if no module loaded
 */
double music_get_duration(void);

/**
 * Get number of orders (patterns in sequence).
 * @return Number of orders, or 0 if no module loaded
 */
int music_get_num_orders(void);

/**
 * Get number of patterns in the module.
 * @return Number of patterns, or 0 if no module loaded
 */
int music_get_num_patterns(void);

/**
 * Get number of rows in a specific pattern.
 * @param pattern Pattern index
 * @return Number of rows, or 0 if invalid
 */
int music_get_pattern_rows(int pattern);

#endif /* MUSIC_H */
