# ALKU (Opening Credits) Part
# Displays "A Future Crew Production" text with fades over scrolling horizon

add_library(alku STATIC
    alku.c
    alku_data.c
)

target_link_libraries(alku PRIVATE sokol_core)
target_include_directories(alku PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/core
    ${SOKOL_PATH}
)
