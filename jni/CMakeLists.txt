cmake_minimum_required(VERSION 3.13)
set(CMAKE_VERBOSE_MAKEFILE on)

set(PACKAGE_NAME "react_codegen_RNShadowListContainerSpec")

file(GLOB react_codegen_SRCS CONFIGURE_DEPENDS *.cpp react/renderer/components/RNShadowListContainerSpec/*.cpp)

add_library(
  ${PACKAGE_NAME}
  SHARED
  ${react_codegen_SRCS}
)

target_include_directories(${PACKAGE_NAME} PUBLIC . react/renderer/components/RNShadowListContainerSpec)

target_link_libraries(
  ${PACKAGE_NAME}
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
  ${PACKAGE_NAME}
  PRIVATE
  -DLOG_TAG=\"ReactNative\"
  -fexceptions
  -frtti
  -std=c++20
  -Wall
)
