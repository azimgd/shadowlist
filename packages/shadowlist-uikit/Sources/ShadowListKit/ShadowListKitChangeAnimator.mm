#import "Internal/ShadowListKitChangeAnimator.h"
#import "Internal/ShadowListKitListView+Private.h"

#include <shadowlist-core/host/ChangeAnimation.hpp>

#include <algorithm>
#include <optional>

using namespace azimgd::shadowlist;

@implementation ShadowListKitChangeAnimator {
  __weak ShadowListKitListView *_list;
  ChangeAnimation _animation;
}

- (instancetype)initWithList:(ShadowListKitListView *)list
{
  if (self = [super init]) {
    _list = list;
  }
  return self;
}

/*
 * Where a cell's top left corner shows on screen, a slide still running included.
 */
- (ScreenPoint)screenOriginOf:(ShadowListKitListCell *)cell
{
  CGPoint offset = _list.contentOffset;
  CGPoint center = cell.center;
  CGSize size = cell.bounds.size;
  double x = center.x - size.width / 2 + cell.transform.tx - offset.x;
  double y = center.y - size.height / 2 + cell.transform.ty - offset.y;
  return {x, y};
}

- (void)captureDeleted:(const std::vector<std::string>&)deleted inserted:(const std::vector<std::string>&)inserted
{
  ShadowListKitListView *list = _list;
  if (!list || !list.window || [list hasHeldRow]) {
    return;
  }
  if (_animation.capture(deleted, inserted)) {
    [self recordScreenOf:list];
  }
}

- (void)recordScreenOf:(ShadowListKitListView *)list
{
  for (auto& entry : list->_mounted) {
    if (!entry.second.hidden) {
      _animation.recordPosition(entry.first, [self screenOriginOf:entry.second]);
    }
  }
}

- (BOOL)fadeOutKey:(const std::string&)key cell:(ShadowListKitListCell *)cell
{
  if (cell.hidden) {
    return NO;
  }
  std::optional<ScreenPoint> previous = _animation.deletedPosition(key);
  if (!previous) {
    return NO;
  }
  ShadowListKitListView *list = _list;
  if (!list) {
    return NO;
  }
  CGPoint offset = list.contentOffset;
  CGSize size = cell.bounds.size;
  cell.transform = CGAffineTransformIdentity;
  cell.center = CGPointMake(previous->x + offset.x + size.width / 2, previous->y + offset.y + size.height / 2);
  __weak ShadowListKitListView *weakList = list;
  [list.itemAnimator listView:list animateDeleteOfCell:cell completion:^{
    [weakList recycleCell:cell];
  }];
  return YES;
}

- (void)run
{
  if (!_animation.isPending()) {
    return;
  }
  ShadowListKitListView *list = _list;
  if (!list) {
    return;
  }
  std::vector<ShadowListKitListCell *> cells;
  std::size_t keyCount = list->_driver.getKeyCount();
  for (auto& entry : list->_mounted) {
    if (!entry.second.hidden && entry.second.row != NSNotFound && (std::size_t)entry.second.row < keyCount) {
      cells.push_back(entry.second);
    }
  }
  std::sort(cells.begin(), cells.end(), [](ShadowListKitListCell *a, ShadowListKitListCell *b) { return a.row < b.row; });
  std::vector<std::string> keys;
  std::vector<ScreenPoint> positions;
  keys.reserve(cells.size());
  positions.reserve(cells.size());
  CGPoint offset = list.contentOffset;
  for (ShadowListKitListCell *cell : cells) {
    keys.push_back(list->_driver.getKeyAt((std::size_t)cell.row));
    CGSize size = cell.bounds.size;
    positions.push_back({cell.center.x - size.width / 2 - offset.x, cell.center.y - size.height / 2 - offset.y});
  }
  std::vector<ChangeStep> steps = _animation.run(keys, positions);
  for (std::size_t at = 0; at < steps.size(); ++at) {
    const ChangeStep& step = steps[at];
    if (step.kind == ChangeStepKind::Insert) {
      [list.itemAnimator listView:list animateInsertOfCell:cells[at]];
    } else {
      [list.itemAnimator listView:list animateMoveOfCell:cells[at] fromOffset:CGPointMake(step.fromX, step.fromY)];
    }
  }
}

@end
