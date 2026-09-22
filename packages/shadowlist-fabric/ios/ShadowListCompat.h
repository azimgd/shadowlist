#pragma once

/*
 * react-native-macos has RCTUIKit.h with the shared RCTUI view and color names.
 * Plain react-native does not, so map those names to their UIKit types here.
 */
#if __has_include(<React/RCTUIKit.h>)
#import <React/RCTUIKit.h>
#else
#import <UIKit/UIKit.h>
#ifndef RCTUIView
#define RCTUIView UIView
#endif
#ifndef RCTUIScrollView
#define RCTUIScrollView UIScrollView
#endif
#ifndef RCTUIScrollViewDelegate
#define RCTUIScrollViewDelegate UIScrollViewDelegate
#endif
#ifndef RCTUIColor
#define RCTUIColor UIColor
#endif
#ifndef RCTPlatformView
#define RCTPlatformView UIView
#endif
#endif
