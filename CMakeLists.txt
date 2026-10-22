cmake_minimum_required(VERSION 3.20)
project(SnowrunnerSpeedo LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Point at a local checkout of maxpowa/SnowLoader. No upstream files copied.
set(SNOWLOADER_ROOT "" CACHE PATH "Path to the upstream SnowLoader source checkout")
if(NOT EXISTS "${SNOWLOADER_ROOT}/include/sdk/script_api.h")
  message(FATAL_ERROR "Set -DSNOWLOADER_ROOT=<path to SnowLoader checkout>")
endif()

add_library(left_hud_probe SHARED scripts/left_hud_probe/script.cpp)
target_include_directories(left_hud_probe PRIVATE "${SNOWLOADER_ROOT}/include")
set_target_properties(left_hud_probe PROPERTIES OUTPUT_NAME "left_hud_probe")
if(MSVC)
  target_compile_options(left_hud_probe PRIVATE /W4)
else()
  target_compile_options(left_hud_probe PRIVATE -Wall -Wextra -Wpedantic)
endif()
