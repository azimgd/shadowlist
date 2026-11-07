#import "Internal/ShadowListKitListCell+Private.h"

@implementation ShadowListKitListCell

- (instancetype)initWithReuseIdentifier:(NSString *)reuseIdentifier
{
  if (self = [super initWithFrame:NSZeroRect]) {
    _reuseIdentifier = [reuseIdentifier copy];
    _index = NSNotFound;
    _row = NSNotFound;
    self.wantsLayer = YES;
    self.layerContentsRedrawPolicy = NSViewLayerContentsRedrawOnSetNeedsDisplay;
  }
  return self;
}

- (BOOL)isFlipped
{
  return YES;
}

- (BOOL)wantsUpdateLayer
{
  return YES;
}

/*
 * Frames, fades and colors change at once. Animations are the item animator's.
 */
- (id<CAAction>)actionForLayer:(CALayer *)layer forKey:(NSString *)event
{
  return (id<CAAction>)NSNull.null;
}

- (void)prepareForReuse
{
}

/*
 * The list answers for its cells: a click in an inactive window selects, and a Control-click
 * shows the list's menu.
 */
- (BOOL)acceptsFirstMouse:(NSEvent *)event
{
  return [self.superview acceptsFirstMouse:event];
}

- (NSMenu *)menuForEvent:(NSEvent *)event
{
  return [super menuForEvent:event] ?: [self.superview menuForEvent:event];
}

/*
 * Fit the cell's Auto Layout constraints to the cross size. The size along the axis is free.
 * The cell lays out at the cross size first, which lets wrapping text fields take that width.
 * Cells that lay out by hand override this.
 */
- (NSSize)sizeThatFits:(NSSize)size
{
  BOOL horizontal = size.width >= CGFLOAT_MAX / 2;
  NSLayoutConstraint *cross = horizontal ? [self.heightAnchor constraintEqualToConstant:size.height]
                                         : [self.widthAnchor constraintEqualToConstant:size.width];
  NSSize previous = self.frame.size;
  cross.active = YES;
  [self setFrameSize:horizontal ? NSMakeSize(previous.width, size.height) : NSMakeSize(size.width, previous.height)];
  [self layoutSubtreeIfNeeded];
  NSSize fits = self.fittingSize;
  cross.active = NO;
  [self setFrameSize:previous];
  return horizontal ? NSMakeSize(fits.width, size.height) : NSMakeSize(size.width, fits.height);
}

- (void)setHighlighted:(BOOL)highlighted
{
  [self setHighlighted:highlighted animated:NO];
}

- (void)setHighlighted:(BOOL)highlighted animated:(BOOL)animated
{
  _highlighted = highlighted;
}

- (void)setSelected:(BOOL)selected
{
  [self setSelected:selected animated:NO];
}

- (void)setSelected:(BOOL)selected animated:(BOOL)animated
{
  _selected = selected;
}

- (void)setEditing:(BOOL)editing
{
  [self setEditing:editing animated:NO];
}

- (void)setEditing:(BOOL)editing animated:(BOOL)animated
{
  _editing = editing;
}

- (void)setHovered:(BOOL)hovered
{
  [self setHovered:hovered animated:NO];
}

- (void)setHovered:(BOOL)hovered animated:(BOOL)animated
{
  _hovered = hovered;
}

- (void)setEmphasized:(BOOL)emphasized
{
  [self setEmphasized:emphasized animated:NO];
}

- (void)setEmphasized:(BOOL)emphasized animated:(BOOL)animated
{
  _emphasized = emphasized;
}

- (void)setMenuTarget:(BOOL)menuTarget
{
  [self setMenuTarget:menuTarget animated:NO];
}

- (void)setMenuTarget:(BOOL)menuTarget animated:(BOOL)animated
{
  _menuTarget = menuTarget;
}

@end
