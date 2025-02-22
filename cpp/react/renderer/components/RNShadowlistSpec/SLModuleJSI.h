#include "SLCommitHook.h"

namespace azimgd::shadowlist {

using namespace facebook;
using namespace facebook::react;

class SLModuleJSI {
  public:
  static void install(facebook::jsi::Runtime &runtime);
  static void install(facebook::jsi::Runtime &runtime, std::shared_ptr<SLCommitHook> &commitHook);
};

}
