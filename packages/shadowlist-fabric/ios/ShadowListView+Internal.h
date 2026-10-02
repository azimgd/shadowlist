#import "ShadowListView.h"

#import "ShadowListViewComponentDescriptor.h"
#import <react/renderer/components/ShadowListViewSpec/RCTComponentViewHelpers.h>

#include <shadowlist-core/host/DragReorder.hpp>
#include <shadowlist-core/host/ScrollSync.hpp>
#include <shadowlist-core/host/StickyLayout.hpp>

#include <vector>

/*
 * Raise a subview above its siblings. UIKit reorders the subviews. AppKit cannot.
 * On macOS we set the layer's z position instead. Higher wins and the default is 0.
 */
static inline void SLRaiseSubview(RCTUIView *parent, RCTUIView *child, CGFloat zPosition)
{
#if TARGET_OS_OSX
  child.layer.zPosition = zPosition;
#else
  (void)zPosition;
  [parent bringSubviewToFront:child];
#endif
}

/*
 * State and methods shared by the ShadowListView categories. Objective-C++ only.
 * Pull to refresh, drag to reorder and snap to item have no clean macOS version and are
 * left out there. Their plain state stays on both platforms so shared code needs no guards.
 */
@interface ShadowListView () <RCTShadowListViewViewProtocol, RCTUIScrollViewDelegate> {
@package
  facebook::react::ShadowListViewShadowNode::ConcreteState::Shared _state;
  RCTUIScrollView *_scrollView;
#if !TARGET_OS_OSX
  NSHashTable<UIPanGestureRecognizer *> *_ancestorPans;
#endif
  RCTUIView *_contentView;

  BOOL _refreshEnabled;
  BOOL _refreshing;
  BOOL _refreshAwaitingSettle;
  NSInteger _refreshSettleToken;
#if !TARGET_OS_OSX
  UIRefreshControl *_refreshControl;
  UIColor *_refreshColor;
#endif

  BOOL _stickyHeader;
  BOOL _stickyFooter;
  BOOL _horizontal;
  BOOL _snapToItem;
  std::vector<double> _snapOffsets;
  BOOL _autoHideHeader;
  BOOL _autoHideFooter;
  azimgd::shadowlist::StickyState _stickyState;
  __weak RCTUIView *_stickyHeaderView;
  __weak RCTUIView *_stickyFooterView;

  std::vector<int> _stickyHeaderIndices;
  std::vector<double> _stickyHeaderOffsets;
  std::vector<double> _stickyHeaderSizes;
  __weak RCTUIView *_sectionHeaderOverlay;
  std::shared_ptr<const std::vector<int>> _copiedStickyHeaderIndices;
  std::shared_ptr<const std::vector<facebook::react::Float>> _copiedStickyHeaderOffsets;
  std::shared_ptr<const std::vector<facebook::react::Float>> _copiedStickyHeaderSizes;
  std::shared_ptr<const std::vector<facebook::react::Float>> _copiedSnapOffsets;

  azimgd::shadowlist::ScrollSync _scrollSync;
  BOOL _inStateUpdate;
  BOOL _inMountObserver;

  BOOL _stickyOrderDirty;
  BOOL _overlayOrderDirty;
  BOOL _mountNeedsSticky;
  BOOL _mountNeedsDragShuffle;

#if !TARGET_OS_OSX
  CADisplayLink *_scrollToTopLink;
#endif
  BOOL _scrollingToTop;
  CFTimeInterval _scrollToTopStartTime;
  CGFloat _scrollToTopProgress;
  BOOL _scrollToTopJumpPending;
  CGFloat _scrollToTopJumpY;
  uint64_t _scrollToTopJumpToken;

  BOOL _dragEnabled;
#if !TARGET_OS_OSX
  UILongPressGestureRecognizer *_dragRecognizer;
  CADisplayLink *_dragDisplayLink;
  __weak UIView *_draggedView;
  __weak UIView *_droppedView;
  CADisplayLink *_dropSettleLink;
#endif
  BOOL _dragging;
  azimgd::shadowlist::DragReorder _drag;
  CGPoint _dragTouchInViewport;
  BOOL _dragDropPending;
  NSInteger _dropInsertionIndex;
  CGFloat _dropReleaseLeading;
  CGFloat _dropReleaseCross;
  NSInteger _columns;
  NSInteger _dropSettleToken;
}

- (NSInteger)indexOfElementView:(RCTUIView *)view;

- (NSString *)keyOfElementView:(RCTUIView *)view;

- (void)applyStickyTransforms:(BOOL)accumulate;

- (void)commitDragEventType:(int)type fromKey:(NSString *)fromKey toKey:(NSString *)toKey;

#if !TARGET_OS_OSX
- (void)handleDragGesture:(UILongPressGestureRecognizer *)gesture;
- (void)updateDrag;
- (void)applyDragShuffle;
- (void)clearDragTransforms;
- (void)teardownDrag;
- (void)settleDroppedView:(UIView *)view;

- (void)applyDragAccessibilityActionsToView:(UIView *)view;
- (BOOL)performAccessibilityMove:(UIView *)view up:(BOOL)up;
#endif

@end
