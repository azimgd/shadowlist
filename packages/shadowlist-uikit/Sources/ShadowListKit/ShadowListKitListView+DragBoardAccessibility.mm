#import "Internal/ShadowListKitListView+Private.h"
#import "Internal/ShadowListKitDragBoard.h"

using namespace azimgd::shadowlist;

extern "C" const char ShadowListKitListViewDragBoardAccessibilityLink = 0;

@implementation ShadowListKitListView (DragBoardAccessibility)

/*
 * The same visible board targets as touch, ordered left to right and then top to bottom.
 */
- (NSArray<ShadowListKitListView *> *)dragBoardMoveTargets
{
  NSMutableArray<ShadowListKitListView *> *targets = [NSMutableArray array];
  if (!_dragBoard || !self.reorderEnabled || self.editing || _sections.isSectioned()) return targets;
  for (ShadowListKitListView *list in _dragBoard.lists) {
    if (list != self && list.window && list.window == self.window && !list.hidden && list.reorderEnabled &&
        list.dragListId.length && !list->_sections.isSectioned()) [targets addObject:list];
  }
  [targets sortUsingComparator:^NSComparisonResult(ShadowListKitListView *first, ShadowListKitListView *second) {
    CGPoint a = [first convertPoint:CGPointZero toView:self.window];
    CGPoint b = [second convertPoint:CGPointZero toView:self.window];
    if (a.x != b.x) return a.x < b.x ? NSOrderedAscending : NSOrderedDescending;
    return a.y == b.y ? NSOrderedSame : a.y < b.y ? NSOrderedAscending : NSOrderedDescending;
  }];
  return targets;
}

/*
 * Accessibility appends into a target through the same application-owned move as a drop.
 * A rejected request says nothing; accepted data supplies the announcement's destination.
 */
- (NSArray<UIAccessibilityCustomAction *> *)dragBoardAccessibilityActionsForCell:(ShadowListKitListCell *)cell
{
  if (cell.index == NSNotFound || ([_userDelegate respondsToSelector:@selector(listView:canMoveItemAtIndex:)] &&
      ![_userDelegate listView:self canMoveItemAtIndex:cell.index])) return @[];
  NSMutableArray<UIAccessibilityCustomAction *> *actions = [NSMutableArray array];
  __weak ShadowListKitListView *weakSelf = self;
  __weak ShadowListKitListCell *weakCell = cell;
  for (ShadowListKitListView *target in [self dragBoardMoveTargets]) {
    __weak ShadowListKitListView *weakTarget = target;
    NSString *label = target.dragListLabel.length ? target.dragListLabel : target.dragListId;
    NSString *name = [NSString stringWithFormat:NSLocalizedString(@"Move to %@", nil), label];
    [actions addObject:[[UIAccessibilityCustomAction alloc] initWithName:name actionHandler:^BOOL(UIAccessibilityCustomAction *) {
      ShadowListKitListView *source = weakSelf;
      ShadowListKitListView *destination = weakTarget;
      ShadowListKitListCell *current = weakCell;
      if (!source || !destination || !current || current.superview != source || current.index == NSNotFound ||
          ![source.dragBoardMoveTargets containsObject:destination] || !source->_dragBoard ||
          source->_dragBoard->core.getPhase() != DragBoardPhase::Idle) return NO;
      BOOL accepted = [source moveDragBoardItemAtIndex:current.index toList:destination
                                    destinationIndex:(NSInteger)destination->_driver.getKeyCount() destinationKey:@""];
      if (accepted) {
        NSString *destinationLabel = destination.dragListLabel.length ? destination.dragListLabel : destination.dragListId;
        UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification,
          [NSString stringWithFormat:NSLocalizedString(@"Moved to %@", nil), destinationLabel]);
      }
      return YES;
    }]];
  }
  return actions;
}

@end
