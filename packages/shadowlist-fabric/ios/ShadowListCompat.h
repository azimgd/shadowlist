#pragma once

/*
 * react-native-macos has RCTUIKit.h with the shared RCTUI view and color names.
 * Plain react-native does not. Map those names to their UIKit types here.
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

#if TARGET_OS_OSX
#define SLDragGestureRecognizer NSPanGestureRecognizer
#define SLDisplayLink RCTPlatformDisplayLink
#define SLAccessibilityCustomAction NSAccessibilityCustomAction
#define SLGestureStateBegan NSGestureRecognizerStateBegan
#define SLGestureStateChanged NSGestureRecognizerStateChanged
#define SLGestureStateEnded NSGestureRecognizerStateEnded
#define SLGestureStateCancelled NSGestureRecognizerStateCancelled
#define SLGestureStateFailed NSGestureRecognizerStateFailed
#else
#define SLDragGestureRecognizer UILongPressGestureRecognizer
#define SLDisplayLink CADisplayLink
#define SLAccessibilityCustomAction UIAccessibilityCustomAction
#define SLGestureStateBegan UIGestureRecognizerStateBegan
#define SLGestureStateChanged UIGestureRecognizerStateChanged
#define SLGestureStateEnded UIGestureRecognizerStateEnded
#define SLGestureStateCancelled UIGestureRecognizerStateCancelled
#define SLGestureStateFailed UIGestureRecognizerStateFailed
#endif
