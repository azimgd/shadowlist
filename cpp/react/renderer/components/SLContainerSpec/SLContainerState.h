#pragma once

#ifdef ANDROID
#include <folly/dynamic.h>
#endif

namespace facebook::react {

class SLContainerState {
  public:
  SLContainerState() = default;

#ifdef ANDROID
  SLContainerState(SLContainerState const &previousState, folly::dynamic data){};
  folly::dynamic getDynamic() const {
    return {};
  };
#endif
};

}
