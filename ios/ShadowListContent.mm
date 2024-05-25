#import "ShadowListContent.h"

#import "ShadowListContentComponentDescriptor.h"
#import "ShadowListContentEventEmitter.h"
#import "ShadowListContentProps.h"
#import "ShadowListContentHelpers.h"
#import "CachedComponentPool/CachedComponentPool.h"

#import "RCTConversions.h"
#import "RCTFabricComponentsPlugins.h"

using namespace facebook::react;

@interface ShadowListContent () <RCTShadowListContentViewProtocol>

@end

@implementation ShadowListContent {
  ShadowListContentShadowNode::ConcreteState::Shared _state;
  CachedComponentPool *_cachedComponentPool;
}

+ (ComponentDescriptorProvider)componentDescriptorProvider
{
  return concreteComponentDescriptorProvider<ShadowListContentComponentDescriptor>();
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    static const auto defaultProps = std::make_shared<const ShadowListContentProps>();
    _props = defaultProps;
    
    auto onCachedComponentMount = ^(NSInteger poolIndex) {
      [self insertSubview:[self->_cachedComponentPool getComponentView:poolIndex] atIndex:poolIndex];
    };
    auto onCachedComponentUnmount = ^(NSInteger poolIndex) {
      [[self->_cachedComponentPool getComponentView:poolIndex] removeFromSuperview];
    };
    _cachedComponentPool = [[CachedComponentPool alloc] initWithObservable:@[]
      onCachedComponentMount:onCachedComponentMount
      onCachedComponentUnmount:onCachedComponentUnmount];
  }

  return self;
}

- (void)mountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  if ([childComponentView superview]) [childComponentView removeFromSuperview];
  [self->_cachedComponentPool insertCachedComponentPoolItem:childComponentView poolIndex:index];
}

- (void)unmountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  if ([childComponentView superview]) [childComponentView removeFromSuperview];
  [self->_cachedComponentPool removeCachedComponentPoolItem:childComponentView poolIndex:index];
}

- (void)updateProps:(Props::Shared const &)props oldProps:(Props::Shared const &)oldProps
{
  [super updateProps:props oldProps:oldProps];
}

Class<RCTComponentViewProtocol> ShadowListContentCls(void)
{
  return ShadowListContent.class;
}

@end
