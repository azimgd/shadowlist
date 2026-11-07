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
 * Where a cell's top left corner shows in the list, a slide still running and a drag shift
 * included.
 */
- (ScreenPoint)screenOriginOf:(ShadowListKitListCell *)cell
{
  ShadowListKitListView *list = _list;
  NSPoint origin = [list convertPoint:cell.frame.origin fromView:cell.superview];
  CALayer *layer = cell.layer;
  CALayer *shown = layer.animationKeys.count > 0 ? layer.presentationLayer : nil;
  if (shown) {
    origin.x += shown.position.x - layer.position.x + shown.transform.m41;
    origin.y += shown.position.y - layer.position.y + shown.transform.m42;
  } else {
    origin.x += layer.transform.m41;
    origin.y += layer.transform.m42;
  }
  return {origin.x, origin.y};
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

/*
 * The cell goes back where it showed, its slides and drag shift stopped. It leaves the pinned
 * slot to the row that pins next.
 */
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
  if (cell == list->_stickyCell) {
    list->_stickyCell = nil;
  }
  [cell.layer removeAllAnimations];
  [list resetDragLookOfCell:cell];
  NSPoint origin = [cell.superview convertPoint:NSMakePoint(previous->x, previous->y) fromView:list];
  ShadowListKitPlace(cell, CGRectMake(origin.x, origin.y, NSWidth(cell.frame), NSHeight(cell.frame)));
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
  for (ShadowListKitListCell *cell : cells) {
    keys.push_back(list->_driver.getKeyAt((std::size_t)cell.row));
    NSPoint origin = [list convertPoint:cell.frame.origin fromView:cell.superview];
    positions.push_back({origin.x, origin.y});
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
