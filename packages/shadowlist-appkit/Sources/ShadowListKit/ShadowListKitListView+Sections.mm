#import "Internal/ShadowListKitListView+Private.h"

using namespace azimgd::shadowlist;

/*
 * Referenced from ShadowListKitListView.mm to keep this file linked without -ObjC.
 */
extern "C" const char ShadowListKitListViewSectionsLink = 0;

// The class interface in ShadowListKitListView.h declares the public members implemented here.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-protocol-method-implementation"

/*
 * Sections over the rows: item and row indices, the section queries and scrolling.
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

@end
#pragma clang diagnostic pop
