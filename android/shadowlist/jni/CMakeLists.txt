cmake_minimum_required(VERSION 3.13)
set(CMAKE_VERBOSE_MAKEFILE on)

set(LIB_JNI_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(LIB_CPP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../cpp)

file(GLOB LIB_CODEGEN_SRCS CONFIGURE_DEPENDS
  ${LIB_JNI_DIR}/*.cpp
  ${LIB_JNI_DIR}/react/renderer/components/RNShadowlistSpec/*.cpp
  ${LIB_CPP_DIR}/react/renderer/components/RNShadowlistSpec/*.cpp
  ${LIB_CPP_DIR}/react/renderer/components/SLContainerSpec/*.cpp
  ${LIB_CPP_DIR}/react/renderer/components/SLElementSpec/*.cpp
)

file(GLOB LIB_INCLUDES_SRCS CONFIGURE_DEPENDS
  ${LIB_CPP_DIR}/fenwick/*.cpp
  ${LIB_CPP_DIR}/json/*.hpp
)

add_library(
  react_codegen_RNShadowlistSpec
  SHARED
  ${LIB_CODEGEN_SRCS}
  ${LIB_INCLUDES_SRCS}
)

target_include_directories(react_codegen_RNShadowlistSpec PUBLIC
  ${LIB_JNI_DIR}/
  ${LIB_CPP_DIR}/fenwick
  ${LIB_CPP_DIR}/json
  ${LIB_CPP_DIR}/react/renderer/components/SLContainerSpec
  ${LIB_CPP_DIR}/react/renderer/components/SLElementSpec
  ${LIB_CPP_DIR}/react/renderer/components/RNShadowlistSpec
  ${LIB_JNI_DIR}/react/renderer/components/RNShadowlistSpec
)

find_library(logger log)

target_link_libraries(
  react_codegen_RNShadowlistSpec
  fbjni
  jsi
  # We need to link different libraries based on whether we are building rncore or not, that's necessary
  # because we want to break a circular dependency between react_codegen_rncore and reactnative
  reactnative
  ${logger}
)

target_compile_options(
  react_codegen_RNShadowlistSpec
  PRIVATE
  -DLOG_TAG=\"ReactNative\"
  -fexceptions
  -frtti
  -std=c++20
  -Wall
)
