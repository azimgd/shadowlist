#pragma once

#include <react/renderer/components/view/ViewEventEmitter.h>

namespace facebook::react {

class SLElementEventEmitter : public ViewEventEmitter {
  public:
  using ViewEventEmitter::ViewEventEmitter;  
};

}
