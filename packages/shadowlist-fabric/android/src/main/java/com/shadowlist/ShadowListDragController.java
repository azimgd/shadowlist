package com.shadowlist;

import android.os.Build;
import android.os.SystemClock;
import android.view.Choreographer;
import android.view.HapticFeedbackConstants;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;

import androidx.annotation.Nullable;

import com.facebook.react.bridge.ReadableMap;
import com.facebook.react.uimanager.PixelUtil;
import com.facebook.react.uimanager.StateWrapper;

import java.util.Arrays;

/*
 * Long press drag to reorder. The held row follows the finger, the list scrolls near the
 * edges and the other rows slide to open a gap. The data order only changes once, on drop.
 * The drag math, the haptic decisions and the wait after a drop run in the core's drag
 * session, see ShadowListDrag, the same one the iOS lists run.
 */
final class ShadowListDragController {
  /*
   * Match DRAG_EVENT_* in shadowlist-core/host/DragReorder.hpp.
   */
  private static final int DRAG_EVENT_START = 1;
  static final int DRAG_EVENT_END = 3;

  private final ShadowListView mView;
  private final ShadowListDrag mDrag = new ShadowListDrag();
  @Nullable private Choreographer.FrameCallback mDragFrameCallback = null;
  /*
   * Watches for the reorder to land after a drop. A reorder of same size rows may never commit state.
   */
  @Nullable private Choreographer.FrameCallback mDropSettleCallback = null;

  private boolean mReorderEnabled = false;
  private boolean mDragging = false;
  @Nullable private ShadowListCellView mDraggedView = null;

  /*
   * The press that picks a row up: where the finger went down and where it is now, in this
   * view. The row lifts after ShadowListDrag.PRESS_DURATION_MS unless the finger moved past
   * the core's slop first, which is tighter along the scroll axis so a slow scroll stays one.
   */
  private boolean mPressPending = false;
  private float mPressDownX = 0f;
  private float mPressDownY = 0f;
  private float mPressX = 0f;
  private float mPressY = 0f;
  private final Runnable mPressLift = () -> {
    mPressPending = false;
    if (mReorderEnabled && !mDragging) {
      beginDrag(mPressX, mPressY);
    }
  };

  /*
   * Latest finger position along and across the scroll axis, relative to the viewport.
   */
  private float mDragTouchInViewport = 0f;
  private float mDragCrossTouchInViewport = 0f;
  /*
   * The previous drag frame's time, 0 before the first frame. The edge scroll speed is per second.
   */
  private long mLastFrameNanos = 0L;
  /*
   * After a drop, keep the rows shifted until the reorder lands.
   */
  private boolean mDragDropPending = false;
  @Nullable private ShadowListCellView mDroppedView = null;
  /*
   * Where the held row was let go. It animates from there into its new slot.
   */
  private float mDropReleaseLeading = 0f;
  private float mDropReleaseCrossLeading = 0f;
  private int mNumberOfColumns = 1;

  /*
   * The other mounted rows for the drag math, refilled each frame. The arrays only grow.
   * mRowViews holds the view at each position.
   */
  private int mRowCount = 0;
  private int[] mRowIndices = new int[0];
  private String[] mRowKeys = new String[0];
  private double[] mRowFrames = new double[0];
  private ShadowListCellView[] mRowViews = new ShadowListCellView[0];

  ShadowListDragController(ShadowListView view) {
    mView = view;
  }

  void setEnabled(boolean reorderEnabled) {
    mReorderEnabled = reorderEnabled;
    if (!reorderEnabled) {
      cancelPress();
      if (mDragging) {
        cancel();
      }
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
   * Sees every event of the gesture, even once the scroll view stops passing them to
   * onInterceptTouchEvent, and runs the press that picks a row up.
   */
  void trackGesture(MotionEvent event) {
    if (!mReorderEnabled || mDragging) {
      return;
    }
    switch (event.getActionMasked()) {
      case MotionEvent.ACTION_DOWN:
        mPressDownX = event.getX();
        mPressDownY = event.getY();
        mPressX = mPressDownX;
        mPressY = mPressDownY;
        mPressPending = true;
        mView.removeCallbacks(mPressLift);
        mView.postDelayed(mPressLift, ShadowListDrag.PRESS_DURATION_MS);
        break;
      case MotionEvent.ACTION_MOVE:
        if (!mPressPending) {
          break;
        }
        mPressX = event.getX();
        mPressY = event.getY();
        float pixelsPerDp = PixelUtil.toPixelFromDIP(1f);
        float movedX = (mPressX - mPressDownX) / pixelsPerDp;
        float movedY = (mPressY - mPressDownY) / pixelsPerDp;
        boolean horizontal = mView.isHorizontal();
        if (!ShadowListDrag.pressHolds(horizontal ? movedX : movedY, horizontal ? movedY : movedX)) {
          cancelPress();
        }
        break;
      case MotionEvent.ACTION_POINTER_DOWN:
      case MotionEvent.ACTION_UP:
      case MotionEvent.ACTION_CANCEL:
        cancelPress();
        break;
      default:
        break;
    }
  }

  private void cancelPress() {
    mPressPending = false;
    mView.removeCallbacks(mPressLift);
  }

  /*
   * Returns true once a drag starts, taking the gesture away from the inner scroll view.
   */
  boolean onInterceptTouchEvent(MotionEvent event) {
    return mDragging;
  }

  /*
   * Returns true when the drag consumed the event. A cancelled gesture puts the row back,
   * like a cancelled press on iOS. Only a finger lifted drops it.
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
          finishDrag();
          return true;
        case MotionEvent.ACTION_CANCEL:
          cancel();
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
     * The drop landing is caught by the drop settle frames instead, since a reorder of same
     * size rows may never commit state.
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

  private void beginDrag(float x, float y) {
    ViewGroup scrollView = mView.getScrollView();
    boolean horizontal = mView.isHorizontal();
    float contentX = x + scrollView.getScrollX();
    float contentY = y + scrollView.getScrollY();

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
    mNumberOfColumns = mView.getNumberOfColumns();
    mDrag.begin(index, view.getRowKey(),
      horizontal ? view.getLeft() : view.getTop(),
      horizontal ? view.getWidth() : view.getHeight(),
      horizontal ? view.getTop() : view.getLeft(),
      horizontal ? view.getHeight() : view.getWidth(),
      horizontal ? contentX : contentY,
      horizontal ? contentY : contentX,
      mNumberOfColumns);
    mDragTouchInViewport = horizontal ? x : y;
    mDragCrossTouchInViewport = horizontal ? y : x;
    mLastFrameNanos = 0L;

    mView.setInnerScrollEnabled(false);
    // An outer scroll view, such as a board's horizontal one, must not take the held row's
    // gesture. The request travels up every ancestor and Android clears it when the gesture ends.
    setAncestorsMayIntercept(false);

    view.invalidateOutline();
    view.setTranslationZ(PixelUtil.toPixelFromDIP((float) ShadowListDrag.LIFT_ELEVATION_DP));
    mView.performHapticFeedback(Build.VERSION.SDK_INT >= Build.VERSION_CODES.R
      ? HapticFeedbackConstants.GESTURE_START
      : HapticFeedbackConstants.LONG_PRESS);

    String originKey = mDrag.getOriginKey();
    dispatchDragEvent(DRAG_EVENT_START, originKey, originKey);
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
          double dtSeconds = mLastFrameNanos == 0L ? 1.0 / 60.0 : (frameTimeNanos - mLastFrameNanos) / 1e9;
          mLastFrameNanos = frameTimeNanos;
          applyDragAutoScroll(dtSeconds);
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

  private void applyDragAutoScroll(double dtSeconds) {
    ViewGroup scrollView = mView.getScrollView();
    ViewGroup contentView = mView.getContentView();
    boolean horizontal = mView.isHorizontal();
    float window = horizontal ? scrollView.getWidth() : scrollView.getHeight();
    float content = horizontal ? contentView.getWidth() : contentView.getHeight();
    float maxOffset = Math.max(0f, content - window);
    float offset = horizontal ? scrollView.getScrollX() : scrollView.getScrollY();

    float newOffset = (float) ShadowListDrag.autoScrollOffset(
      mDragTouchInViewport, window, offset, maxOffset, PixelUtil.toPixelFromDIP(1f), dtSeconds);
    if (Math.round(newOffset) == Math.round(offset)) {
      return;
    }

    int nextX = horizontal ? Math.round(newOffset) : scrollView.getScrollX();
    int nextY = horizontal ? scrollView.getScrollY() : Math.round(newOffset);
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
    ShadowListCellView view = mDraggedView;
    if (!mDragging || view == null) {
      return;
    }

    // A data change during the drag can move the held row. The drop names it by key.
    mDrag.updateOrigin(view.getRowIndex(), view.getRowKey());

    ViewGroup scrollView = mView.getScrollView();
    ViewGroup contentView = mView.getContentView();
    boolean horizontal = mView.isHorizontal();
    float touchAlong = mDragTouchInViewport + (horizontal ? scrollView.getScrollX() : scrollView.getScrollY());
    float touchCross = mDragCrossTouchInViewport + (horizontal ? scrollView.getScrollY() : scrollView.getScrollX());
    double[] translation = mDrag.placeRow(touchAlong, touchCross,
      horizontal ? view.getLeft() : view.getTop(),
      horizontal ? view.getWidth() : view.getHeight(),
      horizontal ? view.getTop() : view.getLeft(),
      horizontal ? view.getHeight() : view.getWidth(),
      horizontal ? contentView.getWidth() : contentView.getHeight(),
      horizontal ? contentView.getHeight() : contentView.getWidth());
    float along = (float) translation[0];
    float cross = (float) translation[1];
    view.setTranslationX(horizontal ? along : cross);
    view.setTranslationY(horizontal ? cross : along);

    collectRows();
    mDrag.updateInsertion(mRowIndices, mRowKeys, mRowFrames, mRowCount);
    if (mDrag.shouldTick(SystemClock.uptimeMillis())) {
      mView.performHapticFeedback(Build.VERSION.SDK_INT >= Build.VERSION_CODES.R
        ? HapticFeedbackConstants.SEGMENT_TICK
        : HapticFeedbackConstants.CLOCK_TICK);
    }
    shuffleCollectedRows();
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
      mRowKeys = new String[childCount];
      mRowFrames = new double[childCount * ShadowListDrag.ROW_SLOTS];
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
      int frame = count * ShadowListDrag.ROW_SLOTS;
      mRowIndices[count] = rowIndex;
      mRowKeys[count] = cellChild.getRowKey();
      mRowFrames[frame + ShadowListDrag.ROW_LEADING] = horizontal ? child.getLeft() : child.getTop();
      mRowFrames[frame + ShadowListDrag.ROW_EXTENT] = horizontal ? child.getWidth() : child.getHeight();
      mRowFrames[frame + ShadowListDrag.ROW_CROSS_LEADING] = horizontal ? child.getTop() : child.getLeft();
      mRowFrames[frame + ShadowListDrag.ROW_CROSS_EXTENT] = horizontal ? child.getHeight() : child.getWidth();
      mRowViews[count] = cellChild;
      count++;
    }
    // Drop views and keys left from a longer frame so they can be freed.
    for (int i = count; i < mRowCount; i++) {
      mRowViews[i] = null;
      mRowKeys[i] = null;
    }
    mRowCount = count;
  }

  /*
   * A row mounted during the drag. Work out the shifts again with it and slide every row.
   */
  void applyDragShuffle() {
    if (!mDragging) {
      return;
    }
    collectRows();
    mDrag.updateInsertion(mRowIndices, mRowKeys, mRowFrames, mRowCount);
    shuffleCollectedRows();
  }

  /*
   * Open a gap by sliding the rows between pickup and landing toward the empty slot. Each
   * moves to exactly where it ends up after the reorder. In a grid a row can move to another
   * column.
   */
  private void shuffleCollectedRows() {
    boolean horizontal = mView.isHorizontal();
    for (int i = 0; i < mRowCount; i++) {
      View child = mRowViews[i];
      double[] offset = mDrag.offsetFor(mRowIndices[i]);
      float shift = (float) offset[0];
      float crossShift = (float) offset[1];
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
      child.animate().cancel();
      child.setTranslationX(0f);
      child.setTranslationY(0f);
      child.setTranslationZ(0f);
    }
  }

  /*
   * A cancel puts every row back the way a drop settles: the held row flies home over the
   * drop's duration and the others slide back like they shifted.
   */
  private void slideDragTransformsHome(@Nullable View held) {
    ViewGroup contentView = mView.getContentView();
    for (int i = 0; i < contentView.getChildCount(); i++) {
      View child = contentView.getChildAt(i);
      if (!(child instanceof ShadowListCellView)) {
        continue;
      }
      child.animate().cancel();
      if (child == held) {
        child.animate().translationX(0f).translationY(0f).setDuration(ShadowListDrag.DROP_DURATION_MS)
          .withEndAction(() -> child.setTranslationZ(0f)).start();
      } else {
        child.setTranslationZ(0f);
        if (child.getTranslationX() != 0f || child.getTranslationY() != 0f) {
          child.animate().translationX(0f).translationY(0f).setDuration(ShadowListDrag.SHIFT_DURATION_MS).start();
        }
      }
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

    int from = mDrag.getOriginIndex();
    int to = mDrag.getInsertionIndex();
    ShadowListCellView view = mDraggedView;
    mDropReleaseLeading = (float) mDrag.getLeading();
    mDropReleaseCrossLeading = (float) mDrag.getCrossLeading();
    mDragging = false;
    mDraggedView = null;

    // Send the one reorder by key. Keep the rows shifted until it lands so nothing snaps back.
    dispatchDragEvent(DRAG_EVENT_END, mDrag.getOriginKey(), mDrag.getInsertionKey());

    if (from == to || view == null) {
      // Dropped where it started. There is nothing to wait for. The row settles from where it was let go.
      mDragDropPending = false;
      mDroppedView = null;
      if (view != null) {
        settleDroppedView(view);
      } else {
        clearDragTransforms();
      }
      return;
    }
    mDragDropPending = true;
    mDroppedView = view;
    // Check each frame for the landing instead of waiting for a state commit.
    mDrag.beginSettle(to, SystemClock.uptimeMillis());
    startDropSettle();
  }

  /*
   * After a drop, check every frame for the reorder to land, then animate the row into place.
   * The core's DropSettle gives up after DRAG_DROP_SETTLE_TIMEOUT_MS, or once the row unmounted.
   */
  private void startDropSettle() {
    if (mDropSettleCallback == null) {
      mDropSettleCallback = new Choreographer.FrameCallback() {
        @Override
        public void doFrame(long frameTimeNanos) {
          if (!mDragDropPending) {
            return;
          }
          ShadowListCellView view = mDroppedView;
          int current = view == null || view.getParent() == null ? -1 : view.getRowIndex();
          int status = mDrag.tickSettle(current, SystemClock.uptimeMillis());
          if (status == ShadowListDrag.SETTLE_WAITING) {
            Choreographer.getInstance().postFrameCallback(this);
            return;
          }
          mDragDropPending = false;
          mDroppedView = null;
          if (status == ShadowListDrag.SETTLE_SETTLE && view != null) {
            settleDroppedView(view);
          } else if (!mDragging) {
            // The reorder never landed, or the row unmounted. Put every row back at once.
            clearDragTransforms();
          }
        }
      };
    }
    Choreographer.getInstance().postFrameCallback(mDropSettleCallback);
  }

  private void stopDropSettle() {
    if (mDropSettleCallback != null) {
      Choreographer.getInstance().removeFrameCallback(mDropSettleCallback);
    }
    mDrag.resetSettle();
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
      child.animate().cancel();
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
    view.animate().translationX(0f).translationY(0f).setDuration(ShadowListDrag.DROP_DURATION_MS)
      .withEndAction(() -> view.setTranslationZ(0f)).start();
  }

  /*
   * Stop right away without reordering. With animated the rows slide back to their places,
   * otherwise they snap there.
   */
  private void tearDownDrag(boolean animated) {
    cancelPress();
    stopDragLoop();
    stopDropSettle();
    if (mDroppedView != null) {
      mDroppedView.animate().cancel();
    }
    // A drag stopped mid-gesture hands the rest of that gesture back to the outer scroll views.
    if (mDragging) {
      setAncestorsMayIntercept(true);
    }
    ShadowListCellView held = mDraggedView;
    mDragging = false;
    mDraggedView = null;
    mDragDropPending = false;
    mDroppedView = null;
    releaseCollectedRows();
    mView.setInnerScrollEnabled(true);
    if (animated) {
      slideDragTransformsHome(held);
    } else {
      clearDragTransforms();
    }
  }

  /*
   * Allows or stops every ancestor of the list from intercepting the current gesture.
   */
  private void setAncestorsMayIntercept(boolean mayIntercept) {
    ViewParent parent = mView.getParent();
    if (parent != null) {
      parent.requestDisallowInterceptTouchEvent(!mayIntercept);
    }
  }

  /*
   * Let go of the rows the latest drag frame collected. A dropped list must not keep them alive.
   */
  private void releaseCollectedRows() {
    Arrays.fill(mRowViews, 0, mRowCount, null);
    Arrays.fill(mRowKeys, 0, mRowCount, null);
    mRowCount = 0;
  }

  /*
   * Cancel any drag without reordering and restore scrolling and transforms at once.
   * Safe to call when nothing is being dragged. Used when Fabric drops or recycles the view.
   */
  void teardown() {
    tearDownDrag(false);
    mDrag.destroy();
  }

  /*
   * Like teardown, but a running drag also sends its end event, with the held row's key on
   * both sides. JS then clears the held key without a reorder, and the core turns its scroll
   * corrections back on. The start event turned them off and only the end event clears that.
   * The rows slide back while the list is on screen.
   */
  void cancel() {
    boolean wasDragging = mDragging;
    if (mDragging) {
      String originKey = mDrag.getOriginKey();
      dispatchDragEvent(DRAG_EVENT_END, originKey, originKey);
    }
    tearDownDrag(wasDragging && mView.isAttachedToWindow());
  }

  // endregion

  // region Accessibility

  /*
   * For TalkBack, swap the row with the nearest mounted row above or below. It goes through
   * the same path as a real drop, like VoiceOver's Move up and Move down on iOS. onDragEnd and
   * useDragReorder handle it as usual.
   */
  boolean performAccessibilityMove(ShadowListCellView view, boolean up) {
    int index = view.getRowIndex();
    String key = view.getRowKey();
    if (!mReorderEnabled || mDragging || index < 0 || key.isEmpty()) {
      return false;
    }
    ViewGroup contentView = mView.getContentView();
    ShadowListCellView neighbor = null;
    int neighborIndex = up ? Integer.MIN_VALUE : Integer.MAX_VALUE;
    for (int i = 0; i < contentView.getChildCount(); i++) {
      View child = contentView.getChildAt(i);
      if (!(child instanceof ShadowListCellView) || child == view) {
        continue;
      }
      int childIndex = ((ShadowListCellView) child).getRowIndex();
      if (childIndex < 0) {
        continue;
      }
      if (up ? (childIndex < index && childIndex > neighborIndex) : (childIndex > index && childIndex < neighborIndex)) {
        neighborIndex = childIndex;
        neighbor = (ShadowListCellView) child;
      }
    }
    if (neighbor == null || neighbor.getRowKey().isEmpty()) {
      return false;
    }
    dispatchDragEvent(DRAG_EVENT_END, key, neighbor.getRowKey());
    return true;
  }

  // endregion

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
}
