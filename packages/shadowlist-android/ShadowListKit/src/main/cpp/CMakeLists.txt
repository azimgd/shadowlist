cmake_minimum_required(VERSION 3.13)
project(shadowlistkit CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

get_filename_component(SHADOWLIST_KIT_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../shadowlist-core" ABSOLUTE)
get_filename_component(SHADOWLIST_KIT_CORE_PARENT_DIR "${SHADOWLIST_KIT_CORE_DIR}" DIRECTORY)
include(${SHADOWLIST_KIT_CORE_DIR}/sources.cmake)

add_library(shadowlistkit SHARED
  ShadowListKitCoreJNI.cpp
  ${SHADOWLIST_CORE_SOURCES}
)

# Core headers include each other as <shadowlist-core/...>, which the parent directory resolves.
target_include_directories(shadowlistkit PRIVATE ${SHADOWLIST_KIT_CORE_PARENT_DIR})

find_library(log-lib log)
target_link_libraries(shadowlistkit ${log-lib})

# The static C++ runtime stays private to the library. Apps with their own libc++ do not see its symbols.
target_link_options(shadowlistkit PRIVATE -Wl,--exclude-libs,ALL)

# The layout pass runs on every scroll frame. Optimized builds use -O3.
target_compile_options(shadowlistkit PRIVATE -fno-rtti $<$<NOT:$<CONFIG:Debug>>:-O3>)

# The core's [SL] debug log is off by default. -PshadowlistDebugLog passes this switch.
if(SHADOWLIST_DEBUG_LOG)
  target_compile_definitions(shadowlistkit PRIVATE SHADOWLIST_DEBUG_LOG=1)
endif()
