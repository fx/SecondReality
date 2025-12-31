#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "core/dis.h"

static sg_pass_action pass_action;

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment()
    });

    /* Initialize DIS */
    dis_partstart();

    /* Dark blue clear color - visible test */
    pass_action = (sg_pass_action){
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.1f, 0.2f, 1.0f } }
    };
}

static void frame(void) {
    /* Tick DIS frame counter */
    dis_frame_tick();

    /* Check for exit request */
    if (dis_exit()) {
        sapp_request_quit();
        return;
    }

    sg_begin_pass(&(sg_pass){ .action = pass_action, .swapchain = sglue_swapchain() });
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
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
