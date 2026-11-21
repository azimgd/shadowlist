cmake_minimum_required(VERSION 3.13)
project(shadowlistkit CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

get_filename_component(SHADOWLIST_KIT_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../shadowlist-core" ABSOLUTE)
get_filename_component(SHADOWLIST_KIT_CORE_PARENT_DIR "${SHADOWLIST_KIT_CORE_DIR}" DIRECTORY)
include(${SHADOWLIST_KIT_CORE_DIR}/sources.cmake)

add_executable(shadowlistkit
  ShadowListKitCoreWasm.cpp
  ${SHADOWLIST_CORE_SOURCES}
)

# The module loads as an ES module named shadowlistkit.mjs next to shadowlistkit.wasm.
set_target_properties(shadowlistkit PROPERTIES SUFFIX ".mjs")

# Core headers include each other as <shadowlist-core/...>, which the parent directory resolves.
target_include_directories(shadowlistkit PRIVATE ${SHADOWLIST_KIT_CORE_PARENT_DIR})

# The layout pass runs on every scroll frame. Optimized builds use -O3.
target_compile_options(shadowlistkit PRIVATE -fno-rtti $<$<NOT:$<CONFIG:Debug>>:-O3>)

target_link_options(shadowlistkit PRIVATE
  -fno-rtti
  $<$<NOT:$<CONFIG:Debug>>:-O3>
  -sMODULARIZE=1
  -sEXPORT_ES6=1
  -sEXPORT_NAME=createShadowListKitModule
  -sENVIRONMENT=web,node
  -sALLOW_MEMORY_GROWTH=1
  -sFILESYSTEM=0
  -sEXPORTED_FUNCTIONS=_malloc,_free
  -sEXPORTED_RUNTIME_METHODS=HEAP8,HEAP32,HEAPF64,HEAPU8
)

# The core's [SL] debug log is off by default. -DSHADOWLIST_DEBUG_LOG=ON passes this switch.
if(SHADOWLIST_DEBUG_LOG)
  target_compile_definitions(shadowlistkit PRIVATE SHADOWLIST_DEBUG_LOG=1)
endif()
