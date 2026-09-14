#import "ShadowListElementView.h"

#import "ShadowListElementViewComponentDescriptor.h"
#import <react/renderer/components/ShadowListViewSpec/EventEmitters.h>
#import <react/renderer/components/ShadowListViewSpec/Props.h>
#import <react/renderer/components/ShadowListViewSpec/RCTComponentViewHelpers.h>

using namespace facebook::react;

@interface ShadowListElementView () <RCTShadowListElementViewViewProtocol>

@end

@implementation ShadowListElementView {
  RCTUIView *_contentView;
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

    _contentView = [[RCTUIView alloc] init];

    self.contentView = _contentView;
  }

  return self;
}

/*
 * Fabric's recycle pool is keyed only by component handle, so a row view returned here can
 * be dequeued by ANY ShadowList in the app -- including one on a different screen. The base
 * RCTViewComponentView::prepareForRecycle resets props, layers and layout metrics but NOT
 * `transform` or `hidden`, and this list writes both from native: drag-to-reorder shifts
 * sibling rows to open a gap (ShadowListView+DragReorder) and lifts the picked-up row with a
 * shadow. A row unmounted mid-drag -- its data deleted, or simply scrolled out of the
 * mounted window -- goes back to the pool still carrying that displacement, and the next
 * list to dequeue it renders a row visibly offset from where it was laid out.
 *
 * Undo everything this component sets outside the props system, so a recycled view starts
 * in the same state a freshly created one would.
 */
- (void)prepareForRecycle
{
  [self.layer removeAllAnimations];
  self.transform = CGAffineTransformIdentity;
  self.hidden = NO;
  self.layer.shadowOpacity = 0.0;
#if TARGET_OS_OSX
  // Raised by SLRaiseSubview for sticky pinning; UIKit reorders subviews instead.
  self.layer.zPosition = 0.0;
#endif
  [super prepareForRecycle];
}

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
