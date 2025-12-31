add_subdirectory(core)

add_executable(SecondReality main.c)
target_link_libraries(SecondReality PRIVATE sokol_core)
target_include_directories(SecondReality PRIVATE ${SOKOL_PATH})

if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    target_link_options(SecondReality PRIVATE
        "SHELL:-s USE_WEBGL2=1"
        "SHELL:-s FULL_ES3=1"
        "SHELL:-s WASM=1"
        "SHELL:-s ALLOW_MEMORY_GROWTH=1"
    )
endif()
