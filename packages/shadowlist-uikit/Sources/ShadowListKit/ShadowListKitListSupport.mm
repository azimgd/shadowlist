#import "Internal/ShadowListKitListView+Private.h"
#import "Internal/ShadowListKitListSupport.h"

NSString *const SHADOWLIST_KIT_SECTION_HEADER_IDENTIFIER = @"ShadowListKitSectionHeader";
NSString *const SHADOWLIST_KIT_SECTION_FOOTER_IDENTIFIER = @"ShadowListKitSectionFooter";

#pragma mark - Settle frame target

@implementation ShadowListKitSettleLinkTarget

- (void)tick:(CADisplayLink *)link
{
  ShadowListKitListView *list = _list;
  if (!list) {
    [link invalidate];
    return;
  }
  [list settleFrame];
}

@end

#pragma mark - Section title cell

@implementation ShadowListKitSectionTitleCell

- (instancetype)initWithReuseIdentifier:(NSString *)reuseIdentifier
{
  if (self = [super initWithReuseIdentifier:reuseIdentifier]) {
    _footer = [reuseIdentifier isEqualToString:SHADOWLIST_KIT_SECTION_FOOTER_IDENTIFIER];
    _label = [UILabel new];
    _label.numberOfLines = 0;
    _label.textColor = UIColor.secondaryLabelColor;
    _label.font = _footer ? [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote]
                          : [UIFont systemFontOfSize:15 weight:UIFontWeightSemibold];
    self.backgroundColor = _footer ? UIColor.clearColor : UIColor.secondarySystemBackgroundColor;
    [self addSubview:_label];
  }
  return self;
}

- (CGSize)sizeThatFits:(CGSize)size
{
  CGSize text = [_label sizeThatFits:CGSizeMake(MAX(0, size.width - 32), CGFLOAT_MAX)];
  return CGSizeMake(size.width, MAX(28, ceil(text.height) + 12));
}

- (void)setTitle:(NSString *)title
{
  _label.text = title;
  [self setNeedsLayout];
}

- (void)layoutSubviews
{
  [super layoutSubviews];
  _label.frame = CGRectMake(16, 6, MAX(0, self.bounds.size.width - 32), MAX(0, self.bounds.size.height - 12));
}

@end

#pragma mark - Delegate proxy

@implementation ShadowListKitDelegateProxy

static BOOL ShadowListKitListHandles(SEL selector)
{
  return selector == @selector(scrollViewDidScroll:) ||
         selector == @selector(scrollViewWillEndDragging:withVelocity:targetContentOffset:) ||
         selector == @selector(scrollViewDidEndDragging:willDecelerate:) ||
         selector == @selector(scrollViewDidEndDecelerating:) ||
         selector == @selector(scrollViewDidEndScrollingAnimation:) ||
         selector == @selector(scrollViewWillBeginDragging:);
}

- (BOOL)respondsToSelector:(SEL)selector
{
  return ShadowListKitListHandles(selector) || [_target respondsToSelector:selector];
}

/*
 * The scroll calls the list handles go to the list and then to the delegate as plain messages.
 * UIScrollView sends scrollViewDidScroll: on every frame of a scroll, where building an
 * invocation each time costs more than the list's own work. Every other call goes straight to
 * the delegate.
 */
- (void)scrollViewDidScroll:(UIScrollView *)scrollView
{
  id<UIScrollViewDelegate> list = (id<UIScrollViewDelegate>)_list;
  id<UIScrollViewDelegate> target = _target;
  [list scrollViewDidScroll:scrollView];
  if ([target respondsToSelector:@selector(scrollViewDidScroll:)]) {
    [target scrollViewDidScroll:scrollView];
  }
}

- (void)scrollViewWillBeginDragging:(UIScrollView *)scrollView
{
  id<UIScrollViewDelegate> list = (id<UIScrollViewDelegate>)_list;
  id<UIScrollViewDelegate> target = _target;
  [list scrollViewWillBeginDragging:scrollView];
  if ([target respondsToSelector:@selector(scrollViewWillBeginDragging:)]) {
    [target scrollViewWillBeginDragging:scrollView];
  }
}

- (void)scrollViewWillEndDragging:(UIScrollView *)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint *)targetContentOffset
{
  id<UIScrollViewDelegate> list = (id<UIScrollViewDelegate>)_list;
  id<UIScrollViewDelegate> target = _target;
  [list scrollViewWillEndDragging:scrollView withVelocity:velocity targetContentOffset:targetContentOffset];
  if ([target respondsToSelector:@selector(scrollViewWillEndDragging:withVelocity:targetContentOffset:)]) {
    [target scrollViewWillEndDragging:scrollView withVelocity:velocity targetContentOffset:targetContentOffset];
  }
}

- (void)scrollViewDidEndDragging:(UIScrollView *)scrollView willDecelerate:(BOOL)decelerate
{
  id<UIScrollViewDelegate> list = (id<UIScrollViewDelegate>)_list;
  id<UIScrollViewDelegate> target = _target;
  [list scrollViewDidEndDragging:scrollView willDecelerate:decelerate];
  if ([target respondsToSelector:@selector(scrollViewDidEndDragging:willDecelerate:)]) {
    [target scrollViewDidEndDragging:scrollView willDecelerate:decelerate];
  }
}

- (void)scrollViewDidEndDecelerating:(UIScrollView *)scrollView
{
  id<UIScrollViewDelegate> list = (id<UIScrollViewDelegate>)_list;
  id<UIScrollViewDelegate> target = _target;
  [list scrollViewDidEndDecelerating:scrollView];
  if ([target respondsToSelector:@selector(scrollViewDidEndDecelerating:)]) {
    [target scrollViewDidEndDecelerating:scrollView];
  }
}

- (void)scrollViewDidEndScrollingAnimation:(UIScrollView *)scrollView
{
  id<UIScrollViewDelegate> list = (id<UIScrollViewDelegate>)_list;
  id<UIScrollViewDelegate> target = _target;
  [list scrollViewDidEndScrollingAnimation:scrollView];
  if ([target respondsToSelector:@selector(scrollViewDidEndScrollingAnimation:)]) {
    [target scrollViewDidEndScrollingAnimation:scrollView];
  }
}

- (id)forwardingTargetForSelector:(SEL)selector
{
  return ShadowListKitListHandles(selector) ? nil : _target;
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)selector
{
  id target = ShadowListKitListHandles(selector) ? (id)_list : (id)_target;
  return [target methodSignatureForSelector:selector] ?: [NSObject instanceMethodSignatureForSelector:@selector(self)];
}

- (void)forwardInvocation:(NSInvocation *)invocation
{
  SEL selector = invocation.selector;
  if (ShadowListKitListHandles(selector)) {
    [invocation invokeWithTarget:_list];
    if ([_target respondsToSelector:selector]) {
      [invocation invokeWithTarget:_target];
    }
  } else if ([_target respondsToSelector:selector]) {
    [invocation invokeWithTarget:_target];
  }
}

@end
