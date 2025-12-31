add_subdirectory(core)

add_executable(SecondReality main.c)
target_link_libraries(SecondReality PRIVATE sokol_core)
target_include_directories(SecondReality PRIVATE ${SOKOL_PATH})

if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    set_target_properties(SecondReality PROPERTIES
        LINK_FLAGS "-s USE_WEBGL2=1 -s FULL_ES3=1 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1"
    )
endif()
