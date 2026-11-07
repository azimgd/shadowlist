#import "Internal/ShadowListKitSwipeActionsView.h"

#import <QuartzCore/QuartzCore.h>

#include <shadowlist-core/host/SwipeReveal.hpp>

#include <numeric>
#include <vector>

using namespace azimgd::shadowlist;

/*
 * The size of an action's title, the medium system font row actions show.
 */
static const CGFloat SHADOWLIST_KIT_SWIPE_FONT_SIZE = 13;

@implementation ShadowListKitSwipeActionsView {
  NSArray<NSButton *> *_leadingButtons;
  NSArray<NSButton *> *_trailingButtons;
  NSArray<ShadowListKitSwipeAction *> *_leadingActions;
  NSArray<ShadowListKitSwipeAction *> *_trailingActions;
  std::vector<double> _leadingWidths;
  std::vector<double> _trailingWidths;
  std::vector<SwipeSpan> _spans;
  CALayer *_mask;
  NSColor *_backgroundColor;
}

- (instancetype)initWithLeading:(ShadowListKitSwipeActionsConfiguration *)leading
                       trailing:(ShadowListKitSwipeActionsConfiguration *)trailing
                     horizontal:(BOOL)horizontal
{
  if (self = [super initWithFrame:NSZeroRect]) {
    _leading = leading;
    _trailing = trailing;
    _horizontal = horizontal;
    self.wantsLayer = YES;
    self.clipsToBounds = YES;
    _leadingActions = leading.actions ?: @[];
    _trailingActions = trailing.actions ?: @[];
    _leadingButtons = [self buttonsFor:_leadingActions widths:_leadingWidths];
    _trailingButtons = [self buttonsFor:_trailingActions widths:_trailingWidths];
    _mask = [CALayer layer];
    [self updateColors];
  }
  return self;
}

- (BOOL)isFlipped
{
  return YES;
}

/*
 * Frames, the mask and colors change at once. The list moves the buttons with the row.
 */
- (id<CAAction>)actionForLayer:(CALayer *)layer forKey:(NSString *)event
{
  return (id<CAAction>)NSNull.null;
}

- (NSArray<NSButton *> *)buttonsFor:(NSArray<ShadowListKitSwipeAction *> *)actions widths:(std::vector<double>&)widths
{
  NSMutableArray<NSButton *> *buttons = [NSMutableArray array];
  NSDictionary *attributes = @{
    NSFontAttributeName : [NSFont systemFontOfSize:SHADOWLIST_KIT_SWIPE_FONT_SIZE weight:NSFontWeightMedium],
    NSForegroundColorAttributeName : NSColor.whiteColor,
  };
  for (ShadowListKitSwipeAction *action in actions) {
    NSButton *button = [NSButton buttonWithTitle:@"" target:self action:@selector(press:)];
    button.bordered = NO;
    button.wantsLayer = YES;
    button.attributedTitle = [[NSAttributedString alloc] initWithString:action.title ?: @"" attributes:attributes];
    button.image = action.image;
    button.imagePosition = action.title.length > 0 ? NSImageLeading : NSImageOnly;
    button.contentTintColor = NSColor.whiteColor;
    button.focusRingType = NSFocusRingTypeNone;
    button.refusesFirstResponder = YES;
    button.accessibilityLabel = action.title ?: action.image.accessibilityDescription;
    NSSize fits = button.fittingSize;
    widths.push_back(swipeButtonSize(_horizontal ? fits.height : fits.width, 1.0));
    [self addSubview:button];
    [buttons addObject:button];
  }
  return buttons;
}

- (void)press:(NSButton *)button
{
  NSUInteger leading = [_leadingButtons indexOfObjectIdenticalTo:button];
  NSUInteger trailing = [_trailingButtons indexOfObjectIdenticalTo:button];
  ShadowListKitSwipeAction *action = leading != NSNotFound    ? _leadingActions[leading]
                                     : trailing != NSNotFound ? _trailingActions[trailing]
                                                              : nil;
  if (action && _onAction) {
    _onAction(action);
  }
}

- (CGFloat)leadingWidth
{
  return (CGFloat)std::accumulate(_leadingWidths.begin(), _leadingWidths.end(), 0.0);
}

- (CGFloat)trailingWidth
{
  return (CGFloat)std::accumulate(_trailingWidths.begin(), _trailingWidths.end(), 0.0);
}

/*
 * Place the buttons for a row moved offset across the axis. full lets the first button fill
 * the gap.
 */
- (void)layoutForOffset:(CGFloat)offset full:(BOOL)full
{
  BOOL leading = offset > 0;
  NSArray<NSButton *> *shown = leading ? _leadingButtons : _trailingButtons;
  NSArray<NSButton *> *other = leading ? _trailingButtons : _leadingButtons;
  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  for (NSButton *button in other) {
    button.hidden = YES;
  }
  CGSize size = self.bounds.size;
  CGFloat crossSize = _horizontal ? size.height : size.width;
  CGFloat alongSize = _horizontal ? size.width : size.height;
  swipeButtonSpans(leading ? _leadingWidths : _trailingWidths, offset, full, crossSize, _spans);
  for (NSUInteger at = 0; at < shown.count && at < _spans.size(); ++at) {
    NSButton *button = shown[at];
    CGFloat start = (CGFloat)_spans[at].start;
    CGFloat width = (CGFloat)_spans[at].size;
    button.hidden = width <= 0;
    button.frame = _horizontal ? CGRectMake(0, start, alongSize, width) : CGRectMake(start, 0, width, alongSize);
  }
  _backgroundColor = (leading ? _leadingActions : _trailingActions).firstObject.backgroundColor;
  [self updateColors];
  // Only the gap shows the buttons' color.
  SwipeSpan gap = swipeRevealedSpan(offset, crossSize);
  _mask.backgroundColor = NSColor.blackColor.CGColor;
  _mask.frame = _horizontal ? CGRectMake(0, (CGFloat)gap.start, alongSize, (CGFloat)gap.size)
                            : CGRectMake((CGFloat)gap.start, 0, (CGFloat)gap.size, alongSize);
  self.layer.mask = _mask;
  [CATransaction commit];
}

- (NSButton *)buttonAtPoint:(NSPoint)point
{
  for (NSButton *button in [_leadingButtons arrayByAddingObjectsFromArray:_trailingButtons]) {
    if (!button.hidden && NSPointInRect(point, button.frame)) {
      return button;
    }
  }
  return nil;
}

#pragma mark - Appearance

- (void)viewDidChangeEffectiveAppearance
{
  [super viewDidChangeEffectiveAppearance];
  [self updateColors];
}

/*
 * Layers take resolved colors. The actions' colors follow the list's appearance.
 */
- (void)updateColors
{
  NSArray<NSButton *> *buttons = [_leadingButtons arrayByAddingObjectsFromArray:_trailingButtons];
  NSArray<ShadowListKitSwipeAction *> *actions = [_leadingActions arrayByAddingObjectsFromArray:_trailingActions];
  [self.effectiveAppearance performAsCurrentDrawingAppearance:^{
    for (NSUInteger at = 0; at < buttons.count; ++at) {
      buttons[at].layer.backgroundColor = actions[at].backgroundColor.CGColor;
    }
    self.layer.backgroundColor = self->_backgroundColor.CGColor;
  }];
}

@end
