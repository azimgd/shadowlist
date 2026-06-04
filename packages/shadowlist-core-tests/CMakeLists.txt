cmake_minimum_required(VERSION 3.13)
project(shadowlist_core_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The platform-agnostic core has no third-party dependencies, so we just compile
# its sources straight into the test binary.
file(GLOB CORE_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../shadowlist-core/*.cpp)
file(GLOB TEST_SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/tests_*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

add_executable(shadowlist_core_tests ${CORE_SRCS} ${TEST_SRCS})

target_include_directories(shadowlist_core_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..   # so <shadowlist-core/...> resolves
  ${CMAKE_CURRENT_SOURCE_DIR})     # so "TestFramework.hpp" / "Harness.hpp" resolve

target_compile_definitions(shadowlist_core_tests PRIVATE
  SLT_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

find_package(Threads REQUIRED)
target_link_libraries(shadowlist_core_tests PRIVATE Threads::Threads)

enable_testing()
add_test(NAME shadowlist_core_tests COMMAND shadowlist_core_tests)
