#ifndef __cplusplus
#error This file must be compiled as Obj-C++. If you are importing it, you must change your file extension to .mm.
#endif

#ifndef RNShadowlistSpec_H
#define RNShadowlistSpec_H

#import <Foundation/Foundation.h>
#import <RCTRequired/RCTRequired.h>
#import <RCTTypeSafety/RCTConvertHelpers.h>
#import <RCTTypeSafety/RCTTypedModuleConstants.h>
#import <React/RCTBridgeModule.h>
#import <React/RCTCxxConvert.h>
#import <React/RCTManagedPointer.h>
#import <ReactCommon/RCTTurboModule.h>
#import <optional>
#import <vector>


@protocol NativeShadowlistSpec <RCTBridgeModule, RCTTurboModule>
- (void)setup;
@end

@interface NativeShadowlistSpecBase : NSObject {
@protected
  facebook::react::EventEmitterCallback _eventEmitterCallback;
}

- (void)setEventEmitterCallback:(EventEmitterCallbackWrapper *)eventEmitterCallbackWrapper;
@end

namespace facebook::react {

/**
  * ObjC++ class for module 'NativeShadowlist'
  */
class JSI_EXPORT NativeShadowlistSpecJSI : public ObjCTurboModule {
public:
  NativeShadowlistSpecJSI(const ObjCTurboModule::InitParams &params);
};

}

#endif
