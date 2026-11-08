#import "Internal/ShadowListKitChangeAnimator.h"
#import "Internal/ShadowListKitListView+Private.h"

#include <shadowlist-core/host/ChangeAnimation.hpp>

#include <algorithm>
#include <optional>

using namespace azimgd::shadowlist;

@implementation ShadowListKitChangeAnimator {
  __weak ShadowListKitListView *_list;
  ChangeAnimation _animation;
  NSMutableSet<ShadowListKitListCell *> *_fading;
}

- (instancetype)initWithList:(ShadowListKitListView *)list
{
  if (self = [super init]) {
    _list = list;
    _fading = [NSMutableSet new];
  }
  return self;
}

/*
 * A list without a size runs no pass that would play the change. It shows nothing to animate
 * from either.
 */
- (void)captureDeleted:(const std::vector<std::string>&)deleted inserted:(const std::vector<std::string>&)inserted
{
  ShadowListKitListView *list = _list;
  if (!list || !list.window || [list hasHeldRow] || list->_windowAlong <= 0 || list->_windowCross <= 0) {
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
 * The cell goes back where it showed, its slides and drag shift stopped.
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
  [self placeCell:cell atScreenOrigin:*previous];
  [_fading addObject:cell];
  __weak ShadowListKitChangeAnimator *weakSelf = self;
  [list.itemAnimator listView:list animateDeleteOfCell:cell completion:^{
    [weakSelf finishFadeOfCell:cell];
  }];
  return YES;
}

/*
 * A fading cell goes back to the pool once. An item animator that calls its completion twice,
 * or the list recycling every cell first, finds it gone.
 */
- (void)finishFadeOfCell:(ShadowListKitListCell *)cell
{
  if (![_fading containsObject:cell]) {
    return;
  }
  [_fading removeObject:cell];
  [_list recycleCell:cell];
}

- (void)finishFades
{
  for (ShadowListKitListCell *cell in [_fading allObjects]) {
    [self finishFadeOfCell:cell];
  }
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
    positions.push_back([self laidOutOriginOf:cell]);
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

#pragma mark - Platform

#if TARGET_OS_OSX

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

/*
 * Where the layout pass put a cell's top left corner in the list.
 */
- (ScreenPoint)laidOutOriginOf:(ShadowListKitListCell *)cell
{
  NSPoint origin = [_list convertPoint:cell.frame.origin fromView:cell.superview];
  return {origin.x, origin.y};
}

/*
 * The cell leaves the pinned slot to the row that pins next.
 */
- (void)placeCell:(ShadowListKitListCell *)cell atScreenOrigin:(ScreenPoint)origin
{
  ShadowListKitListView *list = _list;
  if (cell == list->_stickyCell) {
    list->_stickyCell = nil;
  }
  [cell.layer removeAllAnimations];
  [list resetDragLookOfCell:cell];
  NSPoint place = [cell.superview convertPoint:NSMakePoint(origin.x, origin.y) fromView:list];
  ShadowListKitPlace(cell, CGRectMake(place.x, place.y, NSWidth(cell.frame), NSHeight(cell.frame)));
}

#else

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

- (ScreenPoint)laidOutOriginOf:(ShadowListKitListCell *)cell
{
  CGPoint offset = _list.contentOffset;
  CGSize size = cell.bounds.size;
  return {cell.center.x - size.width / 2 - offset.x, cell.center.y - size.height / 2 - offset.y};
}

- (void)placeCell:(ShadowListKitListCell *)cell atScreenOrigin:(ScreenPoint)origin
{
  CGPoint offset = _list.contentOffset;
  CGSize size = cell.bounds.size;
  cell.transform = CGAffineTransformIdentity;
  cell.center = CGPointMake(origin.x + offset.x + size.width / 2, origin.y + offset.y + size.height / 2);
}

#endif

@end
