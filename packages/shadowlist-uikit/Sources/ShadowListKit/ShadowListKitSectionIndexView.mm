#import "Internal/ShadowListKitSectionIndexView.h"

#include <shadowlist-core/host/SectionIndex.hpp>

using namespace azimgd::shadowlist;

static const CGFloat SHADOWLIST_KIT_SECTION_INDEX_FONT_SIZE = 11;
static const CGFloat SHADOWLIST_KIT_SECTION_INDEX_TITLE_PADDING = 1;

@implementation ShadowListKitSectionIndexView {
  NSInteger _selected;
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    _titles = @[];
    _selected = NSNotFound;
    self.backgroundColor = UIColor.clearColor;
    self.isAccessibilityElement = YES;
    self.accessibilityLabel = @"Section index";
    self.accessibilityTraits = UIAccessibilityTraitAdjustable;
  }
  return self;
}

- (CGSize)sizeThatFits:(CGSize)size
{
  return CGSizeMake(SECTION_INDEX_WIDTH, size.height);
}

- (void)setTitles:(NSArray<NSString *> *)titles
{
  if ([titles isEqualToArray:_titles]) {
    return;
  }
  _titles = [titles copy];
  [self setNeedsDisplay];
}

- (UIFont *)titleFont
{
  return [UIFont systemFontOfSize:SHADOWLIST_KIT_SECTION_INDEX_FONT_SIZE weight:UIFontWeightSemibold];
}

- (CGFloat)displayScale
{
  return self.traitCollection.displayScale > 0 ? self.traitCollection.displayScale : 1;
}

/*
 * A title is one line of its font on whole device pixels with a point of padding above and
 * below, the height React Native's index gives a title.
 */
- (CGFloat)titleHeight
{
  CGFloat scale = [self displayScale];
  return ceil([self titleFont].lineHeight * scale) / scale + 2 * SHADOWLIST_KIT_SECTION_INDEX_TITLE_PADDING;
}

/*
 * The core lays the titles out in rows of SECTION_INDEX_TITLE_HEIGHT times a scale.
 */
- (double)titleScale
{
  return [self titleHeight] / SECTION_INDEX_TITLE_HEIGHT;
}

- (CGFloat)titlesTop
{
  CGFloat scale = [self displayScale];
  double top = sectionIndexTitlesTop(self.bounds.size.height, _titles.count, [self titleScale]);
  return round(top * scale) / scale;
}

- (void)drawRect:(CGRect)rect
{
  NSDictionary *attributes = @{
    NSFontAttributeName : [self titleFont],
    NSForegroundColorAttributeName : self.tintColor,
  };
  CGFloat top = [self titlesTop] + SHADOWLIST_KIT_SECTION_INDEX_TITLE_PADDING;
  CGFloat titleHeight = [self titleHeight];
  CGFloat scale = [self displayScale];
  for (NSUInteger at = 0; at < _titles.count; ++at) {
    NSString *title = _titles[at];
    // A title is centered as wide as its glyphs on whole pixels, and starts on a pixel.
    CGFloat width = ceil([title sizeWithAttributes:attributes].width * scale) / scale;
    CGPoint point = CGPointMake(floor((self.bounds.size.width - width) / 2 * scale) / scale, top + at * titleHeight);
    [title drawAtPoint:point withAttributes:attributes];
  }
}

- (void)selectAtY:(CGFloat)y
{
  if (_titles.count == 0) {
    return;
  }
  NSInteger index = (NSInteger)sectionIndexTitleAt(y, self.bounds.size.height, _titles.count, [self titleScale]);
  if (index == _selected) {
    return;
  }
  _selected = index;
  [[UISelectionFeedbackGenerator new] selectionChanged];
  if (_onSelect) {
    _onSelect(index);
  }
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
  _selected = NSNotFound;
  [self selectAtY:[touches.anyObject locationInView:self].y];
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
  [self selectAtY:[touches.anyObject locationInView:self].y];
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
  _selected = NSNotFound;
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
  _selected = NSNotFound;
}

@end
