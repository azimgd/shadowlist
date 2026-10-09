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
@interface ShadowListKitSettleTarget : NSObject
@property (nonatomic, weak) ShadowListKitListView *list;
@end

/*
 * The cell of a section header or footer the data source gives only a title for.
 */
@interface ShadowListKitSectionTitleCell : ShadowListKitListCell
@property (nonatomic, strong, readonly) UILabel *label;
@property (nonatomic) BOOL footer;
@end

/*
 * Sits between UIScrollView and the user's delegate. The list sees the scroll events it needs
 * for snapping and gesture phases, and everything else goes to the user's delegate.
 */
@interface ShadowListKitDelegateProxy : NSProxy
@property (nonatomic, weak) ShadowListKitListView *list;
@property (nonatomic, weak) id<ShadowListKitListViewDelegate> target;
@end

NS_ASSUME_NONNULL_END
