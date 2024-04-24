// This guard prevent this file to be compiled in the old architecture.
#ifdef RCT_NEW_ARCH_ENABLED
#import <React/RCTViewComponentView.h>
#import <UIKit/UIKit.h>

#ifndef CraigsListViewNativeComponent_h
#define CraigsListViewNativeComponent_h

NS_ASSUME_NONNULL_BEGIN

@interface CraigsListView : RCTViewComponentView
@end

NS_ASSUME_NONNULL_END

#endif /* CraigsListViewNativeComponent_h */
#endif /* RCT_NEW_ARCH_ENABLED */
