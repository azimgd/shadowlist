cmake_minimum_required(VERSION 3.13)
project(shadowlist_core_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The core has no dependencies, so its sources compile straight into the test binary.
file(GLOB CORE_SRCS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../shadowlist-core/*.cpp)
file(GLOB TEST_SRCS CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/tests_*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

add_executable(shadowlist_core_tests ${CORE_SRCS} ${TEST_SRCS})

target_include_directories(shadowlist_core_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..   # for shadowlist-core includes
  ${CMAKE_CURRENT_SOURCE_DIR}     # for the test framework and helpers
  # The native binding parser, which builds without folly
  ${CMAKE_CURRENT_SOURCE_DIR}/../shadowlist-fabric/cpp/react/renderer/components/ShadowListViewSpec)

# Keep core [SL] traces out of test output in every build type.
target_compile_definitions(shadowlist_core_tests PRIVATE SHADOWLIST_DEBUG_LOG=0)

find_package(Threads REQUIRED)
target_link_libraries(shadowlist_core_tests PRIVATE Threads::Threads)

enable_testing()
add_test(NAME shadowlist_core_tests COMMAND shadowlist_core_tests)
