cmake_minimum_required(VERSION 3.18)
project(shadowlist_tty CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Node-API's headers sit beside the node that runs the build. Found afresh each time, as `yarn run`
# puts a temporary node first on the PATH that a cached path would outlive.
unset(NODE_EXECUTABLE CACHE)
find_program(NODE_EXECUTABLE node REQUIRED)
execute_process(
  COMMAND ${NODE_EXECUTABLE} -p "require('node:path').resolve(process.execPath, '../../include/node')"
  OUTPUT_VARIABLE NODE_INCLUDE_DIR
  OUTPUT_STRIP_TRAILING_WHITESPACE)

include(${CMAKE_CURRENT_SOURCE_DIR}/../shadowlist-core/sources.cmake)

add_library(shadowlist_tty MODULE ${SHADOWLIST_CORE_SOURCES} cpp/ShadowListKitCoreNapi.cpp)
set_target_properties(shadowlist_tty PROPERTIES PREFIX "" SUFFIX ".node")
target_include_directories(shadowlist_tty PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..   # for shadowlist-core includes
  ${NODE_INCLUDE_DIR})
target_compile_definitions(shadowlist_tty PRIVATE SHADOWLIST_DEBUG_LOG=0 NAPI_VERSION=8)
target_compile_options(shadowlist_tty PRIVATE -fno-rtti)
if(APPLE)
  # The symbols of Node-API come from the node that loads the addon.
  target_link_options(shadowlist_tty PRIVATE -undefined dynamic_lookup)
endif()
