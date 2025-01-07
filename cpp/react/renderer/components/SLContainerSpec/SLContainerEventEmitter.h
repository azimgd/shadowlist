#pragma once

#include <react/renderer/components/view/ViewEventEmitter.h>


namespace facebook::react {

struct OnVisibleChange {
  int visibleStartIndex;
  int visibleEndIndex;
};
struct OnStartReached {
  int distanceFromStart;
};
struct OnEndReached {
  int distanceFromEnd;
};
struct OnScroll {
  Point contentOffset;
  Size contentSize;
};

class SLContainerEventEmitter : public ViewEventEmitter {
  public:
  using ViewEventEmitter::ViewEventEmitter;
  
  void onVisibleChange(OnVisibleChange value) const;
  void onStartReached(OnStartReached value) const;
  void onEndReached(OnEndReached value) const;
  void onScroll(OnScroll value) const;
};
}
