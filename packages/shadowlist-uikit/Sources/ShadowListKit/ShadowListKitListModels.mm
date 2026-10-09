#import "Internal/ShadowListKitListModels+Private.h"

/*
 * Default duration of the change animations, in seconds.
 */
static const NSTimeInterval SHADOWLIST_KIT_CHANGE_DURATION = 0.25;

#pragma mark - Swipe actions

@implementation ShadowListKitSwipeAction

+ (instancetype)actionWithStyle:(ShadowListKitSwipeActionStyle)style
                          title:(NSString *)title
                        handler:(void (^)(ShadowListKitSwipeAction *, void (^)(BOOL)))handler
{
  ShadowListKitSwipeAction *action = [ShadowListKitSwipeAction new];
  action->_style = style;
  action->_title = [title copy];
  action->_handler = [handler copy];
  return action;
}

- (UIColor *)backgroundColor
{
  if (_backgroundColor) {
    return _backgroundColor;
  }
  return _style == ShadowListKitSwipeActionStyleDestructive ? UIColor.systemRedColor : UIColor.systemGrayColor;
}

@end

@implementation ShadowListKitSwipeActionsConfiguration

+ (instancetype)configurationWithActions:(NSArray<ShadowListKitSwipeAction *> *)actions
{
  ShadowListKitSwipeActionsConfiguration *configuration = [ShadowListKitSwipeActionsConfiguration new];
  configuration->_actions = [actions copy];
  configuration->_performsFirstActionWithFullSwipe = YES;
  return configuration;
}

@end

#pragma mark - Anchor state

@implementation ShadowListKitAnchorState

+ (BOOL)supportsSecureCoding
{
  return YES;
}

- (instancetype)initWithKey:(NSString *)key offset:(CGFloat)offset
{
  if (self = [super init]) {
    _key = [key copy];
    _offset = offset;
  }
  return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
  NSString *key = [coder decodeObjectOfClass:[NSString class] forKey:@"key"];
  if (!key) {
    return nil;
  }
  return [self initWithKey:key offset:(CGFloat)[coder decodeDoubleForKey:@"offset"]];
}

- (void)encodeWithCoder:(NSCoder *)coder
{
  [coder encodeObject:_key forKey:@"key"];
  [coder encodeDouble:_offset forKey:@"offset"];
}

@end

#pragma mark - Changes

@implementation ShadowListKitListChanges

- (instancetype)initWithDeleted:(NSIndexSet *)deleted
                       inserted:(NSIndexSet *)inserted
                      movedFrom:(NSArray<NSNumber *> *)movedFrom
                        movedTo:(NSArray<NSNumber *> *)movedTo
                       reloaded:(NSIndexSet *)reloaded
{
  if (self = [super init]) {
    _deletedIndices = [deleted copy];
    _insertedIndices = [inserted copy];
    _movedFromIndices = [movedFrom copy];
    _movedToIndices = [movedTo copy];
    _reloadedIndices = [reloaded copy];
  }
  return self;
}

- (BOOL)isEmpty
{
  return _deletedIndices.count == 0 && _insertedIndices.count == 0 && _movedFromIndices.count == 0 &&
    _reloadedIndices.count == 0;
}

- (NSString *)description
{
  return [NSString stringWithFormat:@"<ShadowListKitListChanges deleted=%lu inserted=%lu moved=%lu reloaded=%lu>",
    (unsigned long)_deletedIndices.count, (unsigned long)_insertedIndices.count,
    (unsigned long)_movedFromIndices.count, (unsigned long)_reloadedIndices.count];
}

@end

#pragma mark - Default item animator

@implementation ShadowListKitDefaultItemAnimator

- (instancetype)init
{
  if (self = [super init]) {
    _duration = SHADOWLIST_KIT_CHANGE_DURATION;
  }
  return self;
}

- (void)listView:(ShadowListKitListView *)listView animateInsertOfCell:(ShadowListKitListCell *)cell
{
  cell.transform = CGAffineTransformIdentity;
  cell.alpha = 0;
  [UIView animateWithDuration:_duration animations:^{
    cell.alpha = 1;
  }];
}

- (void)listView:(ShadowListKitListView *)listView animateRemovalOfCell:(ShadowListKitListCell *)cell completion:(void (^)(void))completion
{
  [UIView animateWithDuration:_duration animations:^{
    cell.alpha = 0;
  } completion:^(BOOL) {
    completion();
  }];
}

- (void)listView:(ShadowListKitListView *)listView animateMoveOfCell:(ShadowListKitListCell *)cell fromOffset:(CGPoint)offset
{
  if (offset.x == 0 && offset.y == 0 && CGAffineTransformIsIdentity(cell.transform)) {
    return;
  }
  cell.transform = CGAffineTransformMakeTranslation(offset.x, offset.y);
  [UIView animateWithDuration:_duration delay:0 options:UIViewAnimationOptionBeginFromCurrentState animations:^{
    cell.transform = CGAffineTransformIdentity;
    cell.alpha = 1;
  } completion:nil];
}

@end
