#pragma once
#include "SLFenwickTree.hpp"

#ifdef ANDROID
#include <folly/dynamic.h>
#endif

namespace facebook::react {

class SLContainerState {
  public:
  SLContainerState(SLFenwickTree childrenMeasurements);
  SLContainerState() = default;

  SLFenwickTree childrenMeasurements;

#ifdef ANDROID
  SLContainerState(SLContainerState const &previousState, folly::dynamic data){};
  folly::dynamic getDynamic() const {
    return {};
  };
#endif
};

}
