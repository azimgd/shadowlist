#import "SLContainer.h"

#import "SLContainerComponentDescriptor.h"
#import "SLContainerEventEmitter.h"
#import "SLContainerProps.h"
#import "SLContainerHelpers.h"

#import "RCTFabricComponentsPlugins.h"

using namespace facebook::react;

@interface SLContainer () <SLContainerProtocol>

@end

@implementation SLContainer {
  UIScrollView * _contentView;
}

+ (ComponentDescriptorProvider)componentDescriptorProvider
{
  return concreteComponentDescriptorProvider<SLContainerComponentDescriptor>();
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    static const auto defaultProps = std::make_shared<const SLContainerProps>();
    _props = defaultProps;
    _contentView = [UIScrollView new];
    self.contentView = _contentView;
  }

  return self;
}

- (void)layoutSubviews
{
  [self->_contentView layoutSubviews];
}

- (void)mountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  [self->_contentView mountChildComponentView:childComponentView index:index];
}

- (void)unmountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  [self->_contentView unmountChildComponentView:childComponentView index:index];
}

- (void)updateProps:(Props::Shared const &)props oldProps:(Props::Shared const &)oldProps
{
  const auto &oldViewProps = *std::static_pointer_cast<SLContainerProps const>(_props);
  const auto &newViewProps = *std::static_pointer_cast<SLContainerProps const>(props);

  [super updateProps:props oldProps:oldProps];
}

- (void)updateState:(const State::Shared &)state oldState:(const State::Shared &)oldState
{
}

Class<RCTComponentViewProtocol> ShadowlistViewCls(void)
{
  return SLContainer.class;
}

@end
