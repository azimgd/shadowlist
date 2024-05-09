#ifndef CachedComponentViewPool_h
#define CachedComponentViewPool_h

#import "CachedComponentPoolItem.h"

@interface CachedComponentPool : NSObject

@property (nonatomic, strong) NSMutableArray<CachedComponentPoolItem *> *pool;
@property (nonatomic, strong) NSMutableArray<NSNumber *> *mounted;

- (instancetype)initWithObservable:(void (^)(NSInteger poolIndex))onCachedComponentMount
          onCachedComponentUnmount:(void (^)(NSInteger poolIndex))onCachedComponentUnmount;

- (UIView<RCTComponentViewProtocol> *)getComponentView:(NSInteger)poolIndex;
- (BOOL)checkComponentExists:(NSInteger)poolIndex;
- (BOOL)checkComponentMounted:(NSInteger)poolIndex;

- (void)upsertCachedComponentPoolItem:(UIView<RCTComponentViewProtocol> *)childComponentView poolIndex:(NSInteger)poolIndex;
- (void)removeCachedComponentPoolItem:(UIView<RCTComponentViewProtocol> *)childComponentView poolIndex:(NSInteger)poolIndex;

- (void)mountCachedComponentPoolItem:(NSInteger)poolIndex;
- (void)unmountCachedComponentPoolItem:(NSInteger)poolIndex;

- (void)recycle:(NSInteger)visibleStartIndex visibleEndIndex:(NSInteger)visibleEndIndex;

@end

#endif
