#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "core/dis.h"
#include "core/video.h"
#include "audio/music.h"

static sg_pass_action pass_action;

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
        /* Try to load the main demo music for testing */
        if (music_load_file("MAIN/MUSIC0.S3M")) {
            music_play();
        }
    }

    /* Create test pattern: colored vertical bars */
    uint8_t *fb = video_get_framebuffer();
    for (int y = 0; y < VIDEO_HEIGHT_13H; y++) {
        for (int x = 0; x < VIDEO_WIDTH; x++) {
            /* 8 colored bars across the screen */
            fb[y * VIDEO_WIDTH + x] = (uint8_t)((x / 40) * 32);
        }
    }

    /* Set up a colorful test palette */
    for (int i = 0; i < 256; i++) {
        /* Create a simple rainbow palette */
        uint8_t r, g, b;
        if (i < 32) {
            /* Black */
            r = g = b = 0;
        } else if (i < 64) {
            /* Red */
            r = 63; g = 0; b = 0;
        } else if (i < 96) {
            /* Orange */
            r = 63; g = 32; b = 0;
        } else if (i < 128) {
            /* Yellow */
            r = 63; g = 63; b = 0;
        } else if (i < 160) {
            /* Green */
            r = 0; g = 63; b = 0;
        } else if (i < 192) {
            /* Cyan */
            r = 0; g = 63; b = 63;
        } else if (i < 224) {
            /* Blue */
            r = 0; g = 0; b = 63;
        } else {
            /* Magenta */
            r = 63; g = 0; b = 63;
        }
        video_set_color((uint8_t)i, r, g, b);
    }

    /* Black clear color for letterboxing */
    pass_action = (sg_pass_action){
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f } }
    };
}

static void frame(void) {
    dis_frame_tick();

    if (dis_exit()) {
        sapp_request_quit();
    }

    sg_begin_pass(&(sg_pass){ .action = pass_action, .swapchain = sglue_swapchain() });
    video_present();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    music_shutdown();
    video_shutdown();
    sg_shutdown();
}

static void event(const sapp_event* e) {
    /* Let DIS handle events */
    dis_handle_event(e);
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = event,
        .width = 320,
        .height = 200,
        .window_title = "Second Reality",
        .icon.sokol_default = true,
    };
}
