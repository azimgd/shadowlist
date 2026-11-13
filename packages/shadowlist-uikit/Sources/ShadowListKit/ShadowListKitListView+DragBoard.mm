#import "Internal/ShadowListKitListView+Private.h"
#import "Internal/ShadowListKitDragBoard.h"

#include <shadowlist-core/host/DragBoard.hpp>

#include <algorithm>
#include <cmath>

using namespace azimgd::shadowlist;

extern "C" const char ShadowListKitListViewDragBoardLink = 0;

/*
 * The shared core owns the flight. UIKit keeps only weak list membership and its picture.
 * Every point handed to the core is in the home window's coordinates.
 */
@implementation ShadowListKitDragBoard
- (instancetype)init
{
  if (self = [super init]) {
    _lists = NSHashTable.weakObjectsHashTable;
  }
  return self;
}
@end

static NSMapTable<NSString *, ShadowListKitDragBoard *> *ShadowListKitDragBoards(void)
{
  static NSMapTable<NSString *, ShadowListKitDragBoard *> *boards;
  static dispatch_once_t once;
  dispatch_once(&once, ^{ boards = [NSMapTable strongToWeakObjectsMapTable]; });
  return boards;
}

static CGPoint ShadowListKitDragFinger(ShadowListKitListView *list)
{
  CGPoint offset = list.contentOffset;
  return [list convertPoint:CGPointMake(list->_dragTouch.x + offset.x, list->_dragTouch.y + offset.y) toView:list.window];
}

static void ShadowListKitEndForeign(ShadowListKitListView *list)
{
  if (list && list->_driver.isDragForeign()) {
    list->_driver.dragEnd();
    [list applyDragShiftsAnimated:YES];
  }
}

static void ShadowListKitSnapBoard(ShadowListKitDragBoard *board)
{
  ShadowListKitListView *outer = board.sidewaysScrollView;
  board.sidewaysScrollView = nil;
  if (outer.window && outer.snapToItem && !outer.isDragging && !outer.isDecelerating) {
    double target = std::clamp(outer->_driver.nearestSnapOffset([outer offset]), 0.0, (double)[outer maxOffset]);
    [outer animateToOffset:target landing:{}];
  }
}

@implementation ShadowListKitListView (DragBoard)

- (NSString *)dragBoardId { return _dragBoardId; }
- (NSString *)dragListId { return _dragListId; }
- (NSString *)dragListLabel { return _dragListLabel; }
- (void)setDragListLabel:(NSString *)label { _dragListLabel = [label copy]; }

- (void)setDragBoardId:(NSString *)boardId
{
  if ([_dragBoardId isEqualToString:boardId]) return;
  [self cancelHeldRow];
  [self leaveDragBoard];
  _dragBoardId = [boardId copy];
  [self joinDragBoard];
}

- (void)setDragListId:(NSString *)listId
{
  if ([_dragListId isEqualToString:listId]) return;
  [self cancelHeldRow];
  [self leaveDragBoard];
  _dragListId = [listId copy];
  [self joinDragBoard];
}

- (void)joinDragBoard
{
  if (!_dragBoardId.length || !_dragListId.length) return;
  NSMapTable *boards = ShadowListKitDragBoards();
  _dragBoard = [boards objectForKey:_dragBoardId];
  if (!_dragBoard) {
    _dragBoard = [ShadowListKitDragBoard new];
    [boards setObject:_dragBoard forKey:_dragBoardId];
  }
  [_dragBoard.lists addObject:self];
}

- (void)leaveDragBoard
{
  ShadowListKitDragBoard *board = _dragBoard;
  if (!board) return;
  std::string listId = _dragListId.UTF8String ?: "";
  // Weak references can already be nil during deallocation; the shared core still names it.
  bool ending = board->core.getHomeId() == listId || (board->core.getTargetId() == listId && board->core.isLanding());
  board->core.leave(_dragListId.UTF8String ?: "");
  ShadowListKitEndForeign(self);
  if (ending) {
    ShadowListKitSnapBoard(board);
    ShadowListKitEndForeign(board.target);
    if (board.home) board.home->_heldCell.alpha = 1;
    board.landingCell.alpha = 1;
    [board.overlay removeFromSuperview];
    board.overlay = nil;
    board.home = nil;
    board.target = nil;
  } else if (board.target == self) {
    board.target = nil;
  }
  [board.lists removeObject:self];
  _dragBoard = nil;
}

- (void)beginDragBoard
{
  [self joinDragBoard];
  ShadowListKitDragBoard *board = _dragBoard;
  if (!board || !self.window || !_heldCell || _sections.isSectioned()) return;
  // A new pickup ends the older flight and its picture before the core starts another.
  ShadowListKitSnapBoard(board);
  ShadowListKitEndForeign(board.target);
  if (board.home) board.home->_heldCell.alpha = 1;
  [board.overlay removeFromSuperview];
  board.landingCell.alpha = 1;
  board.landingCell = nil;
  board.overlay = nil;
  board.target = nil;
  board.home = self;
  CGPoint finger = ShadowListKitDragFinger(self);
  board.finger = finger;
  board->scroll.begin(finger.x);
  CGPoint center = [_heldCell convertPoint:CGPointMake(CGRectGetMidX(_heldCell.bounds), CGRectGetMidY(_heldCell.bounds)) toView:self.window];
  board->core.begin(_dragListId.UTF8String, _driver.getKeyAt(_driver.getHeldIndex()), {finger.x, finger.y}, {center.x, center.y});
}

- (void)noteDragBoardFinger:(CGPoint)location
{
  if (_dragBoard.home == self && self.window) _dragBoard.finger = [self convertPoint:location toView:self.window];
}

- (void)refreshDragBoardTouch
{
  if (_dragBoard.home != self || !self.window) return;
  CGPoint content = [self convertPoint:_dragBoard.finger fromView:self.window];
  _dragTouch = CGPointMake(content.x - self.contentOffset.x, content.y - self.contentOffset.y);
}

- (BOOL)updateDragBoard
{
  ShadowListKitDragBoard *board = _dragBoard;
  if (!board || board.home != self || !board->core.isDeciding() || !self.window) return NO;
  CGPoint finger = board.finger;
  UIView *hit = [self.window hitTest:finger withEvent:nil];
  ShadowListKitListView *hovered = nil;
  for (UIView *view = hit; view; view = view.superview) {
    if ([view isKindOfClass:ShadowListKitListView.class] && [board.lists containsObject:(id)view]) {
      hovered = (id)view;
      break;
    }
  }
  if (hovered && (!hovered.reorderEnabled || hovered->_sections.isSectioned())) hovered = nil;
  std::vector<std::string> candidates;
  if (hovered) candidates.push_back(hovered.dragListId.UTF8String);
  CGPoint center = [_heldCell convertPoint:CGPointMake(CGRectGetMidX(_heldCell.bounds), CGRectGetMidY(_heldCell.bounds)) toView:self.window];
  DragBoardFrame frame = board->core.frame({finger.x, finger.y}, {center.x, center.y}, candidates, CACurrentMediaTime() * 1000.0);
  if (frame.step.kind != DragBoardStep::Kind::Stay) {
    ShadowListKitEndForeign(board.target);
    board.target = frame.floating ? hovered : nil;
    _driver.dragSetHeldAway(frame.floating);
    if (board.target) {
      CGSize size = _heldCell.bounds.size;
      board.target->_driver.dragBeginForeign(board.target.horizontal ? size.width : size.height, board.target.horizontal ? size.height : size.width);
    }
  }
  if (frame.floating && !board.overlay) {
    board.overlay = [_heldCell snapshotViewAfterScreenUpdates:NO];
    board.overlay.userInteractionEnabled = NO;
    board.overlay.bounds = _heldCell.bounds;
    [self.window addSubview:board.overlay];
  }
  _heldCell.alpha = frame.hideHeld ? 0 : 1;
  board.overlay.hidden = !frame.floating;
  board.overlay.center = CGPointMake(frame.overlayCenter.x, frame.overlayCenter.y);
  ShadowListKitListView *target = board.target;
  if (frame.floating && target) {
    CGPoint placed = [target convertPoint:board.overlay.center fromView:self.window];
    target->_driver.dragPlaceForeignCenter([target along:placed], [target cross:placed]);
    target->_driver.dragUpdateInsertion([target mountedIndices]);
    [target applyDragShiftsAnimated:YES];
  }
  return frame.floating;
}

- (BOOL)scrollDragBoard:(CADisplayLink *)link
{
  ShadowListKitDragBoard *board = _dragBoard;
  if (!board || board.home != self || !board->core.isDeciding()) return NO;
  CGPoint finger = board.finger;
  double dt = link.targetTimestamp - link.timestamp;
  for (UIView *view = self.superview; view; view = view.superview) {
    if (![view isKindOfClass:ShadowListKitListView.class]) continue;
    ShadowListKitListView *outer = (id)view;
    if (!outer.horizontal || !outer.scrollEnabled || [outer maxOffset] <= 0) continue;
    CGPoint content = [outer convertPoint:finger fromView:self.window];
    double offset = [outer offset];
    double delta = board->scroll.delta(DRAG_BOARD_SCROLL_IOS, DRAG_BOARD_SCROLL_ARM_IOS, finger.x,
      content.x - outer.contentOffset.x, outer.bounds.size.width, dt);
    double next = std::clamp(offset + delta, 0.0, (double)[outer maxOffset]);
    if (next != offset) {
      [outer writeOffset:next byUser:YES];
      board.sidewaysScrollView = outer;
      [self refreshDragBoardTouch];
      [self updateDrag];
    }
    break;
  }
  if (!board->core.isFloating()) return NO;
  ShadowListKitListView *target = board.target;
  if (!target || !target.scrollEnabled) return YES;
  CGPoint content = [target convertPoint:finger fromView:self.window];
  double touch = [target along:content] - [target offset] - [target leadingInset];
  double offset = [target offset];
  double next = dragAutoScrollOffset(DRAG_AUTO_SCROLL_IOS, touch, target->_windowAlong, offset, [target maxOffset], dt);
  if (std::fabs(next - offset) >= 0.5) [target writeOffset:next byUser:YES];
  return YES;
}

- (CGPoint)dragBoardReturnOffsetForCenter:(CGPoint)center
{
  if (!_dragBoard || _dragBoard.home != self) return CGPointZero;
  DragPoint offset = _dragBoard->core.returnOffset({center.x, center.y}, CACurrentMediaTime() * 1000.0);
  return CGPointMake(offset.x, offset.y);
}

- (BOOL)finishDragBoard:(BOOL)commit
{
  ShadowListKitDragBoard *board = _dragBoard;
  if (!board || board.home != self) return NO;
  std::uint64_t flight = board->core.getFlight();
  ShadowListKitSnapBoard(board);
  ShadowListKitListView *target = board.target;
  bool foreign = target && target->_driver.isDragForeign();
  DragBoardRelease released = commit ? board->core.release(foreign ? target->_driver.getDragInsertionIndex() : UNDEFINED_INDEX,
    foreign ? target->_driver.getDragInsertionKey() : std::string(), foreign ? target->_driver.getRowCount() : 0, CACurrentMediaTime() * 1000.0) : DragBoardRelease{};
  if (!commit) board->core.cancel();
  BOOL crossed = released.kind != DragBoardRelease::Kind::Home;
  ShadowListKitListCell *landedCell = nil;
  if (released.kind == DragBoardRelease::Kind::Drop && target &&
      [_userDelegate respondsToSelector:@selector(listView:moveItemAtIndex:toListId:destinationIndex:destinationKey:)]) {
    std::string key = board->core.getHeldKey();
    NSInteger source = [self itemForRow:(NSInteger)_driver.getHeldIndex()];
    // The synchronous reload can recycle the source cell. It no longer owns a normal drag.
    ShadowListKitListCell *sourceCell = _heldCell;
    _heldCell = nil;
    _driver.dragEnd();
    sourceCell.alpha = 1;
    @try {
      BOOL accepted = [self moveDragBoardItemAtIndex:source toList:target destinationIndex:(NSInteger)released.index
                                    destinationKey:[NSString stringWithUTF8String:released.key.c_str()]];
      if (board.home != self || board->core.getFlight() != flight) return YES;
      board->core.landed(target.dragListId.UTF8String, accepted);
      auto cell = target->_mounted.find(key);
      if (cell != target->_mounted.end()) landedCell = cell->second;
    } @catch (NSException *exception) {
      ShadowListKitEndForeign(target);
      [board.overlay removeFromSuperview];
      board.overlay = nil;
      board.home = nil;
      board.target = nil;
      board->core.endFlight();
      sourceCell.transform = CGAffineTransformIdentity;
      sourceCell.layer.shadowOpacity = 0;
      @throw exception;
    }
  }
  ShadowListKitEndForeign(target);
  _driver.dragSetHeldAway(false);
  _heldCell.alpha = 1;
  if (landedCell && board.overlay && board->core.beginOverlaySettle()) {
    board.landingCell = landedCell;
    landedCell.alpha = 0;
    CGRect destination = [landedCell convertRect:landedCell.bounds toView:self.window];
    [UIView animateWithDuration:DRAG_DROP_DURATION_MS / 1000.0 delay:0
                        options:UIViewAnimationOptionBeginFromCurrentState | UIViewAnimationOptionAllowUserInteraction
                     animations:^{ board.overlay.frame = destination; }
                     completion:^(BOOL) {
                       if (board->core.getFlight() != flight) return;
                       board.landingCell.alpha = 1;
                       board.landingCell = nil;
                       [board.overlay removeFromSuperview];
                       board.overlay = nil;
                       board.home = nil;
                       board.target = nil;
                       board->core.endFlight();
                     }];
    return crossed;
  }
  [board.overlay removeFromSuperview];
  board.overlay = nil;
  board.home = nil;
  board.target = nil;
  board->core.endFlight();
  return crossed;
}

/*
 * Touch and accessibility share the same scoped data transaction. A callback can remove a
 * list or start another flight; subsequent callbacks then leave that newer state alone.
 */
- (BOOL)moveDragBoardItemAtIndex:(NSInteger)source toList:(ShadowListKitListView *)target destinationIndex:(NSInteger)index destinationKey:(NSString *)destinationKey
{
  NSInteger row = [self rowForItem:source];
  if (row == NSNotFound || (std::size_t)row >= _driver.getKeyCount() ||
      ![_userDelegate respondsToSelector:@selector(listView:moveItemAtIndex:toListId:destinationIndex:destinationKey:)]) return NO;
  if ([_userDelegate respondsToSelector:@selector(listView:canMoveItemAtIndex:)] && ![_userDelegate listView:self canMoveItemAtIndex:source]) return NO;
  std::string key = _driver.getKeyAt((std::size_t)row);
  ShadowListKitDragBoard *board = _dragBoard;
  if (!board || target->_dragBoard != board) return NO;
  ShadowListKitListView *home = board.home;
  std::uint64_t flight = board->core.getFlight();
  BOOL (^current)(void) = ^BOOL {
    return self->_dragBoard == board && target->_dragBoard == board && board.home == home && board->core.getFlight() == flight;
  };
  _driver.setDropHint({DropHint::Kind::Removed, key});
  target->_driver.setDropHint({DropHint::Kind::Inserted, key});
  @try {
    [_userDelegate listView:self moveItemAtIndex:source toListId:target.dragListId destinationIndex:index destinationKey:destinationKey];
    if (!current()) return NO;
    [self reloadData];
    if (!current()) return NO;
    [target reloadData];
    if (!current()) return NO;
    [self layoutIfNeeded];
    if (!current()) return NO;
    [target layoutIfNeeded];
    return current() && target->_driver.indexOfKey(key) != UNDEFINED_INDEX;
  } @finally {
    _driver.setDropHint({});
    target->_driver.setDropHint({});
  }
}

@end
