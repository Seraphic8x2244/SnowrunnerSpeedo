cmake_minimum_required(VERSION 3.20)
project(SnowrunnerSpeedo LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FetchContent)
FetchContent_Declare(minhook GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git GIT_TAG v1.3.4 GIT_SHALLOW TRUE)
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.91.6 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(minhook imgui)
add_library(SnowrunnerSpeedo SHARED
  src/SpeedHud.cpp
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp)
target_include_directories(SnowrunnerSpeedo PRIVATE
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
  ${minhook_SOURCE_DIR}/include)
target_compile_definitions(SnowrunnerSpeedo PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(SnowrunnerSpeedo PRIVATE minhook d3d11 dxgi Psapi user32)
set_target_properties(SnowrunnerSpeedo PROPERTIES PREFIX "" SUFFIX ".asi" OUTPUT_NAME "SnowrunnerSpeedo")
if(MSVC)
  target_compile_options(SnowrunnerSpeedo PRIVATE /W3)
endif()
