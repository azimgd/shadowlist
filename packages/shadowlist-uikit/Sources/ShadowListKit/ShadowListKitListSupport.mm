#import "Internal/ShadowListKitListView+Private.h"
#import "Internal/ShadowListKitListSupport.h"

NSString *const SHADOWLIST_KIT_SECTION_HEADER_IDENTIFIER = @"ShadowListKitSectionHeader";
NSString *const SHADOWLIST_KIT_SECTION_FOOTER_IDENTIFIER = @"ShadowListKitSectionFooter";

#pragma mark - Settle frame target

@implementation ShadowListKitSettleTarget

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

#pragma mark - Section header cell

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
  return selector == @selector(scrollViewWillEndDragging:withVelocity:targetContentOffset:) ||
    selector == @selector(scrollViewDidEndDragging:willDecelerate:) ||
    selector == @selector(scrollViewDidEndDecelerating:) ||
    selector == @selector(scrollViewDidEndScrollingAnimation:) ||
    selector == @selector(scrollViewWillBeginDragging:);
}

- (BOOL)respondsToSelector:(SEL)selector
{
  return ShadowListKitListHandles(selector) || [_target respondsToSelector:selector];
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
