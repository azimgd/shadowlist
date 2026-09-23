#pragma once

#include <jsi/jsi.h>
#include <react/renderer/runtimescheduler/RuntimeScheduler.h>
#include <react/utils/ContextContainer.h>

#include <shadowlist-core/Constants.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>

/*
 * JS side of the device trace, on in debug Apple builds run with SHADOWLIST_FRAME_TRACE=1.
 * Installs globalThis.__shadowlistTrace, which prints an [SLJ] line with a timestamp.
 * It uses the same clock as CACurrentMediaTime, so JS renders line up with native frames in the log.
 */
namespace facebook::react::shadowlist::detail {

#if SHADOWLIST_FRAME_TRACE_COMPILED && defined(__APPLE__)
inline bool jsTraceEnabled() {
  static const bool enabled = [] {
    const char* value = std::getenv("SHADOWLIST_FRAME_TRACE");
    return value != nullptr && std::strcmp(value, "1") == 0;
  }();
  return enabled;
}

inline void installJsTrace(const std::shared_ptr<const ContextContainer>& contextContainer) {
  if (!jsTraceEnabled() || !contextContainer) {
    return;
  }
  auto weakRuntimeScheduler = contextContainer->find<std::weak_ptr<RuntimeScheduler>>(RuntimeSchedulerKey);
  auto runtimeScheduler = weakRuntimeScheduler ? weakRuntimeScheduler->lock() : nullptr;
  if (!runtimeScheduler) {
    return;
  }
  runtimeScheduler->scheduleWork([](jsi::Runtime& runtime) {
    auto global = runtime.global();
    if (global.hasProperty(runtime, "__shadowlistTrace")) {
      return;
    }
    auto trace = jsi::Function::createFromHostFunction(
      runtime,
      jsi::PropNameID::forAscii(runtime, "__shadowlistTrace"),
      1,
      [](jsi::Runtime& runtime, const jsi::Value&, const jsi::Value* arguments, std::size_t count) -> jsi::Value {
        if (count > 0 && arguments[0].isString()) {
          auto message = arguments[0].getString(runtime).utf8(runtime);
          double seconds = static_cast<double>(clock_gettime_nsec_np(CLOCK_UPTIME_RAW)) / 1e9;
          printf("[SLJ] t=%.4f %s\n", seconds, message.c_str());
        }
        return jsi::Value::undefined();
      });
    global.setProperty(runtime, "__shadowlistTrace", std::move(trace));
  });
}
#else
inline void installJsTrace(const std::shared_ptr<const ContextContainer>&) {}
#endif

}

/*
 * Commit counter for the perf suite (shadowlist-core-bench/perf-suite.sh): one [SLC] line per
 * non-layout clone of a list, which is one per Fabric commit that touched it (a React render
 * or a state update). Apple: on with SHADOWLIST_FRAME_TRACE=1, stdout, CLOCK_UPTIME_RAW like
 * [SLJ]. Android: on with adb shell setprop log.tag.SLC D (apps can read log.tag.*), logcat tag SLC.
 */
#if SHADOWLIST_FRAME_TRACE_COMPILED && (defined(__APPLE__) || defined(__ANDROID__))
#if defined(__ANDROID__)
#include <android/log.h>
#include <sys/system_properties.h>
#endif
namespace facebook::react::shadowlist::detail {
inline void traceCommit(int tag) {
#if defined(__APPLE__)
  if (jsTraceEnabled()) {
    printf("[SLC] t=%.4f commit list=%d\n", static_cast<double>(clock_gettime_nsec_np(CLOCK_UPTIME_RAW)) / 1e9, tag);
  }
#else
  static const bool enabled = [] {
    char value[PROP_VALUE_MAX] = {0};
    return __system_property_get("log.tag.SLC", value) > 0 && (value[0] == 'D' || value[0] == 'V');
  }();
  if (enabled) {
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    __android_log_print(ANDROID_LOG_INFO, "SLC", "[SLC] t=%.4f commit list=%d", now.tv_sec + now.tv_nsec / 1e9, tag);
  }
#endif
}
}
#define SL_TRACE_COMMIT(tag) ::facebook::react::shadowlist::detail::traceCommit(static_cast<int>(tag))
#else
#define SL_TRACE_COMMIT(tag) ((void)0)
#endif
