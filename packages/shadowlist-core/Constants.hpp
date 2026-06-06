#pragma once

#include <cstddef>
#include <cstdio>
#include <utility>

namespace azimgd::shadowlist {

constexpr std::size_t UNDEFINED_INDEX = static_cast<std::size_t>(-1);

// Minimum offset delta that counts as a real move (vs. floating-point noise).
constexpr double OFFSET_MOVED_EPSILON = 0.5;

// Offset within this distance of its target counts as arrived.
constexpr double OFFSET_ARRIVED_EPSILON = 1.0;

// Default per-axis size estimate (cross-axis, main-axis) for unmeasured elements.
constexpr std::pair<double, double> DEFAULT_ESTIMATED_ELEMENT_SIZE = {120.0, 120.0};

// Sentinel on the scrollToIndex command channel meaning "scroll to the very end".
// Negative so it never collides with a real index or the inactive prop value.
constexpr double SCROLL_TO_END_INDEX = -3.0;

}

#ifndef SHADOWLIST_DEBUG_LOG
#define SHADOWLIST_DEBUG_LOG 0
#endif

#if SHADOWLIST_DEBUG_LOG
#define SL_LOG(...) do { printf("[SL] " __VA_ARGS__); printf("\n"); } while (0)
#else
#define SL_LOG(...) ((void)0)
#endif

