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
 * Long-press drag-to-reorder. A long press picks a row up; while held it follows the
 * finger each frame, auto-scrolls near the edges, and shuffles siblings to open a gap.
 * Data order stays FIXED during the drag; a single reorder is applied on drop.
 */
class ShadowlistDragController {
  private final ShadowlistView mView;
  private final GestureDetector mDragGestureDetector;
  private Choreographer.FrameCallback mDragFrameCallback;
  /* Polls for the reorder commit landing after a drop, since a same-size reorder may
   * produce no state commit. */
  private Choreographer.FrameCallback mDropSettleCallback;

  private boolean mDragEnabled = false;
  private boolean mDragging = false;
  private ShadowlistElementView mDraggedView = null;
  /* mDragOriginIndex: where the row was picked up. mDragInsertionIndex: where its
   * centre currently sits (the gap). */
  private int mDragOriginIndex = -1;
  private int mDragInsertionIndex = -1;
  /* Size of the picked-up row along the scroll axis (the gap each sibling opens). */
  private float mDraggedExtent = 0f;
  /* Content-space distance from the picked-up cell's leading edge to the touch point. */
  private float mDragGrabOffset = 0f;
  /* Latest touch position along the scroll axis in viewport space. */
  private float mDragTouchViewport = 0f;
  /* After a drop, hold the shuffle transforms until the reorder commit lands. */
  private boolean mDragDropPending = false;
  private ShadowlistElementView mDroppedView = null;
  private int mDropInsertionIndex = -1;
  /* Content-space leading of the dragged row at the last drag frame; captured on drop
   * (mDropReleaseLeading) to animate from the release point into the resting slot. */
  private float mDragLeading = 0f;
  private float mDropReleaseLeading = 0f;
  private static final long DROP_SETTLE_MS = 180;

  ShadowlistDragController(ShadowlistView view, Context context) {
    mView = view;

    // Long press picks a row up; a quick swipe cancels it so the list still scrolls.
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
   * True while a drag or its drop settle owns the scroll offset; core corrections must
   * not move the content during this window.
   */
  boolean ownsScrollOffset() {
    return mDragging || mDragDropPending;
  }

  /*
   * Feed the long-press detector; returns true once a drag is in flight to steal the
   * gesture from the inner scroll view.
   */
  boolean onInterceptTouchEvent(MotionEvent event) {
    if (mDragEnabled) {
      mDragGestureDetector.onTouchEvent(event);
    }
    return mDragging;
  }

  /* Returns true when the drag consumed the event. */
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
   * Run after a state commit: re-glue the picked-up row to the finger, since new rows
   * may have mounted.
   */
  void onStateCommitted() {
    if (mDragging) {
      updateDrag();
    }
    // The post-drop landing is detected by startDropSettle, not here: a same-size
    // reorder may produce no state commit.
  }

  /*
   * After a drop, poll each frame for the reorder commit to land, then animate the row
   * into place. Self-stops once handled.
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
            // Unmounted off-screen: nothing to settle.
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
   * The reorder has landed. Clear the siblings (already at final positions) instantly
   * and animate the dropped row from its release point into its resting slot.
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
   * Full teardown of any in-flight drag with no reorder, restoring scroll and
   * transforms. Safe to call when not dragging.
   */
  void teardown() {
    teardownDrag();
  }

  /* Topmost element child whose resting bounds contain the content-space point. */
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

    // Cancel any in-flight drop animation from a previous drag and start from a clean
    // resting transform.
    view.animate().cancel();
    view.setTranslationX(0f);
    view.setTranslationY(0f);

    // Clear any leftover drop settle and transforms from a previous drag.
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

    // Lift via Z, NOT bringToFront(): reordering the child array desyncs index-based
    // child mounting and corrupts the view tree.
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
    // Report this as a user scroll (do NOT mark it programmatic) so the core virtualizes
    // at this exact offset; otherwise rows blank mid-drag.
    scrollView.scrollTo(nx, ny);
  }

  /*
   * Glue the picked-up row under the finger, recompute where it would insert, and
   * shuffle the siblings to open the gap. Nothing is sent to JS until drop.
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
   * The index the dragged row would insert at: the farthest neighbour whose midpoint
   * the centre has crossed. Stable midpoints give a half-row dead zone, so jitter
   * cannot flip the insertion.
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
   * Open a one-row gap by translating the siblings between pickup and insertion toward
   * the vacated slot. The shift equals the picked-up row's extent, so each sibling lands
   * exactly on its post-reorder position (making the post-drop clear seamless).
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
    // Keep core scroll corrections off during the drag; cleared on the end event.
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

    // Emit the single reorder; hold the shuffle transforms until the reorder commit
    // lands so nothing snaps back.
    dispatchDragEvent(3, from, to);

    if (from == to || view == null) {
      // No reorder (dropped where it started): settle immediately.
      clearDragTransforms();
      mDragDropPending = false;
      mDroppedView = null;
    } else {
      mDragDropPending = true;
      mDroppedView = view;
      mDropInsertionIndex = to;

      // Poll for the landing rather than waiting for a state commit.
      startDropSettle();

      // Safety net: if the reorder never lands, clear the held transforms so the gap
      // cannot get stuck.
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
