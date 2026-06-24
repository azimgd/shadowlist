#pragma once

// react-native-macos ships <React/RCTUIKit.h>, which defines the cross-platform "special
// classes" (RCTUIView / RCTUIScrollView / RCTUIColor / RCTPlatformView and, on macOS, the
// RCTUIScrollViewDelegate protocol). Upstream (plain) react-native has no such header, so on
// iOS-only react-native we fall back to UIKit and alias the same names to their UIKit types.
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
