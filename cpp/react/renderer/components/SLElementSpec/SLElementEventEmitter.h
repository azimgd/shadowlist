#pragma once

#include <react/renderer/components/view/ViewEventEmitter.h>

namespace azimgd::shadowlist {

using namespace facebook;
using namespace facebook::react;

class SLElementEventEmitter : public ViewEventEmitter {
  public:
  using ViewEventEmitter::ViewEventEmitter;  
};

}
