#import <Foundation/Foundation.h>
#import "CachedComponentPool.h"
#import "RCTFabricComponentsPlugins.h"

@interface CachedComponentPool ()

@property (nonatomic, copy) void (^onCachedComponentMount)(NSInteger poolIndex);
@property (nonatomic, copy) void (^onCachedComponentUnmount)(NSInteger poolIndex);

@end

@implementation CachedComponentPool

- (instancetype)initWithObservable:(void (^)(NSInteger poolIndex))onCachedComponentMount
          onCachedComponentUnmount:(void (^)(NSInteger poolIndex))onCachedComponentUnmount {
  self = [super init];
  if (self) {
    self->_pool = [NSMutableArray array];
    self->_mounted = [NSMutableArray array];

    _onCachedComponentMount = [onCachedComponentMount copy];
    _onCachedComponentUnmount = [onCachedComponentUnmount copy];
  }
  return self;
}

- (UIView<RCTComponentViewProtocol> *)getComponentView:(NSInteger)poolIndex {
  return [self->_pool objectAtIndex:poolIndex].component;
}

- (BOOL)checkComponentExists:(NSInteger)poolIndex {
  return poolIndex < self->_pool.count;
}

- (BOOL)checkComponentMounted:(NSInteger)poolIndex {
  return [self->_mounted containsObject:@(poolIndex)];
}

- (void)recycle:(NSInteger)visibleStartIndex visibleEndIndex:(NSInteger)visibleEndIndex {
  auto mountableIndices = [NSMutableArray array];
  auto unmountableIndices = [NSMutableArray array];

  for (NSUInteger poolIndex = 0; poolIndex < visibleStartIndex; poolIndex++) {
    [unmountableIndices addObject:@(poolIndex)];
    [self unmountCachedComponentPoolItem:poolIndex];
  }

  for (NSUInteger poolIndex = visibleEndIndex; poolIndex < [self->_pool count]; poolIndex++) {
    [unmountableIndices addObject:@(poolIndex)];
    [self unmountCachedComponentPoolItem:poolIndex];
  }

  for (NSUInteger poolIndex = visibleStartIndex; poolIndex < visibleEndIndex; poolIndex++) {
    if (![self checkComponentExists:poolIndex]) continue;
  
    [mountableIndices addObject:@(poolIndex)];
    [self mountCachedComponentPoolItem:poolIndex];
  }
}

- (void)upsertCachedComponentPoolItem:(UIView<RCTComponentViewProtocol> *)childComponentView poolIndex:(NSInteger)poolIndex {
  if ([self checkComponentExists:poolIndex]) {
    [self->_pool removeObjectAtIndex:poolIndex];
  }

  auto cachedComponentPoolItem = [CachedComponentPoolItem new];
  cachedComponentPoolItem.component = childComponentView;

  [self->_pool insertObject:cachedComponentPoolItem atIndex:poolIndex];
}

- (void)removeCachedComponentPoolItem:(UIView<RCTComponentViewProtocol> *)childComponentView poolIndex:(NSInteger)poolIndex {
  [childComponentView removeFromSuperview];
  [self->_pool removeObjectAtIndex:poolIndex];
}

- (void)mountCachedComponentPoolItem:(NSInteger)poolIndex {
  assert([self checkComponentExists:poolIndex]);

  if ([self checkComponentMounted:poolIndex]) {
    [self->_mounted removeObject:@(poolIndex)];
  }
  [self->_mounted addObject:@(poolIndex)];
  self.onCachedComponentMount(poolIndex);
}

- (void)unmountCachedComponentPoolItem:(NSInteger)poolIndex {
  assert([self checkComponentExists:poolIndex]);

  [self->_mounted removeObject:@(poolIndex)];
  self.onCachedComponentUnmount(poolIndex);
}

@end

