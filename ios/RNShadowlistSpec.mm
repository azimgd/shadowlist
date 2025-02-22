#import "RNShadowlistSpec.h"

@implementation NativeShadowlistSpecBase

- (void)setEventEmitterCallback:(EventEmitterCallbackWrapper *)eventEmitterCallbackWrapper
{
  _eventEmitterCallback = std::move(eventEmitterCallbackWrapper->_eventEmitterCallback);
}
@end

namespace azimgd::shadowlist {

using namespace facebook;
using namespace facebook::react;

static facebook::jsi::Value __hostFunction_NativeShadowlistSpecJSI_setup(facebook::jsi::Runtime& rt, TurboModule &turboModule, const facebook::jsi::Value* args, size_t count) {
  return static_cast<ObjCTurboModule&>(turboModule).invokeObjCMethod(rt, VoidKind, "setup", @selector(setup), args, count);
}

NativeShadowlistSpecJSI::NativeShadowlistSpecJSI(const ObjCTurboModule::InitParams &params): ObjCTurboModule(params) {
  methodMap_["setup"] = MethodMetadata {0, __hostFunction_NativeShadowlistSpecJSI_setup};
}

}
