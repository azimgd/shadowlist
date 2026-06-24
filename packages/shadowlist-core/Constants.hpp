#pragma once

#include <cstddef>
#include <cstdio>
#include <utility>

namespace azimgd::shadowlist {

constexpr std::size_t UNDEFINED_INDEX = static_cast<std::size_t>(-1);

// Smallest offset change that counts as a real move.
constexpr double OFFSET_MOVED_THRESHOLD = 0.5;

// An offset this close to its target counts as arrived.
constexpr double OFFSET_ARRIVED_THRESHOLD = 1.0;

// Default size estimate (width, height) for unmeasured elements.
constexpr std::pair<double, double> DEFAULT_ESTIMATED_ELEMENT_SIZE = {120.0, 120.0};

// Reserved value on the scrollToIndex command channel meaning "scroll to the end".
constexpr double SCROLL_TO_END_INDEX = -3.0;

}

#ifndef SHADOWLIST_DEBUG_LOG
#define SHADOWLIST_DEBUG_LOG 1
#endif

#if SHADOWLIST_DEBUG_LOG
#define SL_LOG(...) do { printf("[SL] " __VA_ARGS__); printf("\n"); } while (0)
#else
#define SL_LOG(...) ((void)0)
#endif
