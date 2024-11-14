#import "SLContainerChildrenManager.h"

@implementation SLContainerChildrenManager {
  UIView *_scrollContent;
  SLComponentRegistry _childrenRegistry;
  NSMutableDictionary<NSNumber *, UIView<RCTComponentViewProtocol> *> *_childrenPool;
}

- (instancetype)initWithContentView:(UIView *)contentView {
  if (self = [super init]) {
    _childrenRegistry = SLComponentRegistry();
    _childrenRegistry.mountObserver([self](int index, bool isVisible) {
      [self mountObserver:index isVisible:isVisible];
    });
    _childrenPool = [NSMutableDictionary dictionary];
    _scrollContent = contentView;
  }
  return self;
}

- (void)mountObserver:(int)index isVisible:(bool)isVisible {
  if (isVisible) {
    [_scrollContent insertSubview:[_childrenPool objectForKey:@(index)] atIndex:index];
  } else {
    [[_childrenPool objectForKey:@(index)] removeFromSuperview];
  }
}

- (void)mountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  [self->_childrenPool setObject:childComponentView forKey:@(index)];
  self->_childrenRegistry.registerComponent(index);
}

- (void)unmountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  self->_childrenRegistry.unregisterComponent(index);
  [self->_childrenPool removeObjectForKey:@(index)];
}

- (void)mount:(int)visibleStartIndex end:(int)visibleEndIndex
{
  /**
   * Temporary workaround to ensure proper mounting order.
   * Currently, -(void)mountChildComponentView is called before -(void)mount in the initial phase.
   * However, after components are added to the tree (e.g., via setState on the JS side),
   * the order of operations is reversed. This causes the childrenRegistry to attempt mounting
   * components before they are actually added to the pool.
   */
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 16 * NSEC_PER_MSEC), dispatch_get_main_queue(), ^{
    self->_childrenRegistry.mountRange(visibleStartIndex, visibleEndIndex);
  });
}

@end
