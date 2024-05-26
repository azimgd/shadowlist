#ifdef RCT_NEW_ARCH_ENABLED
#import <React/RCTViewComponentView.h>
#import <UIKit/UIKit.h>

#ifndef ShadowListContentDelegate_h
#define ShadowListContentDelegate_h

NS_ASSUME_NONNULL_BEGIN

@protocol ShadowListContentDelegate <NSObject>
@required
- (void)listContentSizeChange:(CGSize)listContentSize;
@end

NS_ASSUME_NONNULL_END

#endif
#endif
