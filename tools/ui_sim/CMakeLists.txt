# x4cam UI simulator: the real core/src/ui code + LVGL on a PC. Usually driven by run.py:
#   python tools/ui_sim/run.py            render every scene and compare with Figma
#   python tools/ui_sim/run.py --live     open a window (needs SDL2)
# Manual build:
#   cmake -S tools/ui_sim -B tools/ui_sim/build && cmake --build tools/ui_sim/build
cmake_minimum_required(VERSION 3.16)
project(x4_ui_sim C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(X4_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
option(X4_SIM_SDL "Build the --live window (needs SDL2)" ON)

# LVGL: same version as the firmware (docs/FIRMWARE.md#toolchain-and-pinned-versions), same
# lv_conf.h (core/include). SOURCE_SUBDIR points nowhere so LVGL's own CMake is not used.
include(FetchContent)
FetchContent_Declare(lvgl
  GIT_REPOSITORY https://github.com/lvgl/lvgl.git
  GIT_TAG v9.6.0
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR none)
FetchContent_MakeAvailable(lvgl)

file(GLOB_RECURSE LVGL_SOURCES ${lvgl_SOURCE_DIR}/src/*.c)
add_library(lvgl STATIC ${LVGL_SOURCES})
target_include_directories(lvgl PUBLIC ${lvgl_SOURCE_DIR} ${X4_ROOT}/core/include)
target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE LV_LVGL_H_INCLUDE_SIMPLE)
# lv_conf.h is the device one; its LV_COLOR_DEPTH deprecation note is not ours to fix here.
target_compile_options(lvgl PUBLIC -Wno-cpp)

file(GLOB UI_ASSETS ${X4_ROOT}/core/src/ui/assets/*.c)
file(GLOB UI_FONTS ${X4_ROOT}/core/src/ui/fonts/*.c)
add_executable(ui_sim
  sim_main.cpp
  ${X4_ROOT}/core/src/ui/ui.cpp
  ${UI_ASSETS}
  ${UI_FONTS}
  ${X4_ROOT}/shared/src/log.cpp
  ${X4_ROOT}/shared/src/errors.cpp)
target_include_directories(ui_sim PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${X4_ROOT}/core/src
  ${X4_ROOT}/shared/include)
target_link_libraries(ui_sim PRIVATE lvgl)
set_source_files_properties(sim_main.cpp ${X4_ROOT}/core/src/ui/ui.cpp
  PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Wno-missing-field-initializers;-Wno-deprecated-declarations")

if(X4_SIM_SDL)
  find_package(SDL2 QUIET)
  if(SDL2_FOUND)
    target_compile_definitions(ui_sim PRIVATE X4_SIM_SDL=1)
    if(TARGET SDL2::SDL2)
      target_link_libraries(ui_sim PRIVATE SDL2::SDL2)
    else()
      target_include_directories(ui_sim PRIVATE ${SDL2_INCLUDE_DIRS})
      target_link_libraries(ui_sim PRIVATE ${SDL2_LIBRARIES})
    endif()
    message(STATUS "ui_sim: SDL2 found, --live enabled")
  else()
    message(STATUS "ui_sim: SDL2 not found, --live disabled (headless scenes still work)")
  endif()
endif()
