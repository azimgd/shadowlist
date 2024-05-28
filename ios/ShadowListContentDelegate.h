#ifdef RCT_NEW_ARCH_ENABLED
#import <React/RCTViewComponentView.h>
#import <UIKit/UIKit.h>

#ifndef ShadowListContentDelegate_h
#define ShadowListContentDelegate_h

NS_ASSUME_NONNULL_BEGIN

@protocol ShadowListContentDelegate <NSObject>
@required
- (void)listContentSizeUpdate:(CGSize)listContentSize;
- (void)visibleChildrenUpdate:(NSInteger)visibleStartIndex visibleEndIndex:(NSInteger)visibleEndIndex;
@end

NS_ASSUME_NONNULL_END

#endif
#endif
