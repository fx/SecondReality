/**
 * Part Loader Framework - Demo part management and sequencing
 *
 * Manages loading, running, and transitioning between demo parts.
 * Each part follows the original Second Reality structure with
 * init/update/render/cleanup lifecycle.
 */

#ifndef PART_H
#define PART_H

#include <stdint.h>

/**
 * Part identifiers - matches the original SCRIPT sequence
 */
typedef enum {
    SR_PART_ALKU = 0,
    SR_PART_BEG,
    SR_PART_3DS,
    SR_PART_PANIC,
    SR_PART_FCP,
    SR_PART_GLENZ,
    SR_PART_DOTS,
    SR_PART_GRID,
    SR_PART_TECHNO,
    SR_PART_HARD,
    SR_PART_COMAN,
    SR_PART_WATER,
    SR_PART_FOREST,
    SR_PART_TUNNELI,
    SR_PART_TWIST,
    SR_PART_PAM,
    SR_PART_JPLOGO,
    SR_PART_LENS,
    SR_PART_DDSTARS,
    SR_PART_PLZPART,
    SR_PART_ENDPIC,
    SR_PART_ENDSCRL,
    SR_PART_CREDITS,
    SR_PART_START,
    SR_PART_END,
    SR_PART_COUNT
} sr_part_id_t;

/**
 * Part execution state
 */
typedef enum {
    SR_PART_STATE_STOPPED = 0,
    SR_PART_STATE_INITIALIZING,
    SR_PART_STATE_RUNNING,
    SR_PART_STATE_CLEANUP
} sr_part_state_t;

/* Forward declaration */
typedef struct sr_part_t sr_part_t;

/**
 * Part lifecycle function types
 */
typedef void (*sr_part_init_fn)(sr_part_t *part);
typedef int (*sr_part_update_fn)(sr_part_t *part, int frame_count);
typedef void (*sr_part_render_fn)(sr_part_t *part);
typedef void (*sr_part_cleanup_fn)(sr_part_t *part);

/**
 * Part transition callback - called when parts change
 */
typedef void (*sr_part_transition_fn)(int from_index, int to_index);

/**
 * Part structure - defines a demo part
 */
struct sr_part_t {
    const char *name;           /* Part name (e.g., "ALKU") */
    const char *description;    /* Part description */
    sr_part_id_t id;            /* Part identifier */
    sr_part_state_t state;      /* Current state */

    /* Lifecycle callbacks */
    sr_part_init_fn init;       /* Called once when part starts */
    sr_part_update_fn update;   /* Called each frame, returns non-zero to advance */
    sr_part_render_fn render;   /* Called each frame to render */
    sr_part_cleanup_fn cleanup; /* Called when part ends */

    void *user_data;            /* Part-specific data */
};

/**
 * Initialize the part loader system.
 */
void part_loader_init(void);

/**
 * Shutdown the part loader system.
 */
void part_loader_shutdown(void);

/**
 * Register a part with the loader.
 * @param part Pointer to part structure (must remain valid)
 * @return 0 on success, -1 if registry full
 */
int part_loader_register(sr_part_t *part);

/**
 * Start running parts from given index.
 * @param start_index Index to start from (0 = first registered part)
 * @return 0 on success, -1 if invalid index
 */
int part_loader_start(int start_index);

/**
 * Update the current part (call each frame).
 * Handles state machine transitions.
 */
void part_loader_tick(void);

/**
 * Render the current part (call each frame).
 */
void part_loader_render(void);

/**
 * Advance to the next part.
 * @return 0 on success, -1 if no more parts
 */
int part_loader_next(void);

/**
 * Get the currently running part.
 * @return Pointer to current part, NULL if none
 */
sr_part_t *part_loader_current(void);

/**
 * Get the current part index.
 * @return Current index, -1 if not running
 */
int part_loader_get_index(void);

/**
 * Get the number of registered parts.
 * @return Number of parts
 */
int part_loader_get_count(void);

/**
 * Check if the loader is running.
 * @return 1 if running, 0 if stopped
 */
int part_loader_is_running(void);

/**
 * Set callback for part transitions.
 * @param callback Function to call on transitions (NULL to remove)
 */
void part_loader_set_transition_callback(sr_part_transition_fn callback);

#endif /* PART_H */
