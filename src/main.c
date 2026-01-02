#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "core/dis.h"
#include "core/video.h"
#include "core/part.h"
#include "audio/music.h"
#include "parts/alku/alku.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static sg_pass_action pass_action;

/* CLI options */
static int g_start_part = 0;        /* Part index to start from */
static int g_single_part = 0;       /* Exit after first part completes */
static int g_initial_part_done = 0; /* Track if initial part finished */

/* Test Part 1: Red/Blue gradient bars */

typedef struct {
    int frame_counter;
} test_part_data_t;

static test_part_data_t test_part_1_data;
static test_part_data_t test_part_2_data;

static void test_part_1_init(sr_part_t *part) {
    printf("[test_part_1] Initializing\n");
    test_part_data_t *data = (test_part_data_t *)part->user_data;
    data->frame_counter = 0;

    /* Set up red/blue gradient palette */
    for (int i = 0; i < 256; i++) {
        uint8_t r = (uint8_t)((i < 128) ? (i * 63 / 128) : 0);
        uint8_t b = (uint8_t)((i >= 128) ? ((i - 128) * 63 / 128) : 0);
        video_set_color((uint8_t)i, r, 0, b);
    }
}

static int test_part_1_update(sr_part_t *part, int frame_count) {
    test_part_data_t *data = (test_part_data_t *)part->user_data;
    data->frame_counter += frame_count;

    /* Transition after 200 frames */
    if (data->frame_counter >= 200) {
        printf("[test_part_1] Reached %d frames, transitioning\n", data->frame_counter);
        return 1;
    }
    return 0;
}

static void test_part_1_render(sr_part_t *part) {
    test_part_data_t *data = (test_part_data_t *)part->user_data;
    uint8_t *fb = video_get_framebuffer();

    /* Animated red/blue gradient bars */
    int offset = data->frame_counter % 256;
    for (int y = 0; y < VIDEO_HEIGHT_13H; y++) {
        for (int x = 0; x < VIDEO_WIDTH; x++) {
            /* Create vertical bars with animation */
            int bar = (x / 20 + offset / 4) % 16;
            uint8_t color = (uint8_t)(bar < 8 ? bar * 16 : 128 + (bar - 8) * 16);
            fb[y * VIDEO_WIDTH + x] = color;
        }
    }
}

static void test_part_1_cleanup(sr_part_t *part) {
    (void)part;
    printf("[test_part_1] Cleanup\n");
}

/* Test Part 2: Green/Yellow gradient bars */

static void test_part_2_init(sr_part_t *part) {
    printf("[test_part_2] Initializing\n");
    test_part_data_t *data = (test_part_data_t *)part->user_data;
    data->frame_counter = 0;

    /* Set up green/yellow gradient palette */
    for (int i = 0; i < 256; i++) {
        uint8_t g = (uint8_t)(i * 63 / 255);
        uint8_t r = (uint8_t)((i >= 128) ? ((i - 128) * 63 / 128) : 0);
        video_set_color((uint8_t)i, r, g, 0);
    }
}

static int test_part_2_update(sr_part_t *part, int frame_count) {
    test_part_data_t *data = (test_part_data_t *)part->user_data;
    data->frame_counter += frame_count;

    /* Exit demo after 200 frames */
    if (data->frame_counter >= 200) {
        printf("[test_part_2] Reached %d frames, ending demo\n", data->frame_counter);
        return 1;
    }
    return 0;
}

static void test_part_2_render(sr_part_t *part) {
    test_part_data_t *data = (test_part_data_t *)part->user_data;
    uint8_t *fb = video_get_framebuffer();

    /* Animated green/yellow gradient bars */
    int offset = data->frame_counter % 256;
    for (int y = 0; y < VIDEO_HEIGHT_13H; y++) {
        for (int x = 0; x < VIDEO_WIDTH; x++) {
            /* Create vertical bars with animation */
            int bar = (x / 20 + offset / 4) % 16;
            uint8_t color = (uint8_t)(bar * 16);
            fb[y * VIDEO_WIDTH + x] = color;
        }
    }
}

static void test_part_2_cleanup(sr_part_t *part) {
    (void)part;
    printf("[test_part_2] Cleanup\n");
}

/* Part definitions */
static sr_part_t test_part_1 = {
    .name = "TEST_PART_1",
    .description = "Red/blue gradient test bars",
    .id = SR_PART_ALKU,
    .init = test_part_1_init,
    .update = test_part_1_update,
    .render = test_part_1_render,
    .cleanup = test_part_1_cleanup,
    .user_data = &test_part_1_data
};

static sr_part_t test_part_2 = {
    .name = "TEST_PART_2",
    .description = "Green/yellow gradient test bars",
    .id = SR_PART_BEG,
    .init = test_part_2_init,
    .update = test_part_2_update,
    .render = test_part_2_render,
    .cleanup = test_part_2_cleanup,
    .user_data = &test_part_2_data
};

static void init(void) {
    /* Initialize DIS first */
    dis_version();

    sg_setup(&(sg_desc){
        .environment = sglue_environment()
    });

    /* Initialize video subsystem */
    video_init();

    /* Initialize audio subsystem */
    if (music_init()) {
        /* Try to load the main demo music - try multiple paths */
        const char *music_paths[] = {
            "MAIN/MUSIC0.S3M",           /* Relative from build dir */
            "../MAIN/MUSIC0.S3M",        /* Relative from src dir */
            "/workspace/MAIN/MUSIC0.S3M" /* Absolute path */
        };
        int loaded = 0;
        for (int i = 0; i < 3 && !loaded; i++) {
            if (music_load_file(music_paths[i])) {
                printf("[main] Loaded music from: %s\n", music_paths[i]);
                music_play();
                loaded = 1;
            }
        }
        if (!loaded) {
            printf("[main] WARNING: Could not load music file MUSIC0.S3M\n");
        }
    } else {
        printf("[main] WARNING: Music subsystem failed to initialize\n");
    }

    /* Initialize part loader */
    part_loader_init();

    /* Register ALKU part (opening credits) */
    part_loader_register(alku_get_part());

    /* Register test parts (for fallback/testing) */
    part_loader_register(&test_part_1);
    part_loader_register(&test_part_2);

    /* Start from specified part (default: 0) */
    part_loader_start(g_start_part);

    /* Black clear color for letterboxing */
    pass_action = (sg_pass_action){
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f } }
    };
}

static int frame_count = 0;

static void frame(void) {
    dis_frame_tick();
    frame_count++;

    if (dis_exit()) {
        printf("[main] frame %d: dis_exit() returned true, quitting\n", frame_count);
        sapp_request_quit();
        return;
    }

    /* Check if demo sequence is complete */
    if (!part_loader_is_running()) {
        printf("[main] frame %d: part_loader_is_running() returned false, quitting\n", frame_count);
        sapp_request_quit();
        return;
    }

    /* Track current part for single-part mode */
    int current_part = part_loader_get_index();

    /* Update and render current part */
    part_loader_tick();
    part_loader_render();

    /* In single-part mode, exit when we transition to a different part */
    if (g_single_part && part_loader_get_index() != current_part) {
        printf("[main] Single-part mode: part completed, exiting\n");
        sapp_request_quit();
        return;
    }

    sg_begin_pass(&(sg_pass){ .action = pass_action, .swapchain = sglue_swapchain() });
    video_present();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    part_loader_shutdown();
    music_shutdown();
    video_shutdown();
    sg_shutdown();
}

static void event(const sapp_event* e) {
    /* Let DIS handle events */
    dis_handle_event(e);

    /* Space advances to next part */
    if (e->type == SAPP_EVENTTYPE_KEY_DOWN && e->key_code == SAPP_KEYCODE_SPACE) {
        printf("[main] Space pressed, advancing to next part\n");
        part_loader_next();
    }
}

static void print_usage(const char *prog) {
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  -p, --part <N>    Start from part N (0=ALKU, 1=TEST1, 2=TEST2)\n");
    printf("  -s, --single      Exit after first part completes\n");
    printf("  -l, --list        List available parts and exit\n");
    printf("  -h, --help        Show this help\n");
}

sapp_desc sokol_main(int argc, char* argv[]) {
    /* Parse CLI arguments */
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--part") == 0) && i + 1 < argc) {
            g_start_part = atoi(argv[++i]);
            printf("[main] Starting from part %d\n", g_start_part);
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--single") == 0) {
            g_single_part = 1;
            printf("[main] Single-part mode enabled\n");
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--list") == 0) {
            printf("Available parts:\n");
            printf("  0: ALKU (Opening Credits)\n");
            printf("  1: TEST_PART_1 (Red/Blue gradient)\n");
            printf("  2: TEST_PART_2 (Green/Yellow gradient)\n");
            exit(0);
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            exit(0);
        }
    }

    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = event,
        .width = 640,
        .height = 400,
        .window_title = "Second Reality",
        .icon.sokol_default = true,
        .swap_interval = 1,  /* Enable vsync for 60fps timing */
        .gl.major_version = 3,
        .gl.minor_version = 3,
    };
}
