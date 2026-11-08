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
 * The cell of a section header or footer the data source gives only a title for.
 */
@interface ShadowListKitSectionTitleCell : ShadowListKitListCell
@property (nonatomic, strong, readonly) NSTextField *label;
@property (nonatomic) BOOL footer;
@end

/*
 * The flipped view the list's cells sit in. Its bounds origin is the scroll position.
 */
@interface ShadowListKitContentView : NSView
@end

NS_ASSUME_NONNULL_END
