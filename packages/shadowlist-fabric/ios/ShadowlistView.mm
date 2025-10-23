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

    self.contentView = _scrollView;
  }

  return self;
}

- (void)intializeShadowlist:(int)size inverted:(bool)inverted horizontal:(bool)horizontal
{
  self.virtualizer = new azimgd::shadowlist::Virtualizer();
  self.container = new azimgd::shadowlist::Container();
  self.observer = new azimgd::shadowlist::Observer(*self.container, 16);

  self.container->resizeElementsTail(size);
  self.container->setObserver(self.observer);

  {
    __weak __typeof(self) weakSelf = self;
    
    /*
     * offsetInitAdjustmentCallback
     */
    self.container->offsetInitAdjustmentCallback = [weakSelf](azimgd::shadowlist::Revision revision) -> void {
      __strong auto strongSelf = weakSelf;

      if (!strongSelf.container->inverted) {
        strongSelf.container->setOffsetInitAdjustmentCompleted(true);
        return;
      }

      [strongSelf setContentOffsetPerRevisionDefault];
    };

    /*
     * offsetMvcpAdjustmentCallback
     */
    self.container->offsetMvcpAdjustmentCallback = [weakSelf](azimgd::shadowlist::Revision revision) -> void {
      __strong auto strongSelf = weakSelf;
    };
    
    /*
     * sizeAdjustmentCallback
     */
    self.container->sizeAdjustmentCallback = [weakSelf](azimgd::shadowlist::Revision revision) -> void {
      __strong auto strongSelf = weakSelf;
      
      [strongSelf setContentSizePerRevisionDefault];
    };
    
    /*
     * measurementCallback
     */
    self.container->measurementCallback = [weakSelf](std::size_t index) -> std::pair<double, double> {
      return {120.0, 120.0};
    };
  }

  self.container->inverted = inverted;
  self.container->horizontal = horizontal;
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
  self.virtualizer->measure(self.container);
  self.container->endRevision();
  
  [self adjustElementOffsetsPerRevisionDefault:childViewProps.index childComponentView:childComponentView];
  
  [_contentView insertSubview:childComponentView atIndex:childViewProps.index];
  
  /*
   * Update all child component view frames based on virtualization offsets
   */
  [self adjustElementsOffsetsPerRevisionDefault:static_cast<size_t>(0)];
}

- (void)unmountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  [childComponentView removeFromSuperview];
}

- (void)finalizeUpdates:(RNComponentViewUpdateMask)updateMask
{
  [super finalizeUpdates:updateMask];

  if (!self.container && self->_eventEmitter) {
    const auto &nextViewProps = static_cast<const ShadowlistViewProps &>(*_props);
    [self intializeShadowlist:nextViewProps.size inverted:nextViewProps.inverted horizontal:nextViewProps.horizontal];
    const auto &eventEmitter = *std::static_pointer_cast<ShadowlistViewEventEmitter const>(self->_eventEmitter);
    eventEmitter.onInitialized({});
  }

  if (!self.container || !self.virtualizer) {
    return;
  }
  
  self.container->startRevision();
  self.container->setContainerOffsetX(self->_scrollView.contentOffset.x);
  self.container->setContainerOffsetY(self->_scrollView.contentOffset.y);
  self.container->setWindowContainerWidth(_scrollView.bounds.size.width);
  self.container->setWindowContainerHeight(_scrollView.bounds.size.height);
  self.virtualizer->measure(self.container);
  self.container->endRevision();
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView *)scrollView
{
  if (!self.container) {
    return;
  }
  
  if (!self->_eventEmitter) {
    return;
  }
  
  if (!self.container->offsetMvcpAdjustmentCallbackCompleted) {
    self.container->setOffsetMvcpAdjustmentCompleted(true);
    return;
  }
  
  if (!self.container->offsetInitAdjustmentCallbackCompleted) {
    self.container->setOffsetInitAdjustmentCompleted(true);
    return;
  }
  
  if (!self.container->sizeAdjustmentCallbackCompleted) {
    self.container->setSizeAdjustmentCallbackCompleted(true);
    return;
  }

  self.container->startRevision();
  self.container->setContainerOffsetX(scrollView.contentOffset.x);
  self.container->setContainerOffsetY(scrollView.contentOffset.y);
  self.virtualizer->measure(self.container);
  self.container->endRevision();

  const auto visibleIndices = self.container->getVisibleIndices();

  const auto &eventEmitter = *std::static_pointer_cast<ShadowlistViewEventEmitter const>(self->_eventEmitter);
  eventEmitter.onVisibleIndicesChange({
    .visibleStartIndex = static_cast<int>(visibleIndices.first),
    .visibleEndIndex = static_cast<int>(visibleIndices.second)
  });
  
  self.container->setSizeAdjustmentCallbackCompleted(false);
}

- (void)handleCommand:(const NSString *)commandName args:(const NSArray *)args
{
  RCTShadowlistViewHandleCommand(self, commandName, args);
}

- (void)prependElements:(NSInteger)size
{
  self.virtualizer->prependElements(self.container, size, self.container->nextRevision);

  [self setContentSizePerRevisionDefault];
  [self setContentOffsetPerRevisionMvcp];

  self.container->startRevision();
  self.container->setContainerOffsetX(self->_scrollView.contentOffset.x);
  self.container->setContainerOffsetY(self->_scrollView.contentOffset.y);
  self.container->endRevision();

  [self adjustElementsOffsetsPerRevisionDefault:static_cast<size_t>(size)];
}

- (void)appendElements:(NSInteger)size
{
  self.virtualizer->appendElements(self.container, size, self.container->nextRevision);

  [self setContentSizePerRevisionDefault];
}

- (void)adjustElementOffsetsPerRevisionDefault:(size_t)index childComponentView:(UIView<RCTComponentViewProtocol>*)childComponentView
{
  double offsetX = self.container->getElementAtIndex(index).offsetX;
  double offsetY = self.container->getElementAtIndex(index).offsetY;

  CGRect frame = childComponentView.frame;
  frame.origin.x = offsetX;
  frame.origin.y = offsetY;
  childComponentView.frame = frame;
}

- (void)adjustElementsOffsetsPerRevisionDefault:(size_t)size
{
  for (UIView<RCTComponentViewProtocol> *childComponentView in _contentView.subviews) {
    if (![childComponentView conformsToProtocol:@protocol(RCTShadowlistElementViewViewProtocol)]) {
      continue;
    }

    const auto &childViewProps = *std::static_pointer_cast<ShadowlistElementViewProps const>(childComponentView.props);
    [self adjustElementOffsetsPerRevisionDefault:(childViewProps.index + size) childComponentView:childComponentView];
  }
}

- (void)setContentSizePerRevisionDefault
{
  self->_scrollView.contentSize = CGSizeMake(
    self.container->nextRevision.totalContainerWidth,
    self.container->nextRevision.totalContainerHeight);
  self->_contentView.frame = CGRectMake(
    0,
    0,
    self.container->nextRevision.totalContainerWidth,
    self.container->nextRevision.totalContainerHeight);
}

- (void)setContentOffsetPerRevisionDefault
{
  CGPoint adjustedOffset;

  if (self.container->horizontal) {
    adjustedOffset = CGPointMake(
      self.container->nextRevision.totalContainerWidth - self.container->nextRevision.windowContainerWidth,
      0);
  } else {
    adjustedOffset = CGPointMake(
      0,
      self.container->nextRevision.totalContainerHeight - self.container->nextRevision.windowContainerHeight);
  }

  [self->_scrollView setContentOffset:adjustedOffset animated:NO];
}

- (void)setContentOffsetPerRevisionMvcp
{
  CGPoint adjustedOffset;

  if (self.container->horizontal) {
    adjustedOffset = CGPointMake(
      self.container->nextRevision.containerOffsetX + self.container->nextRevision.mvcpDiffWidth,
      0);
  } else {
    adjustedOffset = CGPointMake(
      0,
      self.container->nextRevision.containerOffsetY + self.container->nextRevision.mvcpDiffHeight);
  }

  [self->_scrollView setContentOffset:adjustedOffset animated:NO];
}

Class<RCTComponentViewProtocol> ShadowlistViewCls(void)
{
  return ShadowlistView.class;
}

@end
