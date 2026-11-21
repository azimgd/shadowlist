#pragma once

#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>

NS_ASSUME_NONNULL_BEGIN

/*
 * Text and optional fixed height predicted before a cell renders. Lengths are points or dp.
 */
@interface ShadowListKitItemSizeSpec : NSObject
- (instancetype)initWithText:(NSString *)text NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
@property (nonatomic, copy) NSString *text;
@property (nonatomic) CGFloat fontSize;
@property (nonatomic, copy, nullable) NSString *fontFamily;
@property (nonatomic, copy, nullable) NSString *fontWeight;
@property (nonatomic, copy, nullable) NSString *fontStyle;
@property (nonatomic) CGFloat lineHeight;
@property (nonatomic) CGFloat letterSpacing;
@property (nonatomic) NSInteger numberOfLines;
@property (nonatomic) CGFloat insetWidth;
@property (nonatomic) CGFloat insetHeight;
@property (nonatomic) CGFloat widthFraction;
@property (nonatomic) CGFloat fixedHeight;
@end

NS_ASSUME_NONNULL_END
