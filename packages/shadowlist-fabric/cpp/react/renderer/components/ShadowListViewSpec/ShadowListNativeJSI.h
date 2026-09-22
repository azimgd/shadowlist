#pragma once

#include <react/utils/ContextContainer.h>

#include <memory>

namespace facebook::react {

/*
 * Installs globalThis.__shadowListNative, the data API ShadowListNative calls from JS.
 * Each call runs on the JS thread, writes the native store directly and asks for a commit,
 * so row updates skip React props and view commands.
 * View commands would not work here. On Android they can only reach the store through state
 * updates, and those merge so only the last one in a frame survives. Here the state update
 * after a write is just a nudge, so merging is harmless.
 * Safe to call more than once per runtime. JS waits for the global before rendering a list.
 */
void installShadowListNativeJSI(const std::shared_ptr<const ContextContainer>& contextContainer);

}
