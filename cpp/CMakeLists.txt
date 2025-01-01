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
  ${LIB_COMMON_DIR}/module/SLModuleSpec/*.cpp
  ${LIB_COMMON_DIR}/json/*.hpp
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
  ${LIB_COMMON_DIR}/module/SLModuleSpec
  ${LIB_COMMON_DIR}/json
)

target_link_libraries(
  ${LIB_TARGET_NAME}
  fbjni::fbjni
  ReactAndroid::jsi
)
if (ReactAndroid_VERSION_MINOR GREATER_EQUAL 76)
  target_link_libraries(
    ${LIB_TARGET_NAME}
    ReactAndroid::reactnative
  )
elseif (ReactAndroid_VERSION_MINOR GREATER_EQUAL 75)
  target_link_libraries(
    ${LIB_TARGET_NAME}
    ReactAndroid::fabricjni
    ReactAndroid::folly_runtime
    ReactAndroid::glog
    ReactAndroid::react_debug
    ReactAndroid::react_nativemodule_core
    ReactAndroid::react_performance_timeline
    ReactAndroid::react_render_consistency
    ReactAndroid::react_render_core
    ReactAndroid::react_render_debug
    ReactAndroid::react_render_graphics
    ReactAndroid::react_render_imagemanager
    ReactAndroid::react_render_mapbuffer
    ReactAndroid::react_render_observers_events
    ReactAndroid::react_render_textlayoutmanager
    ReactAndroid::reactnativejni
    ReactAndroid::rrc_text
    ReactAndroid::rrc_textinput
    ReactAndroid::rrc_view
    ReactAndroid::runtimeexecutor
    ReactAndroid::yoga
  )
else ()
  message(FATAL_ERROR "shadowlist requires react-native 0.75 or newer.")
endif ()

target_compile_options(
  ${LIB_TARGET_NAME}
  PRIVATE
  -DLOG_TAG=\"ReactNative\"
  -fexceptions
  -frtti
  -std=c++20
  -Wall
)