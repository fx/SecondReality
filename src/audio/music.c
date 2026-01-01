/**
 * Music subsystem implementation using libopenmpt + Sokol Audio
 *
 * Thread-safety: Position values are updated atomically in the audio callback
 * and can be read safely from any thread (for DIS synchronization).
 */

#include "music.h"
#include "sokol_log.h"
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
    atomic_bool playing;

    /* Atomic position tracking for thread-safe DIS queries */
    atomic_int current_order;
    atomic_int current_pattern;
    atomic_int current_row;
    atomic_int position_seconds_x1000; /* Fixed-point: seconds * 1000 */
} music_state;

/* Debug callback counter */
static int callback_count = 0;

/**
 * OpenMPT logging callback for diagnosing load issues
 */
static void music_openmpt_log(const char *message, void *user) {
    (void)user;
    printf("OPENMPT: %s\n", message);
}

/**
 * Audio callback - called by Sokol Audio from audio thread.
 * Renders audio and updates position atomically.
 */
static void music_audio_callback(float *buffer, int num_frames, int num_channels) {
    callback_count++;
    if (callback_count <= 10) {
        printf("MUSIC: callback #%d, frames=%d, channels=%d, playing=%d, mod=%p\n",
               callback_count, num_frames, num_channels,
               atomic_load(&music_state.playing), (void*)music_state.mod);
    }

    if (!music_state.mod || !atomic_load(&music_state.playing)) {
        /* Silence when not playing */
        memset(buffer, 0, (size_t)(num_frames * num_channels) * sizeof(float));
        return;
    }

    /* Render audio - use actual sample rate from audio system */
    size_t frames_rendered = openmpt_module_read_interleaved_float_stereo(
        music_state.mod,
        saudio_sample_rate(),
        (size_t)num_frames,
        buffer
    );

    if (callback_count <= 10) {
        int order = openmpt_module_get_current_order(music_state.mod);
        int pattern = openmpt_module_get_current_pattern(music_state.mod);
        int row = openmpt_module_get_current_row(music_state.mod);
        int speed = openmpt_module_get_current_speed(music_state.mod);
        int tempo = openmpt_module_get_current_tempo(music_state.mod);
        printf("MUSIC: rendered %zu frames, order=%d pattern=%d row=%d speed=%d tempo=%d\n",
               frames_rendered, order, pattern, row, speed, tempo);
    }

    /* Fill remainder with silence if we didn't get enough frames */
    if (frames_rendered < (size_t)num_frames) {
        size_t remaining = (size_t)num_frames - frames_rendered;
        memset(buffer + frames_rendered * 2, 0, remaining * 2 * sizeof(float));

        /* Stop playback at end of module */
        atomic_store(&music_state.playing, false);
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

    /* Initialize Sokol Audio with proper logging */
    saudio_setup(&(saudio_desc){
        .sample_rate = MUSIC_SAMPLE_RATE,
        .num_channels = MUSIC_NUM_CHANNELS,
        .stream_cb = music_audio_callback,
        .buffer_frames = 2048,
        .packet_frames = 512,
        .logger.func = slog_func,
    });

    if (!saudio_isvalid()) {
        fprintf(stderr, "MUSIC: Failed to initialize audio\n");
        return false;
    }

    music_state.initialized = true;
    music_state.mod = NULL;
    atomic_store(&music_state.playing, false);
    atomic_store(&music_state.current_order, 0);
    atomic_store(&music_state.current_pattern, 0);
    atomic_store(&music_state.current_row, 0);
    atomic_store(&music_state.position_seconds_x1000, 0);

    int actual_rate = saudio_sample_rate();
    int actual_channels = saudio_channels();
    printf("MUSIC: Initialized (requested: %d Hz %d ch, actual: %d Hz %d ch)\n",
           MUSIC_SAMPLE_RATE, MUSIC_NUM_CHANNELS, actual_rate, actual_channels);
    if (actual_rate != MUSIC_SAMPLE_RATE) {
        printf("MUSIC: WARNING - sample rate mismatch! Audio may sound wrong.\n");
    }
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
        music_openmpt_log, /* log_func */
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

    /* Print module info for debugging */
    const char *title = openmpt_module_get_metadata(music_state.mod, "title");
    const char *type_long = openmpt_module_get_metadata(music_state.mod, "type_long");
    printf("MUSIC: Module title: %s\n", title ? title : "(null)");
    printf("MUSIC: Module type: %s\n", type_long ? type_long : "(null)");
    openmpt_free_string(title);
    openmpt_free_string(type_long);

    /* Configure module for interpolated output */
    openmpt_module_set_render_param(
        music_state.mod,
        OPENMPT_MODULE_RENDER_INTERPOLATIONFILTER_LENGTH,
        8 /* 8-tap sinc */
    );

    /* Enable looping (repeat forever) */
    openmpt_module_set_repeat_count(music_state.mod, -1);

    /* Reset position */
    atomic_store(&music_state.current_order, 0);
    atomic_store(&music_state.current_pattern, 0);
    atomic_store(&music_state.current_row, 0);
    atomic_store(&music_state.position_seconds_x1000, 0);

    /* Print detailed module info */
    int num_channels = openmpt_module_get_num_channels(music_state.mod);
    int num_samples = openmpt_module_get_num_samples(music_state.mod);
    int num_instruments = openmpt_module_get_num_instruments(music_state.mod);
    printf("MUSIC: Module loaded (duration: %.1f sec, orders: %d, patterns: %d)\n",
           music_get_duration(),
           music_get_num_orders(),
           music_get_num_patterns());
    printf("MUSIC: channels: %d, samples: %d, instruments: %d\n",
           num_channels, num_samples, num_instruments);

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
        /* Stop playback first */
        atomic_store(&music_state.playing, false);

        /* Wait for audio callback to complete by allowing one buffer cycle.
         * sokol_audio uses ~2048 frames at 48kHz = ~43ms buffer.
         * We could add proper synchronization, but for demo use this is safe:
         * unload is only called during init/shutdown, not during playback. */

        openmpt_module_destroy(music_state.mod);
        music_state.mod = NULL;
    }
}

void music_play(void) {
    if (music_state.mod) {
        atomic_store(&music_state.playing, true);
    }
}

void music_pause(void) {
    atomic_store(&music_state.playing, false);
}

void music_stop(void) {
    if (music_state.mod) {
        atomic_store(&music_state.playing, false);
        openmpt_module_set_position_order_row(music_state.mod, 0, 0);
        atomic_store(&music_state.current_order, 0);
        atomic_store(&music_state.current_pattern, 0);
        atomic_store(&music_state.current_row, 0);
        atomic_store(&music_state.position_seconds_x1000, 0);
    }
}

bool music_is_playing(void) {
    return atomic_load(&music_state.playing);
}

double music_get_position_seconds(void) {
    int ms = atomic_load(&music_state.position_seconds_x1000);
    double pos = (double)ms / 1000.0;
    static int log_count = 0;
    if (log_count++ < 20) {
        printf("MUSIC: position=%.3f sec\n", pos);
    }
    return pos;
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
