# The list engine and the host layer, which every list links.
file(GLOB SHADOWLIST_CORE_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_LIST_DIR}/*.cpp
  ${CMAKE_CURRENT_LIST_DIR}/host/*.cpp)

# The template logic, which only the templates libraries and the core tests link. It builds
# without the sources above.
file(GLOB SHADOWLIST_UTILS_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_LIST_DIR}/utils/*.cpp)
