#ifdef RCT_NEW_ARCH_ENABLED
#import "ShadowListContentDelegate.h"
#import "ShadowListContainerDelegate.h"
#import <React/RCTViewComponentView.h>
#import <UIKit/UIKit.h>

#ifndef ShadowListContentNativeComponent_h
#define ShadowListContentNativeComponent_h

NS_ASSUME_NONNULL_BEGIN

@interface ShadowListContent : RCTViewComponentView<UIScrollViewDelegate, ShadowListContainerDelegate>
@property (nonatomic, weak) id<ShadowListContentDelegate> delegate;
@end

NS_ASSUME_NONNULL_END

#endif
#endif
