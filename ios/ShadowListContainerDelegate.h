#ifdef RCT_NEW_ARCH_ENABLED
#import <React/RCTViewComponentView.h>
#import <UIKit/UIKit.h>

#ifndef ShadowListContainerDelegate_h
#define ShadowListContainerDelegate_h

NS_ASSUME_NONNULL_BEGIN

@protocol ShadowListContainerDelegate <NSObject>
@required
- (CGPoint)listContainerScrollOffsetChange:(CGPoint)listContainerScrollOffset;
- (CGPoint)listContainerScrollFocusIndexChange:(NSInteger)focusIndex;
- (CGPoint)listContainerScrollFocusOffsetChange:(NSInteger)focusOffset;
@end

NS_ASSUME_NONNULL_END

#endif
#endif
