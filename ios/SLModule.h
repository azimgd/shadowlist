#import "SLModuleSpec.h"
#import <ReactCommon/RCTTurboModuleWithJSIBindings.h>
#import "SLModuleSpecJSI.h"

@interface SLModule : NSObject <SLModuleSpec, RCTTurboModuleWithJSIBindings>

@end
