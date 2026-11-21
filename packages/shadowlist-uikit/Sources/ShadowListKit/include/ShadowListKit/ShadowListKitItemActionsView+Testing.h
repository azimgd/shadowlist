#pragma once

#import <ShadowListKit/ShadowListKitItemActionsView.h>

/* Drives the installed swipe controller without synthesizing a touch or waiting for a hold. */
@interface ShadowListKitItemActionsView (Testing)
- (void)shadowListKit_swipeDistance:(CGFloat)distance velocity:(CGFloat)velocity;
@end
