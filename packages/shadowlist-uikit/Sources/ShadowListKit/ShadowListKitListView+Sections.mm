#import "Internal/ShadowListKitListView+Private.h"
#import "Internal/ShadowListKitSectionIndexView.h"

#include <vector>

using namespace azimgd::shadowlist;

/*
 * Referenced from ShadowListKitListView.mm to keep this file linked without -ObjC.
 */
extern "C" const char ShadowListKitListViewSectionsLink = 0;

// The class interface in ShadowListKitListView.h declares the public members implemented here.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-protocol-method-implementation"

/*
 * Sections over the rows: item and row indices, the section queries and scrolling, a drop
 * inside its section, sticky rows and the section index along the trailing edge.
 */
@implementation ShadowListKitListView (Sections)

- (NSInteger)itemForRow:(NSInteger)row
{
  if (row < 0) {
    return NSNotFound;
  }
  std::size_t item = _sections.itemForRow((std::size_t)row);
  return item == UNDEFINED_INDEX ? NSNotFound : (NSInteger)item;
}

- (NSInteger)rowForItem:(NSInteger)item
{
  if (item < 0) {
    return NSNotFound;
  }
  std::size_t row = _sections.rowForItem((std::size_t)item);
  return row == UNDEFINED_INDEX ? NSNotFound : (NSInteger)row;
}

- (NSInteger)numberOfSections
{
  return _sections.isSectioned() ? (NSInteger)_sections.getSectionCount() : 1;
}

- (NSInteger)sectionForItemAtIndex:(NSInteger)index
{
  if (index < 0) {
    return NSNotFound;
  }
  std::size_t section = _sections.sectionForItem((std::size_t)index);
  return section == UNDEFINED_INDEX ? NSNotFound : (NSInteger)section;
}

- (NSInteger)firstItemIndexInSection:(NSInteger)section
{
  if (section < 0) {
    return NSNotFound;
  }
  std::size_t item = _sections.firstItemInSection((std::size_t)section);
  return item == UNDEFINED_INDEX ? NSNotFound : (NSInteger)item;
}

- (CGRect)rectForHeaderInSection:(NSInteger)section
{
  std::size_t row = section < 0 ? UNDEFINED_INDEX : _sections.headerRow((std::size_t)section);
  if (row == UNDEFINED_INDEX || row >= _driver.getRowCount()) {
    return CGRectNull;
  }
  return [self rowRect:row];
}

- (CGRect)rectForFooterInSection:(NSInteger)section
{
  std::size_t row = section < 0 ? UNDEFINED_INDEX : _sections.footerRow((std::size_t)section);
  if (row == UNDEFINED_INDEX || row >= _driver.getRowCount()) {
    return CGRectNull;
  }
  return [self rowRect:row];
}

- (void)scrollToSection:(NSInteger)section animated:(BOOL)animated
{
  std::size_t row = section < 0 ? UNDEFINED_INDEX : _sections.firstRowInSection((std::size_t)section);
  if (row == UNDEFINED_INDEX) {
    return;
  }
  [self scrollToRow:row viewPosition:0 animated:animated];
}

/*
 * Map the drop of a held row to item indices. In a list with sections a row stays in its own
 * section and a drop outside goes to the nearest end of it.
 */
- (BOOL)itemsForDragFromRow:(std::size_t)fromRow toRow:(std::size_t)toRow from:(NSInteger *)from to:(NSInteger *)to
{
  std::size_t fromItem = _sections.itemForRow(fromRow);
  std::size_t toItem = _sections.itemForDrop(fromRow, toRow);
  if (fromItem == UNDEFINED_INDEX || toItem == UNDEFINED_INDEX) {
    return NO;
  }
  *from = (NSInteger)fromItem;
  *to = (NSInteger)toItem;
  return fromItem != toItem;
}

- (void)updateStickyRows
{
  std::vector<std::size_t> items = _stickyIndices.count > 0 ? ShadowListKitIndices(_stickyIndices) : std::vector<std::size_t>();
  _driver.setStickyIndices(_sections.stickyRows(items, _stickySectionHeaders));
}

/*
 * The section index along the trailing edge, from the data source's titles. Vertical lists only.
 */
- (void)reloadSectionIndex
{
  NSArray<NSString *> *titles = nil;
  if (!_horizontal && [_dataSource respondsToSelector:@selector(sectionIndexTitlesForListView:)]) {
    titles = [_dataSource sectionIndexTitlesForListView:self];
  }
  if (titles.count == 0) {
    [_sectionIndex removeFromSuperview];
    _sectionIndex = nil;
    return;
  }
  if (!_sectionIndex) {
    _sectionIndex = [ShadowListKitSectionIndexView new];
    _sectionIndex.layer.zPosition = 10;
    __weak ShadowListKitListView *weakSelf = self;
    _sectionIndex.onSelect = ^(NSInteger index) {
      [weakSelf scrollToSectionIndexTitleAtIndex:index];
    };
    [self addSubview:_sectionIndex];
  }
  _sectionIndex.titles = titles;
  [self setNeedsLayout];
}

- (void)scrollToSectionIndexTitleAtIndex:(NSInteger)index
{
  NSInteger section = index;
  if ([_dataSource respondsToSelector:@selector(listView:sectionForSectionIndexTitle:atIndex:)]) {
    section = [_dataSource listView:self sectionForSectionIndexTitle:_sectionIndex.titles[index] atIndex:index];
  }
  [self scrollToSection:section animated:NO];
}

/*
 * Keep the section index on the visible trailing edge, inside the insets.
 */
- (void)layoutSectionIndex
{
  if (!_sectionIndex) {
    return;
  }
  CGRect visible = UIEdgeInsetsInsetRect(self.bounds, self.adjustedContentInset);
  CGFloat width = [_sectionIndex sizeThatFits:visible.size].width;
  CGRect frame = CGRectMake(CGRectGetMaxX(visible) - width, CGRectGetMinY(visible), width, visible.size.height);
  if (!CGRectEqualToRect(_sectionIndex.frame, frame)) {
    _sectionIndex.frame = frame;
    [_sectionIndex setNeedsDisplay];
  }
  [self bringSubviewToFront:_sectionIndex];
}

- (BOOL)touchesShouldCancelInContentView:(UIView *)view
{
  if (_sectionIndex && [view isDescendantOfView:_sectionIndex]) {
    return NO;
  }
  return [super touchesShouldCancelInContentView:view];
}

@end
#pragma clang diagnostic pop
