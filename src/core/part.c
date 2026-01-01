/**
 * Part Loader Framework - Implementation
 *
 * Manages demo part lifecycle and sequencing.
 */

#include "part.h"
#include "dis.h"
#include "video.h"
#include <stdio.h>
#include <string.h>

/* Maximum number of parts that can be registered */
#define PART_REGISTRY_MAX 32

/* Static state */
static sr_part_t *s_registry[PART_REGISTRY_MAX];
static int s_registry_count = 0;
static int s_current_index = -1;
static int s_running = 0;
static sr_part_transition_fn s_transition_callback = NULL;

/**
 * Clear video state for part transition.
 * Sets palette to black and clears framebuffer.
 */
static void part_clear_video(void) {
    /* Set all palette entries to black */
    for (int i = 0; i < 256; i++) {
        video_set_color((uint8_t)i, 0, 0, 0);
    }
    /* Clear framebuffer to index 0 (black) */
    video_clear(0);
}

/**
 * Transition to a new part.
 */
static void part_transition(int from_index, int to_index) {
    printf("[part] Transitioning from %d to %d\n", from_index, to_index);

    /* Notify callback */
    if (s_transition_callback) {
        s_transition_callback(from_index, to_index);
    }

    /* Reset DIS state */
    dis_reset();

    /* Clear video state */
    part_clear_video();
}

void part_loader_init(void) {
    printf("[part] Part loader initialized\n");
    s_registry_count = 0;
    s_current_index = -1;
    s_running = 0;
    s_transition_callback = NULL;
    memset(s_registry, 0, sizeof(s_registry));
}

void part_loader_shutdown(void) {
    printf("[part] Part loader shutdown\n");

    /* Cleanup current part if running */
    if (s_running && s_current_index >= 0 && s_current_index < s_registry_count) {
        sr_part_t *part = s_registry[s_current_index];
        if (part && part->cleanup) {
            part->state = SR_PART_STATE_CLEANUP;
            part->cleanup(part);
            part->state = SR_PART_STATE_STOPPED;
        }
    }

    s_registry_count = 0;
    s_current_index = -1;
    s_running = 0;
}

int part_loader_register(sr_part_t *part) {
    if (!part) {
        printf("[part] Error: NULL part pointer\n");
        return -1;
    }

    if (s_registry_count >= PART_REGISTRY_MAX) {
        printf("[part] Error: Registry full (max %d parts)\n", PART_REGISTRY_MAX);
        return -1;
    }

    part->state = SR_PART_STATE_STOPPED;
    s_registry[s_registry_count] = part;
    printf("[part] Registered part %d: %s\n", s_registry_count, part->name ? part->name : "(unnamed)");
    s_registry_count++;

    return 0;
}

int part_loader_start(int start_index) {
    if (start_index < 0 || start_index >= s_registry_count) {
        printf("[part] Error: Invalid start index %d (count=%d)\n", start_index, s_registry_count);
        return -1;
    }

    s_current_index = start_index;
    s_running = 1;

    /* Prepare for first part */
    part_transition(-1, s_current_index);

    /* Initialize first part */
    sr_part_t *part = s_registry[s_current_index];
    if (part) {
        printf("[part] Starting part: %s\n", part->name ? part->name : "(unnamed)");
        part->state = SR_PART_STATE_INITIALIZING;
        if (part->init) {
            part->init(part);
        }
        part->state = SR_PART_STATE_RUNNING;
    }

    return 0;
}

void part_loader_tick(void) {
    if (!s_running || s_current_index < 0 || s_current_index >= s_registry_count) {
        return;
    }

    sr_part_t *part = s_registry[s_current_index];
    if (!part || part->state != SR_PART_STATE_RUNNING) {
        return;
    }

    /* Get frame count from DIS */
    int frame_count = dis_waitb();

    /* Update the part - non-zero return means advance to next */
    if (part->update) {
        int result = part->update(part, frame_count);
        if (result != 0) {
            part_loader_next();
        }
    }
}

void part_loader_render(void) {
    if (!s_running || s_current_index < 0 || s_current_index >= s_registry_count) {
        return;
    }

    sr_part_t *part = s_registry[s_current_index];
    if (!part || part->state != SR_PART_STATE_RUNNING) {
        return;
    }

    if (part->render) {
        part->render(part);
    }
}

int part_loader_next(void) {
    if (!s_running || s_current_index < 0) {
        return -1;
    }

    /* Cleanup current part */
    sr_part_t *current = s_registry[s_current_index];
    if (current) {
        printf("[part] Ending part: %s\n", current->name ? current->name : "(unnamed)");
        current->state = SR_PART_STATE_CLEANUP;
        if (current->cleanup) {
            current->cleanup(current);
        }
        current->state = SR_PART_STATE_STOPPED;
    }

    int from_index = s_current_index;
    s_current_index++;

    /* Check if we've reached the end */
    if (s_current_index >= s_registry_count) {
        printf("[part] Demo sequence complete\n");
        s_running = 0;
        s_current_index = -1;
        return -1;
    }

    /* Transition to next part */
    part_transition(from_index, s_current_index);

    /* Initialize next part */
    sr_part_t *next = s_registry[s_current_index];
    if (next) {
        printf("[part] Starting part: %s\n", next->name ? next->name : "(unnamed)");
        next->state = SR_PART_STATE_INITIALIZING;
        if (next->init) {
            next->init(next);
        }
        next->state = SR_PART_STATE_RUNNING;
    }

    return 0;
}

sr_part_t *part_loader_current(void) {
    if (!s_running || s_current_index < 0 || s_current_index >= s_registry_count) {
        return NULL;
    }
    return s_registry[s_current_index];
}

int part_loader_get_index(void) {
    if (!s_running) {
        return -1;
    }
    return s_current_index;
}

int part_loader_get_count(void) {
    return s_registry_count;
}

int part_loader_is_running(void) {
    return s_running;
}

void part_loader_set_transition_callback(sr_part_transition_fn callback) {
    s_transition_callback = callback;
}
