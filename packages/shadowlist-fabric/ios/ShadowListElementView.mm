#import "ShadowListElementView.h"
#import "ShadowListView+Private.h"

#import "ShadowListElementViewComponentDescriptor.h"
#import <react/renderer/components/ShadowListViewSpec/EventEmitters.h>
#import <react/renderer/components/ShadowListViewSpec/Props.h>
#import <react/renderer/components/ShadowListViewSpec/RCTComponentViewHelpers.h>

#include <shadowlist-core/host/SwipeReveal.hpp>

#include <cmath>
#include <numeric>
#include <vector>

using namespace facebook::react;
using namespace azimgd::shadowlist;

#if !TARGET_OS_OSX
/*
 * How long a released row takes to rest, the core's SWIPE_SETTLE_DURATION_MS.
 */
static const NSTimeInterval SL_SWIPE_DURATION = SWIPE_SETTLE_DURATION_MS / 1000.0;

/*
 * A swipe action color from JS is a processed color, a 0xAARRGGBB number.
 */
static UIColor *SLColorFromProcessedColor(double color)
{
  // Casting a non finite double to an integer is undefined. Treat it as transparent.
  uint32_t argb = std::isfinite(color) ? (uint32_t)(int64_t)color : 0;
  return [UIColor colorWithRed:((argb >> 16) & 0xFF) / 255.0
                         green:((argb >> 8) & 0xFF) / 255.0
                          blue:(argb & 0xFF) / 255.0
                         alpha:((argb >> 24) & 0xFF) / 255.0];
}

/*
 * Codegen only defines == for these structs in serializable state builds.
 */
template <typename Action>
static bool SLSameSwipeActions(const std::vector<Action>& a, const std::vector<Action>& b)
{
  if (a.size() != b.size()) {
    return false;
  }
  for (std::size_t index = 0; index < a.size(); ++index) {
    if (a[index].title != b[index].title || a[index].color != b[index].color ||
        a[index].destructive != b[index].destructive) {
      return false;
    }
  }
  return true;
}

#pragma mark - Actions view

/*
 * The buttons behind a swiped row, for both sides. It sits under the row's content with the
 * content's resting frame. The side being revealed shows its buttons stretched over the gap the
 * content leaves. Past the full swipe point the first button fills all of it.
 */
@interface ShadowListSwipeActionsView : UIView
@property (nonatomic, copy, nullable) void (^onAction)(BOOL leading, NSInteger index);
@end

@implementation ShadowListSwipeActionsView {
  NSArray<UIButton *> *_leadingButtons;
  NSArray<UIButton *> *_trailingButtons;
  std::vector<double> _leadingWidths;
  std::vector<double> _trailingWidths;
  std::vector<SwipeSpan> _spans;
}

- (instancetype)initWithProps:(const ShadowListElementViewProps&)props
{
  if (self = [super initWithFrame:CGRectZero]) {
    self.clipsToBounds = YES;
    NSMutableArray<UIButton *> *leading = [NSMutableArray array];
    for (const auto& action : props.leadingSwipeActions) {
      [leading addObject:[self buttonWithTitle:action.title color:action.color leading:YES widths:_leadingWidths]];
    }
    NSMutableArray<UIButton *> *trailing = [NSMutableArray array];
    for (const auto& action : props.trailingSwipeActions) {
      [trailing addObject:[self buttonWithTitle:action.title color:action.color leading:NO widths:_trailingWidths]];
    }
    _leadingButtons = leading;
    _trailingButtons = trailing;
  }
  return self;
}

- (UIButton *)buttonWithTitle:(const std::string&)title
                        color:(double)color
                      leading:(BOOL)leading
                       widths:(std::vector<double>&)widths
{
  NSString *text = [NSString stringWithUTF8String:title.c_str()] ?: @"";
  NSInteger index = (NSInteger)widths.size();
  UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem];
  button.backgroundColor = SLColorFromProcessedColor(color);
  button.tintColor = UIColor.whiteColor;
  button.titleLabel.font = [UIFont systemFontOfSize:15 weight:UIFontWeightMedium];
  button.clipsToBounds = YES;
  [button setTitle:text forState:UIControlStateNormal];
  [button setTitleColor:UIColor.whiteColor forState:UIControlStateNormal];
  button.accessibilityLabel = text;
  __weak ShadowListSwipeActionsView *weakSelf = self;
  [button addAction:[UIAction actionWithHandler:^(UIAction *) {
            ShadowListSwipeActionsView *strongSelf = weakSelf;
            if (strongSelf.onAction) {
              strongSelf.onAction(leading, index);
            }
          }]
   forControlEvents:UIControlEventTouchUpInside];
  CGSize fits = [button sizeThatFits:CGSizeMake(CGFLOAT_MAX, CGFLOAT_MAX)];
  widths.push_back(swipeButtonSize(fits.width, 1.0));
  [self addSubview:button];
  return button;
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
 * Place the buttons for content moved offset across the row. full lets the first button fill
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
  CGSize size = self.bounds.size;
  swipeButtonSpans(leading ? _leadingWidths : _trailingWidths, offset, full, size.width, _spans);
  for (NSUInteger at = 0; at < shown.count && at < _spans.size(); ++at) {
    UIButton *button = shown[at];
    CGFloat start = (CGFloat)_spans[at].start;
    CGFloat width = (CGFloat)_spans[at].size;
    button.hidden = width <= 0;
    button.frame = CGRectMake(start, 0, width, size.height);
  }
  self.backgroundColor = shown.firstObject.backgroundColor;
  // Only the gap shows the buttons' color.
  SwipeSpan gap = swipeRevealedSpan(offset, size.width);
  CALayer *mask = [CALayer layer];
  mask.backgroundColor = UIColor.blackColor.CGColor;
  mask.frame = CGRectMake((CGFloat)gap.start, 0, (CGFloat)gap.size, size.height);
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

@interface ShadowListElementView () <RCTShadowListElementViewViewProtocol,
                                     UIGestureRecognizerDelegate,
                                     UIContextMenuInteractionDelegate>
@end
#else
@interface ShadowListElementView () <RCTShadowListElementViewViewProtocol>
@end
#endif

/*
 * Swipe actions and context menus come from props. A pan across a vertical list moves the
 * content and shows the action buttons behind it. SwipeReveal in the host layer decides how far
 * it follows and where it rests. Horizontal lists do not swipe. Touching and holding shows the
 * row's menu through UIContextMenuInteraction, unless the list can be reordered. The hold then
 * lifts the row. Both are iOS only.
 */
@implementation ShadowListElementView {
  RCTUIView *_contentView;
#if !TARGET_OS_OSX
  UIPanGestureRecognizer *_swipePan;
  UITapGestureRecognizer *_swipeTap;
  ShadowListSwipeActionsView *_swipeActionsView;
  SwipeReveal _swipe;
  CGFloat _swipeOffset;
  NSInteger _swipeToken;
  NSArray<UIAccessibilityCustomAction *> *_swipeAccessibilityActions;
  UIContextMenuInteraction *_menuInteraction;
#endif
}

+ (ComponentDescriptorProvider)componentDescriptorProvider
{
  return concreteComponentDescriptorProvider<ShadowListElementViewComponentDescriptor>();
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    static const auto defaultProps = std::make_shared<const ShadowListElementViewProps>();
    _props = defaultProps;

    _contentView = [RCTUIView new];

    self.contentView = _contentView;

#if !TARGET_OS_OSX
    _swipePan = [[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(handleSwipePan:)];
    _swipePan.delegate = self;
    _swipePan.enabled = NO;
    [self addGestureRecognizer:_swipePan];
    _swipeTap = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(handleSwipeTap:)];
    _swipeTap.delegate = self;
    _swipeTap.enabled = NO;
    [self addGestureRecognizer:_swipeTap];
#endif
  }

  return self;
}

/*
 * Fabric shares recycled row views across every list in the app, even on other screens.
 * The base reset skips transform and hidden, and a drag sets both. A row recycled mid drag
 * would show up shifted in the next list. A swiped row would show up open.
 * Reset everything we set from native here.
 */
- (void)prepareForRecycle
{
  [self.layer removeAllAnimations];
  self.transform = CGAffineTransformIdentity;
  self.hidden = NO;
  self.layer.shadowOpacity = 0.0;
  self.layer.shadowPath = nil;
  _nativeAccessibilityActions = nil;
#if TARGET_OS_OSX
  // SLRaiseSubview raises this for sticky pinning. UIKit reorders subviews instead.
  self.layer.zPosition = 0.0;
#else
  [_contentView.layer removeAllAnimations];
  [self tearDownSwipe];
  // Turning the pan off cancels one in flight. updateProps turns it back on.
  _swipePan.enabled = NO;
  _swipeAccessibilityActions = nil;
#endif
  [super prepareForRecycle];
}

- (void)updateProps:(const Props::Shared&)props oldProps:(const Props::Shared&)oldProps
{
  [super updateProps:props oldProps:oldProps];
#if !TARGET_OS_OSX
  const auto& next = *std::static_pointer_cast<const ShadowListElementViewProps>(props);
  auto previous = std::static_pointer_cast<const ShadowListElementViewProps>(oldProps);
  BOOL hasSwipeActions = !next.leadingSwipeActions.empty() || !next.trailingSwipeActions.empty();
  BOOL swipeActionsChanged = !previous || !SLSameSwipeActions(previous->leadingSwipeActions, next.leadingSwipeActions) ||
    !SLSameSwipeActions(previous->trailingSwipeActions, next.trailingSwipeActions) ||
    previous->leadingFullSwipe != next.leadingFullSwipe || previous->trailingFullSwipe != next.trailingFullSwipe;
  if (swipeActionsChanged || (hasSwipeActions && !_swipeAccessibilityActions)) {
    /*
     * Open buttons show the previous actions. Close without animation, unless the row is slid
     * out by a full swipe whose action changed it. That row slides back.
     */
    if (_swipeActionsView && swipeActionsChanged) {
      [self closeSwipeActionsAnimated:[self isSwipedOut]];
    }
    [self updateSwipeAccessibilityActions:next];
  }
  _swipePan.enabled = hasSwipeActions;

  BOOL hasMenu = !next.contextMenuActions.empty();
  if (hasMenu && !_menuInteraction) {
    _menuInteraction = [[UIContextMenuInteraction alloc] initWithDelegate:self];
    [self addInteraction:_menuInteraction];
  } else if (!hasMenu && _menuInteraction) {
    [self removeInteraction:_menuInteraction];
    _menuInteraction = nil;
  }
#endif
}

/*
 * Children sit where Yoga put them, padding included. The base view would also inset the
 * content view by the padding, which would apply it twice. A swipe transform would skew the
 * frame. Lay out at rest, then put the swipe back.
 */
- (void)updateLayoutMetrics:(const LayoutMetrics&)layoutMetrics oldLayoutMetrics:(const LayoutMetrics&)oldLayoutMetrics
{
  CGRect contentFrame = CGRectMake(0, 0, layoutMetrics.frame.size.width, layoutMetrics.frame.size.height);
#if !TARGET_OS_OSX
  if (!_swipeActionsView) {
    [super updateLayoutMetrics:layoutMetrics oldLayoutMetrics:oldLayoutMetrics];
    _contentView.frame = contentFrame;
    return;
  }
  CGAffineTransform swiped = _contentView.transform;
  _contentView.transform = CGAffineTransformIdentity;
  [super updateLayoutMetrics:layoutMetrics oldLayoutMetrics:oldLayoutMetrics];
  _contentView.frame = contentFrame;
  _contentView.transform = swiped;
  _swipeActionsView.bounds = _contentView.bounds;
  _swipeActionsView.center = _contentView.center;
  if (_swipeOffset != 0) {
    [_swipeActionsView layoutForOffset:_swipeOffset full:_swipe.isPastFullSwipe(_swipeOffset)];
  }
#else
  [super updateLayoutMetrics:layoutMetrics oldLayoutMetrics:oldLayoutMetrics];
  _contentView.frame = contentFrame;
#endif
}

#pragma mark - Accessibility

- (NSArray<SLAccessibilityCustomAction *> *)accessibilityCustomActions
{
  NSArray<SLAccessibilityCustomAction *> *propActions = [super accessibilityCustomActions];
  NSArray<SLAccessibilityCustomAction *> *nativeActions = _nativeAccessibilityActions;
#if !TARGET_OS_OSX
  if (_swipeAccessibilityActions.count > 0) {
    nativeActions = nativeActions.count > 0 ? [nativeActions arrayByAddingObjectsFromArray:_swipeAccessibilityActions]
                                            : _swipeAccessibilityActions;
  }
#endif
  if (nativeActions.count == 0) {
    return propActions;
  }
  return propActions.count > 0 ? [propActions arrayByAddingObjectsFromArray:nativeActions] : nativeActions;
}

#if !TARGET_OS_OSX
/*
 * For VoiceOver, each swipe action is a custom action on the row.
 */
- (void)updateSwipeAccessibilityActions:(const ShadowListElementViewProps&)props
{
  NSMutableArray<UIAccessibilityCustomAction *> *actions = [NSMutableArray array];
  __weak ShadowListElementView *weakSelf = self;
  auto add = [&](const std::string& title, BOOL leading, NSInteger index) {
    NSString *name = [NSString stringWithUTF8String:title.c_str()] ?: @"";
    [actions addObject:[[UIAccessibilityCustomAction alloc] initWithName:name
                                                           actionHandler:^BOOL(UIAccessibilityCustomAction *) {
                                                             [weakSelf emitSwipeActionLeading:leading index:index full:NO];
                                                             return YES;
                                                           }]];
  };
  for (std::size_t index = 0; index < props.leadingSwipeActions.size(); ++index) {
    add(props.leadingSwipeActions[index].title, YES, (NSInteger)index);
  }
  for (std::size_t index = 0; index < props.trailingSwipeActions.size(); ++index) {
    add(props.trailingSwipeActions[index].title, NO, (NSInteger)index);
  }
  _swipeAccessibilityActions = actions.count > 0 ? actions : nil;
}
#endif

#pragma mark - Swipe actions

- (BOOL)isSwipeOpen
{
#if TARGET_OS_OSX
  return NO;
#else
  return _swipeActionsView != nil;
#endif
}

- (BOOL)isSwipedOut
{
#if TARGET_OS_OSX
  return NO;
#else
  return _swipeActionsView && _swipe.isSwipedOut(_swipeOffset);
#endif
}

- (void)closeSwipeActionsAnimated:(BOOL)animated
{
#if !TARGET_OS_OSX
  if (!_swipeActionsView) {
    return;
  }
  if (!animated) {
    [_contentView.layer removeAllAnimations];
    [self tearDownSwipe];
    return;
  }
  SwipeRest closed;
  [self settleSwipeTo:closed];
#endif
}

#if !TARGET_OS_OSX
- (ShadowListView *)listView
{
  for (UIView *ancestor = self.superview; ancestor; ancestor = ancestor.superview) {
    if ([ancestor isKindOfClass:[ShadowListView class]]) {
      return (ShadowListView *)ancestor;
    }
  }
  return nil;
}

- (const ShadowListElementViewProps&)elementProps
{
  return *std::static_pointer_cast<const ShadowListElementViewProps>(_props);
}

- (void)emitSwipeActionLeading:(BOOL)leading index:(NSInteger)index full:(BOOL)full
{
  if (!_eventEmitter) {
    return;
  }
  std::static_pointer_cast<const ShadowListElementViewEventEmitter>(_eventEmitter)
    ->onSwipeAction({.leading = (bool)leading, .actionIndex = (int)index, .fullSwipe = (bool)full});
}

/*
 * Whether a pan swipes this row. A pan along the list scrolls instead. A closed row only swipes
 * toward a side with actions.
 */
- (BOOL)swipeTakesPan
{
  ShadowListView *list = [self listView];
  if (!list || list->_horizontal || list->_dragging) {
    return NO;
  }
  CGPoint velocity = [_swipePan velocityInView:self];
  if (velocity.x == 0 && velocity.y == 0) {
    // A pan that starts after a pause has no velocity yet. Its movement says where it goes.
    velocity = [_swipePan translationInView:self];
  }
  if (std::fabs(velocity.x) <= std::fabs(velocity.y)) {
    return NO;
  }
  if (_swipeActionsView) {
    return YES;
  }
  const auto& props = [self elementProps];
  return velocity.x > 0 ? !props.leadingSwipeActions.empty() : !props.trailingSwipeActions.empty();
}

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer *)gesture
{
  if (gesture == _swipePan) {
    return [self swipeTakesPan];
  }
  if (gesture == _swipeTap) {
    return _swipeActionsView != nil;
  }
  return [super gestureRecognizerShouldBegin:gesture];
}

/*
 * Scroll views around the row wait for the swipe to fail. A pan along the list fails it right
 * away.
 */
- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gesture
  shouldBeRequiredToFailByGestureRecognizer:(UIGestureRecognizer *)other
{
  if (gesture != _swipePan || !_swipePan.enabled) {
    return NO;
  }
  UIView *view = other.view;
  return [view isKindOfClass:[UIScrollView class]] && other == ((UIScrollView *)view).panGestureRecognizer &&
    [self isDescendantOfView:view];
}

/*
 * A tap on an open row closes it, unless it hits an action button.
 */
- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gesture shouldReceiveTouch:(UITouch *)touch
{
  if (gesture != _swipeTap) {
    return YES;
  }
  return ![_swipeActionsView hasButtonAtPoint:[touch locationInView:_swipeActionsView]];
}

- (void)handleSwipeTap:(UITapGestureRecognizer *)tap
{
  if (tap.state == UIGestureRecognizerStateEnded) {
    [self closeSwipeActionsAnimated:YES];
  }
}

- (void)handleSwipePan:(UIPanGestureRecognizer *)pan
{
  switch (pan.state) {
    case UIGestureRecognizerStateBegan: {
      if (_contentView.layer.animationKeys.count > 0) {
        // Catch the row where a running settle shows it.
        _swipeOffset = _contentView.layer.presentationLayer.affineTransform.tx;
        [_contentView.layer removeAllAnimations];
        _contentView.transform = CGAffineTransformMakeTranslation(_swipeOffset, 0);
      }
      if (!_swipeActionsView) {
        [self openActions];
      }
      if (!_swipeActionsView) {
        return;
      }
      [[self listView] closeSwipeActionsExcept:self];
      SLCancelReactTouches(self);
      _swipe.begin(_swipe.getSpec(), _swipeOffset);
      break;
    }
    case UIGestureRecognizerStateChanged: {
      if (!_swipeActionsView) {
        return;
      }
      BOOL wasPast = _swipe.isPastFullSwipe(_swipeOffset);
      [self setSwipeOffset:(CGFloat)_swipe.drag([pan translationInView:self].x)];
      if (_swipe.isPastFullSwipe(_swipeOffset) != wasPast) {
        [[UIImpactFeedbackGenerator new] impactOccurred];
      }
      break;
    }
    case UIGestureRecognizerStateEnded:
    case UIGestureRecognizerStateCancelled:
    case UIGestureRecognizerStateFailed: {
      if (!_swipeActionsView) {
        return;
      }
      [self settleSwipeTo:_swipe.settle(_swipeOffset, [pan velocityInView:self].x, SWIPE_FLING_VELOCITY)];
      break;
    }
    default:
      break;
  }
}

/*
 * Put the actions of this row under its content, closed.
 */
- (void)openActions
{
  const auto& props = [self elementProps];
  if (props.leadingSwipeActions.empty() && props.trailingSwipeActions.empty()) {
    return;
  }
  ShadowListSwipeActionsView *view = [[ShadowListSwipeActionsView alloc] initWithProps:props];
  __weak ShadowListElementView *weakSelf = self;
  view.onAction = ^(BOOL leading, NSInteger index) {
    [weakSelf performSwipeActionLeading:leading index:index];
  };
  view.bounds = _contentView.bounds;
  view.center = _contentView.center;
  [_contentView.superview insertSubview:view belowSubview:_contentView];
  SwipeSpec spec;
  spec.leadingWidth = [view leadingWidth];
  spec.trailingWidth = [view trailingWidth];
  spec.leadingFullSwipe = props.leadingFullSwipe;
  spec.trailingFullSwipe = props.trailingFullSwipe;
  spec.rowSize = _contentView.bounds.size.width;
  _swipe.begin(spec, 0);
  _swipeActionsView = view;
  _swipeOffset = 0;
  // Taps on the content close the row instead of pressing it.
  _contentView.userInteractionEnabled = NO;
  _swipeTap.enabled = YES;
}

- (void)setSwipeOffset:(CGFloat)offset
{
  _swipeOffset = offset;
  _contentView.transform = CGAffineTransformMakeTranslation(offset, 0);
  if (offset != 0) {
    [_swipeActionsView layoutForOffset:offset full:_swipe.isPastFullSwipe(offset)];
  }
}

- (void)settleSwipeTo:(SwipeRest)rest
{
  NSInteger token = _swipeToken;
  [UIView animateWithDuration:SL_SWIPE_DURATION
    delay:0
    usingSpringWithDamping:1
    initialSpringVelocity:0
    options:UIViewAnimationOptionBeginFromCurrentState | UIViewAnimationOptionAllowUserInteraction
    animations:^{
      [self setSwipeOffset:(CGFloat)rest.offset];
      [self->_swipeActionsView layoutIfNeeded];
    }
    completion:^(BOOL) {
      if (rest.side == SwipeSide::None && token == self->_swipeToken && self->_swipeOffset == 0) {
        [self tearDownSwipe];
      }
    }];
  if (!rest.full) {
    return;
  }
  // The row stays out until JS closes it with closeFullSwipe, or the action removes it.
  [self emitSwipeActionLeading:rest.side == SwipeSide::Leading index:0 full:YES];
}

/*
 * A tapped action runs and the row closes.
 */
- (void)performSwipeActionLeading:(BOOL)leading index:(NSInteger)index
{
  [self emitSwipeActionLeading:leading index:index full:NO];
  [self closeSwipeActionsAnimated:YES];
}

- (void)tearDownSwipe
{
  [_swipeActionsView removeFromSuperview];
  _swipeActionsView = nil;
  _contentView.transform = CGAffineTransformIdentity;
  _contentView.userInteractionEnabled = YES;
  _swipeTap.enabled = NO;
  _swipeOffset = 0;
  ++_swipeToken;
}

#pragma mark - Context menu

- (UIContextMenuConfiguration *)contextMenuInteraction:(UIContextMenuInteraction *)interaction
                        configurationForMenuAtLocation:(CGPoint)location
{
  const auto& props = [self elementProps];
  ShadowListView *list = [self listView];
  // A list that can be reordered lifts the row on the hold.
  if (props.contextMenuActions.empty() || _swipeActionsView || !_eventEmitter ||
      (list && (list->_reorderEnabled || list->_dragging))) {
    return nil;
  }
  // The menu reports to the row it opened on, even if this view is recycled meanwhile.
  auto emitter = std::static_pointer_cast<const ShadowListElementViewEventEmitter>(_eventEmitter);
  NSMutableArray<UIMenuElement *> *children = [NSMutableArray array];
  for (std::size_t index = 0; index < props.contextMenuActions.size(); ++index) {
    const auto& item = props.contextMenuActions[index];
    UIImage *image = item.systemImage.empty()
      ? nil
      : [UIImage systemImageNamed:[NSString stringWithUTF8String:item.systemImage.c_str()]];
    int actionIndex = (int)index;
    UIAction *action = [UIAction actionWithTitle:[NSString stringWithUTF8String:item.title.c_str()] ?: @""
                                           image:image
                                      identifier:nil
                                         handler:^(UIAction *) {
                                           emitter->onContextMenuAction({.actionIndex = actionIndex});
                                         }];
    UIMenuElementAttributes attributes = 0;
    if (item.destructive) {
      attributes |= UIMenuElementAttributesDestructive;
    }
    if (item.disabled) {
      attributes |= UIMenuElementAttributesDisabled;
    }
    action.attributes = attributes;
    [children addObject:action];
  }
  UIMenu *menu = [UIMenu menuWithTitle:[NSString stringWithUTF8String:props.contextMenuTitle.c_str()] ?: @""
                              children:children];
  return [UIContextMenuConfiguration configurationWithIdentifier:nil
                                                 previewProvider:nil
                                                  actionProvider:^UIMenu *(NSArray<UIMenuElement *> *) {
                                                    return menu;
                                                  }];
}

- (void)contextMenuInteraction:(UIContextMenuInteraction *)interaction
  willDisplayMenuForConfiguration:(UIContextMenuConfiguration *)configuration
                         animator:(id<UIContextMenuInteractionAnimating>)animator
{
  // The hold is a menu, not a press on the row.
  SLCancelReactTouches(self);
}
#endif

#pragma mark - Commands

- (void)handleCommand:(const NSString *)commandName args:(const NSArray *)args
{
  RCTShadowListElementViewHandleCommand(self, commandName, args);
}

/*
 * The full swipe's action finished and the row is still here. Slide it back.
 */
- (void)closeFullSwipe
{
  if ([self isSwipedOut]) {
    [self closeSwipeActionsAnimated:YES];
  }
}

#pragma mark - Children

- (void)mountChildComponentView:(RCTUIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  [_contentView insertSubview:childComponentView atIndex:index];
}

- (void)unmountChildComponentView:(RCTUIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  [childComponentView removeFromSuperview];
}

Class<RCTComponentViewProtocol> ShadowListElementViewCls(void)
{
  return ShadowListElementView.class;
}

@end
