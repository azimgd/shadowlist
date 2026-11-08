#pragma once

#import <ShadowListKit/ShadowListKitListView.h>

NS_ASSUME_NONNULL_BEGIN

/*
 * Reuse identifier of the built-in section header and footer cells.
 */
extern NSString *const SHADOWLIST_KIT_SECTION_HEADER_IDENTIFIER;
extern NSString *const SHADOWLIST_KIT_SECTION_FOOTER_IDENTIFIER;

/*
 * What an assistive app hears after a page scroll: which items show out of all of them. An app
 * localizes it under the key ShadowListKitRowsShown in its ShadowListKit strings table.
 */
static inline NSString *ShadowListKitRowsShownAnnouncement(NSRange visible, NSUInteger count)
{
  NSString *format = NSLocalizedStringWithDefaultValue(@"ShadowListKitRowsShown", @"ShadowListKit", NSBundle.mainBundle,
    @"Rows %lu to %lu of %lu", @"The first and last item that show and the item count, after a page scroll");
  return [NSString localizedStringWithFormat:format, (unsigned long)visible.location + 1, (unsigned long)NSMaxRange(visible),
    (unsigned long)count];
}

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
 * for snapping and gesture phases and passes them on as the delegate's listViewDidScroll: family.
 * The scroll view's calls also go to the user's delegate when it answers them.
 */
@interface ShadowListKitDelegateProxy : NSProxy
@property (nonatomic, weak) ShadowListKitListView *list;
@property (nonatomic, weak) id<ShadowListKitListViewDelegate> target;
@end
#endif

NS_ASSUME_NONNULL_END
