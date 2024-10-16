#import "SLContainerChildrenManager.h"

@implementation SLContainerChildrenManager {
  UIView *_contentView;
  SLComponentRegistry _childrenRegistry;
  NSMutableDictionary<NSNumber *, UIView<RCTComponentViewProtocol> *> *_childrenPool;
}

- (instancetype)initWithContentView:(UIView *)contentView {
  if (self = [super init]) {
    _childrenRegistry = SLComponentRegistry(10);
    _childrenRegistry.mountObserver([self](int index, bool isVisible) {
      [self mountObserver:index isVisible:isVisible];
    });
    _childrenPool = [NSMutableDictionary dictionary];
    _contentView = contentView;
  }
  return self;
}

- (void)mountObserver:(int)index isVisible:(bool)isVisible {
  if (isVisible) {
    [_contentView insertSubview:[_childrenPool objectForKey:@(index)] atIndex:index];
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
  self->_childrenRegistry.mountRange(visibleStartIndex, visibleEndIndex);
}

@end
