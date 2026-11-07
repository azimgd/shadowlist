#pragma once

#import <TargetConditionals.h>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif

/*
 * The platform's color and image classes, for code both kits compile.
 */
#if TARGET_OS_OSX
typedef NSColor ShadowListKitColor;
typedef NSImage ShadowListKitImage;
#else
typedef UIColor ShadowListKitColor;
typedef UIImage ShadowListKitImage;
#endif
