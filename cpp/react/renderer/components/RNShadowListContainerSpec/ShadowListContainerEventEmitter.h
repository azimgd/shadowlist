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

  struct BatchLayout {
    int size;
  };

  void onVisibleChange(VisibleMetrics value) const;
  void onBatchLayout(BatchLayout value) const;
};

}
