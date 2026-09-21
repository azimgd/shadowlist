#pragma once

#include <react/utils/ContextContainer.h>

#include <memory>

namespace facebook::react {

/*
 * Installs `globalThis.__shadowListNative`, the JS side of ShadowListNative's data API. Every
 * call is synchronous on the JS thread: it mutates the list's native store directly and asks for
 * a commit, so a row update never round-trips through React props or a view command.
 *
 * Why JSI rather than view commands: a command reaches the host view on the UI thread, which on
 * Android is Java with no path back to the C++ store except a state update, and state updates
 * for one node coalesce (EventQueue keeps only the last), which would drop all but the last of
 * several mutations in one frame. Here JS writes the store itself; the state update that follows
 * is only a nudge, so coalescing it is harmless. It also gives synchronous reads (getItem,
 * resolveTag) and needs no host code at all on either platform.
 *
 * Scheduled onto the JS thread through the RuntimeScheduler in the ContextContainer; idempotent
 * per runtime. JS waits for the global to appear before rendering a list (see ShadowListNative.tsx).
 */
void installShadowListNativeJSI(const std::shared_ptr<const ContextContainer>& contextContainer);

}
