/**
 * Music subsystem implementation using libopenmpt + Sokol Audio
 *
 * Thread-safety: Position values are updated atomically in the audio callback
 * and can be read safely from any thread (for DIS synchronization).
 */

#include "music.h"
#include "sokol_audio.h"
#include <libopenmpt/libopenmpt.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Audio configuration */
#define MUSIC_SAMPLE_RATE 48000
#define MUSIC_NUM_CHANNELS 2

/* Internal state */
static struct {
    openmpt_module *mod;
    bool initialized;
    bool playing;

    /* Atomic position tracking for thread-safe DIS queries */
    atomic_int current_order;
    atomic_int current_pattern;
    atomic_int current_row;
    atomic_int position_seconds_x1000; /* Fixed-point: seconds * 1000 */
} music_state;

/**
 * Audio callback - called by Sokol Audio from audio thread.
 * Renders audio and updates position atomically.
 */
static void music_audio_callback(float *buffer, int num_frames, int num_channels) {
    if (!music_state.mod || !music_state.playing) {
        /* Silence when not playing */
        memset(buffer, 0, (size_t)(num_frames * num_channels) * sizeof(float));
        return;
    }

    /* Render audio */
    size_t frames_rendered = openmpt_module_read_interleaved_float_stereo(
        music_state.mod,
        MUSIC_SAMPLE_RATE,
        (size_t)num_frames,
        buffer
    );

    /* Fill remainder with silence if we didn't get enough frames */
    if (frames_rendered < (size_t)num_frames) {
        size_t remaining = (size_t)num_frames - frames_rendered;
        memset(buffer + frames_rendered * 2, 0, remaining * 2 * sizeof(float));

        /* Stop playback at end of module */
        music_state.playing = false;
    }

    /* Update position atomically for thread-safe queries */
    atomic_store(&music_state.current_order,
                 openmpt_module_get_current_order(music_state.mod));
    atomic_store(&music_state.current_pattern,
                 openmpt_module_get_current_pattern(music_state.mod));
    atomic_store(&music_state.current_row,
                 openmpt_module_get_current_row(music_state.mod));

    double pos = openmpt_module_get_position_seconds(music_state.mod);
    atomic_store(&music_state.position_seconds_x1000, (int)(pos * 1000.0));
}

bool music_init(void) {
    if (music_state.initialized) {
        return true;
    }

    /* Initialize Sokol Audio */
    saudio_setup(&(saudio_desc){
        .sample_rate = MUSIC_SAMPLE_RATE,
        .num_channels = MUSIC_NUM_CHANNELS,
        .stream_cb = music_audio_callback,
        .buffer_frames = 2048,
        .packet_frames = 512,
    });

    if (!saudio_isvalid()) {
        fprintf(stderr, "MUSIC: Failed to initialize audio\n");
        return false;
    }

    music_state.initialized = true;
    music_state.mod = NULL;
    music_state.playing = false;
    atomic_store(&music_state.current_order, 0);
    atomic_store(&music_state.current_pattern, 0);
    atomic_store(&music_state.current_row, 0);
    atomic_store(&music_state.position_seconds_x1000, 0);

    printf("MUSIC: Initialized (sample rate: %d Hz)\n", saudio_sample_rate());
    return true;
}

void music_shutdown(void) {
    if (!music_state.initialized) {
        return;
    }

    music_unload();
    saudio_shutdown();
    music_state.initialized = false;
    printf("MUSIC: Shutdown complete\n");
}

bool music_load(const void *data, size_t size) {
    if (!music_state.initialized) {
        fprintf(stderr, "MUSIC: Not initialized\n");
        return false;
    }

    /* Unload existing module */
    music_unload();

    /* Load new module */
    music_state.mod = openmpt_module_create_from_memory2(
        data, size,
        NULL, /* log_func */
        NULL, /* log_user */
        NULL, /* error_func */
        NULL, /* error_user */
        NULL, /* error_code */
        NULL, /* error_message */
        NULL  /* ctls */
    );

    if (!music_state.mod) {
        fprintf(stderr, "MUSIC: Failed to load module\n");
        return false;
    }

    /* Configure module for interpolated output */
    openmpt_module_set_render_param(
        music_state.mod,
        OPENMPT_MODULE_RENDER_INTERPOLATIONFILTER_LENGTH,
        8 /* 8-tap sinc */
    );

    /* Reset position */
    atomic_store(&music_state.current_order, 0);
    atomic_store(&music_state.current_pattern, 0);
    atomic_store(&music_state.current_row, 0);
    atomic_store(&music_state.position_seconds_x1000, 0);

    printf("MUSIC: Module loaded (duration: %.1f sec, orders: %d, patterns: %d)\n",
           music_get_duration(),
           music_get_num_orders(),
           music_get_num_patterns());

    return true;
}

bool music_load_file(const char *path) {
    if (!path) {
        return false;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "MUSIC: Cannot open file: %s\n", path);
        return false;
    }

    /* Get file size */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        fprintf(stderr, "MUSIC: Invalid file size\n");
        fclose(f);
        return false;
    }

    /* Read file into memory */
    void *data = malloc((size_t)size);
    if (!data) {
        fprintf(stderr, "MUSIC: Out of memory\n");
        fclose(f);
        return false;
    }

    size_t read = fread(data, 1, (size_t)size, f);
    fclose(f);

    if (read != (size_t)size) {
        fprintf(stderr, "MUSIC: Read error\n");
        free(data);
        return false;
    }

    /* Load module from memory */
    bool result = music_load(data, (size_t)size);
    free(data);

    if (result) {
        printf("MUSIC: Loaded file: %s\n", path);
    }

    return result;
}

void music_unload(void) {
    if (music_state.mod) {
        music_state.playing = false;
        openmpt_module_destroy(music_state.mod);
        music_state.mod = NULL;
    }
}

void music_play(void) {
    if (music_state.mod) {
        music_state.playing = true;
    }
}

void music_pause(void) {
    music_state.playing = false;
}

void music_stop(void) {
    if (music_state.mod) {
        music_state.playing = false;
        openmpt_module_set_position_order_row(music_state.mod, 0, 0);
        atomic_store(&music_state.current_order, 0);
        atomic_store(&music_state.current_pattern, 0);
        atomic_store(&music_state.current_row, 0);
        atomic_store(&music_state.position_seconds_x1000, 0);
    }
}

bool music_is_playing(void) {
    return music_state.playing;
}

double music_get_position_seconds(void) {
    int ms = atomic_load(&music_state.position_seconds_x1000);
    return (double)ms / 1000.0;
}

int music_get_current_order(void) {
    return atomic_load(&music_state.current_order);
}

int music_get_current_pattern(void) {
    return atomic_load(&music_state.current_pattern);
}

int music_get_current_row(void) {
    return atomic_load(&music_state.current_row);
}

void music_set_position(int order, int row) {
    if (music_state.mod) {
        openmpt_module_set_position_order_row(music_state.mod, order, row);
        atomic_store(&music_state.current_order, order);
        atomic_store(&music_state.current_row, row);
    }
}

double music_get_duration(void) {
    if (!music_state.mod) {
        return 0.0;
    }
    return openmpt_module_get_duration_seconds(music_state.mod);
}

int music_get_num_orders(void) {
    if (!music_state.mod) {
        return 0;
    }
    return openmpt_module_get_num_orders(music_state.mod);
}

int music_get_num_patterns(void) {
    if (!music_state.mod) {
        return 0;
    }
    return openmpt_module_get_num_patterns(music_state.mod);
}

int music_get_pattern_rows(int pattern) {
    if (!music_state.mod) {
        return 0;
    }
    return openmpt_module_get_pattern_num_rows(music_state.mod, pattern);
}
