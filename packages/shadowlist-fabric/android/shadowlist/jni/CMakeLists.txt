cmake_minimum_required(VERSION 3.13)
set(CMAKE_VERBOSE_MAKEFILE on)

set(LIB_JNI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../build/generated/source/codegen/jni)
set(LIB_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
set(LIB_CPP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../cpp)

# In the monorepo, compile the core straight from packages/shadowlist-core.
# A published package has no such path, so use the copy shipped inside it.
get_filename_component(LIB_CORE_CANONICAL "${CMAKE_CURRENT_SOURCE_DIR}/../../../../shadowlist-core" ABSOLUTE)
get_filename_component(LIB_CORE_VENDORED "${CMAKE_CURRENT_SOURCE_DIR}/../../../shadowlist-core" ABSOLUTE)
if(EXISTS "${LIB_CORE_CANONICAL}/Container.cpp")
  set(LIB_CORE_DIR "${LIB_CORE_CANONICAL}")
else()
  set(LIB_CORE_DIR "${LIB_CORE_VENDORED}")
endif()

get_filename_component(LIB_CORE_PARENT_DIR "${LIB_CORE_DIR}" DIRECTORY)

file(GLOB LIB_CODEGEN_SRCS CONFIGURE_DEPENDS
  ${LIB_JNI_DIR}/*.cpp
  ${LIB_JNI_DIR}/react/renderer/components/ShadowListViewSpec/*.cpp
)

file(GLOB LIB_INCLUDES_SRCS CONFIGURE_DEPENDS
  ${LIB_CPP_DIR}/react/renderer/components/ShadowListViewSpec/*.cpp
  ${LIB_CPP_DIR}/react/renderer/components/ShadowListElementViewSpec/*.cpp
  ${LIB_CPP_DIR}/react/renderer/components/ShadowListTemplateViewSpec/*.cpp
  
  ${LIB_CORE_DIR}/*.cpp
)

add_library(
  react_codegen_ShadowListViewSpec
  SHARED
  ${LIB_CODEGEN_SRCS}
  ${LIB_INCLUDES_SRCS}
)

target_include_directories(react_codegen_ShadowListViewSpec PUBLIC
  ${LIB_CPP_DIR}/
  ${LIB_CORE_DIR}/
  ${LIB_CORE_PARENT_DIR}/
  ${LIB_ROOT_DIR}/

  ${LIB_CPP_DIR}/react/renderer/components/ShadowListViewSpec
  ${LIB_CPP_DIR}/react/renderer/components/ShadowListElementViewSpec
  ${LIB_CPP_DIR}/react/renderer/components/ShadowListTemplateViewSpec
  ${LIB_JNI_DIR}/
)

find_library(logger log)

target_link_libraries(
  react_codegen_ShadowListViewSpec
  fbjni
  jsi
  # Link reactnative here, not rncore, to avoid a circular dependency.
  reactnative
  ${logger}
)

target_compile_reactnative_options(react_codegen_ShadowListViewSpec PRIVATE)