add_subdirectory(core)
add_subdirectory(audio)
add_subdirectory(parts/alku)

add_executable(SecondReality main.c)
target_link_libraries(SecondReality PRIVATE sokol_core audio alku)
target_include_directories(SecondReality PRIVATE
    ${SOKOL_PATH}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/parts/alku
)

if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    target_link_options(SecondReality PRIVATE
        "SHELL:-s USE_WEBGL2=1"
        "SHELL:-s FULL_ES3=1"
        "SHELL:-s WASM=1"
        "SHELL:-s ALLOW_MEMORY_GROWTH=1"
        "SHELL:-s INITIAL_MEMORY=16777216"
        "SHELL:-s STACK_SIZE=1048576"
    )
endif()
