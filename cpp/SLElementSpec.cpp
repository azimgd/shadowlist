#ifdef ANDROID
#include "SLElementSpec.h"

namespace facebook::react {

std::shared_ptr<TurboModule> SLElementSpec_ModuleProvider(const std::string &moduleName, const JavaTurboModule::InitParams &params) {
  return nullptr;
}

}
#endif
