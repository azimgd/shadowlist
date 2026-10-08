#import "Internal/SLKSectionIndexView.h"

/*
 * Height of one title and the strip's width, in points.
 */
static const CGFloat SLK_INDEX_TITLE_HEIGHT = 16;
static const CGFloat SLK_INDEX_WIDTH = 24;

@implementation SLKSectionIndexView {
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
  return CGSizeMake(SLK_INDEX_WIDTH, size.height);
}

- (void)setTitles:(NSArray<NSString *> *)titles
{
  if ([titles isEqualToArray:_titles]) {
    return;
  }
  _titles = [titles copy];
  [self setNeedsDisplay];
}

- (CGFloat)titlesTop
{
  return MAX(0, (self.bounds.size.height - SLK_INDEX_TITLE_HEIGHT * _titles.count) / 2);
}

- (void)drawRect:(CGRect)rect
{
  NSDictionary *attributes = @{
    NSFontAttributeName : [UIFont systemFontOfSize:11 weight:UIFontWeightSemibold],
    NSForegroundColorAttributeName : self.tintColor,
  };
  CGFloat top = [self titlesTop];
  for (NSUInteger at = 0; at < _titles.count; ++at) {
    NSString *title = _titles[at];
    CGSize size = [title sizeWithAttributes:attributes];
    CGPoint point = CGPointMake((self.bounds.size.width - size.width) / 2,
      top + at * SLK_INDEX_TITLE_HEIGHT + (SLK_INDEX_TITLE_HEIGHT - size.height) / 2);
    [title drawAtPoint:point withAttributes:attributes];
  }
}

- (void)selectAtY:(CGFloat)y
{
  if (_titles.count == 0) {
    return;
  }
  NSInteger index = (NSInteger)floor((y - [self titlesTop]) / SLK_INDEX_TITLE_HEIGHT);
  index = MIN(MAX(index, 0), (NSInteger)_titles.count - 1);
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
