#ifdef RCT_NEW_ARCH_ENABLED
#import <React/RCTViewComponentView.h>
#import <UIKit/UIKit.h>

#ifndef ShadowListContentDelegate_h
#define ShadowListContentDelegate_h

NS_ASSUME_NONNULL_BEGIN

typedef struct {
  NSInteger visibleStartIndex;
  NSInteger visibleEndIndex;
  NSInteger visibleStartOffset;
  NSInteger visibleEndOffset;
  NSInteger headBlankStart;
  NSInteger headBlankEnd;
  NSInteger tailBlankStart;
  NSInteger tailBlankEnd;
} VisibleChildren;

@protocol ShadowListContentDelegate <NSObject>
@required
- (void)listContentSizeUpdate:(CGSize)listContentSize;
- (void)visibleChildrenUpdate:(VisibleChildren)visibleChildren;
@end

NS_ASSUME_NONNULL_END

#endif
#endif
