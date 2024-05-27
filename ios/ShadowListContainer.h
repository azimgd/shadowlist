#ifdef RCT_NEW_ARCH_ENABLED
#import "ShadowListContainerDelegate.h"
#import "ShadowListContentDelegate.h"
#import <React/RCTViewComponentView.h>
#import <UIKit/UIKit.h>

#ifndef ShadowListContainerNativeComponent_h
#define ShadowListContainerNativeComponent_h

NS_ASSUME_NONNULL_BEGIN

@interface ShadowListContainer : RCTViewComponentView<UIScrollViewDelegate, ShadowListContentDelegate>
@property (nonatomic, weak) id<ShadowListContainerDelegate> delegate;
@end

NS_ASSUME_NONNULL_END

#endif
#endif
