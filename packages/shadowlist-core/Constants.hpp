#pragma once

#include <cstddef>
#include <cstdio>
#include <utility>

namespace azimgd::shadowlist {

constexpr std::size_t UNDEFINED_INDEX = static_cast<std::size_t>(-1);

/*
 * The offset moved far enough to re-measure the window (rather than floating-point
 * noise), so a re-measure / user-takeover is warranted.
 */
constexpr double OFFSET_MOVED_EPSILON = 0.5;

/*
 * The offset is close enough to its target (a pending scroll or the bottom) to
 * count as arrived.
 */
constexpr double OFFSET_ARRIVED_EPSILON = 1.0;

/*
 * Default per-axis size estimate (cross-axis, main-axis) for unmeasured elements.
 */
constexpr std::pair<double, double> DEFAULT_ESTIMATED_ELEMENT_SIZE = {120.0, 120.0};

/*
 * Sentinel for the scrollToIndex command channel meaning "scroll to the very end".
 * scrollToEnd reuses the same nonce-based one-shot command as scrollToIndex (no
 * separate state field), and the core resolves this value to a bottom-pinning
 * correction that re-targets the end as off-screen rows are measured (so it lands
 * on the true bottom of a variable-height list rather than a stale estimate).
 * Negative, so it never collides with a real index or the "inactive" prop value.
 */
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

