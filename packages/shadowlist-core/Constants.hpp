#pragma once

#include <cstddef>
#include <cstdio>
#include <utility>

namespace azimgd::shadowlist {

constexpr std::size_t UNDEFINED_INDEX = static_cast<std::size_t>(-1);

// Index of the first revision.
constexpr std::size_t REVISION_COUNT_FIRST = 0;

// The smallest offset change that counts as a real move.
constexpr double OFFSET_MOVED_THRESHOLD = 0.5;

// An offset this close to its target counts as arrived.
constexpr double OFFSET_ARRIVED_THRESHOLD = 1.0;

/*
 * How close to the bottom of an inverted list still counts as at the bottom for the bottom pin.
 * Wide on purpose: a stray touch, a bounce or a keyboard resize moves a few points, and
 * treating that as scrolling away would drop the reader off the stream. Real drags go much further.
 */
constexpr double INVERTED_FOLLOW_BAND = 24.0;

// Width and height we assume for a row until it is measured.
constexpr std::pair<double, double> DEFAULT_ESTIMATED_ELEMENT_SIZE = {120.0, 120.0};

// Sent as the scrollToIndex index to mean scroll to the end.
constexpr double SCROLL_TO_END_INDEX = -3.0;

}

/*
 * Debug log is off in release builds, which all define NDEBUG, and on otherwise.
 * Define SHADOWLIST_DEBUG_LOG before this header to force either way.
 */
#ifndef SHADOWLIST_DEBUG_LOG
#ifdef NDEBUG
#define SHADOWLIST_DEBUG_LOG 0
#else
#define SHADOWLIST_DEBUG_LOG 1
#endif
#endif

/*
 * The device trace is compiled in with the debug log and turned on at runtime with
 * SHADOWLIST_FRAME_TRACE=1. A release build can set SHADOWLIST_FRAME_TRACE_COMPILED=1
 * to get the trace without the per commit log.
 */
#ifndef SHADOWLIST_FRAME_TRACE_COMPILED
#define SHADOWLIST_FRAME_TRACE_COMPILED SHADOWLIST_DEBUG_LOG
#endif

#if SHADOWLIST_DEBUG_LOG
#if defined(__ANDROID__)
/*
 * Android drops stdout, so log through logcat. Read it with adb logcat -s SL.
 * The SL tag matches the Java side, so one filter shows both.
 */
#include <android/log.h>
#define SL_LOG(...) __android_log_print(ANDROID_LOG_INFO, "SL", __VA_ARGS__)
#else
#define SL_LOG(...) do { printf("[SL] " __VA_ARGS__); printf("\n"); } while (0)
#endif
#else
#define SL_LOG(...) ((void)0)
#endif
