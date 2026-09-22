package com.shadowlist;

import android.content.Context;
import android.view.Choreographer;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import com.facebook.react.bridge.ReadableMap;
import com.facebook.react.bridge.WritableMap;
import com.facebook.react.bridge.WritableNativeMap;
import com.facebook.react.uimanager.PixelUtil;
import com.facebook.react.uimanager.StateWrapper;

/*
 * Long press drag to reorder. The held row follows the finger, the list scrolls near the
 * edges and the other rows slide to open a gap. The data order only changes once, on drop.
 */
class ShadowListDragController {
  private final ShadowListView mView;
  private final GestureDetector mDragGestureDetector;
  private Choreographer.FrameCallback mDragFrameCallback;
  // Watches for the reorder to land after a drop. A reorder of same size rows may never commit state.
  private Choreographer.FrameCallback mDropSettleCallback;

  private boolean mDragEnabled = false;
  private boolean mDragging = false;
  private ShadowListElementView mDraggedView = null;
  /*
   * Where the row was picked up and where the gap is now. These move the views on screen.
   * JS gets the keys below instead.
   */
  private int mDragOriginIndex = -1;
  private int mDragInsertionIndex = -1;
  // Keys of the held row and the row at the gap, sent to JS so it moves the right item.
  private String mDragOriginKey = "";
  private String mDragInsertionKey = "";
  // Size of the held row along the scroll axis, which is also the size of the gap.
  private float mDraggedExtent = 0f;
  // Distance from the held row's leading edge to the finger, in content space.
  private float mDragGrabOffset = 0f;
  // Latest finger position along the scroll axis, relative to the viewport.
  private float mDragTouchInViewport = 0f;
  // After a drop, keep the rows shifted until the reorder lands.
  private boolean mDragDropPending = false;
  private ShadowListElementView mDroppedView = null;
  private int mDropInsertionIndex = -1;
  /*
   * Leading edge of the held row on the last frame. Saved on drop so the row can animate
   * from where it was let go into its new slot.
   */
  private float mDragLeading = 0f;
  private float mDropReleaseLeading = 0f;
  private static final long DROP_SETTLE_MS = 180;

  ShadowListDragController(ShadowListView view, Context context) {
    mView = view;

    // A long press picks a row up. A quick swipe cancels it so the list still scrolls.
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

  // The held row, or null when nothing is being dragged.
  @Nullable ShadowListElementView getDraggedView() {
    return mDraggedView;
  }

  /*
   * True while a drag or its drop animation owns the scroll offset.
   * Core corrections must not move the content during this time.
   */
  boolean ownsScrollOffset() {
    return mDragging || mDragDropPending;
  }

  /*
   * Feeds the long press detector. Returns true once a drag starts, taking the gesture
   * away from the inner scroll view.
   */
  boolean onInterceptTouchEvent(MotionEvent event) {
    if (mDragEnabled) {
      mDragGestureDetector.onTouchEvent(event);
    }
    return mDragging;
  }

  /*
   * Returns true when the drag consumed the event.
   */
  boolean onTouchEvent(MotionEvent event) {
    if (mDragEnabled) {
      mDragGestureDetector.onTouchEvent(event);
    }
    if (mDragging) {
      switch (event.getActionMasked()) {
        case MotionEvent.ACTION_MOVE:
          mDragTouchInViewport = mView.isHorizontal() ? event.getX() : event.getY();
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
   * New rows may have mounted, so put the held row back under the finger.
   */
  void onStateCommitted() {
    if (mDragging) {
      updateDrag();
    }
    /*
     * The drop landing is caught by startDropSettle instead, since a reorder of same size
     * rows may never commit state.
     */
  }

  /*
   * After a drop, check every frame for the reorder to land, then animate the row into place.
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
            // The row was unmounted off screen, so there is nothing to animate.
            clearDragTransforms();
            mDragDropPending = false;
            mDroppedView = null;
            return;
          }
          if (mDroppedView.getElementIndex() == mDropInsertionIndex) {
            ShadowListElementView view = mDroppedView;
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
   * The reorder has landed. The other rows are already in place, so reset them at once
   * and animate the dropped row from where it was let go.
   */
  private void settleDroppedView(ShadowListElementView view) {
    ViewGroup contentView = mView.getContentView();
    boolean horizontal = mView.isHorizontal();

    for (int i = 0; i < contentView.getChildCount(); i++) {
      View child = contentView.getChildAt(i);
      if (!(child instanceof ShadowListElementView) || child == view) {
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
   * Cancel any drag without reordering and restore scrolling and transforms.
   * Safe to call when nothing is being dragged.
   */
  void teardown() {
    teardownDrag();
  }

  /*
   * The topmost row whose resting frame contains the point.
   */
  private ShadowListElementView elementViewAtContentPoint(float contentX, float contentY) {
    ViewGroup contentView = mView.getContentView();
    ShadowListElementView result = null;
    for (int i = 0; i < contentView.getChildCount(); i++) {
      View child = contentView.getChildAt(i);
      if (!(child instanceof ShadowListElementView)) {
        continue;
      }
      if (contentX >= child.getLeft() && contentX < child.getRight()
          && contentY >= child.getTop() && contentY < child.getBottom()) {
        result = (ShadowListElementView) child;
      }
    }
    return result;
  }

  private void beginDrag(MotionEvent event) {
    ViewGroup scrollView = mView.getScrollView();
    boolean horizontal = mView.isHorizontal();
    float scrollX = scrollView.getScrollX();
    float scrollY = scrollView.getScrollY();
    float contentX = event.getX() + scrollX;
    float contentY = event.getY() + scrollY;

    ShadowListElementView view = elementViewAtContentPoint(contentX, contentY);
    if (view == null) {
      return;
    }
    int index = view.getElementIndex();
    if (index < 0) {
      return;
    }

    // Stop any drop animation left from the previous drag.
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
    mDragOriginKey = view.getElementKey();
    mDragInsertionKey = mDragOriginKey;

    float restingLeading = horizontal ? view.getLeft() : view.getTop();
    mDraggedExtent = horizontal ? view.getWidth() : view.getHeight();
    float touchAxisContent = horizontal ? contentX : contentY;
    mDragGrabOffset = touchAxisContent - restingLeading;
    mDragTouchInViewport = horizontal ? event.getX() : event.getY();

    mView.setInnerScrollEnabled(false);

    /*
     * Lift the row with Z. Don't use bringToFront, it reorders the children and breaks
     * index based mounting.
     */
    view.setTranslationZ(PixelUtil.toPixelFromDIP(8));

    dispatchDragEvent(1, mDragOriginKey, mDragOriginKey);
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
    float touch = mDragTouchInViewport;

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

    int nextX = horizontal ? (int) newOffset : scrollView.getScrollX();
    int nextY = horizontal ? scrollView.getScrollY() : (int) newOffset;
    /*
     * Don't mark this scroll as programmatic. The core must lay out rows at this exact
     * offset or they go blank during the drag.
     */
    scrollView.scrollTo(nextX, nextY);
  }

  /*
   * Keep the held row under the finger, find where it would land and slide the other rows
   * to open a gap. JS hears nothing until the drop.
   */
  private void updateDrag() {
    if (!mDragging || mDraggedView == null) {
      return;
    }

    ViewGroup scrollView = mView.getScrollView();
    ViewGroup contentView = mView.getContentView();
    boolean horizontal = mView.isHorizontal();
    float offset = horizontal ? scrollView.getScrollX() : scrollView.getScrollY();
    float touchContent = mDragTouchInViewport + offset;
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
   * A data change during the drag can shift the held row's index, so read it from the view.
   * Fall back to the index saved at pickup only when the view has none.
   */
  private int currentDragOriginIndex() {
    if (mDraggedView != null) {
      int liveIndex = mDraggedView.getElementIndex();
      if (liveIndex >= 0) {
        return liveIndex;
      }
    }
    return mDragOriginIndex;
  }

  /*
   * The row lands past the farthest neighbour whose midpoint its centre has crossed.
   * Using midpoints leaves half a row of slack, so small jitters don't flip the result.
   */
  private int insertionIndexForCenter(float center) {
    ViewGroup contentView = mView.getContentView();
    boolean horizontal = mView.isHorizontal();
    int originIndex = currentDragOriginIndex();
    int insertion = originIndex;
    /*
     * Remember the key at the landing slot so the drop event names a row, not just an index.
     * If nothing was crossed, the row stays where it started.
     */
    String insertionKey = mDragOriginKey;
    for (int i = 0; i < contentView.getChildCount(); i++) {
      View child = contentView.getChildAt(i);
      if (!(child instanceof ShadowListElementView) || child == mDraggedView) {
        continue;
      }
      ShadowListElementView elementChild = (ShadowListElementView) child;
      int elementIndex = elementChild.getElementIndex();
      if (elementIndex < 0) {
        continue;
      }
      float lead = horizontal ? child.getLeft() : child.getTop();
      float extent = horizontal ? child.getWidth() : child.getHeight();
      float midpoint = lead + extent / 2f;
      if (elementIndex > originIndex && center > midpoint && elementIndex > insertion) {
        insertion = elementIndex;
        insertionKey = elementChild.getElementKey();
      } else if (elementIndex < originIndex && center < midpoint && elementIndex < insertion) {
        insertion = elementIndex;
        insertionKey = elementChild.getElementKey();
      }
    }
    mDragInsertionKey = insertionKey;
    return insertion;
  }

  /*
   * Open a gap by sliding the rows between pickup and landing toward the empty slot.
   * Each moves by the held row's size, which is exactly where it ends up after the reorder.
   */
  void applyDragShuffle() {
    ViewGroup contentView = mView.getContentView();
    boolean horizontal = mView.isHorizontal();
    int originIndex = currentDragOriginIndex();
    for (int i = 0; i < contentView.getChildCount(); i++) {
      View child = contentView.getChildAt(i);
      if (!(child instanceof ShadowListElementView) || child == mDraggedView) {
        continue;
      }
      int elementIndex = ((ShadowListElementView) child).getElementIndex();
      if (elementIndex < 0) {
        continue;
      }
      float shift = 0f;
      if (originIndex < mDragInsertionIndex && elementIndex > originIndex && elementIndex <= mDragInsertionIndex) {
        shift = -mDraggedExtent;
      } else if (mDragInsertionIndex < originIndex && elementIndex >= mDragInsertionIndex && elementIndex < originIndex) {
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

  /*
   * Reset every row's drag offset and lift.
   */
  private void clearDragTransforms() {
    ViewGroup contentView = mView.getContentView();
    for (int i = 0; i < contentView.getChildCount(); i++) {
      View child = contentView.getChildAt(i);
      if (!(child instanceof ShadowListElementView)) {
        continue;
      }
      child.setTranslationX(0f);
      child.setTranslationY(0f);
      child.setTranslationZ(0f);
    }
  }

  private void dispatchDragEvent(int type, String fromKey, String toKey) {
    StateWrapper state = mView.getStateWrapper();
    if (state == null) {
      return;
    }
    double sequence = 1;
    ReadableMap currentStateData = state.getStateData();
    if (currentStateData != null && currentStateData.hasKey("dragEventSequence")) {
      sequence = currentStateData.getDouble("dragEventSequence") + 1;
    }

    WritableMap map = new WritableNativeMap();
    map.putDouble("dragEventSequence", sequence);
    map.putDouble("dragEventType", type);
    map.putString("dragFromKey", fromKey != null ? fromKey : "");
    map.putString("dragToKey", toKey != null ? toKey : "");
    // Keep core scroll corrections off during the drag. The end event turns them back on.
    map.putBoolean("userScrolled", type != 3);
    // Like every host update, carry over the live offset and the last scroll command.
    mView.carryLiveOffset(map);
    mView.carryScrollCommand(map);
    if (ShadowListView.DEBUG_LOG) {
      ShadowListView.slLog("java.drag dispatch type=" + type + " from=" + fromKey + " to=" + toKey);
    }
    state.updateState(map);
  }

  private void finishDrag() {
    if (!mDragging) {
      return;
    }
    stopDragLoop();
    mView.setInnerScrollEnabled(true);

    int from = currentDragOriginIndex();
    int to = mDragInsertionIndex;
    ShadowListElementView view = mDraggedView;
    mDropReleaseLeading = mDragLeading;
    mDragging = false;
    mDraggedView = null;

    // Send the one reorder by key. Keep the rows shifted until it lands so nothing snaps back.
    dispatchDragEvent(3, mDragOriginKey, mDragInsertionKey);

    if (from == to || view == null) {
      // Dropped where it started, so there is nothing to wait for.
      clearDragTransforms();
      mDragDropPending = false;
      mDroppedView = null;
    } else {
      mDragDropPending = true;
      mDroppedView = view;
      mDropInsertionIndex = to;

      // Check each frame for the landing instead of waiting for a state commit.
      startDropSettle();

      // If the reorder never lands, reset the rows so the gap can't get stuck.
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

  /*
   * Stop right away without reordering, used when drag gets disabled.
   */
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
