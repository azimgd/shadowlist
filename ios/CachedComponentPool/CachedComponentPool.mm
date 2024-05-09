#import <Foundation/Foundation.h>
#import "CachedComponentPool.h"
#import "RCTFabricComponentsPlugins.h"

@interface CachedComponentPool ()

@property (nonatomic, copy) void (^onCachedComponentMount)(NSArray<NSNumber *> *);
@property (nonatomic, copy) void (^onCachedComponentUnmount)(NSArray<NSNumber *> *);

@end

@implementation CachedComponentPool

- (instancetype)initWithObservable:(void (^)(NSArray<NSNumber *> *poolIndex))onCachedComponentMount
          onCachedComponentUnmount:(void (^)(NSArray<NSNumber *> *poolIndex))onCachedComponentUnmount {
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
    [mountableIndices addObject:@(poolIndex)];
    [self mountCachedComponentPoolItem:poolIndex];
  }
  
  self.onCachedComponentMount(mountableIndices);
  self.onCachedComponentUnmount(unmountableIndices);
}

- (void)upsertCachedComponentPoolItem:(UIView<RCTComponentViewProtocol> *)childComponentView poolIndex:(NSInteger)poolIndex {
  if ([self checkComponentExists:poolIndex]) {
    [self removeCachedComponentPoolItem:childComponentView poolIndex:poolIndex];
  }

  auto cachedComponentPoolItem = [CachedComponentPoolItem new];
  cachedComponentPoolItem.component = childComponentView;

  [self->_pool insertObject:cachedComponentPoolItem atIndex:poolIndex];
}

- (void)removeCachedComponentPoolItem:(UIView<RCTComponentViewProtocol> *)childComponentView poolIndex:(NSInteger)poolIndex {
  [self->_pool removeObjectAtIndex:poolIndex];
}

- (void)mountCachedComponentPoolItem:(NSInteger)poolIndex {
  assert([self checkComponentExists:poolIndex]);

  if ([self checkComponentMounted:poolIndex]) {
    [self unmountCachedComponentPoolItem:poolIndex];
  }
  [self->_mounted addObject:@(poolIndex)];
}

- (void)unmountCachedComponentPoolItem:(NSInteger)poolIndex {
  assert([self checkComponentExists:poolIndex]);

  [self->_mounted removeObject:@(poolIndex)];
}

@end

