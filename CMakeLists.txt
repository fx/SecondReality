cmake_minimum_required(VERSION 3.16)
project(SecondReality C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Sokol headers path
set(SOKOL_PATH ${CMAKE_SOURCE_DIR}/third_party/sokol)

add_subdirectory(src)
