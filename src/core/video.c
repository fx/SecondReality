/**
 * Video Subsystem - VGA-style framebuffer with Sokol GPU upload
 *
 * Reimplements VGA Mode 13h (320x200) and Mode X (320x400) using
 * an indexed-color framebuffer converted to RGBA for GPU display.
 */

#include "video.h"
#include "sokol_gfx.h"
#include "sokol_app.h"
#include <string.h>

/* Framebuffer size: 320x400 = 128KB for Mode X */
#define FB_SIZE (VIDEO_WIDTH * VIDEO_HEIGHT_X)

/* Embedded shaders */

#if defined(SOKOL_GLCORE)
/* GLSL 3.30 for desktop OpenGL */
static const char *vs_source =
    "#version 330\n"
    "out vec2 uv;\n"
    "void main() {\n"
    "    float x = float((gl_VertexID & 1) << 2) - 1.0;\n"
    "    float y = float((gl_VertexID & 2) << 1) - 1.0;\n"
    "    uv = vec2((x + 1.0) * 0.5, (1.0 - y) * 0.5);\n"
    "    gl_Position = vec4(x, y, 0.0, 1.0);\n"
    "}\n";

static const char *fs_source =
    "#version 330\n"
    "uniform sampler2D tex;\n"
    "in vec2 uv;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "    frag_color = texture(tex, uv);\n"
    "}\n";
#elif defined(SOKOL_GLES3)
/* GLSL ES 3.00 for WebGL2/GLES3 */
static const char *vs_source =
    "#version 300 es\n"
    "out vec2 uv;\n"
    "void main() {\n"
    "    float x = float((gl_VertexID & 1) << 2) - 1.0;\n"
    "    float y = float((gl_VertexID & 2) << 1) - 1.0;\n"
    "    uv = vec2((x + 1.0) * 0.5, (1.0 - y) * 0.5);\n"
    "    gl_Position = vec4(x, y, 0.0, 1.0);\n"
    "}\n";

static const char *fs_source =
    "#version 300 es\n"
    "precision mediump float;\n"
    "uniform sampler2D tex;\n"
    "in vec2 uv;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "    frag_color = texture(tex, uv);\n"
    "}\n";
#elif defined(SOKOL_D3D11)
/* HLSL for D3D11 */
static const char *vs_source =
    "struct vs_out {\n"
    "    float2 uv : TEXCOORD0;\n"
    "    float4 pos : SV_Position;\n"
    "};\n"
    "vs_out main(uint vid : SV_VertexID) {\n"
    "    vs_out o;\n"
    "    float x = float((vid & 1) << 2) - 1.0;\n"
    "    float y = float((vid & 2) << 1) - 1.0;\n"
    "    o.uv = float2((x + 1.0) * 0.5, (1.0 - y) * 0.5);\n"
    "    o.pos = float4(x, y, 0.0, 1.0);\n"
    "    return o;\n"
    "}\n";

static const char *fs_source =
    "Texture2D<float4> tex : register(t0);\n"
    "SamplerState smp : register(s0);\n"
    "float4 main(float2 uv : TEXCOORD0) : SV_Target0 {\n"
    "    return tex.Sample(smp, uv);\n"
    "}\n";
#elif defined(SOKOL_METAL)
/* Metal shaders */
static const char *vs_source =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct vs_out {\n"
    "    float4 pos [[position]];\n"
    "    float2 uv;\n"
    "};\n"
    "vertex vs_out _main(uint vid [[vertex_id]]) {\n"
    "    vs_out o;\n"
    "    float x = float((vid & 1) << 2) - 1.0;\n"
    "    float y = float((vid & 2) << 1) - 1.0;\n"
    "    o.uv = float2((x + 1.0) * 0.5, (1.0 - y) * 0.5);\n"
    "    o.pos = float4(x, y, 0.0, 1.0);\n"
    "    return o;\n"
    "}\n";

static const char *fs_source =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "fragment float4 _main(float2 uv [[stage_in]],\n"
    "                      texture2d<float> tex [[texture(0)]],\n"
    "                      sampler smp [[sampler(0)]]) {\n"
    "    return tex.sample(smp, uv);\n"
    "}\n";
#else
#error "Unknown Sokol backend"
#endif

/* Internal video state */
static struct {
    uint8_t framebuffer[FB_SIZE];
    uint8_t palette[768];
    uint32_t rgba_lut[256];
    uint32_t rgba_staging[VIDEO_WIDTH * VIDEO_HEIGHT_X];
    sg_image image;
    sg_view texture_view;
    sg_sampler sampler;
    sg_shader shader;
    sg_pipeline pipeline;
    int mode;
    uint16_t start_offset;
    uint8_t hscroll;
    int palette_dirty;
    int initialized;
} video_state;

/* Rebuild RGBA lookup table from palette */
static void rebuild_rgba_lut(void) {
    for (int i = 0; i < 256; i++) {
        /* VGA uses 6-bit color (0-63), convert to 8-bit (0-255) */
        uint8_t r6 = video_state.palette[i * 3 + 0];
        uint8_t g6 = video_state.palette[i * 3 + 1];
        uint8_t b6 = video_state.palette[i * 3 + 2];
        /* Expand 6-bit to 8-bit: (val << 2) | (val >> 4) */
        uint8_t r = (r6 << 2) | (r6 >> 4);
        uint8_t g = (g6 << 2) | (g6 >> 4);
        uint8_t b = (b6 << 2) | (b6 >> 4);
        /* Pack as RGBA (little-endian: 0xAABBGGRR) */
        video_state.rgba_lut[i] = 0xFF000000 | ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;
    }
    video_state.palette_dirty = 0;
}

/* Convert indexed framebuffer to RGBA staging buffer */
static void convert_framebuffer_to_rgba(void) {
    int height = (video_state.mode == VIDEO_MODE_X) ? VIDEO_HEIGHT_X : VIDEO_HEIGHT_13H;
    int pixel_count = VIDEO_WIDTH * height;
    const uint8_t *src = video_state.framebuffer + video_state.start_offset;
    uint32_t *dst = video_state.rgba_staging;

    /* Handle hscroll offset (fine scrolling) */
    int hscroll = video_state.hscroll & 3;
    (void)hscroll; /* TODO: implement fine scrolling if needed */

    for (int i = 0; i < pixel_count; i++) {
        dst[i] = video_state.rgba_lut[src[i]];
    }
}

void video_init(void) {
    memset(&video_state, 0, sizeof(video_state));
    video_state.mode = VIDEO_MODE_13H;
    video_state.palette_dirty = 1;

    /* Create default grayscale palette */
    for (int i = 0; i < 256; i++) {
        uint8_t gray = (uint8_t)(i >> 2); /* 0-255 -> 0-63 */
        video_state.palette[i * 3 + 0] = gray;
        video_state.palette[i * 3 + 1] = gray;
        video_state.palette[i * 3 + 2] = gray;
    }
    rebuild_rgba_lut();

    /* Create texture for framebuffer (sized for Mode X) */
    video_state.image = sg_make_image(&(sg_image_desc){
        .width = VIDEO_WIDTH,
        .height = VIDEO_HEIGHT_X,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .usage = {
            .immutable = false,
            .stream_update = true
        },
        .label = "video_fb"
    });

    /* Create texture view for sampling */
    video_state.texture_view = sg_make_view(&(sg_view_desc){
        .texture.image = video_state.image,
        .label = "video_tex_view"
    });

    /* Create sampler with nearest filtering for crisp pixels */
    video_state.sampler = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .label = "video_smp"
    });

    /* Create shader */
    video_state.shader = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source = vs_source,
        .fragment_func.source = fs_source,
        .views[0] = {
            .texture = {
                .stage = SG_SHADERSTAGE_FRAGMENT,
                .image_type = SG_IMAGETYPE_2D,
                .sample_type = SG_IMAGESAMPLETYPE_FLOAT
            }
        },
        .samplers[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .sampler_type = SG_SAMPLERTYPE_FILTERING
        },
        .texture_sampler_pairs[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .view_slot = 0,
            .sampler_slot = 0,
            .glsl_name = "tex"
        },
        .label = "video_shd"
    });

    /* Create pipeline */
    video_state.pipeline = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = video_state.shader,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLES,
        .label = "video_pip"
    });

    video_state.initialized = 1;
}

void video_shutdown(void) {
    if (!video_state.initialized) {
        return;
    }
    sg_destroy_pipeline(video_state.pipeline);
    sg_destroy_shader(video_state.shader);
    sg_destroy_sampler(video_state.sampler);
    sg_destroy_view(video_state.texture_view);
    sg_destroy_image(video_state.image);
    memset(&video_state, 0, sizeof(video_state));
}

void video_set_mode(int mode) {
    if (mode == VIDEO_MODE_13H || mode == VIDEO_MODE_X) {
        video_state.mode = mode;
    }
}

int video_get_mode(void) {
    return video_state.mode;
}

uint8_t *video_get_framebuffer(void) {
    return video_state.framebuffer;
}

void video_clear(uint8_t color) {
    memset(video_state.framebuffer, color, FB_SIZE);
}

void video_set_palette(const uint8_t palette[768]) {
    memcpy(video_state.palette, palette, 768);
    video_state.palette_dirty = 1;
}

void video_set_palette_range(uint8_t start, uint8_t count, const uint8_t *data) {
    if (start + count > 256) {
        count = 256 - start;
    }
    memcpy(&video_state.palette[start * 3], data, count * 3);
    video_state.palette_dirty = 1;
}

void video_set_color(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    video_state.palette[index * 3 + 0] = r;
    video_state.palette[index * 3 + 1] = g;
    video_state.palette[index * 3 + 2] = b;
    video_state.palette_dirty = 1;
}

void video_get_palette(uint8_t palette[768]) {
    memcpy(palette, video_state.palette, 768);
}

void video_set_start(uint16_t offset) {
    video_state.start_offset = offset;
}

void video_set_hscroll(uint8_t pixels) {
    video_state.hscroll = pixels;
}

void video_present(void) {
    if (!video_state.initialized) {
        return;
    }

    /* Rebuild LUT if palette changed */
    if (video_state.palette_dirty) {
        rebuild_rgba_lut();
    }

    /* Convert indexed framebuffer to RGBA */
    convert_framebuffer_to_rgba();

    /* Calculate actual height based on mode */
    int height = (video_state.mode == VIDEO_MODE_X) ? VIDEO_HEIGHT_X : VIDEO_HEIGHT_13H;

    /* Update texture with RGBA data */
    sg_update_image(video_state.image, &(sg_image_data){
        .mip_levels[0] = {
            .ptr = video_state.rgba_staging,
            .size = VIDEO_WIDTH * height * sizeof(uint32_t)
        }
    });

    /* Calculate letterbox viewport for 4:3 aspect ratio */
    int win_width = sapp_width();
    int win_height = sapp_height();

    /* Target aspect ratio based on mode */
    float target_aspect = (float)VIDEO_WIDTH / (float)height;
    float win_aspect = (float)win_width / (float)win_height;

    int vp_x, vp_y, vp_w, vp_h;
    if (win_aspect > target_aspect) {
        /* Window is wider than 4:3, letterbox on sides */
        vp_h = win_height;
        vp_w = (int)(win_height * target_aspect);
        vp_x = (win_width - vp_w) / 2;
        vp_y = 0;
    } else {
        /* Window is taller than 4:3, letterbox on top/bottom */
        vp_w = win_width;
        vp_h = (int)(win_width / target_aspect);
        vp_x = 0;
        vp_y = (win_height - vp_h) / 2;
    }

    /* Apply viewport and draw fullscreen triangle */
    sg_apply_viewport(vp_x, vp_y, vp_w, vp_h, true);
    sg_apply_pipeline(video_state.pipeline);
    sg_apply_bindings(&(sg_bindings){
        .views[0] = video_state.texture_view,
        .samplers[0] = video_state.sampler
    });
    sg_draw(0, 3, 1);
}
