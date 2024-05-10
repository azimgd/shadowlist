#ifndef CachedComponentViewPool_h
#define CachedComponentViewPool_h

#import "CachedComponentPoolItem.h"

@interface CachedComponentPool : NSObject

@property (nonatomic, strong) NSMutableArray<CachedComponentPoolItem *> *pool;
@property (nonatomic, strong) NSMutableArray<NSNumber *> *mounted;
@property (nonatomic, strong) NSArray<NSNumber *> *stickyIndices;

- (instancetype)initWithObservable:(NSArray<NSNumber *> *)stickyIndices
  onCachedComponentMount:(void (^)(NSInteger poolIndex))onCachedComponentMount
  onCachedComponentUnmount:(void (^)(NSInteger poolIndex))onCachedComponentUnmount;

- (UIView<RCTComponentViewProtocol> *)getComponentView:(NSInteger)poolIndex;
- (NSInteger)countPool;
- (BOOL)checkComponentExists:(NSInteger)poolIndex;
- (BOOL)checkComponentMounted:(NSInteger)poolIndex;

- (void)insertCachedComponentPoolItem:(UIView<RCTComponentViewProtocol> *)childComponentView poolIndex:(NSInteger)poolIndex;
- (void)removeCachedComponentPoolItem:(UIView<RCTComponentViewProtocol> *)childComponentView poolIndex:(NSInteger)poolIndex;

- (void)mountCachedComponentPoolItem:(NSInteger)poolIndex;
- (void)unmountCachedComponentPoolItem:(NSInteger)poolIndex;

- (void)recycle:(NSInteger)visibleStartIndex visibleEndIndex:(NSInteger)visibleEndIndex;

@end

#endif
