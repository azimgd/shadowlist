cmake_minimum_required(VERSION 3.13)
set(CMAKE_VERBOSE_MAKEFILE ON)

set(LIB_LITERAL SLContainerSpec)
set(LIB_LITERAL_FENWICK fenwick)
set(LIB_LITERAL_REGISTRY registry)
set(LIB_TARGET_NAME react_codegen_${LIB_LITERAL})

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

file(GLOB LIB_IMPORT_SRCS CONFIGURE_DEPENDS SLContainerSpec.cpp)
file(GLOB LIB_CUSTOM_SRCS CONFIGURE_DEPENDS
  ${LIB_COMMON_DIR}/react/renderer/components/${LIB_LITERAL}/*.cpp
  ${LIB_COMMON_DIR}/${LIB_LITERAL_FENWICK}/*.cpp
  ${LIB_COMMON_DIR}/${LIB_LITERAL_REGISTRY}/*.cpp
)

add_library(
  ${LIB_TARGET_NAME}
  SHARED
  ${LIB_IMPORT_SRCS}
  ${LIB_CUSTOM_SRCS}
)

target_include_directories(
  ${LIB_TARGET_NAME}
  PUBLIC 
  ${LIB_COMMON_DIR}
  ${LIB_IMPORT_SRCS}
  ${LIB_CUSTOM_SRCS}
  ${LIB_COMMON_DIR}/react/renderer/components/${LIB_LITERAL}
  ${LIB_COMMON_DIR}/${LIB_LITERAL_FENWICK}
  ${LIB_COMMON_DIR}/${LIB_LITERAL_REGISTRY}
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