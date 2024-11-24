cmake_minimum_required(VERSION 3.13)
set(CMAKE_VERBOSE_MAKEFILE ON)

set(LIB_TARGET_NAME react_codegen_SLContainerSpec)
set(LIB_ANDROID_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../android)
set(LIB_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_compile_options(
  -fexceptions
  -frtti
  -std=c++20
  -Wall
  -Wpedantic
  -Wno-gnu-zero-variadic-macro-arguments
)

file(GLOB LIB_CUSTOM_SRCS CONFIGURE_DEPENDS
  SLContainerSpec.cpp
  SLElementSpec.cpp
  ${LIB_COMMON_DIR}/react/renderer/components/SLContainerSpec/*.cpp
  ${LIB_COMMON_DIR}/react/renderer/components/SLElementSpec/*.cpp
  ${LIB_COMMON_DIR}/fenwick/*.cpp
  ${LIB_COMMON_DIR}/registry/*.cpp
  ${LIB_COMMON_DIR}/helpers/*.cpp
)

add_library(
  ${LIB_TARGET_NAME}
  SHARED
  ${LIB_IMPORT_SRCS}
  ${LIB_CUSTOM_SRCS}
  cpp-adapter.cpp
)

target_include_directories(
  ${LIB_TARGET_NAME}
  PUBLIC 
  ${LIB_COMMON_DIR}
  ${LIB_IMPORT_SRCS}
  ${LIB_CUSTOM_SRCS}
  ${LIB_COMMON_DIR}/react/renderer/components/SLContainerSpec
  ${LIB_COMMON_DIR}/react/renderer/components/SLElementSpec
  ${LIB_COMMON_DIR}/fenwick
  ${LIB_COMMON_DIR}/registry
  ${LIB_COMMON_DIR}/helpers
)

target_link_libraries(
  ${LIB_TARGET_NAME}
  fbjni
  folly_runtime
  glog
  jsi
  react_codegen_rncore
  react_debug
  react_nativemodule_core
  react_render_componentregistry
  react_render_core
  react_render_debug
  react_render_graphics
  react_render_imagemanager
  react_render_mapbuffer
  react_utils
  rrc_image
  rrc_view
  turbomodulejsijni
  yoga
)

target_compile_options(
  ${LIB_TARGET_NAME}
  PRIVATE
  -DLOG_TAG=\"ReactNative\"
  -fexceptions
  -frtti
  -std=c++20
  -Wall
)