#import <ShadowListKit/SLKListView.h>

NS_ASSUME_NONNULL_BEGIN

@interface SLKListChanges ()

- (instancetype)initWithDeleted:(NSIndexSet *)deleted
                       inserted:(NSIndexSet *)inserted
                      movedFrom:(NSArray<NSNumber *> *)movedFrom
                        movedTo:(NSArray<NSNumber *> *)movedTo
                       reloaded:(NSIndexSet *)reloaded;

@end

NS_ASSUME_NONNULL_END
