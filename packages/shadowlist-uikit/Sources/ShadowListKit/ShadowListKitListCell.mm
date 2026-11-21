#import "Internal/ShadowListKitListCell+Private.h"
#import "Internal/ShadowListKitListView+Private.h"

@implementation ShadowListKitListCell

- (instancetype)initWithReuseIdentifier:(NSString *)reuseIdentifier
{
  if (self = [super initWithFrame:CGRectZero]) {
    _reuseIdentifier = [reuseIdentifier copy];
    _index = NSNotFound;
    _row = NSNotFound;
  }
  return self;
}

- (void)prepareForReuse
{
}

- (NSArray<UIAccessibilityCustomAction *> *)accessibilityCustomActions
{
  NSArray<UIAccessibilityCustomAction *> *own = super.accessibilityCustomActions;
  ShadowListKitListView *list = [self.superview isKindOfClass:ShadowListKitListView.class] ? (id)self.superview : nil;
  NSArray<UIAccessibilityCustomAction *> *board = [list dragBoardAccessibilityActionsForCell:self] ?: @[];
  NSArray<UIAccessibilityCustomAction *> *swipe = [list swipeAccessibilityActionsForCell:self] ?: @[];
  NSArray<UIAccessibilityCustomAction *> *native = [board arrayByAddingObjectsFromArray:swipe];
  return native.count ? [(own ?: @[]) arrayByAddingObjectsFromArray:native] : own;
}

/*
 * Fit the cell's Auto Layout constraints to the cross size. The size along the axis is free.
 * Cells that lay out by hand override this.
 */
- (CGSize)sizeThatFits:(CGSize)size
{
  BOOL horizontal = size.width >= CGFLOAT_MAX / 2;
  CGSize target = horizontal ? CGSizeMake(UILayoutFittingCompressedSize.width, size.height)
                             : CGSizeMake(size.width, UILayoutFittingCompressedSize.height);
  UILayoutPriority width = horizontal ? UILayoutPriorityFittingSizeLevel : UILayoutPriorityRequired;
  UILayoutPriority height = horizontal ? UILayoutPriorityRequired : UILayoutPriorityFittingSizeLevel;
  return [self systemLayoutSizeFittingSize:target withHorizontalFittingPriority:width verticalFittingPriority:height];
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

@end
