# Audio subsystem library
# Provides music playback using libopenmpt + Sokol Audio

add_library(audio STATIC audio.c music.c)
target_include_directories(audio PUBLIC ${SOKOL_PATH})

# Find libopenmpt using pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(OPENMPT REQUIRED libopenmpt)

target_include_directories(audio PRIVATE ${OPENMPT_INCLUDE_DIRS})
target_link_libraries(audio PUBLIC ${OPENMPT_LIBRARIES})
target_compile_options(audio PRIVATE ${OPENMPT_CFLAGS_OTHER})

# Platform-specific audio backends
if(EMSCRIPTEN)
    # Emscripten uses Web Audio API via Sokol (no additional libs needed)
elseif(APPLE)
    target_link_libraries(audio PUBLIC
        "-framework AudioToolbox"
    )
elseif(WIN32)
    target_link_libraries(audio PUBLIC ole32 winmm)
else()
    # Linux - use ALSA
    find_package(ALSA REQUIRED)
    target_include_directories(audio PRIVATE ${ALSA_INCLUDE_DIRS})
    target_link_libraries(audio PUBLIC ${ALSA_LIBRARIES})
endif()
