/**
 * Video Subsystem - VGA-style framebuffer with Sokol GPU upload
 *
 * Provides Mode 13h (320x200) and Mode X (320x400) framebuffers
 * with 256-color palette support. Converts indexed color to RGBA
 * and uploads to GPU texture for display.
 */

#ifndef VIDEO_H
#define VIDEO_H

#include <stdint.h>

/* Video mode constants */
#define VIDEO_MODE_13H      0   /* 320x200, standard VGA */
#define VIDEO_MODE_X        1   /* 320x400, Mode X tweaked */

/* Resolution constants */
#define VIDEO_WIDTH         320
#define VIDEO_HEIGHT_13H    200
#define VIDEO_HEIGHT_X      400

/**
 * Initialize the video subsystem.
 * Must be called after sg_setup().
 */
void video_init(void);

/**
 * Shutdown the video subsystem.
 * Must be called before sg_shutdown().
 */
void video_shutdown(void);

/**
 * Set the video mode.
 * @param mode VIDEO_MODE_13H or VIDEO_MODE_X
 */
void video_set_mode(int mode);

/**
 * Get the current video mode.
 * @return Current video mode
 */
int video_get_mode(void);

/**
 * Get pointer to the framebuffer.
 * Size is VIDEO_WIDTH * VIDEO_HEIGHT_X (128KB) for Mode X compatibility.
 * @return Pointer to indexed color framebuffer
 */
uint8_t *video_get_framebuffer(void);

/**
 * Clear the framebuffer with a color index.
 * @param color Palette index (0-255)
 */
void video_clear(uint8_t color);

/**
 * Set the entire 256-color palette.
 * @param palette 768 bytes (256 colors * 3 RGB), each component 0-63
 */
void video_set_palette(const uint8_t palette[768]);

/**
 * Set a range of palette entries.
 * @param start Starting palette index
 * @param count Number of colors to set
 * @param data RGB data (count * 3 bytes), each component 0-63
 */
void video_set_palette_range(uint8_t start, uint8_t count, const uint8_t *data);

/**
 * Set a single palette color.
 * @param index Palette index (0-255)
 * @param r Red component (0-63)
 * @param g Green component (0-63)
 * @param b Blue component (0-63)
 */
void video_set_color(uint8_t index, uint8_t r, uint8_t g, uint8_t b);

/**
 * Get the current palette.
 * @param palette Output buffer for 768 bytes
 */
void video_get_palette(uint8_t palette[768]);

/**
 * Set display start offset for page flipping.
 * @param offset Byte offset into framebuffer
 */
void video_set_start(uint16_t offset);

/**
 * Set horizontal scroll offset for fine scrolling.
 * @param pixels Pixel offset (0-3 for Mode X)
 */
void video_set_hscroll(uint8_t pixels);

/**
 * Convert framebuffer to RGBA, upload to GPU, and draw fullscreen quad.
 * Call between sg_begin_pass() and sg_end_pass().
 */
void video_present(void);

#endif /* VIDEO_H */
