#pragma once

#include <react/renderer/components/view/ViewEventEmitter.h>


namespace facebook::react {

struct OnVisibleChange {
  int visibleStartIndex;
  int visibleEndIndex;
};
struct OnEndReached {
  int distanceFromEnd;
};

class SLContainerEventEmitter : public ViewEventEmitter {
  public:
  using ViewEventEmitter::ViewEventEmitter;
  
  void onVisibleChange(OnVisibleChange value) const;
  void onEndReached(OnEndReached value) const;
};
}
