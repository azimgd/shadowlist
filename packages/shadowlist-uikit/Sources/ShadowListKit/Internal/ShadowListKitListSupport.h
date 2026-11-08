#pragma once

#import <ShadowListKit/ShadowListKitListView.h>

NS_ASSUME_NONNULL_BEGIN

/*
 * Reuse identifier of the built-in section header and footer cells.
 */
extern NSString *const SHADOWLIST_KIT_SECTION_HEADER_IDENTIFIER;
extern NSString *const SHADOWLIST_KIT_SECTION_FOOTER_IDENTIFIER;

/*
 * The settle display link's target. It holds the list weakly, which lets the list go away
 * while the link exists.
 */
@interface ShadowListKitSettleLinkTarget : NSObject
@property (nonatomic, weak) ShadowListKitListView *list;
- (void)tick:(CADisplayLink *)link;
@end

/*
 * The cell of a section header or footer the data source gives only a title for. setTitle:
 * shows a title and lays the cell out again.
 */
@interface ShadowListKitSectionTitleCell : ShadowListKitListCell
#if TARGET_OS_OSX
@property (nonatomic, strong, readonly) NSTextField *label;
#else
@property (nonatomic, strong, readonly) UILabel *label;
#endif
@property (nonatomic) BOOL footer;
- (void)setTitle:(nullable NSString *)title;
@end

#if TARGET_OS_OSX
/*
 * The flipped view the list's cells sit in. Its bounds origin is the scroll position.
 */
@interface ShadowListKitContentView : NSView
@end
#else
/*
 * Sits between UIScrollView and the user's delegate. The list sees the scroll events it needs
 * for snapping and gesture phases, and everything else goes to the user's delegate.
 */
@interface ShadowListKitDelegateProxy : NSProxy
@property (nonatomic, weak) ShadowListKitListView *list;
@property (nonatomic, weak) id<ShadowListKitListViewDelegate> target;
@end
#endif

NS_ASSUME_NONNULL_END
