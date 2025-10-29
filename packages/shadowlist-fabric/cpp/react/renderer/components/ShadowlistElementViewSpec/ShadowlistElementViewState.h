#pragma once

#include <react/renderer/core/StateData.h>
#ifdef RN_SERIALIZABLE_STATE
#include <folly/dynamic.h>
#endif

namespace facebook::react {

using ShadowlistElementViewState = StateData;

}
