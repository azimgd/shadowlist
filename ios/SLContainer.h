#ifdef RCT_NEW_ARCH_ENABLED
#import <React/RCTViewComponentView.h>
#import <UIKit/UIKit.h>

#ifndef SLContainerNativeComponent_h
#define SLContainerNativeComponent_h

NS_ASSUME_NONNULL_BEGIN

@interface SLContainer : RCTViewComponentView<UIScrollViewDelegate>
@end

NS_ASSUME_NONNULL_END

#endif
#endif
