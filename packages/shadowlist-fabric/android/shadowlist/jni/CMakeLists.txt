cmake_minimum_required(VERSION 3.13)
set(CMAKE_VERBOSE_MAKEFILE on)

set(LIB_JNI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../build/generated/source/codegen/jni)
set(LIB_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
set(LIB_CPP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../cpp)

# Shared core sources. In the monorepo the canonical copy lives at
# packages/shadowlist-core, so compile it directly - Android needs no copy phase.
# In a published package that path does not exist, so fall back to the vendored
# mirror shipped inside the package (see scripts/vendor-core.js / the podspec).
get_filename_component(LIB_CORE_CANONICAL "${CMAKE_CURRENT_SOURCE_DIR}/../../../../shadowlist-core" ABSOLUTE)
get_filename_component(LIB_CORE_VENDORED "${CMAKE_CURRENT_SOURCE_DIR}/../../../shadowlist-core" ABSOLUTE)
if(EXISTS "${LIB_CORE_CANONICAL}/Container.cpp")
  set(LIB_CORE_DIR "${LIB_CORE_CANONICAL}")
else()
  set(LIB_CORE_DIR "${LIB_CORE_VENDORED}")
endif()

file(GLOB LIB_CODEGEN_SRCS CONFIGURE_DEPENDS
  ${LIB_JNI_DIR}/*.cpp
  ${LIB_JNI_DIR}/react/renderer/components/ShadowlistViewSpec/*.cpp
)

file(GLOB LIB_INCLUDES_SRCS CONFIGURE_DEPENDS
  ${LIB_CPP_DIR}/react/renderer/components/ShadowlistViewSpec/*.cpp
  ${LIB_CPP_DIR}/react/renderer/components/ShadowlistElementViewSpec/*.cpp
  ${LIB_CPP_DIR}/react/renderer/components/ShadowlistTemplateViewSpec/*.cpp
  
  ${LIB_CORE_DIR}/*.cpp
)

add_library(
  react_codegen_ShadowlistViewSpec
  SHARED
  ${LIB_CODEGEN_SRCS}
  ${LIB_INCLUDES_SRCS}
)

target_include_directories(react_codegen_ShadowlistViewSpec PUBLIC
  ${LIB_CPP_DIR}/
  ${LIB_CORE_DIR}/
  ${LIB_ROOT_DIR}/

  ${LIB_CPP_DIR}/react/renderer/components/ShadowlistViewSpec
  ${LIB_CPP_DIR}/react/renderer/components/ShadowlistElementViewSpec
  ${LIB_CPP_DIR}/react/renderer/components/ShadowlistTemplateViewSpec
  ${LIB_JNI_DIR}/
)

find_library(logger log)

target_link_libraries(
  react_codegen_ShadowlistViewSpec
  fbjni
  jsi
  # We need to link different libraries based on whether we are building rncore or not, that's necessary
  # because we want to break a circular dependency between react_codegen_rncore and reactnative
  reactnative
  ${logger}
)

target_compile_reactnative_options(react_codegen_ShadowlistViewSpec PRIVATE)