cmake_minimum_required(VERSION 3.15)
project(GeniusSDK)

set(CMAKE_CXX_STANDARD 14)
set(LIBRARY_OUTPUT_PATH  ..//lib/${CMAKE_GENERATOR_PLATFORM}/${CMAKE_BUILD_TYPE})

include(/platform/${CMAKE_GENERATOR_PLATFORM}/CMakeLists.txt)
message("plaform: ${CMAKE_GENERATOR_PLATFORM}")

include_directories(
        ../include
        ..//${CMAKE_GENERATOR_PLATFORM}/include
)

file(GLOB ALL_SRCS
    "../include/*.h"
    "../${CMAKE_GENERATOR_PLATFORM}/include/$*.h"
    "../include/*.hpp"
    "../src/*.cpp"
    "../${CMAKE_GENERATOR_PLATFORM}/include/$*.cpp"
    "../src/*.c"
    "${PROJECT_SOURCE_DIR}/${CMAKE_GENERATOR_PLATFORM}/include/$*.c"
    )

add_library(GeniusSDK SHARED ${ALL_SRCS})