#include "RNShadowlistSpec.h"

namespace facebook::react {

static facebook::jsi::Value __hostFunction_NativeShadowlistSpecJSI_setup(facebook::jsi::Runtime& rt, TurboModule &turboModule, const facebook::jsi::Value* args, size_t count) {
  static jmethodID cachedMethodId = nullptr;
  return static_cast<JavaTurboModule &>(turboModule).invokeJavaMethod(rt, VoidKind, "setup", "()V", args, count, cachedMethodId);
}

NativeShadowlistSpecJSI::NativeShadowlistSpecJSI(const JavaTurboModule::InitParams &params): JavaTurboModule(params) {
  methodMap_["setup"] = MethodMetadata {0, __hostFunction_NativeShadowlistSpecJSI_setup};
}

std::shared_ptr<TurboModule> RNShadowlistSpec_ModuleProvider(const std::string &moduleName, const JavaTurboModule::InitParams &params) {
  if (moduleName == "Shadowlist") {
    return std::make_shared<NativeShadowlistSpecJSI>(params);
  }
  return nullptr;
}

}
