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
  if ([uniqueId isEqual:@"ListHeaderComponentUniqueId"]) {
    return [_scrollContent addSubview:childComponentView];
  }
  if ([uniqueId isEqual:@"ListFooterComponentUniqueId"]) {
    return [_scrollContent addSubview:childComponentView];
  }
  
  [self->_childrenPool setObject:childComponentView forKey:uniqueId];
  self->_childrenRegistry.registerComponent([uniqueId UTF8String]);
}

- (void)unmountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView uniqueId:(NSString *)uniqueId index:(NSInteger)index
{
  if ([uniqueId isEqual:@"ListHeaderComponentUniqueId"]) {
    return [childComponentView removeFromSuperview];
  }
  if ([uniqueId isEqual:@"ListFooterComponentUniqueId"]) {
    return [childComponentView removeFromSuperview];
  }

  self->_childrenRegistry.unregisterComponent([uniqueId UTF8String]);
  [self->_childrenPool removeObjectForKey:uniqueId];
}

- (void)mount:(int)visibleStartIndex visibleEndIndex:(int)visibleEndIndex
{
  std::vector<std::string> mounted = {};
  for (NSString *key in self->_childrenPool) {
    UIView<RCTComponentViewProtocol> *childComponentView = self->_childrenPool[key];
    const auto &childViewProps = *std::static_pointer_cast<SLElementProps const>([childComponentView props]);
    
    if (childViewProps.index >= visibleStartIndex && childViewProps.index <= visibleEndIndex) {
      mounted.push_back(childViewProps.uniqueId);
    }
  }

  self->_childrenRegistry.mount(mounted);
}

@end
