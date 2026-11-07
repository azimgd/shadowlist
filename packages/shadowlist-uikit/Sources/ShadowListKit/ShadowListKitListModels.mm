#import "Internal/ShadowListKitListModels+Private.h"
#import "Internal/ShadowListKitPlatform.h"

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

- (ShadowListKitColor *)backgroundColor
{
  if (_backgroundColor) {
    return _backgroundColor;
  }
  return _style == ShadowListKitSwipeActionStyleDestructive ? [ShadowListKitColor systemRedColor] : [ShadowListKitColor systemGrayColor];
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
