#pragma once

#include <react/renderer/components/view/ViewEventEmitter.h>

namespace facebook::react {

class ShadowListContainerEventEmitter : public ViewEventEmitter {
  public:
    using ViewEventEmitter::ViewEventEmitter;

  struct VisibleMetrics {
    int start;
    int end;
  };

  struct EndReached {
    int distanceFromEnd;
  };

  struct StartReached {
    int distanceFromStart;
  };

  void onVisibleChange(VisibleMetrics value) const;
  void onEndReached(EndReached value) const;
  void onStartReached(StartReached value) const;
};

}
