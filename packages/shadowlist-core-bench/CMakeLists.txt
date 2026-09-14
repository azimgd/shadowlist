cmake_minimum_required(VERSION 3.13)
project(shadowlist_core_bench CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# The platform-agnostic core has no third-party dependencies, so compile its sources
# straight into the benchmark binary (mirrors shadowlist-core-tests).
file(GLOB CORE_SRCS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../shadowlist-core/*.cpp)

add_executable(shadowlist_core_bench ${CORE_SRCS} ${CMAKE_CURRENT_SOURCE_DIR}/bench.cpp)

target_compile_definitions(shadowlist_core_bench PRIVATE SHADOWLIST_DEBUG_LOG=0)

target_include_directories(shadowlist_core_bench PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..   # so <shadowlist-core/...> resolves
  ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Threads REQUIRED)
target_link_libraries(shadowlist_core_bench PRIVATE Threads::Threads)
