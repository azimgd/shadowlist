#pragma once

#include <react/renderer/components/view/ViewEventEmitter.h>

namespace facebook::react {

class ShadowListContainerEventEmitter : public ViewEventEmitter {
  public:
    using ViewEventEmitter::ViewEventEmitter;

  struct VisibleChildrenUpdate {
    int visibleStartIndex;
    int visibleEndIndex;
  };

  struct EndReached {
    int distanceFromEnd;
  };

  struct StartReached {
    int distanceFromStart;
  };

  void onVisibleChildrenUpdate(VisibleChildrenUpdate value) const;
  void onEndReached(EndReached value) const;
  void onStartReached(StartReached value) const;
};

}
