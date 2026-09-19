#pragma once

#include <cstddef>
#include <cstdio>
#include <utility>

namespace azimgd::shadowlist {

constexpr std::size_t UNDEFINED_INDEX = static_cast<std::size_t>(-1);

// First revision index.
constexpr std::size_t REVISION_COUNT_FIRST = 0;

// Smallest offset change that counts as a real move.
constexpr double OFFSET_MOVED_THRESHOLD = 0.5;

// An offset this close to its target counts as arrived.
constexpr double OFFSET_ARRIVED_THRESHOLD = 1.0;

/*
 * Dead band around the bottom of an inverted list within which the view still counts as
 * "at the bottom", for engaging and releasing the bottom pin (see
 * Container::invertedBottomReleased).
 *
 * Far wider than OFFSET_MOVED_THRESHOLD, which exists to tell a real move from float
 * noise. Here the noise is physical: a stray touch, a rubber-band bounce or a
 * keyboard resize moves the offset by a few points, and treating that as "the reader
 * scrolled away" strands them off the stream for the rest of the reply. A deliberate drag
 * moves an order of magnitude further than this, so nothing intentional falls inside it.
 */
constexpr double INVERTED_FOLLOW_BAND = 24.0;

// Default size estimate (width, height) for unmeasured elements.
constexpr std::pair<double, double> DEFAULT_ESTIMATED_ELEMENT_SIZE = {120.0, 120.0};

// Reserved value on the scrollToIndex command channel meaning "scroll to the end".
constexpr double SCROLL_TO_END_INDEX = -3.0;

}

/*
 * Off by default in release builds (NDEBUG is defined by CMake Release configs, Xcode's
 * Release configuration, and Android release/minified variants without any extra wiring
 * from this library's own build files), on by default otherwise. A consumer can still
 * force either value by defining SHADOWLIST_DEBUG_LOG before this header is included.
 */
#ifndef SHADOWLIST_DEBUG_LOG
#ifdef NDEBUG
#define SHADOWLIST_DEBUG_LOG 0
#else
#define SHADOWLIST_DEBUG_LOG 1
#endif
#endif

/*
 * The device trace (iOS host [SLF] frames, [SLJ] JS lines), still switched on at runtime by
 * SHADOWLIST_FRAME_TRACE=1. Compiled with the debug log by default; a Release build can
 * define SHADOWLIST_FRAME_TRACE_COMPILED=1 alone to trace without the per-commit [SL] output.
 */
#ifndef SHADOWLIST_FRAME_TRACE_COMPILED
#define SHADOWLIST_FRAME_TRACE_COMPILED SHADOWLIST_DEBUG_LOG
#endif

#if SHADOWLIST_DEBUG_LOG
#if defined(__ANDROID__)
/*
 * Android discards an app's stdout, so route the trace through logcat instead:
 * `adb logcat -s SL`. The tag matches the Java host's LOG_TAG and the "[SL]" prefix used
 * on other platforms, so one filter shows both native and host traces.
 */
#include <android/log.h>
#define SL_LOG(...) __android_log_print(ANDROID_LOG_INFO, "SL", __VA_ARGS__)
#else
#define SL_LOG(...) do { printf("[SL] " __VA_ARGS__); printf("\n"); } while (0)
#endif
#else
#define SL_LOG(...) ((void)0)
#endif
