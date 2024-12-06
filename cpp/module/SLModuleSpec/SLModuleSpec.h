#ifndef SLModuleSpec_H
#define SLModuleSpec_H

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

@protocol SLModuleSpec <RCTBridgeModule, RCTTurboModule>
  - (void)setup;
@end

@interface SLModuleSpecBase : NSObject {
@protected
facebook::react::EventEmitterCallback _eventEmitterCallback;
}

- (void)setEventEmitterCallback:(EventEmitterCallbackWrapper *)eventEmitterCallbackWrapper;
@end

namespace facebook::react {
  class JSI_EXPORT SLModuleSpecJSI : public ObjCTurboModule {
  public:
    SLModuleSpecJSI(const ObjCTurboModule::InitParams &params);
  };
}

#endif
