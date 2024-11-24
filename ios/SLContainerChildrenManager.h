#ifndef SLContainerChildrenManager_h
#define SLContainerChildrenManager_h

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import "RCTComponentViewProtocol.h"
#import "SLComponentRegistry.h"

@interface SLContainerChildrenManager : NSObject

- (instancetype)initWithContentView:(UIView *)contentView;
- (void)mountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView uniqueId:(NSString *)uniqueId index:(NSInteger)index;
- (void)unmountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView uniqueId:(NSString *)uniqueId index:(NSInteger)index;
- (void)mount:(int)visibleStartIndex visibleEndIndex:(int)visibleEndIndex firstChildUniqueId:(NSString *)firstChildUniqueId lastChildUniqueId:(NSString *)lastChildUniqueId;

@end
#endif
