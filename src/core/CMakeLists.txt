add_library(sokol_core STATIC sokol.c dis.c video.c)
target_include_directories(sokol_core PUBLIC ${SOKOL_PATH} ${CMAKE_SOURCE_DIR}/src)

if(EMSCRIPTEN)
    target_compile_definitions(sokol_core PRIVATE SOKOL_GLES3)
elseif(APPLE)
    target_compile_definitions(sokol_core PRIVATE SOKOL_METAL)
    target_compile_options(sokol_core PRIVATE -x objective-c)
    target_link_libraries(sokol_core PUBLIC
        "-framework QuartzCore"
        "-framework Cocoa"
        "-framework MetalKit"
        "-framework Metal"
    )
elseif(WIN32)
    target_compile_definitions(sokol_core PRIVATE SOKOL_D3D11)
    target_link_libraries(sokol_core PUBLIC d3d11 dxgi)
else()
    # Linux
    target_compile_definitions(sokol_core PRIVATE SOKOL_GLCORE)
    find_package(Threads REQUIRED)
    target_link_libraries(sokol_core PUBLIC
        X11 Xi Xcursor GL dl m Threads::Threads
    )
endif()
