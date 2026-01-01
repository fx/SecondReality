/**
 * Music subsystem stub for Emscripten builds
 *
 * No-op implementation of the music API when libopenmpt is not available.
 * This allows the demo to build and run without music support.
 */

#include "music.h"

bool music_init(void) {
    return true;
}

void music_shutdown(void) {
}

bool music_load(const void *data, size_t size) {
    (void)data;
    (void)size;
    return false;
}

bool music_load_file(const char *path) {
    (void)path;
    return false;
}

void music_unload(void) {
}

void music_play(void) {
}

void music_pause(void) {
}

void music_stop(void) {
}

bool music_is_playing(void) {
    return false;
}

double music_get_position_seconds(void) {
    return 0.0;
}

int music_get_current_order(void) {
    return 0;
}

int music_get_current_pattern(void) {
    return 0;
}

int music_get_current_row(void) {
    return 0;
}

void music_set_position(int order, int row) {
    (void)order;
    (void)row;
}

double music_get_duration(void) {
    return 0.0;
}

int music_get_num_orders(void) {
    return 0;
}

int music_get_num_patterns(void) {
    return 0;
}

int music_get_pattern_rows(int pattern) {
    (void)pattern;
    return 0;
}
