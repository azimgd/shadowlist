#import <ShadowListKit/ShadowListKitListView.h>

NS_ASSUME_NONNULL_BEGIN

/*
 * mountGeneration is the mount pass that last wanted this cell. Cells left behind go back to
 * the pool. row is the row the core places for the cell, or NSNotFound in the pool. It differs
 * from index in a list with sections. separatorLayer is the line below an item, made on first use.
 */
@interface ShadowListKitListCell ()

@property (nonatomic, copy, readwrite, nullable) NSString *reuseIdentifier;
@property (nonatomic, readwrite) NSInteger index;
@property (nonatomic) NSInteger row;
@property (nonatomic) NSUInteger mountGeneration;
@property (nonatomic, strong, nullable) CALayer *separatorLayer;

@end

NS_ASSUME_NONNULL_END
