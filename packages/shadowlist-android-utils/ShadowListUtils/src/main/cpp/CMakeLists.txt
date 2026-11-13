cmake_minimum_required(VERSION 3.13)
project(shadowlistutils CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The monorepo reads packages/shadowlist-core. A host that builds the library with a copy of the
# core passes the copy's folder as SHADOWLIST_UTILS_CORE_DIR. The folder is named shadowlist-core.
if(NOT SHADOWLIST_UTILS_CORE_DIR)
  get_filename_component(SHADOWLIST_UTILS_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../shadowlist-core" ABSOLUTE)
endif()
get_filename_component(SHADOWLIST_UTILS_CORE_PARENT_DIR "${SHADOWLIST_UTILS_CORE_DIR}" DIRECTORY)

# Only the template logic compiles here. The list core is already inside the kit's shadowlistkit
# library, and a second copy of it would double the binary.
include(${SHADOWLIST_UTILS_CORE_DIR}/sources.cmake)

add_library(shadowlistutils SHARED
  ${CMAKE_CURRENT_LIST_DIR}/ShadowListUtilsCoreJNI.cpp
  ${SHADOWLIST_UTILS_SOURCES}
)

# Core headers include each other as <shadowlist-core/...>, which the parent directory resolves.
target_include_directories(shadowlistutils PRIVATE ${SHADOWLIST_UTILS_CORE_PARENT_DIR})

# The static C++ runtime stays private to the library. Apps with their own libc++ do not see its symbols.
target_link_options(shadowlistutils PRIVATE -Wl,--exclude-libs,ALL)

# A row's layout calls this code for every row it sizes. Optimized builds use -O3.
target_compile_options(shadowlistutils PRIVATE -fno-rtti $<$<NOT:$<CONFIG:Debug>>:-O3>)

# A host that links a second copy of the core into the same app renames this copy's namespace,
# like SHADOWLIST_UTILS_COMPILE_DEFINITIONS="azimgd=host_utils_azimgd".
if(SHADOWLIST_UTILS_COMPILE_DEFINITIONS)
  target_compile_definitions(shadowlistutils PRIVATE ${SHADOWLIST_UTILS_COMPILE_DEFINITIONS})
endif()
