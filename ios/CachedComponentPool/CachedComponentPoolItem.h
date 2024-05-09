#ifndef CachedComponentViewPoolItem_h
#define CachedComponentViewPoolItem_h

#import "RCTFabricComponentsPlugins.h"

@interface CachedComponentPoolItem : NSObject

@property (nonatomic, strong) UIView<RCTComponentViewProtocol> *component;

@end

#endif
