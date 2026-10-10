package com.shadowlist;

import android.content.Context;
import android.view.Choreographer;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import com.facebook.react.bridge.ReadableMap;
import com.facebook.react.uimanager.PixelUtil;
import com.facebook.react.uimanager.StateWrapper;

import java.util.Arrays;

/*
 * Long press drag to reorder. The held row follows the finger, the list scrolls near the
 * edges and the other rows slide to open a gap. The data order only changes once, on drop.
 */
final class ShadowListDragController {
  /*
   * Match DRAG_EVENT_* in shadowlist-core/host/DragReorder.hpp.
   */
  private static final int DRAG_EVENT_START = 1;
  static final int DRAG_EVENT_END = 3;
  private static final long DROP_SETTLE_MS = 180;
  /*
   * If the reorder never lands, reset the rows after this long so the gap can't get stuck.
   */
  private static final long DROP_FALLBACK_MS = 300;
  /*
   * The held row is lifted with Z. bringToFront would reorder the children and break index
   * based mounting.
   */
  private static final float LIFT_ELEVATION_DP = 8f;

  private final ShadowListView mView;
  private final GestureDetector mDragGestureDetector;
  @Nullable private Choreographer.FrameCallback mDragFrameCallback = null;
  /*
   * Watches for the reorder to land after a drop. A reorder of same size rows may never commit state.
   */
  @Nullable private Choreographer.FrameCallback mDropSettleCallback = null;

  private boolean mReorderEnabled = false;
  private boolean mDragging = false;
  @Nullable private ShadowListCellView mDraggedView = null;
  /*
   * Where the row was picked up and where the gap is now. These move the views on screen.
   * JS gets the keys below instead.
   */
  private int mDragOriginIndex = -1;
  private int mDragInsertionIndex = -1;
  /*
   * Keys of the held row and the row at the gap. JS moves the item by key.
   */
  private String mDragOriginKey = "";
  private String mDragInsertionKey = "";
  /*
   * Size of the held row along the scroll axis, which is also the size of the gap.
   */
  private float mDraggedExtent = 0f;
  /*
   * Distance from the held row's leading edge to the finger, in content space.
   */
  private float mDragGrabOffset = 0f;
  /*
   * Latest finger position along the scroll axis, relative to the viewport.
   */
  private float mDragTouchInViewport = 0f;
  /*
   * After a drop, keep the rows shifted until the reorder lands.
   */
  private boolean mDragDropPending = false;
  @Nullable private ShadowListCellView mDroppedView = null;
  private int mDropInsertionIndex = -1;
  /*
   * Leading edge of the held row on the latest frame. Saved on drop so the row can animate
   * from where it was let go into its new slot.
   */
  private float mDragLeading = 0f;
  private float mDropReleaseLeading = 0f;
  /*
   * The cross axis values of the ones above, for grid drags across columns.
   */
  private int mNumberOfColumns = 1;
  private float mDragCrossGrabOffset = 0f;
  private float mDragCrossTouchInViewport = 0f;
  private float mDragCrossLeading = 0f;
  private float mDropReleaseCrossLeading = 0f;
  /*
   * One instance, removed before each post. A fallback left from an earlier drop must not cut
   * a later drop's settle short.
   */
  private final Runnable mDropFallback = () -> {
    if (mDragDropPending && !mDragging) {
      stopDropSettle();
      clearDragTransforms();
      mDragDropPending = false;
      mDroppedView = null;
    }
  };

  /*
   * The other mounted rows for the drag math, refilled each frame. The arrays only grow.
   * Frames allocate nothing. mRowViews holds the view at each position.
   */
  private int mRowCount = 0;
  private int[] mRowIndices = new int[0];
  private double[] mRowLeadings = new double[0];
  private double[] mRowExtents = new double[0];
  private double[] mRowShifts = new double[0];
  private double[] mRowCrossLeadings = new double[0];
  private double[] mRowCrossExtents = new double[0];
  private double[] mRowCrossShifts = new double[0];
  private ShadowListCellView[] mRowViews = new ShadowListCellView[0];

  ShadowListDragController(ShadowListView view, Context context) {
    mView = view;

    // A long press picks a row up. A quick swipe cancels it so the list still scrolls.
    mDragGestureDetector = new GestureDetector(context, new GestureDetector.SimpleOnGestureListener() {
      @Override
      public void onLongPress(MotionEvent event) {
        if (mReorderEnabled && !mDragging) {
          beginDrag(event);
        }
      }
    });
    mDragGestureDetector.setIsLongpressEnabled(true);
  }

  void setEnabled(boolean reorderEnabled) {
    mReorderEnabled = reorderEnabled;
    if (!reorderEnabled && mDragging) {
      cancel();
    }
  }

  boolean isDragging() {
    return mDragging;
  }

  boolean isEnabled() {
    return mReorderEnabled;
  }

  /*
   * The held row, or null when nothing is being dragged.
   */
  @Nullable ShadowListCellView getDraggedView() {
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
   * Feeds the long press detector every event of the gesture, even once the scroll view stops
   * passing them to onInterceptTouchEvent.
   */
  void trackGesture(MotionEvent event) {
    if (mReorderEnabled) {
      mDragGestureDetector.onTouchEvent(event);
    }
  }

  /*
   * Returns true once a drag starts, taking the gesture away from the inner scroll view.
   */
  boolean onInterceptTouchEvent(MotionEvent event) {
    return mDragging;
  }

  /*
   * Returns true when the drag consumed the event.
   */
  boolean onTouchEvent(MotionEvent event) {
    if (mDragging) {
      switch (event.getActionMasked()) {
        case MotionEvent.ACTION_MOVE:
          mDragTouchInViewport = mView.isHorizontal() ? event.getX() : event.getY();
          mDragCrossTouchInViewport = mView.isHorizontal() ? event.getY() : event.getX();
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
   * New rows may have mounted. Put the held row back under the finger.
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

  // region Pick up

  /*
   * The topmost row whose resting frame contains the point.
   */
  @Nullable ShadowListCellView cellViewAtContentPoint(float contentX, float contentY) {
    ViewGroup contentView = mView.getContentView();
    ShadowListCellView result = null;
    for (int i = 0; i < contentView.getChildCount(); i++) {
      View child = contentView.getChildAt(i);
      if (!(child instanceof ShadowListCellView)) {
        continue;
      }
      if (contentX >= child.getLeft() && contentX < child.getRight()
          && contentY >= child.getTop() && contentY < child.getBottom()) {
        result = (ShadowListCellView) child;
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

    ShadowListCellView view = cellViewAtContentPoint(contentX, contentY);
    if (view == null) {
      return;
    }
    int index = view.getRowIndex();
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
    mDragOriginKey = view.getRowKey();
    mDragInsertionKey = mDragOriginKey;

    float restingLeading = horizontal ? view.getLeft() : view.getTop();
    mDraggedExtent = horizontal ? view.getWidth() : view.getHeight();
    float touchAxisContent = horizontal ? contentX : contentY;
    mDragGrabOffset = touchAxisContent - restingLeading;
    mDragTouchInViewport = horizontal ? event.getX() : event.getY();
    mNumberOfColumns = mView.getNumberOfColumns();
    float restingCross = horizontal ? view.getTop() : view.getLeft();
    mDragCrossGrabOffset = (horizontal ? contentY : contentX) - restingCross;
    mDragCrossTouchInViewport = horizontal ? event.getY() : event.getX();
    mDragCrossLeading = restingCross;

    mView.setInnerScrollEnabled(false);

    view.setTranslationZ(PixelUtil.toPixelFromDIP(LIFT_ELEVATION_DP));

    dispatchDragEvent(DRAG_EVENT_START, mDragOriginKey, mDragOriginKey);
    startDragLoop();
    updateDrag();
  }

  // endregion

  // region Follow

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

    float newOffset = (float) ShadowListGeometry.dragAutoScrollOffset(
      touch, window, offset, maxOffset, PixelUtil.toPixelFromDIP(1f));
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

    float desiredLeading = (float) ShadowListGeometry.dragHeldLeading(touchContent, mDragGrabOffset, extent, contentExtent);
    mDragLeading = desiredLeading;

    float translation = desiredLeading - restingLeading;
    float restingCross = horizontal ? mDraggedView.getTop() : mDraggedView.getLeft();
    float crossExtent = horizontal ? mDraggedView.getHeight() : mDraggedView.getWidth();
    float crossTranslation = 0f;
    if (mNumberOfColumns > 1) {
      float crossOffset = horizontal ? scrollView.getScrollY() : scrollView.getScrollX();
      float crossContentExtent = horizontal ? contentView.getHeight() : contentView.getWidth();
      mDragCrossLeading = (float) ShadowListGeometry.dragHeldLeading(
        mDragCrossTouchInViewport + crossOffset, mDragCrossGrabOffset, crossExtent, crossContentExtent);
      crossTranslation = mDragCrossLeading - restingCross;
    }
    mDraggedView.setTranslationX(horizontal ? translation : crossTranslation);
    mDraggedView.setTranslationY(horizontal ? crossTranslation : translation);

    // The held row's key can change with the data too, and the drop names it by key.
    String liveKey = mDraggedView.getRowKey();
    if (liveKey != null && !liveKey.isEmpty()) {
      mDragOriginKey = liveKey;
    }
    collectRows();
    int position;
    if (mNumberOfColumns > 1) {
      position = ShadowListGeometry.dragGridInsertionPosition(
        mRowIndices, mRowLeadings, mRowExtents, mRowCrossLeadings, mRowCrossExtents, mRowCount,
        currentDragOriginIndex(), restingLeading, extent, restingCross, crossExtent,
        mDragInsertionIndex, desiredLeading + extent / 2f, mDragCrossLeading + crossExtent / 2f);
    } else {
      position = ShadowListGeometry.dragInsertionPosition(
        mRowIndices, mRowLeadings, mRowExtents, mRowCount, currentDragOriginIndex(), desiredLeading + extent / 2f);
    }
    if (position < 0) {
      mDragInsertionIndex = currentDragOriginIndex();
      mDragInsertionKey = mDragOriginKey;
    } else {
      mDragInsertionIndex = mRowIndices[position];
      mDragInsertionKey = mRowViews[position].getRowKey();
    }
    shuffleCollectedRows();
  }

  /*
   * A data change during the drag can shift the held row's index. Read it from the view.
   * Fall back to the index saved at pickup only when the view has none.
   */
  private int currentDragOriginIndex() {
    if (mDraggedView != null) {
      int liveIndex = mDraggedView.getRowIndex();
      if (liveIndex >= 0) {
        return liveIndex;
      }
    }
    return mDragOriginIndex;
  }

  /*
   * Fill the row arrays with every mounted row but the held one, at its resting place.
   */
  private void collectRows() {
    ViewGroup contentView = mView.getContentView();
    boolean horizontal = mView.isHorizontal();
    int childCount = contentView.getChildCount();
    if (mRowIndices.length < childCount) {
      mRowIndices = new int[childCount];
      mRowLeadings = new double[childCount];
      mRowExtents = new double[childCount];
      mRowShifts = new double[childCount];
      mRowCrossLeadings = new double[childCount];
      mRowCrossExtents = new double[childCount];
      mRowCrossShifts = new double[childCount];
      mRowViews = new ShadowListCellView[childCount];
    }
    int count = 0;
    for (int i = 0; i < childCount; i++) {
      View child = contentView.getChildAt(i);
      if (!(child instanceof ShadowListCellView) || child == mDraggedView) {
        continue;
      }
      ShadowListCellView cellChild = (ShadowListCellView) child;
      int rowIndex = cellChild.getRowIndex();
      if (rowIndex < 0) {
        continue;
      }
      mRowIndices[count] = rowIndex;
      mRowLeadings[count] = horizontal ? child.getLeft() : child.getTop();
      mRowExtents[count] = horizontal ? child.getWidth() : child.getHeight();
      mRowCrossLeadings[count] = horizontal ? child.getTop() : child.getLeft();
      mRowCrossExtents[count] = horizontal ? child.getHeight() : child.getWidth();
      mRowViews[count] = cellChild;
      count++;
    }
    // Drop views left from a longer frame so they can be freed.
    for (int i = count; i < mRowCount; i++) {
      mRowViews[i] = null;
    }
    mRowCount = count;
  }

  /*
   * Open a gap by sliding the rows between pickup and landing toward the empty slot.
   * Each moves by the held row's size, which is exactly where it ends up after the reorder.
   * In a grid a row can move to another column.
   */
  void applyDragShuffle() {
    collectRows();
    shuffleCollectedRows();
  }

  private void shuffleCollectedRows() {
    if (mRowCount == 0) {
      return;
    }
    boolean horizontal = mView.isHorizontal();
    if (mNumberOfColumns > 1 && mDraggedView != null) {
      ShadowListCellView held = mDraggedView;
      ShadowListGeometry.dragGridShifts(
        mRowIndices, mRowLeadings, mRowExtents, mRowCrossLeadings, mRowCrossExtents, mRowCount,
        currentDragOriginIndex(),
        horizontal ? held.getLeft() : held.getTop(),
        horizontal ? held.getWidth() : held.getHeight(),
        horizontal ? held.getTop() : held.getLeft(),
        horizontal ? held.getHeight() : held.getWidth(),
        mDragInsertionIndex, mNumberOfColumns, mRowShifts, mRowCrossShifts);
    } else {
      ShadowListGeometry.dragShifts(
        mRowIndices, mRowCount, currentDragOriginIndex(), mDragInsertionIndex, mDraggedExtent, mRowShifts);
      for (int i = 0; i < mRowCount; i++) {
        mRowCrossShifts[i] = 0.0;
      }
    }
    for (int i = 0; i < mRowCount; i++) {
      View child = mRowViews[i];
      float shift = (float) mRowShifts[i];
      float crossShift = (float) mRowCrossShifts[i];
      child.setTranslationX(horizontal ? shift : crossShift);
      child.setTranslationY(horizontal ? crossShift : shift);
    }
  }

  /*
   * Reset every row's drag offset and lift.
   */
  private void clearDragTransforms() {
    ViewGroup contentView = mView.getContentView();
    for (int i = 0; i < contentView.getChildCount(); i++) {
      View child = contentView.getChildAt(i);
      if (!(child instanceof ShadowListCellView)) {
        continue;
      }
      child.setTranslationX(0f);
      child.setTranslationY(0f);
      child.setTranslationZ(0f);
    }
  }

  // endregion

  // region Drop

  private void finishDrag() {
    if (!mDragging) {
      return;
    }
    stopDragLoop();
    mView.setInnerScrollEnabled(true);

    int from = currentDragOriginIndex();
    int to = mDragInsertionIndex;
    ShadowListCellView view = mDraggedView;
    mDropReleaseLeading = mDragLeading;
    mDropReleaseCrossLeading = mDragCrossLeading;
    mDragging = false;
    mDraggedView = null;

    // Send the one reorder by key. Keep the rows shifted until it lands so nothing snaps back.
    dispatchDragEvent(DRAG_EVENT_END, mDragOriginKey, mDragInsertionKey);

    if (from == to || view == null) {
      // Dropped where it started. There is nothing to wait for.
      clearDragTransforms();
      mDragDropPending = false;
      mDroppedView = null;
    } else {
      mDragDropPending = true;
      mDroppedView = view;
      mDropInsertionIndex = to;

      // Check each frame for the landing instead of waiting for a state commit.
      startDropSettle();

      mView.removeCallbacks(mDropFallback);
      mView.postDelayed(mDropFallback, DROP_FALLBACK_MS);
    }
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
            // The row was unmounted off screen. There is nothing to animate.
            clearDragTransforms();
            mDragDropPending = false;
            mDroppedView = null;
            return;
          }
          if (mDroppedView.getRowIndex() == mDropInsertionIndex) {
            ShadowListCellView view = mDroppedView;
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
   * The reorder has landed. The other rows are already in place. Reset them at once
   * and animate the dropped row from where it was let go.
   */
  private void settleDroppedView(ShadowListCellView view) {
    ViewGroup contentView = mView.getContentView();
    boolean horizontal = mView.isHorizontal();

    for (int i = 0; i < contentView.getChildCount(); i++) {
      View child = contentView.getChildAt(i);
      if (!(child instanceof ShadowListCellView) || child == view) {
        continue;
      }
      child.setTranslationX(0f);
      child.setTranslationY(0f);
      child.setTranslationZ(0f);
    }

    float newResting = horizontal ? view.getLeft() : view.getTop();
    float startTranslation = mDropReleaseLeading - newResting;
    float startCross = 0f;
    if (mNumberOfColumns > 1) {
      startCross = mDropReleaseCrossLeading - (horizontal ? view.getTop() : view.getLeft());
    }

    view.animate().cancel();
    view.setTranslationX(horizontal ? startTranslation : startCross);
    view.setTranslationY(horizontal ? startCross : startTranslation);
    view.animate().translationX(0f).translationY(0f).setDuration(DROP_SETTLE_MS)
      .withEndAction(() -> view.setTranslationZ(0f)).start();
  }

  /*
   * Stop right away without reordering, used when drag gets disabled.
   */
  private void tearDownDrag() {
    stopDragLoop();
    stopDropSettle();
    mView.removeCallbacks(mDropFallback);
    if (mDroppedView != null) {
      mDroppedView.animate().cancel();
    }
    mDragging = false;
    mDraggedView = null;
    mDragDropPending = false;
    mDroppedView = null;
    releaseCollectedRows();
    mView.setInnerScrollEnabled(true);
    clearDragTransforms();
  }

  /*
   * Let go of the rows the latest drag frame collected. A dropped list must not keep them alive.
   */
  private void releaseCollectedRows() {
    Arrays.fill(mRowViews, 0, mRowCount, null);
    mRowCount = 0;
  }

  /*
   * Cancel any drag without reordering and restore scrolling and transforms.
   * Safe to call when nothing is being dragged.
   */
  void teardown() {
    tearDownDrag();
  }

  /*
   * Like teardown, but a running drag also sends its end event, with the held row's key on
   * both sides. JS then clears the held key without a reorder, and the core turns its scroll
   * corrections back on. The start event turned them off and only the end event clears that.
   */
  void cancel() {
    if (mDragging) {
      dispatchDragEvent(DRAG_EVENT_END, mDragOriginKey, mDragOriginKey);
    }
    tearDownDrag();
  }

  private void dispatchDragEvent(int type, String sourceKey, String destinationKey) {
    StateWrapper state = mView.getStateWrapper();
    if (state == null) {
      return;
    }
    double sequence = 1;
    ReadableMap currentStateData = state.getStateData();
    if (currentStateData != null && currentStateData.hasKey("dragEventSequence")) {
      sequence = currentStateData.getDouble("dragEventSequence") + 1;
    }
    // Like every host update, the event carries the live offset and the latest scroll command.
    mView.dispatchDragEvent(type, sourceKey, destinationKey, sequence);
  }

  // endregion
}
