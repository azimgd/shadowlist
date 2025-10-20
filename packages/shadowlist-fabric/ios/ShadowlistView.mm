#import "ShadowlistView.h"

#import <react/renderer/components/ShadowlistViewSpec/ComponentDescriptors.h>
#import <react/renderer/components/ShadowlistViewSpec/EventEmitters.h>
#import <react/renderer/components/ShadowlistViewSpec/Props.h>
#import <react/renderer/components/ShadowlistViewSpec/RCTComponentViewHelpers.h>

#import "RCTFabricComponentsPlugins.h"
#import <shadowlist-core/Container.hpp>
#import <shadowlist-core/Virtualizer.hpp>
#import <shadowlist-core/Observer.hpp>


using namespace facebook::react;

@interface ShadowlistView () <RCTShadowlistViewViewProtocol, UIScrollViewDelegate>

@property (nonatomic, assign) azimgd::shadowlist::Container *container;
@property (nonatomic, assign) azimgd::shadowlist::Virtualizer *virtualizer;
@property (nonatomic, assign) azimgd::shadowlist::Observer *observer;

@end

@implementation ShadowlistView {
  UIScrollView * _scrollView;
  UIView * _contentView;
}

+ (ComponentDescriptorProvider)componentDescriptorProvider
{
  return concreteComponentDescriptorProvider<ShadowlistViewComponentDescriptor>();
}

- (void)mountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  if (![childComponentView conformsToProtocol:@protocol(RCTShadowlistElementViewViewProtocol)]) {
    return;
  }

  const auto &childViewProps = *std::static_pointer_cast<ShadowlistElementViewProps const>(childComponentView.props);

  self.virtualizer->updateElementAtIndex(self.container, childViewProps.index, self.container->nextRevision, {
    .width = childComponentView.frame.size.width,
    .height = childComponentView.frame.size.height
  });

  self.container->startRevision();
  self.container->setContainerOffsetX(self->_scrollView.contentOffset.x);
  self.container->setContainerOffsetY(self->_scrollView.contentOffset.y);
  self.virtualizer->measureDefault(self.container);
  self.container->endRevision();
  
  double offsetX = self.container->getElementAtIndex(childViewProps.index).offsetX;
  double offsetY = self.container->getElementAtIndex(childViewProps.index).offsetY;

  CGRect frame = childComponentView.frame;
  frame.origin.x = offsetX;
  frame.origin.y = offsetY;
  childComponentView.frame = frame;
  
  [_contentView insertSubview:childComponentView atIndex:childViewProps.index];
  
  /*
   * Update all child component view frames based on virtualization offsets
   */
  for (UIView<RCTComponentViewProtocol> *childComponentView in _contentView.subviews) {
    if (![childComponentView conformsToProtocol:@protocol(RCTShadowlistElementViewViewProtocol)]) {
      continue;
    }

    const auto &childViewProps = *std::static_pointer_cast<ShadowlistElementViewProps const>(childComponentView.props);
    double offsetX = self.container->getElementAtIndex(childViewProps.index).offsetX;
    double offsetY = self.container->getElementAtIndex(childViewProps.index).offsetY;

    CGRect frame = childComponentView.frame;
    frame.origin.x = offsetX;
    frame.origin.y = offsetY;
    childComponentView.frame = frame;
  }
}

- (void)unmountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  [childComponentView removeFromSuperview];
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    static const auto defaultProps = std::make_shared<const ShadowlistViewProps>();
    _props = defaultProps;

    _scrollView = [[UIScrollView alloc] init];
    _scrollView.delegate = self;
    _scrollView.showsVerticalScrollIndicator = YES;
    _scrollView.showsHorizontalScrollIndicator = YES;
    _scrollView.scrollEnabled = YES;
    _scrollView.contentInsetAdjustmentBehavior = UIScrollViewContentInsetAdjustmentNever;

    _contentView = [[UIView alloc] init];
    [_scrollView addSubview:_contentView];

    self.virtualizer = new azimgd::shadowlist::Virtualizer();
    self.container = new azimgd::shadowlist::Container();
    self.observer = new azimgd::shadowlist::Observer(*self.container, 16);
  
    self.container->setObserver(self.observer);
    
    {
      __weak __typeof(self) weakSelf = self;
      self.container->offsetInitAdjustmentCallback = [](azimgd::shadowlist::Revision revision) -> void {};
      self.container->offsetMvcpAdjustmentCallback = [](azimgd::shadowlist::Revision revision) -> void {};
      self.container->sizeAdjustmentCallback = [](azimgd::shadowlist::Revision revision) -> void {};
      self.container->measurementCallback = [](std::size_t index) -> std::pair<double, double> { return {100.0, 100.0}; };
    }

    self.container->inverted = false;
    self.container->horizontal = false;
    self.container->resizeElements(1000);

    self.contentView = _scrollView;
  }

  return self;
}

- (void)updateProps:(Props::Shared const &)props oldProps:(Props::Shared const &)oldProps
{
  const auto &oldViewProps = *std::static_pointer_cast<ShadowlistViewProps const>(_props);
  const auto &newViewProps = *std::static_pointer_cast<ShadowlistViewProps const>(props);

  [super updateProps:props oldProps:oldProps];
}

- (void)finalizeUpdates:(RNComponentViewUpdateMask)updateMask
{
  [super finalizeUpdates:updateMask];

  if (!self.container || !self.virtualizer) {
    return;
  }
  
  self.container->startRevision();
  self.container->setContainerOffsetX(self->_scrollView.contentOffset.x);
  self.container->setContainerOffsetY(self->_scrollView.contentOffset.y);
  self.container->setWindowContainerWidth(_scrollView.bounds.size.width);
  self.container->setWindowContainerHeight(_scrollView.bounds.size.height);
  self.virtualizer->measureDefault(self.container);
  self.container->endRevision();
  
  CGFloat contentWidth = self.container->nextRevision.totalContainerWidth;
  CGFloat contentHeight = self.container->nextRevision.totalContainerHeight;

  _scrollView.contentSize = CGSizeMake(contentWidth, contentHeight);
  _contentView.frame = CGRectMake(0, 0, contentWidth, contentHeight);
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView *)scrollView
{
  if (!self.container) {
    return;
  }

  self.container->startRevision();
  self.container->setContainerOffsetX(scrollView.contentOffset.x);
  self.container->setContainerOffsetY(scrollView.contentOffset.y);
  self.virtualizer->measureDefault(self.container);
  self.container->endRevision();
  
  CGFloat contentWidth = self.container->nextRevision.totalContainerWidth;
  CGFloat contentHeight = self.container->nextRevision.totalContainerHeight;

  _scrollView.contentSize = CGSizeMake(contentWidth, contentHeight);
  _contentView.frame = CGRectMake(0, 0, contentWidth, contentHeight);
  
  if(!self->_eventEmitter) {
    return;
  }

  const auto visibleIndices = self.container->getVisibleIndices();

  const auto &eventEmitter = *std::static_pointer_cast<ShadowlistViewEventEmitter const>(self->_eventEmitter);
  eventEmitter.onVisibleIndicesChange({
    .visibleStartIndex = static_cast<int>(visibleIndices.first),
    .visibleEndIndex = static_cast<int>(visibleIndices.second)
  });
}

Class<RCTComponentViewProtocol> ShadowlistViewCls(void)
{
  return ShadowlistView.class;
}

@end
