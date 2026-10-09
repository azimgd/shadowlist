#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

/*
 * The section index along a list's trailing edge. Touching or sliding over a title calls
 * onSelect with the title's position.
 */
@interface ShadowListKitSectionIndexView : UIView

@property (nonatomic, copy) NSArray<NSString *> *titles;
@property (nonatomic, copy, nullable) void (^onSelect)(NSInteger index);

@end

NS_ASSUME_NONNULL_END
