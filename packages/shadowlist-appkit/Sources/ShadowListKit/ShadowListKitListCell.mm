#import "Internal/ShadowListKitListCell+Private.h"
#import "Internal/ShadowListKitListView+Private.h"

#import <objc/runtime.h>

static IMP ShadowListKitBaseSetAffineTransform;

/*
 * Drops an identity while the layer's transform has no rotation, which only the list sets. A
 * rotation of the cell still lands and comes off.
 */
static void ShadowListKitCellLayerSetAffineTransform(CALayer *layer, SEL selector, CGAffineTransform transform)
{
  CATransform3D current = layer.transform;
  BOOL unrotated = current.m12 == 0 && current.m21 == 0 && current.m11 > 0 && current.m22 > 0;
  if (CGAffineTransformIsIdentity(transform) && unrotated) {
    return;
  }
  ((void (*)(CALayer *, SEL, CGAffineTransform))ShadowListKitBaseSetAffineTransform)(layer, selector, transform);
}

/*
 * AppKit owns the geometry of a layer backed view's layer. Whenever it syncs it, on a frame
 * change and when it builds the layer tree a run loop turn after a view joins a window, it
 * writes the view's rotation into the layer with setAffineTransform:. For a cell without a
 * rotation that write is the identity, which took off the shift and the lift a drag keeps in the
 * transform. A cell's layer is AppKit's own layer class with that write dropped, a subclass made
 * at run time because AppKit's layer class is private. It is made once, for the first class
 * AppKit gives. A layer of another class stays as it is.
 */
static Class ShadowListKitCellLayerClass(Class base)
{
  static Class cellLayerClass;
  static Class cellLayerBase;
  if (cellLayerBase) {
    return cellLayerBase == base ? cellLayerClass : nil;
  }
  SEL selector = @selector(setAffineTransform:);
  NSString *name = [@"ShadowListKitCellLayer_" stringByAppendingString:NSStringFromClass(base)];
  Class subclass = objc_getClass(name.UTF8String);
  if (!subclass) {
    subclass = objc_allocateClassPair(base, name.UTF8String, 0);
    if (!subclass) {
      return nil;
    }
    Method method = class_getInstanceMethod(base, selector);
    class_addMethod(subclass, selector, (IMP)ShadowListKitCellLayerSetAffineTransform, method_getTypeEncoding(method));
    objc_registerClassPair(subclass);
  }
  ShadowListKitBaseSetAffineTransform = class_getMethodImplementation(base, selector);
  cellLayerBase = base;
  cellLayerClass = subclass;
  return subclass;
}

@implementation ShadowListKitListCell

- (instancetype)initWithReuseIdentifier:(NSString *)reuseIdentifier
{
  if (self = [super initWithFrame:NSZeroRect]) {
    _reuseIdentifier = [reuseIdentifier copy];
    _index = NSNotFound;
    _row = NSNotFound;
    self.wantsLayer = YES;
    self.layerContentsRedrawPolicy = NSViewLayerContentsRedrawOnSetNeedsDisplay;
    self.accessibilityElement = YES;
    self.accessibilityRole = NSAccessibilityCellRole;
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

- (CALayer *)makeBackingLayer
{
  CALayer *layer = [super makeBackingLayer];
  Class cellLayerClass = ShadowListKitCellLayerClass(object_getClass(layer));
  if (cellLayerClass) {
    object_setClass(layer, cellLayerClass);
  }
  return layer;
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

#pragma mark - Accessibility

/*
 * A shown cell is the cell of its row's accessibility element, in the list's one column.
 */
- (id)accessibilityParent
{
  ShadowListKitListView *list = (ShadowListKitListView *)self.superview.superview;
  id parent = [list isKindOfClass:[ShadowListKitListView class]] ? [list accessibilityParentOfCell:self] : nil;
  return parent ?: [super accessibilityParent];
}

- (NSRange)accessibilityRowIndexRange
{
  return _row != NSNotFound ? NSMakeRange((NSUInteger)_row, 1) : NSMakeRange(0, 0);
}

- (NSRange)accessibilityColumnIndexRange
{
  return NSMakeRange(0, 1);
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
