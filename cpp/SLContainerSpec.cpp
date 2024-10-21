#ifdef ANDROID
#include "SLContainerSpec.h"

namespace facebook::react {

std::shared_ptr<TurboModule> SLContainerSpec_ModuleProvider(const std::string &moduleName, const JavaTurboModule::InitParams &params) {
  return nullptr;
}

}
#endif