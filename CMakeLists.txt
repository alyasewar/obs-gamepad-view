cmake_minimum_required(VERSION 3.21)
project(input_visualizer_obs VERSION 0.1.0 LANGUAGES C CXX)

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

add_library(input_visualizer MODULE
  src/plugin.cpp
  src/input_state.mm
  src/theme_loader.cpp
)

target_include_directories(input_visualizer PRIVATE ${OBS_INCLUDE_DIR})
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
  target_link_libraries(input_visualizer PRIVATE "${OBS_LIB_DIR}/libobs.framework" "-framework GameController" "-framework CoreGraphics" "-framework CoreFoundation" "-framework Foundation")
  set_target_properties(input_visualizer PROPERTIES SUFFIX ".so")
else()
  target_link_libraries(input_visualizer PRIVATE obs)
endif()

add_custom_command(TARGET input_visualizer POST_BUILD
  COMMAND "${CMAKE_SOURCE_DIR}/cmake/convert-svgz.sh" "${CMAKE_SOURCE_DIR}/themes"
  COMMENT "Converting SVGZ assets to PNG"
)

install(TARGETS input_visualizer DESTINATION obs-plugins/input-visualizer)
install(DIRECTORY themes DESTINATION obs-plugins/input-visualizer)
