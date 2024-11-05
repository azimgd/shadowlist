#pragma once

#include <react/renderer/components/view/ViewEventEmitter.h>


namespace facebook::react {

struct OnVisibleChange {
  int visibleStartIndex;
  int visibleEndIndex;
};

class SLContainerEventEmitter : public ViewEventEmitter {
  public:
  using ViewEventEmitter::ViewEventEmitter;
  
  void onVisibleChange(OnVisibleChange value) const;
};
}
