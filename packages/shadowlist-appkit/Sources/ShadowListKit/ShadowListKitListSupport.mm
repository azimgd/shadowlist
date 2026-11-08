#import "Internal/ShadowListKitListView+Private.h"
#import "Internal/ShadowListKitListSupport.h"

#import <QuartzCore/QuartzCore.h>

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
    _label = [NSTextField wrappingLabelWithString:@""];
    _label.textColor = NSColor.secondaryLabelColor;
    _label.font = _footer ? [NSFont systemFontOfSize:NSFont.smallSystemFontSize]
                          : [NSFont preferredFontForTextStyle:NSFontTextStyleHeadline options:@{}];
    _label.translatesAutoresizingMaskIntoConstraints = YES;
    [self addSubview:_label];
  }
  return self;
}

- (NSSize)sizeThatFits:(NSSize)size
{
  CGFloat width = MAX(0, size.width - 32);
  NSSize text = [_label.cell cellSizeForBounds:NSMakeRect(0, 0, width, CGFLOAT_MAX)];
  return NSMakeSize(size.width, MAX(28, ceil(text.height) + 12));
}

/*
 * A header is opaque, which hides the rows under it while it is pinned. AppKit resolves the
 * color for the cell's appearance here.
 */
- (void)updateLayer
{
  self.layer.backgroundColor = _footer ? nil : NSColor.windowBackgroundColor.CGColor;
}

- (void)viewDidChangeEffectiveAppearance
{
  [super viewDidChangeEffectiveAppearance];
  self.needsDisplay = YES;
}

- (void)layout
{
  [super layout];
  _label.frame = NSMakeRect(16, 6, MAX(0, self.bounds.size.width - 32), MAX(0, self.bounds.size.height - 12));
}

@end

#pragma mark - Content view

@implementation ShadowListKitContentView

- (instancetype)initWithFrame:(NSRect)frame
{
  if (self = [super initWithFrame:frame]) {
    self.wantsLayer = YES;
    self.layerContentsRedrawPolicy = NSViewLayerContentsRedrawNever;
  }
  return self;
}

- (BOOL)isFlipped
{
  return YES;
}

- (id<CAAction>)actionForLayer:(CALayer *)layer forKey:(NSString *)event
{
  return (id<CAAction>)NSNull.null;
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event
{
  return [self.superview acceptsFirstMouse:event];
}

- (NSMenu *)menuForEvent:(NSEvent *)event
{
  return [super menuForEvent:event] ?: [self.superview menuForEvent:event];
}

@end
