// Backend is set via CMake compile definitions:
// - SOKOL_GLES3 for Emscripten
// - SOKOL_METAL for macOS
// - SOKOL_D3D11 for Windows
// - SOKOL_GLCORE for Linux

#define SOKOL_IMPL
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
