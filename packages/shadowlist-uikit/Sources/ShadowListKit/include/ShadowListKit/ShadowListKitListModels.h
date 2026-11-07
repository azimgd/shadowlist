#pragma once

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, ShadowListKitSwipeActionStyle) {
  ShadowListKitSwipeActionStyleNormal,
  ShadowListKitSwipeActionStyleDestructive,
};

/*
 * A button shown behind a swiped item. The handler runs when the button is tapped or the item
 * is swiped all the way. Call completion with whether the action was performed. The item then
 * closes, unless the action deleted it.
 */
@interface ShadowListKitSwipeAction : NSObject

+ (instancetype)actionWithStyle:(ShadowListKitSwipeActionStyle)style
                          title:(nullable NSString *)title
                        handler:(void (^)(ShadowListKitSwipeAction *action, void (^completion)(BOOL performed)))handler;

@property (nonatomic, readonly) ShadowListKitSwipeActionStyle style;
@property (nonatomic, copy, nullable) NSString *title;
@property (nonatomic, strong, nullable) UIImage *image;
@property (nonatomic, strong, null_resettable) UIColor *backgroundColor;
@property (nonatomic, copy, readonly) void (^handler)(ShadowListKitSwipeAction *action, void (^completion)(BOOL performed));

@end

/*
 * The actions of one side of an item. The first action is nearest the edge. With
 * performsFirstActionWithFullSwipe, the default, swiping the item all the way performs it, like
 * swipe to dismiss.
 */
@interface ShadowListKitSwipeActionsConfiguration : NSObject

+ (instancetype)configurationWithActions:(NSArray<ShadowListKitSwipeAction *> *)actions;

@property (nonatomic, copy, readonly) NSArray<ShadowListKitSwipeAction *> *actions;
@property (nonatomic) BOOL performsFirstActionWithFullSwipe;

@end

/*
 * A scroll position that survives data changes: the key of the item at the viewport start and
 * how far the viewport start is past that item's leading edge.
 */
@interface ShadowListKitAnchorState : NSObject <NSSecureCoding>

- (instancetype)initWithKey:(NSString *)key offset:(CGFloat)offset NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

@property (nonatomic, copy, readonly) NSString *key;
@property (nonatomic, readonly) CGFloat offset;

@end

/*
 * What applyChanges found. Deleted and moved from are indices in the previous data, inserted,
 * moved to and reloaded indices in the new data.
 */
@interface ShadowListKitListChanges : NSObject

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

@property (nonatomic, copy, readonly) NSIndexSet *deletedIndices;
@property (nonatomic, copy, readonly) NSIndexSet *insertedIndices;
@property (nonatomic, copy, readonly) NSArray<NSNumber *> *movedFromIndices;
@property (nonatomic, copy, readonly) NSArray<NSNumber *> *movedToIndices;
@property (nonatomic, copy, readonly) NSIndexSet *reloadedIndices;
@property (nonatomic, readonly, getter=isEmpty) BOOL empty;

@end

NS_ASSUME_NONNULL_END
