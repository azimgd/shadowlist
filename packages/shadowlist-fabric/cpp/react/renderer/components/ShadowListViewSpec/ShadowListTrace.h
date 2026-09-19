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
 * JS side of the device trace (debug Apple builds launched with SHADOWLIST_FRAME_TRACE=1, the
 * same switch as the host's [SLF] frame trace). Installs `globalThis.__shadowlistTrace(message)`,
 * which prints `[SLJ] t=<seconds>` to stdout. The clock is mach_absolute_time, the one
 * CACurrentMediaTime reads, so JS renders line up with the host's committed frames in one log.
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
