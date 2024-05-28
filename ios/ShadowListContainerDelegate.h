#ifdef RCT_NEW_ARCH_ENABLED
#import <React/RCTViewComponentView.h>
#import <UIKit/UIKit.h>

#ifndef ShadowListContainerDelegate_h
#define ShadowListContainerDelegate_h

NS_ASSUME_NONNULL_BEGIN

@protocol ShadowListContainerDelegate <NSObject>
@required
- (CGPoint)listContainerScrollOffsetUpdate:(CGPoint)listContainerScrollOffset;
- (CGPoint)listContainerScrollFocusIndexUpdate:(NSInteger)focusIndex;
- (CGPoint)listContainerScrollFocusOffsetUpdate:(NSInteger)focusOffset;
@end

NS_ASSUME_NONNULL_END

#endif
#endif
