#include "SLModuleSpecJSI.h"

namespace facebook::react {

static jsi::Value __hostFunction_SLModuleCxxSpecJSI_setup(jsi::Runtime &rt, TurboModule &turboModule, const jsi::Value* args, size_t count) {
  static_cast<SLModuleCxxSpecJSI *>(&turboModule)->setup(
    rt
  );
  return jsi::Value::undefined();
}

SLModuleCxxSpecJSI::SLModuleCxxSpecJSI(std::shared_ptr<CallInvoker> jsInvoker): TurboModule("Shadowlist", jsInvoker) {
  methodMap_["setup"] = MethodMetadata {0, __hostFunction_SLModuleCxxSpecJSI_setup};
}

}
