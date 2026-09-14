#import "ShadowListTemplateView.h"

#import "ShadowListTemplateViewComponentDescriptor.h"
#import <react/renderer/components/ShadowListViewSpec/EventEmitters.h>
#import <react/renderer/components/ShadowListViewSpec/Props.h>
#import <react/renderer/components/ShadowListViewSpec/RCTComponentViewHelpers.h>

using namespace facebook::react;

@interface ShadowListTemplateView () <RCTShadowListTemplateViewViewProtocol>

@end

@implementation ShadowListTemplateView {
  RCTUIView *_contentView;
}

+ (ComponentDescriptorProvider)componentDescriptorProvider
{
  return concreteComponentDescriptorProvider<ShadowListTemplateViewComponentDescriptor>();
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    static const auto defaultProps = std::make_shared<const ShadowListTemplateViewProps>();
    _props = defaultProps;

    _contentView = [[RCTUIView alloc] init];

    self.contentView = _contentView;
  }

  return self;
}

/*
 * Fabric's recycle pool is keyed only by component handle, so a template view returned here
 * can be dequeued by ANY ShadowList in the app -- including one on a different screen. The
 * base RCTViewComponentView::prepareForRecycle resets props, layers and layout metrics but
 * NOT `transform` or `hidden`, and this list writes both from native
 * (ShadowListView+Sticky): a pinned header/footer carries a translation of up to the whole
 * content size, and the section-header overlay is hidden outright whenever no section is
 * active.
 *
 * `hidden` is the damaging one. A hidden overlay recycled as another list's `header` is
 * never shown again -- applyStickyTransforms rewrites the transform on every scroll tick but
 * has no reason to touch visibility -- while the shadow node still measures and reserves the
 * header's size. The result is a freshly opened list on an unrelated screen laid out around
 * a header nobody can see.
 */
- (void)prepareForRecycle
{
  self.transform = CGAffineTransformIdentity;
  self.hidden = NO;
#if TARGET_OS_OSX
  // Raised by SLRaiseSubview to keep sticky views above the rows; UIKit reorders instead.
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

Class<RCTComponentViewProtocol> ShadowListTemplateViewCls(void)
{
  return ShadowListTemplateView.class;
}

@end
