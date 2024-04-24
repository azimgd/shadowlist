// This guard prevent this file to be compiled in the old architecture.
#ifdef RCT_NEW_ARCH_ENABLED
#import <React/RCTViewComponentView.h>
#import <UIKit/UIKit.h>

#ifndef CraigsListContainerNativeComponent_h
#define CraigsListContainerNativeComponent_h

NS_ASSUME_NONNULL_BEGIN

@interface CraigsListContainer : RCTViewComponentView
@end

NS_ASSUME_NONNULL_END

#endif /* CraigsListContainerNativeComponent_h */
#endif /* RCT_NEW_ARCH_ENABLED */
