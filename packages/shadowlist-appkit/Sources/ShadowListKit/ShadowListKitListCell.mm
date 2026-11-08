#import "Internal/ShadowListKitListCell+Private.h"
#import "Internal/ShadowListKitListView+Private.h"

#import <objc/runtime.h>

static IMP ShadowListKitBaseSetAffineTransform;
static IMP ShadowListKitBaseSetPosition;
static IMP ShadowListKitBaseSetHidden;
static Class ShadowListKitMadeCellLayerClass;

@interface ShadowListKitListCell ()

- (CGPoint)placedLayerPosition;
- (BOOL)isPlacedLayerHidden;

@end

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
 * The cell of a layer while the list places it by its layer, or nil.
 */
static ShadowListKitListCell *ShadowListKitCellPlacedByLayer(CALayer *layer)
{
  id delegate = layer.delegate;
  if (![delegate isKindOfClass:[ShadowListKitListCell class]]) {
    return nil;
  }
  ShadowListKitListCell *cell = delegate;
  return cell.placedByLayer ? cell : nil;
}

/*
 * While the list places a cell by its layer, the layer stays where the list put it.
 */
static void ShadowListKitCellLayerSetPosition(CALayer *layer, SEL selector, CGPoint position)
{
  ShadowListKitListCell *cell = ShadowListKitCellPlacedByLayer(layer);
  if (cell) {
    position = [cell placedLayerPosition];
  }
  ((void (*)(CALayer *, SEL, CGPoint))ShadowListKitBaseSetPosition)(layer, selector, position);
}

static void ShadowListKitCellLayerSetHidden(CALayer *layer, SEL selector, BOOL hidden)
{
  ShadowListKitListCell *cell = ShadowListKitCellPlacedByLayer(layer);
  if (cell) {
    hidden = [cell isPlacedLayerHidden];
  }
  ((void (*)(CALayer *, SEL, BOOL))ShadowListKitBaseSetHidden)(layer, selector, hidden);
}

static void ShadowListKitAddLayerMethod(Class subclass, Class base, SEL selector, IMP implementation)
{
  Method method = class_getInstanceMethod(base, selector);
  class_addMethod(subclass, selector, implementation, method_getTypeEncoding(method));
}

/*
 * AppKit owns the geometry of a layer backed view's layer. Whenever it syncs it, on a frame
 * change and when it builds the layer tree a run loop turn after a view joins a window, it
 * writes the view's rotation into the layer with setAffineTransform:, and its frame and hidden
 * state with setPosition:, setBounds: and setHidden:. For a cell without a rotation the
 * transform write is the identity, which took off the shift and the lift a drag keeps in the
 * transform. A driven list moves a cell by its layer while it scrolls, and the view's frame
 * catches up later. A cell's layer is AppKit's own layer class with the identity dropped and the
 * position and hidden state the list gave kept while it places the cell by its layer, a subclass
 * made at run time because AppKit's layer class is private. It is made once, for the first
 * class AppKit gives. A layer of another class stays as it is.
 */
static Class ShadowListKitCellLayerClass(Class base)
{
  static Class cellLayerBase;
  if (cellLayerBase) {
    return cellLayerBase == base ? ShadowListKitMadeCellLayerClass : nil;
  }
  NSString *name = [@"ShadowListKitCellLayer_" stringByAppendingString:NSStringFromClass(base)];
  Class subclass = objc_getClass(name.UTF8String);
  if (!subclass) {
    subclass = objc_allocateClassPair(base, name.UTF8String, 0);
    if (!subclass) {
      return nil;
    }
    ShadowListKitAddLayerMethod(
      subclass, base, @selector(setAffineTransform:), (IMP)ShadowListKitCellLayerSetAffineTransform);
    ShadowListKitAddLayerMethod(subclass, base, @selector(setPosition:), (IMP)ShadowListKitCellLayerSetPosition);
    ShadowListKitAddLayerMethod(subclass, base, @selector(setHidden:), (IMP)ShadowListKitCellLayerSetHidden);
    objc_registerClassPair(subclass);
  }
  ShadowListKitBaseSetAffineTransform = class_getMethodImplementation(base, @selector(setAffineTransform:));
  ShadowListKitBaseSetPosition = class_getMethodImplementation(base, @selector(setPosition:));
  ShadowListKitBaseSetHidden = class_getMethodImplementation(base, @selector(setHidden:));
  cellLayerBase = base;
  ShadowListKitMadeCellLayerClass = subclass;
  return subclass;
}

@implementation ShadowListKitListCell {
  CGPoint _layerOrigin;
  BOOL _layerHidden;
}

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

#pragma mark - Layer placement

- (BOOL)canPlaceByLayer
{
  return ShadowListKitMadeCellLayerClass && object_getClass(self.layer) == ShadowListKitMadeCellLayerClass;
}

/*
 * The layer moves to an origin in the content and the view's frame stays where it was.
 */
- (void)placeLayerAtOrigin:(CGPoint)origin
{
  if (!_placedByLayer) {
    _placedByLayer = YES;
    _layerHidden = self.hidden;
  }
  _layerOrigin = origin;
  self.layer.position = [self placedLayerPosition];
}

- (void)setLayerHidden:(BOOL)hidden
{
  if (!_placedByLayer) {
    _placedByLayer = YES;
    _layerOrigin = self.frame.origin;
  }
  _layerHidden = hidden;
  self.layer.hidden = hidden;
}

- (CGPoint)placedLayerPosition
{
  CALayer *layer = self.layer;
  CGPoint anchor = layer.anchorPoint;
  CGSize size = layer.bounds.size;
  return CGPointMake(_layerOrigin.x + anchor.x * size.width, _layerOrigin.y + anchor.y * size.height);
}

- (BOOL)isPlacedLayerHidden
{
  return _layerHidden;
}

- (CGRect)shownFrame
{
  return _placedByLayer ? CGRect{_layerOrigin, self.frame.size} : self.frame;
}

- (BOOL)isShown
{
  return _placedByLayer ? !_layerHidden : !self.hidden;
}

/*
 * The view's frame and hidden state take the layer's.
 */
- (void)catchUpView
{
  if (!_placedByLayer) {
    return;
  }
  _placedByLayer = NO;
  if (!NSEqualPoints(self.frame.origin, _layerOrigin)) {
    [self setFrameOrigin:_layerOrigin];
  }
  if (self.hidden != _layerHidden) {
    self.hidden = _layerHidden;
  }
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
