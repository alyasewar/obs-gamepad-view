cmake_minimum_required(VERSION 3.21)
project(input_visualizer_obs VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT OBS_INCLUDE_DIR OR NOT OBS_LIB_DIR)
  message(FATAL_ERROR "Set OBS_INCLUDE_DIR and OBS_LIB_DIR to your OBS install or build.")
endif()

if(NOT OBS_DEPS_INCLUDE_DIR)
  message(STATUS "OBS_DEPS_INCLUDE_DIR not set; build may fail if simde headers are missing")
endif()
if(NOT OBS_CONFIG_INCLUDE_DIR)
  message(STATUS "OBS_CONFIG_INCLUDE_DIR not set; build may fail if obsconfig.h is missing")
endif()

find_package(Python3 COMPONENTS Interpreter REQUIRED)

# How far above the 1000px authoring canvas the sheets are rasterised. 2 keeps
# the pad crisp when a scene scales the source up; drop it to 1 to halve the
# texture memory the plugin holds for the active theme.
set(IV_ASSET_SCALE "2" CACHE STRING "Rasterisation factor for controller sheets")

add_library(input_visualizer MODULE
  src/plugin.cpp
  src/json.cpp
  src/layout.cpp
  src/renderer.cpp
  src/theme_loader.cpp
  src/input_state.mm
)

target_include_directories(input_visualizer PRIVATE ${OBS_INCLUDE_DIR} src)
if(OBS_DEPS_INCLUDE_DIR)
  target_include_directories(input_visualizer PRIVATE ${OBS_DEPS_INCLUDE_DIR})
endif()
if(OBS_CONFIG_INCLUDE_DIR)
  target_include_directories(input_visualizer PRIVATE ${OBS_CONFIG_INCLUDE_DIR})
endif()
target_link_directories(input_visualizer PRIVATE ${OBS_LIB_DIR})

if(WIN32)
  target_link_libraries(input_visualizer PRIVATE obs)
  set_target_properties(input_visualizer PROPERTIES SUFFIX ".dll")
elseif(APPLE)
  target_link_libraries(input_visualizer PRIVATE
    "${OBS_LIB_DIR}/libobs.framework"
    "-framework GameController"
    "-framework CoreGraphics"
    "-framework CoreFoundation"
    "-framework Foundation"
    "-framework Carbon")
  set_target_properties(input_visualizer PROPERTIES SUFFIX ".so")
else()
  target_link_libraries(input_visualizer PRIVATE obs)
endif()

# Rasterise the controller sheets once per theme, plus copy the effect and
# locale files, into the data tree the plugin loads at runtime.
set(IV_DATA_DIR "${CMAKE_BINARY_DIR}/data")
add_custom_target(input_visualizer_assets ALL
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/cmake/build-assets.py"
          --source "${CMAKE_SOURCE_DIR}"
          --out "${IV_DATA_DIR}"
          --scale "${IV_ASSET_SCALE}"
  COMMENT "Rendering controller sheets for each theme"
  VERBATIM
)
add_dependencies(input_visualizer input_visualizer_assets)

# Verifies every layout.json rect still lines up with its overlay sheet. The two
# SVGs per pad duplicate their geometry, so this is what catches a coordinate
# edited in one and not the other.
add_custom_target(check_layout
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tools/check_layout.py"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Checking controller layout manifests"
  VERBATIM
)

enable_testing()
add_executable(layout_selftest
  tools/layout_selftest.cpp
  src/json.cpp
  src/layout.cpp
)
target_include_directories(layout_selftest PRIVATE src)
add_test(NAME layout_selftest
         COMMAND layout_selftest "${CMAKE_SOURCE_DIR}")

install(TARGETS input_visualizer DESTINATION obs-plugins/input-visualizer)
install(DIRECTORY "${IV_DATA_DIR}/" DESTINATION data/obs-plugins/input-visualizer)
