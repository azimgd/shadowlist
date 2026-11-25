#pragma once

#import <ShadowListKit/ShadowListKitListView.h>

#include <shadowlist-core/host/DragBoard.hpp>

/*
 * The shared core owns the flight. UIKit owns weak list membership and the temporary picture.
 */
@interface ShadowListKitDragBoard : NSObject {
@public
  azimgd::shadowlist::DragBoard core;
  azimgd::shadowlist::DragBoardScroll scroll;
}
@property (nonatomic, strong) NSHashTable<ShadowListKitListView *> *lists;
@property (nonatomic, weak) ShadowListKitListView *home;
@property (nonatomic, weak) ShadowListKitListView *target;
@property (nonatomic, strong) UIView *overlay;
@property (nonatomic, weak) ShadowListKitListCell *landingCell;
@property (nonatomic) CGPoint finger;
@property (nonatomic, weak) ShadowListKitListView *sidewaysScrollView;
@end
