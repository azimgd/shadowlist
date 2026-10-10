#pragma once

#include <cstddef>
#include <cstdio>
#include <utility>

namespace azimgd::shadowlist {

constexpr std::size_t UNDEFINED_INDEX = static_cast<std::size_t>(-1);

constexpr std::size_t REVISION_COUNT_FIRST = 0;

/*
 * The smallest offset change that counts as a real move.
 */
constexpr double OFFSET_MOVED_THRESHOLD = 0.5;

/*
 * An offset this close to its target counts as arrived.
 */
constexpr double OFFSET_ARRIVED_THRESHOLD = 1.0;

/*
 * How close to the bottom of an inverted list still counts as at the bottom for the bottom pin.
 * Wide on purpose: a stray touch, a bounce or a keyboard resize moves a few points, and
 * treating that as scrolling away would drop the reader off the stream. Real drags go much further.
 */
constexpr double INVERTED_FOLLOW_BAND = 24.0;

/*
 * How far each end of an OffsetBand is pulled in from the offset where something flips.
 * A host offset right on that edge then counts as outside and still sends its frame.
 */
constexpr double OFFSET_BAND_MARGIN = 0.5;

/*
 * Width and height we assume for a row until it is measured.
 */
constexpr std::pair<double, double> DEFAULT_ESTIMATED_ROW_SIZE = {120.0, 120.0};

/*
 * Sent as the scrollToRow index to mean scroll to the end.
 */
constexpr double SCROLL_TO_END_INDEX = -3.0;

/*
 * Sent as the scrollToRow index to mean scroll to a content offset. viewOffset then carries the
 * absolute content offset instead of a distance past a row.
 */
constexpr double SCROLL_TO_OFFSET_INDEX = -4.0;

}

/*
 * The [SL] debug log prints on every pass. It is off unless the build defines
 * SHADOWLIST_DEBUG_LOG=1. Each host has a switch: SHADOWLIST_DEBUG_LOG=1 pod install for
 * the Fabric pod, -DSHADOWLIST_DEBUG_LOG=1 for the CMake builds (the Fabric Android library
 * and the core tests), -PshadowlistDebugLog for ShadowListKit on Android and a
 * GCC_PREPROCESSOR_DEFINITIONS entry for ShadowListKit on iOS.
 */
#ifndef SHADOWLIST_DEBUG_LOG
#define SHADOWLIST_DEBUG_LOG 0
#endif

/*
 * The device trace is compiled into debug builds, which leave NDEBUG undefined, and into any
 * build with the debug log. It is turned on at runtime with SHADOWLIST_FRAME_TRACE=1. A release
 * build can set SHADOWLIST_FRAME_TRACE_COMPILED=1 to get the trace without the per pass log.
 */
#ifndef SHADOWLIST_FRAME_TRACE_COMPILED
#if SHADOWLIST_DEBUG_LOG || !defined(NDEBUG)
#define SHADOWLIST_FRAME_TRACE_COMPILED 1
#else
#define SHADOWLIST_FRAME_TRACE_COMPILED 0
#endif
#endif

#if SHADOWLIST_DEBUG_LOG
#if defined(__ANDROID__)
/*
 * Android drops stdout. Log through logcat. Read it with adb logcat -s SL.
 * The SL tag matches the Java side and one filter shows both.
 */
#include <android/log.h>
#define SL_LOG(...) __android_log_print(ANDROID_LOG_INFO, "SL", __VA_ARGS__)
#else
#define SL_LOG(...) do { printf("[SL] " __VA_ARGS__); printf("\n"); } while (0)
#endif
#else
#define SL_LOG(...) ((void)0)
#endif
