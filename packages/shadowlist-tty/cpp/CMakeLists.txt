cmake_minimum_required(VERSION 3.13)
project(shadowlist_tty CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../shadowlist-core/sources.cmake)

add_library(shadowlist_tty MODULE ${SHADOWLIST_CORE_SOURCES} ShadowListKitCoreNapi.cpp)

target_include_directories(shadowlist_tty PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../..   # for shadowlist-core includes
  ${NODE_INCLUDE_DIR})                # for node_api.h

target_compile_definitions(shadowlist_tty PRIVATE SHADOWLIST_DEBUG_LOG=0 NAPI_VERSION=8)
target_compile_options(shadowlist_tty PRIVATE -fno-rtti)

# Node resolves the napi_* symbols when it loads the addon.
if(APPLE)
  target_link_options(shadowlist_tty PRIVATE -undefined dynamic_lookup)
endif()

set_target_properties(shadowlist_tty PROPERTIES
  PREFIX ""
  SUFFIX ".node"
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../build)
