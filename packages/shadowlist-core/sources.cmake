# The core's sources, for every CMake build that compiles it: tests, bench and Android.
file(GLOB SHADOWLIST_CORE_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_LIST_DIR}/*.cpp
  ${CMAKE_CURRENT_LIST_DIR}/host/*.cpp)
