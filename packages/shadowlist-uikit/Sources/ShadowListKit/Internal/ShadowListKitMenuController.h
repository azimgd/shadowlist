#pragma once

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@protocol ShadowListKitMenuHost <NSObject>
@property (nonatomic, readonly) UIView *menuRootView;
@property (nonatomic, readonly) BOOL menusEnabled;
- (nullable UIView *)menuContentAtPoint:(CGPoint)point;
- (BOOL)isMenuContentValid:(UIView *)content;
- (BOOL)canShowMenuForContent:(UIView *)content;
- (nullable UIMenu *)menuForContent:(UIView *)content;
- (nullable id<NSCopying>)menuIdentifierForContent:(UIView *)content;
- (nullable UIView *)menuContentForIdentifier:(nullable id<NSCopying>)identifier;
- (void)contextMenuWillShowForContent:(UIView *)content;
@end

/* Native menu ownership and previews shared by list and standalone action hosts. */
@interface ShadowListKitMenuController : NSObject <UIContextMenuInteractionDelegate, UIEditMenuInteractionDelegate>
- (instancetype)initWithHost:(id<ShadowListKitMenuHost>)host;
- (void)refresh;
- (void)reset;
- (BOOL)showMenuForContent:(UIView *)content atPoint:(CGPoint)point;
@end

NS_ASSUME_NONNULL_END
