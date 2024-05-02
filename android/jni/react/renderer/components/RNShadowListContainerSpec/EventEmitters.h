#pragma once

#include <react/renderer/components/view/ViewEventEmitter.h>


namespace facebook::react {
class ShadowListContainerEventEmitter : public ViewEventEmitter {
 public:
  using ViewEventEmitter::ViewEventEmitter;

  struct OnVisibleChange {
      int start;
    int end;
    };
  void onVisibleChange(OnVisibleChange value) const;
};
class ShadowListItemEventEmitter : public ViewEventEmitter {
 public:
  using ViewEventEmitter::ViewEventEmitter;

  
  
};
}
