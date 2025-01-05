#include "RNShadowlistSpecJSI.h"

namespace facebook::react {

static jsi::Value __hostFunction_NativeShadowlistCxxSpecJSI_setup(jsi::Runtime &rt, TurboModule &turboModule, const jsi::Value* args, size_t count) {
  static_cast<NativeShadowlistCxxSpecJSI *>(&turboModule)->setup(rt);
  return jsi::Value::undefined();
}

NativeShadowlistCxxSpecJSI::NativeShadowlistCxxSpecJSI(std::shared_ptr<CallInvoker> jsInvoker) : TurboModule("Shadowlist", jsInvoker) {
  methodMap_["setup"] = MethodMetadata {0, __hostFunction_NativeShadowlistCxxSpecJSI_setup};
}

}
