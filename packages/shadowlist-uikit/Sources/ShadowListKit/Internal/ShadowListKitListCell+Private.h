#pragma once

#import <ShadowListKit/ShadowListKitListView.h>

NS_ASSUME_NONNULL_BEGIN

/*
 * mountGeneration is the mount pass that last wanted this cell. Cells left behind go back to
 * the pool. row is the row the core places for the cell, or NSNotFound in the pool. It differs
 * from index in a list with sections. separatorLayer is the line below an item, made on first use.
 *
 * Mac: a driven list moves and hides a cell by its layer while it scrolls, which changes no view
 * geometry. placedByLayer says the view's frame and hidden state lag behind the layer's. A cell
 * whose layer is not the kit's own layer class cannot be placed that way (canPlaceByLayer).
 * placeLayerAtOrigin: moves the layer to an origin in the content, setLayerHidden: hides or
 * shows it, shownFrame and shown are where and whether it shows, and catchUpView gives the view
 * the layer's frame and hidden state.
 */
@interface ShadowListKitListCell ()

@property (nonatomic, copy, readwrite, nullable) NSString *reuseIdentifier;
@property (nonatomic, readwrite) NSInteger index;
@property (nonatomic) NSInteger row;
@property (nonatomic) NSUInteger mountGeneration;
@property (nonatomic, strong, nullable) CALayer *separatorLayer;
#if TARGET_OS_OSX
@property (nonatomic, readonly, getter=isPlacedByLayer) BOOL placedByLayer;
@property (nonatomic, readonly) CGRect shownFrame;
@property (nonatomic, readonly, getter=isShown) BOOL shown;

- (BOOL)canPlaceByLayer;
- (void)placeLayerAtOrigin:(CGPoint)origin;
- (void)setLayerHidden:(BOOL)hidden;
- (void)catchUpView;
#endif

@end

NS_ASSUME_NONNULL_END
