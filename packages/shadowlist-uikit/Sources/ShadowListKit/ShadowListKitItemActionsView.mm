#import <ShadowListKit/ShadowListKitItemActionsView.h>
#import <ShadowListKit/ShadowListKitItemActionsView+Testing.h>
#import "Internal/ShadowListKitMenuController.h"
#import "Internal/ShadowListKitSwipeController.h"

@interface ShadowListKitItemActionsView () <ShadowListKitSwipeHost, ShadowListKitMenuHost, UIGestureRecognizerDelegate>
@end

@implementation ShadowListKitItemActionsView {
  UIView *_surface;
  ShadowListKitSwipeController *_swipeController;
  ShadowListKitMenuController *_menuController;
  UITapGestureRecognizer *_closeTap;
  NSMapTable<UIView *, NSArray<UIAccessibilityCustomAction *> *> *_accessibilityOwners;
  BOOL _refreshingAccessibility;
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) [self installActions];
  return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
  if (self = [super initWithCoder:coder]) [self installActions];
  return self;
}

- (void)installActions
{
  _accessibilityOwners = [[NSMapTable alloc] initWithKeyOptions:NSPointerFunctionsWeakMemory | NSPointerFunctionsObjectPointerPersonality
    valueOptions:NSPointerFunctionsStrongMemory capacity:0];
  _surface = [UIView new];
  _contentView = [UIView new];
  [_surface addSubview:_contentView];
  [self addSubview:_surface];
  self.clipsToBounds = YES;
  _swipeController = [[ShadowListKitSwipeController alloc] initWithHost:self];
  _menuController = [[ShadowListKitMenuController alloc] initWithHost:self];
  _closeTap = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(closeFromTap:)];
  _closeTap.delegate = self;
  [self addGestureRecognizer:_closeTap];
}

- (void)setLeadingSwipeActions:(ShadowListKitSwipeActionsConfiguration *)configuration
{
  if (_leadingSwipeActions == configuration) return;
  [_swipeController closeAnimated:NO];
  _leadingSwipeActions = configuration;
  [self refreshAccessibility];
}

- (void)setTrailingSwipeActions:(ShadowListKitSwipeActionsConfiguration *)configuration
{
  if (_trailingSwipeActions == configuration) return;
  [_swipeController closeAnimated:NO];
  _trailingSwipeActions = configuration;
  [self refreshAccessibility];
}

- (void)setContextMenu:(UIMenu *)menu
{
  if (_contextMenu == menu) return;
  [_menuController reset];
  _contextMenu = menu;
  [_menuController refresh];
}

- (void)layoutSubviews
{
  [super layoutSubviews];
  _surface.bounds = self.bounds;
  _surface.center = CGPointMake(CGRectGetMidX(self.bounds), CGRectGetMidY(self.bounds));
  _contentView.bounds = _surface.bounds;
  _contentView.center = CGPointMake(CGRectGetMidX(_surface.bounds), CGRectGetMidY(_surface.bounds));
  [_swipeController layout];
  [self refreshAccessibility];
}

- (CGSize)sizeThatFits:(CGSize)size
{
  return [_contentView systemLayoutSizeFittingSize:CGSizeMake(size.width, UILayoutFittingCompressedSize.height)
    withHorizontalFittingPriority:UILayoutPriorityRequired verticalFittingPriority:UILayoutPriorityFittingSizeLevel];
}

- (void)didMoveToWindow
{
  [super didMoveToWindow];
  if (!self.window) {
    [_swipeController reset];
    [_menuController reset];
    [self refreshAccessibility];
  } else [self refreshAccessibility];
}

- (void)closeSwipeActionsAnimated:(BOOL)animated { [_swipeController closeAnimated:animated]; }
- (void)closeFromTap:(UITapGestureRecognizer *)tap { [_swipeController ownsTapAtPoint:[tap locationInView:self]]; }

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gesture shouldReceiveTouch:(UITouch *)touch
{
  return gesture != _closeTap || (_swipeController.isOpen && ![_swipeController hasButtonAtPoint:[touch locationInView:self]]);
}

- (NSArray<UIAccessibilityCustomAction *> *)accessibilityCustomActions
{
  NSArray *own = [super accessibilityCustomActions] ?: @[];
  NSArray *actions = [_swipeController accessibilityActionsForContent:_surface];
  return actions.count ? [own arrayByAddingObjectsFromArray:actions] : own;
}

#pragma mark - Accessible standalone content

/* Caller containers keep their explicit tree and Grouped.Row keeps its dynamic getter proxy. */
- (NSArray<UIView *> *)nativeAccessibilityOwners
{
  if (!self.window || self.isAccessibilityElement) return @[];
  for (UIView *ancestor = self; ancestor; ancestor = ancestor.superview) {
    if (ancestor.hidden || ancestor.alpha <= 0 || ancestor.accessibilityElementsHidden) return @[];
    if (ancestor != self && (ancestor.isAccessibilityElement || ancestor.accessibilityElements != nil)) return @[];
  }
  NSMutableArray<UIView *> *owners = [NSMutableArray array];
  NSHashTable<UIView *> *visited = [NSHashTable hashTableWithOptions:NSPointerFunctionsObjectPointerPersonality];
  NSArray *explicitElements = self.accessibilityElements;
  if (explicitElements != nil) {
    for (id element in explicitElements) {
      if ([element isKindOfClass:UIView.class] && [(UIView *)element isDescendantOfView:_contentView]) {
        [self collectAccessibilityOwners:element into:owners visited:visited];
      }
    }
  } else [self collectAccessibilityOwners:_contentView into:owners visited:visited];
  return owners;
}

- (void)collectAccessibilityOwners:(UIView *)view into:(NSMutableArray<UIView *> *)owners visited:(NSHashTable<UIView *> *)visited
{
  if ([visited containsObject:view] || view.window != self.window) return;
  // An explicit array can name a deep child; native ancestor hiding still applies.
  for (UIView *ancestor = view; ancestor && ancestor != self; ancestor = ancestor.superview) {
    if (ancestor.hidden || ancestor.alpha <= 0 || (ancestor != view && ancestor.accessibilityElementsHidden)) return;
  }
  [visited addObject:view];
  if (view.isAccessibilityElement) { [owners addObject:view]; return; }
  if (view.accessibilityElementsHidden) return;
  NSArray *elements = view.accessibilityElements;
  if (elements == nil) elements = view.subviews;
  for (id element in elements) {
    if ([element isKindOfClass:UIView.class] && [(UIView *)element isDescendantOfView:_contentView]) {
      [self collectAccessibilityOwners:element into:owners visited:visited];
    }
  }
}

- (void)clearOwnedAccessibilityActions
{
  for (UIView *owner in _accessibilityOwners.keyEnumerator.allObjects) {
    NSArray *owned = [_accessibilityOwners objectForKey:owner];
    NSArray *current = owner.accessibilityCustomActions ?: @[];
    NSMutableArray *remaining = [NSMutableArray arrayWithCapacity:current.count];
    for (UIAccessibilityCustomAction *action in current) {
      if ([owned indexOfObjectIdenticalTo:action] == NSNotFound) [remaining addObject:action];
    }
    if (remaining.count != current.count) owner.accessibilityCustomActions = remaining;
  }
  [_accessibilityOwners removeAllObjects];
}

- (void)refreshAccessibility
{
  if (_refreshingAccessibility) { [self setNeedsLayout]; return; }
  if (!_accessibilityOwners.count && !_leadingSwipeActions.actions.count && !_trailingSwipeActions.actions.count) return;
  _refreshingAccessibility = YES;
  @try {
    [self clearOwnedAccessibilityActions];
    if (!_leadingSwipeActions.actions.count && !_trailingSwipeActions.actions.count) return;
    for (UIView *owner in [self nativeAccessibilityOwners]) {
      NSArray<UIAccessibilityCustomAction *> *actions = [_swipeController accessibilityActionsForContent:_surface];
      NSMutableArray<UIAccessibilityCustomAction *> *owned = [NSMutableArray arrayWithCapacity:actions.count];
      __weak ShadowListKitItemActionsView *weakSelf = self;
      __weak UIView *weakOwner = owner;
      for (UIAccessibilityCustomAction *action in actions) {
        [owned addObject:[[UIAccessibilityCustomAction alloc] initWithName:action.name image:action.image
          actionHandler:^BOOL(UIAccessibilityCustomAction *) {
            ShadowListKitItemActionsView *view = weakSelf;
            UIView *current = weakOwner;
            if (!view || !current || [[view nativeAccessibilityOwners] indexOfObjectIdenticalTo:current] == NSNotFound) return NO;
            return action.actionHandler ? action.actionHandler(action) : NO;
          }]];
      }
      if (owned.count) {
        [_accessibilityOwners setObject:owned forKey:owner];
        owner.accessibilityCustomActions = [(owner.accessibilityCustomActions ?: @[]) arrayByAddingObjectsFromArray:owned];
        if ([[self nativeAccessibilityOwners] indexOfObjectIdenticalTo:owner] == NSNotFound) {
          [self clearOwnedAccessibilityActions];
          [self setNeedsLayout];
          break;
        }
      }
    }
  } @finally { _refreshingAccessibility = NO; }
}

#pragma mark - Swipe host

- (UIView *)swipeRootView { return self; }
- (BOOL)isHorizontal { return NO; }
- (UIView *)swipeContentAtPoint:(CGPoint)point { return CGRectContainsPoint(self.bounds, point) ? _surface : nil; }
- (BOOL)isSwipeContentValid:(UIView *)content { return content == _surface && content.superview == self && self.window != nil; }
- (ShadowListKitSwipeActionsConfiguration *)leadingActionsForContent:(UIView *)content { return _leadingSwipeActions; }
- (ShadowListKitSwipeActionsConfiguration *)trailingActionsForContent:(UIView *)content { return _trailingSwipeActions; }
- (void)unhighlightSwipeContent:(UIView *)content {}

#pragma mark - Menu host

- (UIView *)menuRootView { return self; }
- (BOOL)menusEnabled { return _contextMenu.children.count > 0; }
- (UIView *)menuContentAtPoint:(CGPoint)point { return [self swipeContentAtPoint:point]; }
- (BOOL)isMenuContentValid:(UIView *)content { return [self isSwipeContentValid:content]; }
- (BOOL)canShowMenuForContent:(UIView *)content { return !_swipeController.isOpen; }
- (UIMenu *)menuForContent:(UIView *)content { return _contextMenu; }
- (id<NSCopying>)menuIdentifierForContent:(UIView *)content { return nil; }
- (UIView *)menuContentForIdentifier:(id<NSCopying>)identifier { return _surface; }
- (void)contextMenuWillShowForContent:(UIView *)content { if (_onContextMenuWillShow) _onContextMenuWillShow(); }

@end

@implementation ShadowListKitItemActionsView (Testing)
- (void)shadowListKit_swipeDistance:(CGFloat)distance velocity:(CGFloat)velocity
{
  [_swipeController scriptSwipeOfContent:_surface distance:distance velocity:velocity];
}
@end
