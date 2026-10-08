#import "Internal/SLKChangeAnimator.h"
#import "Internal/SLKListView+Private.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

@implementation SLKChangeAnimator {
  __weak SLKListView *_list;
  BOOL _pending;
  // Screen center of every mounted row before the change, by key.
  std::unordered_map<std::string, CGPoint> _before;
  std::unordered_set<std::string> _inserted;
  std::unordered_set<std::string> _removed;
}

- (instancetype)initWithList:(SLKListView *)list
{
  if (self = [super init]) {
    _list = list;
  }
  return self;
}

/*
 * Where a cell shows on screen now, a slide still running included.
 */
- (CGPoint)screenCenterOf:(SLKListCell *)cell
{
  CGPoint offset = _list.contentOffset;
  CGPoint center = cell.center;
  return CGPointMake(center.x + cell.transform.tx - offset.x, center.y + cell.transform.ty - offset.y);
}

- (void)captureRemoved:(const std::vector<std::string>&)removed inserted:(const std::vector<std::string>&)inserted
{
  SLKListView *list = _list;
  if (!list || !list.window || [list hasHeldRow]) {
    return;
  }
  if (!_pending) {
    [self recordScreen];
    _removed.clear();
    _inserted.clear();
    _pending = YES;
  }
  std::unordered_set<std::string> insertedNow(inserted.begin(), inserted.end());
  std::unordered_set<std::string> removedNow(removed.begin(), removed.end());
  // A key on both sides moved. It slides like any row that stays.
  for (const std::string& key : removed) {
    if (insertedNow.count(key) == 0) {
      _removed.insert(key);
    } else {
      _inserted.erase(key);
    }
  }
  for (const std::string& key : inserted) {
    if (removedNow.count(key) == 0) {
      _inserted.insert(key);
    } else {
      _removed.erase(key);
    }
  }
}

- (void)recordScreen
{
  _before.clear();
  for (auto& entry : _list->_mounted) {
    if (!entry.second.hidden) {
      _before[entry.first] = [self screenCenterOf:entry.second];
    }
  }
}

- (BOOL)fadeOutKey:(const std::string&)key cell:(SLKListCell *)cell
{
  auto previous = _before.find(key);
  if (!_pending || _removed.count(key) == 0 || cell.hidden || previous == _before.end()) {
    return NO;
  }
  SLKListView *list = _list;
  CGPoint offset = list.contentOffset;
  cell.transform = CGAffineTransformIdentity;
  cell.center = CGPointMake(previous->second.x + offset.x, previous->second.y + offset.y);
  __weak SLKListView *weakList = list;
  [list.itemAnimator listView:list animateRemovalOfCell:cell completion:^{
    [weakList recycleCell:cell];
  }];
  return YES;
}

- (void)run
{
  if (!_pending) {
    return;
  }
  _pending = NO;
  SLKListView *list = _list;
  std::vector<SLKListCell *> cells;
  for (auto& entry : list->_mounted) {
    if (!entry.second.hidden && entry.second.row != NSNotFound) {
      cells.push_back(entry.second);
    }
  }
  std::sort(cells.begin(), cells.end(), [](SLKListCell *a, SLKListCell *b) { return a.row < b.row; });
  CGPoint shift = CGPointZero;
  for (SLKListCell *cell : cells) {
    const std::string& key = list->_driver.getKeyAt((std::size_t)cell.row);
    auto previous = _before.find(key);
    if (previous != _before.end()) {
      CGPoint current = CGPointMake(cell.center.x - list.contentOffset.x, cell.center.y - list.contentOffset.y);
      shift = CGPointMake(previous->second.x - current.x, previous->second.y - current.y);
      [list.itemAnimator listView:list animateMoveOfCell:cell fromOffset:shift];
    } else if (_inserted.count(key) > 0) {
      [list.itemAnimator listView:list animateInsertOfCell:cell];
    } else {
      // Came into view without a place on screen before. It moves with the row above it.
      [list.itemAnimator listView:list animateMoveOfCell:cell fromOffset:shift];
    }
  }
  _before.clear();
  _inserted.clear();
  _removed.clear();
}

@end
