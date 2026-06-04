#ifndef Constants_hpp
#define Constants_hpp

#include <cstddef>
#include <cstdio>

namespace azimgd::shadowlist {

constexpr std::size_t UNDEFINED_INDEX = static_cast<std::size_t>(-1);

}

#ifndef SHADOWLIST_DEBUG_LOG
#define SHADOWLIST_DEBUG_LOG 0
#endif

#if SHADOWLIST_DEBUG_LOG
#define SL_LOG(...) do { printf("[SL] " __VA_ARGS__); printf("\n"); } while (0)
#else
#define SL_LOG(...) ((void)0)
#endif

#endif
