#import <React/RCTViewComponentView.h>
#import "ShadowListCompat.h"

#ifndef ShadowListElementViewNativeComponent_h
#define ShadowListElementViewNativeComponent_h

NS_ASSUME_NONNULL_BEGIN

@interface ShadowListElementView : RCTViewComponentView
/*
 * Native VoiceOver actions, like the drag Move up and Move down, added after any from props.
 * RCTViewComponentView builds accessibilityCustomActions from props and ignores the setter.
 */
@property (nonatomic, copy, nullable) NSArray<SLAccessibilityCustomAction *> *nativeAccessibilityActions;
@end

NS_ASSUME_NONNULL_END

#endif // ShadowListElementViewNativeComponent_h
