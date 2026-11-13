#pragma once

#import <UIKit/UIKit.h>
#import <ShadowListKit/ShadowListKitListModels.h>

NS_ASSUME_NONNULL_BEGIN

/*
 * The swipe actions and native context menu of a list item, outside a list. Add content to
 * contentView. A library surface moves during a swipe; child transforms stay unchanged.
 * Standalone actions use the cross axis of a vertical list.
 */
@interface ShadowListKitItemActionsView : UIView
@property (nonatomic, readonly) UIView *contentView;
@property (nonatomic, strong, nullable) ShadowListKitSwipeActionsConfiguration *leadingSwipeActions;
@property (nonatomic, strong, nullable) ShadowListKitSwipeActionsConfiguration *trailingSwipeActions;
@property (nonatomic, strong, nullable) UIMenu *contextMenu;
@property (nonatomic, copy, nullable) void (^onContextMenuWillShow)(void);
- (void)closeSwipeActionsAnimated:(BOOL)animated NS_SWIFT_NAME(closeSwipeActions(animated:));
@end

NS_ASSUME_NONNULL_END
