#import "Internal/ShadowListKitSwipeActionsView.h"

#import "Internal/ShadowListKitSwipeLayout.h"

#include <shadowlist-core/host/SwipeReveal.hpp>

#include <vector>

using namespace azimgd::shadowlist;

@implementation ShadowListKitSwipeActionsView {
  NSArray<UIButton *> *_leadingButtons;
  NSArray<UIButton *> *_trailingButtons;
  std::vector<double> _leadingWidths;
  std::vector<double> _trailingWidths;
  std::vector<SwipeSpan> _spans;
  ShadowListKitSwipeLayout _layout;
}

- (instancetype)initWithLeading:(ShadowListKitSwipeActionsConfiguration *)leading
                       trailing:(ShadowListKitSwipeActionsConfiguration *)trailing
                     horizontal:(BOOL)horizontal
{
  if (self = [super initWithFrame:CGRectZero]) {
    _leading = leading;
    _trailing = trailing;
    _horizontal = horizontal;
    self.clipsToBounds = YES;
    _leadingButtons = [self buttonsFor:leading widths:_leadingWidths];
    _trailingButtons = [self buttonsFor:trailing widths:_trailingWidths];
  }
  return self;
}

- (NSArray<UIButton *> *)buttonsFor:(ShadowListKitSwipeActionsConfiguration *)configuration widths:(std::vector<double>&)widths
{
  NSMutableArray<UIButton *> *buttons = [NSMutableArray array];
  for (ShadowListKitSwipeAction *action in configuration.actions) {
    UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem];
    button.backgroundColor = action.backgroundColor;
    button.tintColor = UIColor.whiteColor;
    button.titleLabel.font = [UIFont systemFontOfSize:15 weight:UIFontWeightMedium];
    button.clipsToBounds = YES;
    [button setTitle:action.title forState:UIControlStateNormal];
    [button setTitleColor:UIColor.whiteColor forState:UIControlStateNormal];
    [button setImage:action.image forState:UIControlStateNormal];
    button.accessibilityLabel = action.title;
    __weak ShadowListKitSwipeActionsView *weakSelf = self;
    [button addAction:[UIAction actionWithHandler:^(UIAction *) {
      ShadowListKitSwipeActionsView *view = weakSelf;
      if (view && view->_onAction) {
        view->_onAction(action);
      }
    }] forControlEvents:UIControlEventTouchUpInside];
    CGSize fits = [button sizeThatFits:CGSizeMake(CGFLOAT_MAX, CGFLOAT_MAX)];
    widths.push_back(swipeButtonSize(_horizontal ? fits.height : fits.width, 1.0));
    [self addSubview:button];
    [buttons addObject:button];
  }
  return buttons;
}

- (CGFloat)leadingWidth
{
  return ShadowListKitSwipeWidth(_leadingWidths);
}

- (CGFloat)trailingWidth
{
  return ShadowListKitSwipeWidth(_trailingWidths);
}

/*
 * Place the buttons for a row moved offset across the axis. full lets the first button fill
 * the gap.
 */
- (void)layoutForOffset:(CGFloat)offset full:(BOOL)full
{
  BOOL leading = offset > 0;
  NSArray<UIButton *> *shown = leading ? _leadingButtons : _trailingButtons;
  NSArray<UIButton *> *other = leading ? _trailingButtons : _leadingButtons;
  for (UIButton *button in other) {
    button.hidden = YES;
  }
  ShadowListKitLayOutSwipe(leading ? _leadingWidths : _trailingWidths, offset, full, self.bounds.size, _horizontal, _spans,
    _layout);
  for (NSUInteger at = 0; at < shown.count && at < _layout.buttons.size(); ++at) {
    UIButton *button = shown[at];
    CGRect frame = _layout.buttons[at];
    button.hidden = (_horizontal ? frame.size.height : frame.size.width) <= 0;
    button.frame = frame;
  }
  self.backgroundColor = shown.firstObject.backgroundColor;
  // Only the gap shows the buttons' color.
  self.layer.mask = nil;
  CALayer *mask = [CALayer layer];
  mask.backgroundColor = UIColor.blackColor.CGColor;
  mask.frame = _layout.gap;
  self.layer.mask = mask;
}

- (BOOL)hasButtonAtPoint:(CGPoint)point
{
  for (UIButton *button in [_leadingButtons arrayByAddingObjectsFromArray:_trailingButtons]) {
    if (!button.hidden && CGRectContainsPoint(button.frame, point)) {
      return YES;
    }
  }
  return NO;
}

@end
