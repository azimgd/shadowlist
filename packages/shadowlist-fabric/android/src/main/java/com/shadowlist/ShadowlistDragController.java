package com.shadowlist;

import android.content.Context;
import android.view.Choreographer;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;

import com.facebook.react.bridge.ReadableMap;
import com.facebook.react.bridge.WritableMap;
import com.facebook.react.bridge.WritableNativeMap;
import com.facebook.react.uimanager.PixelUtil;
import com.facebook.react.uimanager.StateWrapper;

/*
 * Native long-press drag-to-reorder, split out of ShadowlistView. The whole gesture
 * runs on the UI thread (like the sticky pin), so it never waits on the commit cycle.
 * A long press picks an element up; while held we translate it under the finger every
 * Choreographer frame, auto-scroll near the viewport edges, and detect when its centre
 * crosses into a neighbour's slot. Each crossing is relayed to JS through state (a live
 * array move) and the surviving rows flow to their new slots via the normal layout; the
 * picked-up view re-glues to the finger against its new resting position. The data
 * order stays FIXED during the drag and a single reorder is applied on drop; JS only
 * sees onDragStart and onDragEnd (there is no mid-drag event).
 *
 * The host view owns the scroll/content views and the Fabric state; this controller
 * reaches them through the ShadowlistView accessors and drives the gesture entry points
 * the view forwards (onInterceptTouchEvent / onTouchEvent / mount / state commit).
 */
class ShadowlistDragController {
  private final ShadowlistView mView;
  private final GestureDetector mDragGestureDetector;
  private Choreographer.FrameCallback mDragFrameCallback;
  /* Polls for the reorder commit landing after a drop, so the settle fires even when
   * the reorder produces no host state commit (a same-size reorder that does not move
   * the viewport top), which onStateCommitted alone would miss. */
  private Choreographer.FrameCallback mDropSettleCallback;

  private boolean mDragEnabled = false;
  private boolean mDragging = false;
  private ShadowlistElementView mDraggedView = null;
  /* The data order stays FIXED during the drag. mDragOriginIndex is where the row was
   * picked up; mDragInsertionIndex is where its centre currently sits (the gap), used
   * to shuffle the siblings between them. The single reorder is applied on drop. */
  private int mDragOriginIndex = -1;
  private int mDragInsertionIndex = -1;
  /* Size of the picked-up row along the scroll axis - the amount each shuffled sibling
   * is offset to open the gap. */
  private float mDraggedExtent = 0f;
  /* Distance along the scroll axis (content space) from the picked-up cell's leading
   * edge to the touch point, so the cell stays under the same spot of the finger. */
  private float mDragGrabOffset = 0f;
  /* Latest touch position along the scroll axis in viewport space - stable while the
   * content auto-scrolls under a still finger. */
  private float mDragTouchViewport = 0f;
  /* After a drop, hold the shuffle transforms until JS's single reorder commit lands
   * (the dropped row's index prop reaching mDropInsertionIndex), then clear them. */
  private boolean mDragDropPending = false;
  private ShadowlistElementView mDroppedView = null;
  private int mDropInsertionIndex = -1;
  /* Content-space leading of the dragged row at the last drag frame; captured on drop
   * (mDropReleaseLeading) so the row can animate from where the finger released it into
   * its post-reorder resting slot rather than snapping. */
  private float mDragLeading = 0f;
  private float mDropReleaseLeading = 0f;
  private static final long DROP_SETTLE_MS = 180;

  ShadowlistDragController(ShadowlistView view, Context context) {
    mView = view;

    // Long press picks a row up for drag-to-reorder. A quick swipe moves past the
    // touch slop and cancels the long press, so it still scrolls.
    mDragGestureDetector = new GestureDetector(context, new GestureDetector.SimpleOnGestureListener() {
      @Override
      public void onLongPress(MotionEvent event) {
        if (mDragEnabled && !mDragging) {
          beginDrag(event);
        }
      }
    });
    mDragGestureDetector.setIsLongpressEnabled(true);
  }

  void setEnabled(boolean dragEnabled) {
    mDragEnabled = dragEnabled;
    if (!dragEnabled && mDragging) {
      teardownDrag();
    }
  }

  boolean isDragging() {
    return mDragging;
  }

  /*
   * While a drag is in flight (or settling after a drop) the drag owns the scroll
   * offset, so a core correction must not yank the content under the finger or jump the
   * list as the reorder lands.
   */
  boolean ownsScrollOffset() {
    return mDragging || mDragDropPending;
  }

  /*
   * Feed the long-press detector and, once a drag is in flight, report that the host
   * should steal the gesture from the inner scroll view (the view returns true from
   * onInterceptTouchEvent; Android delivers ACTION_CANCEL to the scroll view).
   */
  boolean onInterceptTouchEvent(MotionEvent event) {
    if (mDragEnabled) {
      mDragGestureDetector.onTouchEvent(event);
    }
    return mDragging;
  }

  /* Returns true when the drag consumed the event (the view skips super.onTouchEvent). */
  boolean onTouchEvent(MotionEvent event) {
    if (mDragEnabled) {
      mDragGestureDetector.onTouchEvent(event);
    }
    if (mDragging) {
      switch (event.getActionMasked()) {
        case MotionEvent.ACTION_MOVE:
          mDragTouchViewport = mView.isHorizontal() ? event.getX() : event.getY();
          updateDrag();
          return true;
        case MotionEvent.ACTION_UP:
        case MotionEvent.ACTION_CANCEL:
          finishDrag();
          return true;
        default:
          return true;
      }
    }
    return false;
  }

  /*
   * Run after a state commit: re-glue the picked-up row to the finger (an auto-scroll
   * re-virtualization may have mounted new rows) and, once JS's reorder commit lands
   * (the dropped row's index prop reaching its dropped slot), clear the held shuffle.
   */
  void onStateCommitted() {
    if (mDragging) {
      updateDrag();
    }
    // The post-drop landing is detected by the Choreographer poll (startDropSettle),
    // NOT here: a same-size reorder that does not move the viewport top produces no host
    // state commit, so this path would miss it and the row would snap via the net.
  }

  /*
   * After a drop, poll each frame for the reorder commit to land (the dropped row's
   * index prop reaching its slot) and then animate it into place; if the row unmounted
   * off-screen there is nothing to settle. Self-stops once handled (or via the 300ms
   * safety net / a new pickup).
   */
  private void startDropSettle() {
    if (mDropSettleCallback == null) {
      mDropSettleCallback = new Choreographer.FrameCallback() {
        @Override
        public void doFrame(long frameTimeNanos) {
          if (!mDragDropPending || mDroppedView == null) {
            return;
          }
          if (mDroppedView.getParent() == null) {
            // Unmounted off-screen: nothing to settle visually.
            clearDragTransforms();
            mDragDropPending = false;
            mDroppedView = null;
            return;
          }
          if (mDroppedView.getElementIndex() == mDropInsertionIndex) {
            ShadowlistElementView view = mDroppedView;
            mDragDropPending = false;
            mDroppedView = null;
            settleDroppedView(view);
            return;
          }
          Choreographer.getInstance().postFrameCallback(this);
        }
      };
    }
    Choreographer.getInstance().postFrameCallback(mDropSettleCallback);
  }

  private void stopDropSettle() {
    if (mDropSettleCallback != null) {
      Choreographer.getInstance().removeFrameCallback(mDropSettleCallback);
    }
  }

  /*
   * The reorder has landed and the dropped row now sits at its post-reorder slot
   * (getLeft/getTop). The siblings already rest at their final positions, so clear them
   * instantly; only the dropped row needs to travel from where the finger released it
   * (mDropReleaseLeading) into the slot - animate that so the drop settles smoothly
   * instead of snapping.
   */
  private void settleDroppedView(ShadowlistElementView view) {
    ViewGroup contentView = mView.getContentView();
    boolean horizontal = mView.isHorizontal();

    for (int i = 0; i < contentView.getChildCount(); i++) {
      View child = contentView.getChildAt(i);
      if (!(child instanceof ShadowlistElementView) || child == view) {
        continue;
      }
      child.setTranslationX(0f);
      child.setTranslationY(0f);
      child.setTranslationZ(0f);
    }

    float newResting = horizontal ? view.getLeft() : view.getTop();
    float startTranslation = mDropReleaseLeading - newResting;

    view.animate().cancel();
    if (horizontal) {
      view.setTranslationX(startTranslation);
      view.setTranslationY(0f);
      view.animate().translationX(0f).setDuration(DROP_SETTLE_MS)
        .withEndAction(() -> view.setTranslationZ(0f)).start();
    } else {
      view.setTranslationY(startTranslation);
      view.setTranslationX(0f);
      view.animate().translationY(0f).setDuration(DROP_SETTLE_MS)
        .withEndAction(() -> view.setTranslationZ(0f)).start();
    }
  }

  /*
   * Full teardown of any in-flight drag with no reorder. Called when the host view is
   * dropped/recycled (ShadowlistViewManager.onDropViewInstance) so the self-reposting
   * Choreographer callback cannot keep driving a detached view, and the inner scroll /
   * transforms are restored. Safe to call when not dragging.
   */
  void teardown() {
    teardownDrag();
  }

  /* Topmost element child whose resting bounds contain the content-space point. The
   * translation a drag applies does not affect getLeft/getTop, so they give the
   * resting position even while a row is being dragged. */
  private ShadowlistElementView elementViewAtContent(float cx, float cy) {
    ViewGroup contentView = mView.getContentView();
    ShadowlistElementView result = null;
    for (int i = 0; i < contentView.getChildCount(); i++) {
      View child = contentView.getChildAt(i);
      if (!(child instanceof ShadowlistElementView)) {
        continue;
      }
      if (cx >= child.getLeft() && cx < child.getRight()
          && cy >= child.getTop() && cy < child.getBottom()) {
        result = (ShadowlistElementView) child;
      }
    }
    return result;
  }

  private void beginDrag(MotionEvent event) {
    ViewGroup scrollView = mView.getScrollView();
    boolean horizontal = mView.isHorizontal();
    float scrollX = scrollView.getScrollX();
    float scrollY = scrollView.getScrollY();
    float cx = event.getX() + scrollX;
    float cy = event.getY() + scrollY;

    ShadowlistElementView view = elementViewAtContent(cx, cy);
    if (view == null) {
      return;
    }
    int index = view.getElementIndex();
    if (index < 0) {
      return;
    }

    // Cancel any in-flight drop animation from a previous drag of this same view, so
    // its withEndAction (which resets translationZ) cannot fire mid-drag, and start
    // from a clean resting transform.
    view.animate().cancel();
    view.setTranslationX(0f);
    view.setTranslationY(0f);

    // Clean slate: stop any in-flight drop settle and drop leftover post-drop
    // transforms from a previous drag.
    stopDropSettle();
    mDragDropPending = false;
    mDroppedView = null;
    clearDragTransforms();

    mDragging = true;
    mDraggedView = view;
    mDragOriginIndex = index;
    mDragInsertionIndex = index;

    float restingLeading = horizontal ? view.getLeft() : view.getTop();
    mDraggedExtent = horizontal ? view.getWidth() : view.getHeight();
    float touchAxisContent = horizontal ? cx : cy;
    mDragGrabOffset = touchAxisContent - restingLeading;
    mDragTouchViewport = horizontal ? event.getX() : event.getY();

    mView.setInnerScrollEnabled(false);

    // Lift feedback: raise the row above its siblings via Z (draw order), NOT
    // bringToFront(). bringToFront() reorders the child array, which desyncs Fabric's
    // index-based child mounting (removeContentViewAt) and corrupts the view tree -
    // breaking consecutive drags and scrambling row content. Android draws children
    // ordered by Z since API 21, so a higher translationZ lifts it without reordering.
    view.setTranslationZ(PixelUtil.toPixelFromDIP(8));

    dispatchDragEvent(1, index, index);
    startDragLoop();
    updateDrag();
  }

  private void startDragLoop() {
    if (mDragFrameCallback == null) {
      mDragFrameCallback = new Choreographer.FrameCallback() {
        @Override
        public void doFrame(long frameTimeNanos) {
          if (!mDragging) {
            return;
          }
          applyDragAutoScroll();
          updateDrag();
          Choreographer.getInstance().postFrameCallback(this);
        }
      };
    }
    Choreographer.getInstance().postFrameCallback(mDragFrameCallback);
  }

  private void stopDragLoop() {
    if (mDragFrameCallback != null) {
      Choreographer.getInstance().removeFrameCallback(mDragFrameCallback);
    }
  }

  private void applyDragAutoScroll() {
    ViewGroup scrollView = mView.getScrollView();
    ViewGroup contentView = mView.getContentView();
    boolean horizontal = mView.isHorizontal();
    float window = horizontal ? scrollView.getWidth() : scrollView.getHeight();
    float content = horizontal ? contentView.getWidth() : contentView.getHeight();
    float maxOffset = Math.max(0f, content - window);
    float offset = horizontal ? scrollView.getScrollX() : scrollView.getScrollY();
    float touch = mDragTouchViewport;

    float edge = PixelUtil.toPixelFromDIP(60);
    float maxSpeed = PixelUtil.toPixelFromDIP(12);
    float delta = 0f;
    if (touch < edge) {
      delta = -maxSpeed * (1f - touch / edge);
    } else if (touch > window - edge) {
      delta = maxSpeed * (1f - (window - touch) / edge);
    }
    if (delta == 0f) {
      return;
    }

    float newOffset = Math.min(Math.max(offset + delta, 0f), maxOffset);
    if (newOffset == offset) {
      return;
    }

    int nx = horizontal ? (int) newOffset : scrollView.getScrollX();
    int ny = horizontal ? scrollView.getScrollY() : (int) newOffset;
    // Let the resulting onScrollChanged report this as a user scroll (do NOT mark it
    // programmatic): the drag owns the scroll position, so the core must virtualize at
    // this exact offset and abandon any maintain-visible-position correction. Marking
    // it programmatic makes the core keep its correction and compute the visible window
    // at a different offset than the viewport, which blanks rows mid-drag.
    scrollView.scrollTo(nx, ny);
  }

  /*
   * Glue the picked-up row under the finger, recompute where it would insert, and
   * shuffle the siblings to open the gap. Nothing is sent to JS - the data order is
   * fixed until drop - so this never triggers a re-render.
   */
  private void updateDrag() {
    if (!mDragging || mDraggedView == null) {
      return;
    }

    ViewGroup scrollView = mView.getScrollView();
    ViewGroup contentView = mView.getContentView();
    boolean horizontal = mView.isHorizontal();
    float offset = horizontal ? scrollView.getScrollX() : scrollView.getScrollY();
    float touchContent = mDragTouchViewport + offset;
    float restingLeading = horizontal ? mDraggedView.getLeft() : mDraggedView.getTop();
    float extent = horizontal ? mDraggedView.getWidth() : mDraggedView.getHeight();
    float contentExtent = horizontal ? contentView.getWidth() : contentView.getHeight();

    float desiredLeading = touchContent - mDragGrabOffset;
    desiredLeading = Math.max(0f, Math.min(desiredLeading, Math.max(0f, contentExtent - extent)));
    mDragLeading = desiredLeading;

    float translation = desiredLeading - restingLeading;
    if (horizontal) {
      mDraggedView.setTranslationX(translation);
      mDraggedView.setTranslationY(0f);
    } else {
      mDraggedView.setTranslationY(translation);
      mDraggedView.setTranslationX(0f);
    }

    mDragInsertionIndex = insertionIndexForCenter(desiredLeading + extent / 2f);
    applyDragShuffle();
  }

  /*
   * The index the dragged row would insert at, by comparing its centre against each
   * neighbour's MIDPOINT relative to the (fixed) pickup index. Because the data order
   * never changes mid-drag the midpoints are stable, giving a clean half-row dead zone
   * around each one - boundary jitter cannot flip the insertion back and forth. Returns
   * the farthest neighbour whose midpoint the centre has crossed.
   */
  private int insertionIndexForCenter(float center) {
    ViewGroup contentView = mView.getContentView();
    boolean horizontal = mView.isHorizontal();
    int insertion = mDragOriginIndex;
    for (int i = 0; i < contentView.getChildCount(); i++) {
      View child = contentView.getChildAt(i);
      if (!(child instanceof ShadowlistElementView) || child == mDraggedView) {
        continue;
      }
      int idx = ((ShadowlistElementView) child).getElementIndex();
      if (idx < 0) {
        continue;
      }
      float lead = horizontal ? child.getLeft() : child.getTop();
      float extent = horizontal ? child.getWidth() : child.getHeight();
      float mid = lead + extent / 2f;
      if (idx > mDragOriginIndex && center > mid) {
        insertion = Math.max(insertion, idx);
      } else if (idx < mDragOriginIndex && center < mid) {
        insertion = Math.min(insertion, idx);
      }
    }
    return insertion;
  }

  /*
   * Open a one-row gap at the insertion point by translating the siblings between the
   * pickup and the insertion toward the vacated pickup slot. The shift equals the
   * picked-up row's extent, so each shuffled sibling lands exactly on its post-reorder
   * resting position - which is why clearing the transforms after the drop commit is
   * seamless (the siblings do not move, only the dragged row settles into the gap).
   */
  void applyDragShuffle() {
    ViewGroup contentView = mView.getContentView();
    boolean horizontal = mView.isHorizontal();
    for (int i = 0; i < contentView.getChildCount(); i++) {
      View child = contentView.getChildAt(i);
      if (!(child instanceof ShadowlistElementView) || child == mDraggedView) {
        continue;
      }
      int idx = ((ShadowlistElementView) child).getElementIndex();
      if (idx < 0) {
        continue;
      }
      float shift = 0f;
      if (mDragOriginIndex < mDragInsertionIndex && idx > mDragOriginIndex && idx <= mDragInsertionIndex) {
        shift = -mDraggedExtent;
      } else if (mDragInsertionIndex < mDragOriginIndex && idx >= mDragInsertionIndex && idx < mDragOriginIndex) {
        shift = mDraggedExtent;
      }
      if (horizontal) {
        child.setTranslationX(shift);
        child.setTranslationY(0f);
      } else {
        child.setTranslationY(shift);
        child.setTranslationX(0f);
      }
    }
  }

  /* Reset every element view's drag transform and lift. */
  private void clearDragTransforms() {
    ViewGroup contentView = mView.getContentView();
    for (int i = 0; i < contentView.getChildCount(); i++) {
      View child = contentView.getChildAt(i);
      if (!(child instanceof ShadowlistElementView)) {
        continue;
      }
      child.setTranslationX(0f);
      child.setTranslationY(0f);
      child.setTranslationZ(0f);
    }
  }

  private void dispatchDragEvent(int type, int from, int to) {
    StateWrapper state = mView.getStateWrapper();
    if (state == null) {
      return;
    }
    double nonce = 1;
    ReadableMap currentStateData = state.getStateData();
    if (currentStateData != null && currentStateData.hasKey("dragEventNonce")) {
      nonce = currentStateData.getDouble("dragEventNonce") + 1;
    }

    WritableMap map = new WritableNativeMap();
    map.putDouble("dragEventNonce", nonce);
    map.putDouble("dragEventType", type);
    map.putDouble("dragFromIndex", from);
    map.putDouble("dragToIndex", to);
    // Keep the core's scroll corrections off for the duration of the drag (the drag
    // owns the offset); a reorder commit must virtualize at the live offset, not snap
    // to an anchor. Cleared on the end event so normal scrolling resumes afterwards.
    map.putBoolean("userScrolled", type != 3);
    ShadowlistView.slLog("java.drag dispatch type=" + type + " from=" + from + " to=" + to);
    state.updateState(map);
  }

  private void finishDrag() {
    if (!mDragging) {
      return;
    }
    stopDragLoop();
    mView.setInnerScrollEnabled(true);

    int from = mDragOriginIndex;
    int to = mDragInsertionIndex;
    ShadowlistElementView view = mDraggedView;
    mDropReleaseLeading = mDragLeading;
    mDragging = false;
    mDraggedView = null;

    // Emit the single reorder. Hold the shuffle transforms (the rows already sit at
    // their post-reorder positions) until JS's reorder commit lands, then
    // clearDragTransforms runs from updateState - so nothing snaps back in between.
    dispatchDragEvent(3, from, to);

    if (from == to || view == null) {
      // No reorder (dropped where it started): no commit will move the index, so settle
      // immediately instead of waiting for a landing that never changes anything.
      clearDragTransforms();
      mDragDropPending = false;
      mDroppedView = null;
    } else {
      mDragDropPending = true;
      mDroppedView = view;
      mDropInsertionIndex = to;

      // Poll for the landing rather than waiting for a host state commit (which a
      // same-size reorder may never produce).
      startDropSettle();

      // Safety net: if the reorder never lands (e.g. a consumer that ignores
      // onReorder), clear the held transforms anyway so the gap cannot get stuck. A
      // new drag clears at pickup.
      mView.postDelayed(() -> {
        if (mDragDropPending && !mDragging) {
          stopDropSettle();
          clearDragTransforms();
          mDragDropPending = false;
          mDroppedView = null;
        }
      }, 300);
    }
  }

  /* Immediate teardown with no reorder (drag disabled). */
  private void teardownDrag() {
    stopDragLoop();
    stopDropSettle();
    if (mDroppedView != null) {
      mDroppedView.animate().cancel();
    }
    mDragging = false;
    mDraggedView = null;
    mDragDropPending = false;
    mDroppedView = null;
    mView.setInnerScrollEnabled(true);
    clearDragTransforms();
  }
}
