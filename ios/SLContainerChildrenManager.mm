#import "SLContainerChildrenManager.h"
#import "SLElementProps.h"

using namespace facebook::react;

@implementation SLContainerChildrenManager {
  UIView *_scrollContent;
  SLComponentRegistry _childrenRegistry;
  NSMutableDictionary<NSString *, UIView<RCTComponentViewProtocol> *> *_childrenPool;
}

- (instancetype)initWithContentView:(UIView *)contentView {
  if (self = [super init]) {
    _childrenRegistry = SLComponentRegistry();
    _childrenRegistry.mountObserver([self](std::string uniqueId, bool isVisible) {
      [self mountObserver:uniqueId isVisible:isVisible];
    });
    _childrenPool = [NSMutableDictionary dictionary];
    _scrollContent = contentView;
  }
  return self;
}

- (void)mountObserver:(std::string)uniqueId isVisible:(bool)isVisible {
  auto childComponentView = [_childrenPool objectForKey:[NSString stringWithUTF8String:uniqueId.c_str()]];
  const auto &childViewProps = *std::static_pointer_cast<SLElementProps const>([childComponentView props]);

  if (isVisible) {
    [_scrollContent insertSubview:childComponentView atIndex:childViewProps.index];
  } else {
    [childComponentView removeFromSuperview];
  }
}

- (void)mountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView uniqueId:(NSString *)uniqueId index:(NSInteger)index
{
  [self->_childrenPool setObject:childComponentView forKey:uniqueId];
  self->_childrenRegistry.registerComponent([uniqueId UTF8String]);
}

- (void)unmountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView uniqueId:(NSString *)uniqueId index:(NSInteger)index
{
  self->_childrenRegistry.unregisterComponent([uniqueId UTF8String]);
  [self->_childrenPool removeObjectForKey:uniqueId];
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
    std::vector<std::string> mounted = {};
    for (NSString *key in self->_childrenPool) {
      UIView<RCTComponentViewProtocol> *childComponentView = self->_childrenPool[key];
      const auto &childViewProps = *std::static_pointer_cast<SLElementProps const>([childComponentView props]);
      
      if (childViewProps.index >= visibleStartIndex && childViewProps.index <= visibleEndIndex)
        mounted.push_back(childViewProps.uniqueId);
    }

    self->_childrenRegistry.mount(mounted);
  });
}

@end
