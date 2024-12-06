#import "SLModuleSpec.h"

@implementation SLModuleSpecBase

- (void)setEventEmitterCallback:(EventEmitterCallbackWrapper *)eventEmitterCallbackWrapper
{
  _eventEmitterCallback = std::move(eventEmitterCallbackWrapper->_eventEmitterCallback);
}
@end

namespace facebook::react {

static facebook::jsi::Value __hostFunction_SLModuleSpecJSI_setup(facebook::jsi::Runtime& rt, TurboModule &turboModule, const facebook::jsi::Value* args, size_t count) {
  return static_cast<ObjCTurboModule&>(turboModule).invokeObjCMethod(rt, VoidKind, "setup", @selector(setup), args, count);
}

SLModuleSpecJSI::SLModuleSpecJSI(const ObjCTurboModule::InitParams &params): ObjCTurboModule(params) {
  methodMap_["setup"] = MethodMetadata {0, __hostFunction_SLModuleSpecJSI_setup};
}

}
