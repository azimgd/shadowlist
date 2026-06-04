#ifndef Constants_hpp
#define Constants_hpp

#include <cstddef>
#include <cstdio>

namespace azimgd::shadowlist {

constexpr std::size_t UNDEFINED_INDEX = static_cast<std::size_t>(-1);

}

/*
 * Temporary debug logging. Set SHADOWLIST_DEBUG_LOG to 0 (or remove these calls)
 * once the inverted/virtualization behavior is confirmed. Output is prefixed with
 * "[SL]" and goes to stderr (visible in the Xcode console / `react-native log-ios`).
 */
#ifndef SHADOWLIST_DEBUG_LOG
#define SHADOWLIST_DEBUG_LOG 0
#endif

#if SHADOWLIST_DEBUG_LOG
#define SL_LOG(...) do { printf("[SL] " __VA_ARGS__); printf("\n"); } while (0)
#else
#define SL_LOG(...) ((void)0)
#endif

#endif
