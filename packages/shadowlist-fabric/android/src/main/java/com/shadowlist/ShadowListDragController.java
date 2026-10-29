package com.shadowlist;

import android.os.Build;
import android.os.SystemClock;
import android.view.Choreographer;
import android.view.HapticFeedbackConstants;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.widget.HorizontalScrollView;

import androidx.annotation.Nullable;

import com.facebook.react.bridge.ReadableMap;
import com.facebook.react.uimanager.PixelUtil;
import com.facebook.react.uimanager.StateWrapper;
import com.facebook.react.views.scroll.ReactHorizontalScrollView;

import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.WeakHashMap;

/*
 * Long press drag to reorder. The held row follows the finger, the list scrolls near the
 * edges and the other rows slide to open a gap. The data order only changes once, on drop.
 * The drag math, the haptic decisions and the wait after a drop run in the core's drag
 * session, see ShadowListDrag, the same one the iOS lists run. On a drag board the row can
 * also leave for another list of the board, see ShadowListDragBoard and the Drag board regions.
 */
final class ShadowListDragController {
  /*
   * Match DRAG_EVENT_* in shadowlist-core/host/DragReorder.hpp.
   */
  private static final int DRAG_EVENT_START = 1;
  static final int DRAG_EVENT_END = 3;
  static final int DRAG_EVENT_LAND = 4;

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

  /*
   * The drag board this list is on, null while dragBoardId is empty. mBoardToken names this
   * list for the board's core, unique on the board where dragListId may not be.
   * mBoardHiddenView is the row the board hid while its picture floats above the window.
   * mBoardDropPending holds the gap a row left in its home list until the data change that
   * removes it lands. mBoardLandingKey is the key of the row dropped in this list, until it
   * mounts here. mBoardAnimateShifts makes the next shuffle slide, when the row came home.
   * mForeign and mHeldAway mirror the core session's foreign and held away states.
   */
  @Nullable private ShadowListDragBoard mBoard = null;
  private String mDragBoardId = "";
  private String mDragListId = "";
  private String mDragListLabel = "";
  private String mBoardToken = "";
  @Nullable private ShadowListCellView mBoardHiddenView = null;
  private boolean mBoardDropPending = false;
  @Nullable private String mBoardLandingKey = null;
  private boolean mBoardAnimateShifts = false;
  private boolean mForeign = false;
  private boolean mHeldAway = false;
  /*
   * The finger in window pixels. A drag board scrolls the view around this list, which moves
   * the viewport touch under a finger that holds still. Each drag frame on a board reads it
   * again from the window point. mSidewaysCarry keeps the part of a pixel the board's sideways
   * scroll has not moved yet.
   */
  private float mFingerWindowX = 0f;
  private float mFingerWindowY = 0f;
  private double mSidewaysCarry = 0.0;
  /*
   * Where each row's running slide on a drag board ends. A slide starts again only for a new
   * target.
   */
  private final WeakHashMap<View, float[]> mShiftTargets = new WeakHashMap<>();
  private final int[] mLocation = new int[2];

  /*
   * React Native's horizontal scroll view keeps pagingEnabled private. The board reads it to
   * snap a paged board after a drag. Looked up once, null when it is not there.
   */
  @Nullable private static Field sPagingField = null;
  private static boolean sPagingFieldLooked = false;

  ShadowListDragController(ShadowListView view) {
    mView = view;
  }

  void setEnabled(boolean reorderEnabled) {
    boolean wasEnabled = mReorderEnabled;
    mReorderEnabled = reorderEnabled;
    if (!reorderEnabled) {
      cancelPress();
      if (mDragging) {
        cancel();
      }
      // A list that turns drag off stops being a drag board's target, or the landing list.
      if (wasEnabled) {
        withdrawFromBoardFlight();
      }
    }
    if (wasEnabled != reorderEnabled && mBoard != null) {
      mBoard.listsChanged();
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
   * True while a drag or its drop animation owns the scroll offset, or while a row from another
   * list of the drag board hovers this one or a drop between lists is landing.
   * Core corrections must not move the content during this time.
   */
  boolean ownsScrollOffset() {
    return mDragging || mDragDropPending || mForeign || mBoardDropPending;
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
          noteFingerInWindow(event.getX(), event.getY());
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
   * New rows may have mounted. Put the held row back under the finger. On a drag board a
   * drop between lists may have landed.
   */
  void onStateCommitted() {
    if (mDragging) {
      updateDrag();
    }
    /*
     * The drop landing is caught by the drop settle frames instead, since a reorder of same
     * size rows may never commit state.
     */
    boardMountDidFinish(false);
  }

  /*
   * The row keys changed. A drop between the lists of a drag board lands with them.
   */
  void onKeysChanged() {
    boardMountDidFinish(false);
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
    int index = mView.rowIndexOf(view);
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
    // A new drag ends any foreign session the core held for the board.
    mForeign = false;
    mHeldAway = false;
    mDragTouchInViewport = horizontal ? x : y;
    mDragCrossTouchInViewport = horizontal ? y : x;
    noteFingerInWindow(x, y);
    mLastFrameNanos = 0L;

    // A drop still landing on this list's drag board ends before the next pickup.
    boardDidPickUp(view);

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

  /*
   * The finger at a point in this view, kept in window pixels.
   */
  private void noteFingerInWindow(float x, float y) {
    mView.getLocationInWindow(mLocation);
    mFingerWindowX = mLocation[0] + x;
    mFingerWindowY = mLocation[1] + y;
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
          refreshDragTouchFromWindow();
          // A row floating over another list of the drag board scrolls that list instead.
          if (!applyDragBoardAutoScroll(dtSeconds)) {
            applyDragAutoScroll(mDragTouchInViewport, dtSeconds);
          }
          // The board's sideways scroll just moved this list under the finger.
          refreshDragTouchFromWindow();
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

  /*
   * On a drag board the view around this list may have scrolled under a finger that held still.
   * Find the finger in this view again from where it is in the window.
   */
  private void refreshDragTouchFromWindow() {
    if (mBoard == null || !mView.isAttachedToWindow()) {
      return;
    }
    mView.getLocationInWindow(mLocation);
    float x = mFingerWindowX - mLocation[0];
    float y = mFingerWindowY - mLocation[1];
    boolean horizontal = mView.isHorizontal();
    mDragTouchInViewport = horizontal ? x : y;
    mDragCrossTouchInViewport = horizontal ? y : x;
  }

  /*
   * Scroll near the edges for a finger at touch along the scroll axis in this view.
   */
  void applyDragAutoScroll(float touch, double dtSeconds) {
    ViewGroup scrollView = mView.getScrollView();
    ViewGroup contentView = mView.getContentView();
    boolean horizontal = mView.isHorizontal();
    float window = horizontal ? scrollView.getWidth() : scrollView.getHeight();
    float content = horizontal ? contentView.getWidth() : contentView.getHeight();
    float maxOffset = Math.max(0f, content - window);
    float offset = horizontal ? scrollView.getScrollX() : scrollView.getScrollY();

    float newOffset = (float) ShadowListDrag.autoScrollOffset(
      touch, window, offset, maxOffset, PixelUtil.toPixelFromDIP(1f), dtSeconds);
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
    mDrag.updateOrigin(mView.rowIndexOf(view), view.getRowKey());

    // Over another list of the drag board the row floats above the window. The board moves it.
    if (updateDragBoard()) {
      return;
    }
    if (!mDragging) {
      // The board cancelled the drag on this frame.
      return;
    }

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
    float translationX = horizontal ? along : cross;
    float translationY = horizontal ? cross : along;
    // A row that came back from another list of the board starts where its picture was.
    ShadowListDragBoard board = mBoard;
    if (board != null && board.isDecidingFor(this)) {
      contentView.getLocationInWindow(mLocation);
      double[] offset = board.returnOffset(
        mLocation[0] + view.getLeft() + translationX + view.getWidth() / 2f,
        mLocation[1] + view.getTop() + translationY + view.getHeight() / 2f,
        SystemClock.uptimeMillis());
      translationX += (float) offset[0];
      translationY += (float) offset[1];
    }
    view.setTranslationX(translationX);
    view.setTranslationY(translationY);

    collectRows();
    mDrag.updateInsertion(mRowIndices, mRowKeys, mRowFrames, mRowCount);
    tickDropSpot();
    shuffleCollectedRows(boardShiftsSlide());
    // A row that came home on this frame's board update slid its neighbors back once.
    mBoardAnimateShifts = false;
  }

  /*
   * A tick when the drop spot moves, in the list the row is over: the home list while the row
   * is in it, or the list it hovers on a drag board. The core spaces the ticks out.
   */
  private void tickDropSpot() {
    if (mDrag.shouldTick(SystemClock.uptimeMillis())) {
      mView.performHapticFeedback(Build.VERSION.SDK_INT >= Build.VERSION_CODES.R
        ? HapticFeedbackConstants.SEGMENT_TICK
        : HapticFeedbackConstants.CLOCK_TICK);
    }
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
      int rowIndex = mView.rowIndexOf(cellChild);
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
   * Whether a row mounted now has to shift: during a drag, or while a row from another list of
   * the drag board hovers this one.
   */
  boolean needsMountShuffle() {
    return mDragging || mForeign;
  }

  /*
   * A row mounted during the drag. Work out the shifts again with it and slide every row.
   */
  void applyDragShuffle() {
    if (!mDragging && !mForeign) {
      return;
    }
    collectRows();
    mDrag.updateInsertion(mRowIndices, mRowKeys, mRowFrames, mRowCount);
    shuffleCollectedRows(boardShiftsSlide());
  }

  /*
   * On a drag board a row leaves or enters a list as a whole gap at once. The rows slide there
   * instead of jumping, while the row is away from its home list, hovers this list from another
   * one, or just came back. A reorder within one list moves its rows like any list.
   */
  private boolean boardShiftsSlide() {
    return mBoard != null && (mForeign || mHeldAway || mBoardAnimateShifts);
  }

  /*
   * Open a gap by sliding the rows between pickup and landing toward the empty slot. Each
   * moves to exactly where it ends up after the reorder. In a grid a row can move to another
   * column. A slide starts only when a row's target changes.
   */
  private void shuffleCollectedRows(boolean slide) {
    boolean horizontal = mView.isHorizontal();
    for (int i = 0; i < mRowCount; i++) {
      View child = mRowViews[i];
      double[] offset = mDrag.offsetFor(mRowIndices[i]);
      float shift = (float) offset[0];
      float crossShift = (float) offset[1];
      float translationX = horizontal ? shift : crossShift;
      float translationY = horizontal ? crossShift : shift;
      if (!slide) {
        if (mShiftTargets.remove(child) != null) {
          child.animate().cancel();
        }
        child.setTranslationX(translationX);
        child.setTranslationY(translationY);
        continue;
      }
      float[] target = mShiftTargets.get(child);
      if (target != null ? target[0] == translationX && target[1] == translationY
          : child.getTranslationX() == translationX && child.getTranslationY() == translationY) {
        continue;
      }
      mShiftTargets.put(child, new float[] {translationX, translationY});
      child.animate().translationX(translationX).translationY(translationY)
        .setDuration(ShadowListDrag.SHIFT_DURATION_MS)
        .withEndAction(() -> mShiftTargets.remove(child))
        .start();
    }
  }

  /*
   * Reset every row's drag offset and lift.
   */
  private void clearDragTransforms() {
    mShiftTargets.clear();
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
    mShiftTargets.clear();
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
    // A row let go over another list of the drag board drops there.
    if (finishDragBoard()) {
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
          int current = view == null || view.getParent() == null ? -1 : mView.rowIndexOf(view);
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
    mShiftTargets.clear();
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
    withdrawFromBoardFlight();
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
    if (mHeldAway) {
      mDrag.setHeldAway(false);
      mHeldAway = false;
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
    mForeign = false;
    mHeldAway = false;
  }

  /*
   * Like teardown, but a running drag also sends its end event, with the held row's key on
   * both sides. JS then clears the held key without a reorder, and the core turns its scroll
   * corrections back on. The start event turned them off and only the end event clears that.
   * The rows slide back while the list is on screen. A row floating over another list of the
   * drag board flies back to its place.
   */
  void cancel() {
    boolean wasDragging = mDragging;
    if (mDragging) {
      String originKey = mDrag.getOriginKey();
      dispatchDragEvent(DRAG_EVENT_END, originKey, originKey);
    }
    cancelDragBoard();
    tearDownDrag(wasDragging && mView.isAttachedToWindow());
  }

  // endregion

  // region Drag board membership

  ShadowListView getView() {
    return mView;
  }

  String getBoardToken() {
    return mBoardToken;
  }

  /*
   * The name TalkBack gives this list in another list's Move to action: dragListLabel, or the
   * dragListId without one.
   */
  String getDragListLabel() {
    return mDragListLabel.isEmpty() ? mDragListId : mDragListLabel;
  }

  /*
   * Join the board named by dragBoardId, or leave it when the id is empty or changes.
   */
  void setDragBoardId(@Nullable String boardId) {
    String id = boardId != null ? boardId : "";
    if (id.equals(mDragBoardId)) {
      return;
    }
    leaveDragBoard();
    mDragBoardId = id;
    if (!id.isEmpty()) {
      mBoard = ShadowListDragBoard.boardWithId(id);
      mBoardToken = mBoard.join(this);
    }
  }

  void setDragListId(@Nullable String listId) {
    String id = listId != null ? listId : "";
    if (id.equals(mDragListId)) {
      return;
    }
    mDragListId = id;
    if (mBoard != null) {
      mBoard.listsChanged();
    }
  }

  void setDragListLabel(@Nullable String label) {
    String next = label != null ? label : "";
    if (next.equals(mDragListLabel)) {
      return;
    }
    mDragListLabel = next;
    if (mBoard != null) {
      mBoard.listsChanged();
    }
  }

  /*
   * Leave the board, on drop or a new id.
   */
  void leaveDragBoard() {
    ShadowListDragBoard board = mBoard;
    mDragBoardId = "";
    if (board == null) {
      return;
    }
    withdrawFromBoardFlight();
    showBoardHiddenView();
    mBoardDropPending = false;
    mBoardLandingKey = null;
    mBoardAnimateShifts = false;
    mBoard = null;
    mBoardToken = "";
    board.leave(this);
    // This list's rows lose their Move to actions.
    updateCellAccessibilityActions();
  }

  /*
   * The list came onto or left the window. The other lists' Move to actions follow.
   */
  void onWindowChanged() {
    if (mBoard != null) {
      mBoard.listsChanged();
    }
  }

  /*
   * Whether a row of the board can come into this list now. The home list only needs to be on
   * screen with drag on. Any other list also needs its state, no drag or drop of its own and a
   * dragListId to name it to JS.
   */
  boolean canTakeBoardRow(boolean isHome) {
    if (!mView.isAttachedToWindow() || !mView.isShown() || !mReorderEnabled) {
      return false;
    }
    if (isHome) {
      return true;
    }
    return mView.getStateWrapper() != null && !mDragging && !mDragDropPending && !mDragListId.isEmpty();
  }

  /*
   * This list stops taking part in the board's flight, when it leaves the board or the window
   * or turns drag off. As the target of a floating row it closes its gap, and the row floats on
   * without a target until it hovers another list. As the list a dropped row lands in it ends
   * the flight. As the home list of a dropped row it only clears its own part, and the row
   * still lands. Any other flight this list is in ends, and a drag it is home to stops deciding.
   */
  private void withdrawFromBoardFlight() {
    ShadowListDragBoard board = mBoard;
    if (board == null) {
      return;
    }
    if (board.isLanding()) {
      if (board.getTarget() == this) {
        board.endFlight();
      } else if (board.getHome() == this) {
        board.setHome(null);
        boardFlightDidEnd();
      }
      return;
    }
    if (board.getTarget() == this && board.getHome() != this && board.isFloating()
        && board.getTargetId().equals(mBoardToken)) {
      board.leaveCore(mBoardToken);
      board.setTarget(null);
      endBoardForeign();
    } else if (board.getHome() == this || board.getTarget() == this) {
      board.endFlight();
    } else if (board.isDecidingFor(this)) {
      board.cancelCore();
    }
  }

  // endregion

  // region Drag board home list

  /*
   * A pickup in a list on a board. A previous flight still settling ends first, and the core
   * starts deciding for this one with the finger's grab on the row. A window without a root
   * view to hold the floating picture keeps the drag in its list.
   */
  private void boardDidPickUp(ShadowListCellView view) {
    ShadowListDragBoard board = mBoard;
    if (board == null) {
      return;
    }
    board.endFlight();
    // A row still hidden for a picture flying home shows again.
    showBoardHiddenView();
    View root = mView.getRootView();
    if (!(root instanceof ViewGroup)) {
      return;
    }
    mView.getContentView().getLocationInWindow(mLocation);
    board.begin(this, view.getRowKey(), mFingerWindowX, mFingerWindowY,
      mLocation[0] + view.getLeft() + view.getWidth() / 2f,
      mLocation[1] + view.getTop() + view.getHeight() / 2f,
      root.getWidth(), root.getHeight());
    mSidewaysCarry = 0.0;
  }

  /*
   * Every drag frame on a board. The core says what the hover changes and where the picture
   * goes, and this list carries it out. Returns false while the row is home, where the normal
   * drag goes on. Floating, the overlay follows the finger, this list keeps its gap closed and
   * the target opens one.
   */
  private boolean updateDragBoard() {
    ShadowListDragBoard board = mBoard;
    ShadowListCellView view = mDraggedView;
    if (board == null || view == null || !mView.isAttachedToWindow() || !board.isDecidingFor(this)) {
      return false;
    }
    View root = mView.getRootView();
    if (board.windowSizeChanged(root.getWidth(), root.getHeight())) {
      // The window turned or resized under the overlay, which is placed in window pixels.
      cancel();
      return false;
    }
    if (mBoardHiddenView == view && !view.getRowKey().equals(board.getHeldKey())) {
      // The floating row's cell now shows another row. The row it held is gone. Stop the drag.
      cancel();
      return false;
    }
    mView.getContentView().getLocationInWindow(mLocation);
    float shownX = mLocation[0] + view.getLeft() + view.getTranslationX() + view.getWidth() / 2f;
    float shownY = mLocation[1] + view.getTop() + view.getTranslationY() + view.getHeight() / 2f;
    int count = board.candidatesAt(mFingerWindowX, mFingerWindowY, this);
    int step = board.frame(mFingerWindowX, mFingerWindowY, shownX, shownY, count, SystemClock.uptimeMillis());
    switch (step) {
      case ShadowListDragBoard.STEP_LIFT_OUT:
        if (!liftBoardView(view)) {
          cancel();
          return false;
        }
        targetBoardList(board.listWithToken(board.getStepNext()));
        break;
      case ShadowListDragBoard.STEP_RETURN_HOME:
        returnBoardHome();
        return false;
      case ShadowListDragBoard.STEP_RETARGET: {
        ShadowListDragController previous = board.getTarget();
        if (previous != null) {
          previous.endBoardForeign();
        }
        targetBoardList(board.listWithToken(board.getStepNext()));
        break;
      }
      default:
        break;
    }
    double[] frame = board.getFrame();
    if (frame[ShadowListDragBoard.FRAME_FLOATING] == 0.0 || board.getOverlay() == null) {
      return false;
    }
    view.setVisibility(frame[ShadowListDragBoard.FRAME_HIDE_HELD] != 0.0 ? View.INVISIBLE : View.VISIBLE);
    float centerX = (float) frame[ShadowListDragBoard.FRAME_OVERLAY_X];
    float centerY = (float) frame[ShadowListDragBoard.FRAME_OVERLAY_Y];
    board.moveOverlayCenterTo(centerX, centerY);
    // The rows after the held one close its gap.
    collectRows();
    mDrag.updateInsertion(mRowIndices, mRowKeys, mRowFrames, mRowCount);
    shuffleCollectedRows(true);
    ShadowListDragController target = board.getTarget();
    if (target != null) {
      target.placeBoardForeignAtWindowCenter(centerX, centerY);
    }
    return true;
  }

  /*
   * Lift the held row out of this list. Its picture goes above the window where the row shows
   * now, the row hides, and the rows after it close the gap. The core moves the picture under
   * the finger over the next frames.
   */
  private boolean liftBoardView(ShadowListCellView view) {
    ShadowListDragBoard board = mBoard;
    if (board == null) {
      return false;
    }
    mView.getContentView().getLocationInWindow(mLocation);
    float left = mLocation[0] + view.getLeft() + view.getTranslationX();
    float top = mLocation[1] + view.getTop() + view.getTranslationY();
    if (!board.showOverlay(view, left, top)) {
      return false;
    }
    board.setHome(this);
    board.setTarget(null);
    view.setVisibility(View.INVISIBLE);
    mBoardHiddenView = view;
    mDrag.setHeldAway(true);
    mHeldAway = true;
    return true;
  }

  /*
   * The floating row starts hovering list, which opens a gap for it.
   */
  private void targetBoardList(@Nullable ShadowListDragController list) {
    ShadowListDragBoard board = mBoard;
    ShadowListCellView view = mDraggedView;
    if (board == null) {
      return;
    }
    board.setTarget(list);
    if (list != null && view != null) {
      list.beginBoardForeign(view.getWidth(), view.getHeight());
    }
  }

  /*
   * The row came back over this list. It shows again under the finger, eased in from where its
   * picture was, and the target closes its gap. The rows here slide back on this frame's
   * updateDrag.
   */
  private void returnBoardHome() {
    ShadowListDragBoard board = mBoard;
    if (board == null) {
      return;
    }
    ShadowListDragController target = board.getTarget();
    board.setTarget(null);
    if (target != null) {
      target.endBoardForeign();
    }
    board.removeOverlay();
    board.setHome(null);
    mDrag.setHeldAway(false);
    mHeldAway = false;
    mBoardAnimateShifts = true;
    // The next drop spot ticks for the way back.
    mDrag.resetFeedback(-1);
    showBoardHiddenView();
  }

  /*
   * Scroll for a drag on a board, before the frame's updateDrag. Near the window's left or
   * right edge the scroll view around the lists scrolls sideways. While the row floats, the
   * target scrolls near its own edges instead of this list. Returns true then.
   */
  private boolean applyDragBoardAutoScroll(double dtSeconds) {
    ShadowListDragBoard board = mBoard;
    if (board == null || !mDragging || !mView.isAttachedToWindow() || !board.isDecidingFor(this)) {
      return false;
    }
    scrollBoardSideways(dtSeconds);
    if (!board.isFloating() || board.getOverlay() == null) {
      return false;
    }
    ShadowListDragController target = board.getTarget();
    if (target != null) {
      ShadowListView targetView = target.getView();
      targetView.getLocationInWindow(mLocation);
      float x = mFingerWindowX - mLocation[0];
      float y = mFingerWindowY - mLocation[1];
      if (x >= 0 && y >= 0 && x < targetView.getWidth() && y < targetView.getHeight()) {
        target.applyDragAutoScroll(targetView.isHorizontal() ? x : y, dtSeconds);
      }
    }
    return true;
  }

  /*
   * The nearest horizontal scroll view around this list that can scroll moves toward the edge
   * the finger is near. It reveals the next list of a board laid out in a row. The core's
   * DragBoardScroll waits until the finger has moved toward that edge from where it lifted the
   * row, and stops when the finger heads back. A vertical reorder near the edge of a wide
   * column stays put. The ancestors were told not to intercept this gesture at pickup, and a
   * scroll set here never takes it from the list.
   */
  private void scrollBoardSideways(double dtSeconds) {
    ShadowListDragBoard board = mBoard;
    if (board == null) {
      return;
    }
    for (ViewParent parent = mView.getParent(); parent instanceof View; parent = parent.getParent()) {
      if (!(parent instanceof HorizontalScrollView)) {
        continue;
      }
      HorizontalScrollView outer = (HorizontalScrollView) parent;
      if (outer.getChildCount() == 0) {
        continue;
      }
      int window = outer.getWidth() - outer.getPaddingLeft() - outer.getPaddingRight();
      int maxOffset = Math.max(0, outer.getChildAt(0).getWidth() - window);
      if (maxOffset <= 0) {
        continue;
      }
      outer.getLocationInWindow(mLocation);
      int offset = outer.getScrollX();
      double delta = board.scrollDelta(mFingerWindowX, mFingerWindowX - mLocation[0], outer.getWidth(), dtSeconds);
      // Keep the part of a pixel a slow scroll moves on each frame.
      double wanted = offset + delta + (delta != 0.0 ? mSidewaysCarry : 0.0);
      int next = (int) Math.max(0, Math.min(maxOffset, Math.round(wanted)));
      mSidewaysCarry = next > 0 && next < maxOffset ? wanted - next : 0.0;
      if (next != offset) {
        outer.scrollTo(next, outer.getScrollY());
        board.setSidewaysScrollView(outer);
      }
      return;
    }
  }

  /*
   * Setting the offset never runs React Native's paging, which snapToInterval and
   * snapToOffsets turn on for Android. A board the drag scrolled would rest between pages. A
   * fling without speed moves it to the nearest page, interval or offset once the drag ends.
   */
  private void snapBoardScrollView() {
    ShadowListDragBoard board = mBoard;
    ViewGroup outer = board != null ? board.takeSidewaysScrollView() : null;
    if (!(outer instanceof ReactHorizontalScrollView) || !isPagingEnabled((ReactHorizontalScrollView) outer)) {
      return;
    }
    ((ReactHorizontalScrollView) outer).fling(0);
  }

  private static boolean isPagingEnabled(ReactHorizontalScrollView outer) {
    if (!sPagingFieldLooked) {
      sPagingFieldLooked = true;
      try {
        Field field = ReactHorizontalScrollView.class.getDeclaredField("mPagingEnabled");
        field.setAccessible(true);
        sPagingField = field;
      } catch (Exception exception) {
        sPagingField = null;
      }
    }
    if (sPagingField == null) {
      return false;
    }
    try {
      return sPagingField.getBoolean(outer);
    } catch (Exception exception) {
      return false;
    }
  }

  /*
   * Let go on a board. The core decides with the target's drop spot. A row let go while home
   * returns false, and the normal drop runs. A floating row drops into the target, which is the
   * last list hovered, even when the finger is now between lists. The target hears the landing
   * first, which tells its core to keep the offset when the row arrives. Then this list sends
   * onDragEnd with the target's list id, the index and the key of the row the moved one goes
   * before, and both lists keep their gaps until the data change lands. The row goes back home
   * only when there is no target, the target has no drop spot or the row has no key.
   */
  private boolean finishDragBoard() {
    ShadowListDragBoard board = mBoard;
    if (board == null || !board.isDecidingFor(this)) {
      return false;
    }
    snapBoardScrollView();
    ShadowListDragController target = board.getTarget();
    boolean foreign = target != null && target.mForeign;
    int release = board.release(
      foreign ? target.mDrag.getInsertionIndex() : -1,
      foreign ? target.mDrag.getInsertionKey() : "",
      foreign ? target.getView().getRowKeyCount() : -1,
      SystemClock.uptimeMillis());
    if (release == ShadowListDragBoard.RELEASE_HOME) {
      return false;
    }
    ShadowListDragController destination = board.listWithToken(board.getReleaseListId());
    if (release == ShadowListDragBoard.RELEASE_CANCEL || destination == null || destination != target) {
      board.endCore();
      cancel();
      return true;
    }

    stopDragLoop();
    mView.setInnerScrollEnabled(true);

    String heldKey = board.getHeldKey();
    mDragging = false;
    mDraggedView = null;
    mBoardDropPending = true;
    target.mBoardLandingKey = heldKey;

    target.dispatchDragEvent(DRAG_EVENT_LAND, heldKey, heldKey);
    dispatchDragEvent(DRAG_EVENT_END, heldKey, board.getReleaseKey(), target.mDragListId, board.getReleaseIndex());
    board.startLandingLoop();
    return true;
  }

  /*
   * A cancel. The core stops deciding. While the row floats the target closes its gap, and the
   * picture flies back onto the row in this list, which shows again when it arrives. cancel
   * calls this before tearing down the drag, which slides this list's rows back.
   */
  private void cancelDragBoard() {
    ShadowListDragBoard board = mBoard;
    if (board == null) {
      return;
    }
    if (board.isDecidingFor(this)) {
      snapBoardScrollView();
      board.cancelCore();
    }
    if (board.getHome() != this || board.getOverlay() == null || board.isLanding()) {
      return;
    }
    // The flight ends now. Only the picture is left, flying home.
    View overlay = board.detachOverlay();
    ShadowListDragController target = board.getTarget();
    board.setHome(null);
    board.setTarget(null);
    if (target != null) {
      target.endBoardForeign();
    }
    if (mHeldAway) {
      mDrag.setHeldAway(false);
      mHeldAway = false;
    }

    ShadowListCellView home = mBoardHiddenView;
    if (overlay == null || home == null || !home.isAttachedToWindow()) {
      ShadowListDragBoard.removeOverlayView(overlay);
      showBoardHiddenView();
      return;
    }
    mView.getContentView().getLocationInWindow(mLocation);
    board.animateOverlayTo(overlay, mLocation[0] + home.getLeft(), mLocation[1] + home.getTop(), () -> {
      ShadowListDragBoard.removeOverlayView(overlay);
      if (mBoardHiddenView == home) {
        showBoardHiddenView();
      }
    });
  }

  // endregion

  // region Drag board target list

  /*
   * A row from another list starts hovering this one, with its size in pixels.
   */
  private void beginBoardForeign(int width, int height) {
    boolean horizontal = mView.isHorizontal();
    mNumberOfColumns = mView.getNumberOfColumns();
    // The first drop spot here ticks for the move into this list.
    mDrag.beginForeign(horizontal ? width : height, horizontal ? height : width, mNumberOfColumns);
    mForeign = true;
  }

  /*
   * Place the incoming row's center, in window pixels, and open the gap where it would drop.
   */
  private void placeBoardForeignAtWindowCenter(float centerX, float centerY) {
    if (!mForeign) {
      return;
    }
    mView.getContentView().getLocationInWindow(mLocation);
    float x = centerX - mLocation[0];
    float y = centerY - mLocation[1];
    boolean horizontal = mView.isHorizontal();
    mDrag.placeForeignCenter(horizontal ? x : y, horizontal ? y : x);
    collectRows();
    mDrag.updateInsertion(mRowIndices, mRowKeys, mRowFrames, mRowCount);
    tickDropSpot();
    shuffleCollectedRows(true);
  }

  /*
   * The incoming row left this list. The rows slide back.
   */
  private void endBoardForeign() {
    if (!mForeign) {
      return;
    }
    mDrag.endForeign();
    mForeign = false;
    collectRows();
    shuffleCollectedRows(true);
    releaseCollectedRows();
  }

  // endregion

  // region Drag board landing

  @Nullable private ShadowListCellView boardCellForKey(String key) {
    ViewGroup contentView = mView.getContentView();
    for (int i = 0; i < contentView.getChildCount(); i++) {
      View child = contentView.getChildAt(i);
      if (child instanceof ShadowListCellView && key.equals(((ShadowListCellView) child).getRowKey())) {
        return (ShadowListCellView) child;
      }
    }
    return null;
  }

  /*
   * After a state commit or a key change, and every frame while a drop lands. The home list
   * clears its gap once the row has left its data. The target clears its gap as soon as its
   * data holds the row, since its rows then rest in their new places and a gap kept by index
   * would shift them twice. The picture settles onto the row once its cell is laid out, which
   * only the landing frames check. A mount may still be placing the cell.
   */
  void boardMountDidFinish(boolean fromFrame) {
    ShadowListDragBoard board = mBoard;
    if (board == null) {
      return;
    }
    String heldKey = board.getHeldKey();
    if (heldKey.isEmpty()) {
      heldKey = null;
    }
    ShadowListCellView hidden = mBoardHiddenView;
    boolean hiddenHoldsHeldRow = hidden != null && heldKey != null && hidden.getParent() != null
      && heldKey.equals(hidden.getRowKey());
    if (hidden != null && !hiddenHoldsHeldRow) {
      showBoardHiddenView();
      // The floating row's cell now shows another row. The row it held is gone. Stop the drag.
      if (board.getHome() == this && board.isFloating() && mDragging) {
        cancel();
        return;
      }
    }
    if (mBoardDropPending
        && (heldKey == null || (!mView.rowKeysContain(heldKey) && boardCellForKey(heldKey) == null))) {
      mBoardDropPending = false;
      if (!mDragging) {
        clearDragTransforms();
      }
    }
    String landingKey = mBoardLandingKey;
    if (landingKey == null) {
      return;
    }
    if (board.getPhase() == ShadowListDragBoard.PHASE_LANDING
        && board.landed(mBoardToken, mView.rowKeysContain(landingKey))) {
      mDrag.endForeign();
      mForeign = false;
      clearDragTransforms();
    }
    if (fromFrame && board.getPhase() == ShadowListDragBoard.PHASE_SETTLING && !board.isSettling()
        && mBoardToken.equals(board.getTargetId())) {
      ShadowListCellView cell = boardCellForKey(landingKey);
      if (cell != null && cell.isLaidOut()) {
        landBoardOnView(cell);
      }
    }
  }

  private void landBoardOnView(ShadowListCellView cell) {
    ShadowListDragBoard board = mBoard;
    mBoardLandingKey = null;
    if (board == null) {
      return;
    }
    if (board.getOverlay() == null || !cell.isAttachedToWindow()) {
      board.endFlight();
      return;
    }
    cell.setVisibility(View.INVISIBLE);
    mBoardHiddenView = cell;
    mView.getContentView().getLocationInWindow(mLocation);
    board.settleOverlay(mLocation[0] + cell.getLeft(), mLocation[1] + cell.getTop());
  }

  /*
   * A row the board hid shows again before Fabric takes it away.
   */
  void boardViewWillUnmount(View view) {
    if (view == mBoardHiddenView) {
      showBoardHiddenView();
    }
  }

  private void showBoardHiddenView() {
    ShadowListCellView hidden = mBoardHiddenView;
    mBoardHiddenView = null;
    if (hidden != null) {
      hidden.setVisibility(View.VISIBLE);
    }
  }

  /*
   * The flight ended. Show the hidden row and clear what this list held for it.
   */
  void boardFlightDidEnd() {
    showBoardHiddenView();
    mBoardLandingKey = null;
    if (mForeign) {
      mDrag.endForeign();
      mForeign = false;
      clearDragTransforms();
    }
    if (mBoardDropPending) {
      mBoardDropPending = false;
      if (!mDragging) {
        clearDragTransforms();
      }
    }
    if (mDragging && mHeldAway) {
      mDrag.setHeldAway(false);
      mHeldAway = false;
    }
  }

  // endregion

  // region Accessibility

  /*
   * For TalkBack, swap the row with the nearest mounted row above or below. It goes through
   * the same path as a real drop, like VoiceOver's Move up and Move down on iOS. onDragEnd and
   * useDragReorder handle it as usual.
   */
  boolean performAccessibilityMove(ShadowListCellView view, boolean up) {
    int index = mView.rowIndexOf(view);
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
      int childIndex = mView.rowIndexOf((ShadowListCellView) child);
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

  /*
   * The other lists of the drag board a row of this list can move to with TalkBack: shown on
   * the window with their state, drag on and a dragListId, like on iOS. Empty off a board or with
   * drag off.
   */
  List<ShadowListDragController> boardMoveTargets() {
    ArrayList<ShadowListDragController> targets = new ArrayList<>();
    ShadowListDragBoard board = mBoard;
    if (board == null || !mReorderEnabled) {
      return targets;
    }
    for (ShadowListDragController list : board.getLists()) {
      if (list != this && list.mReorderEnabled && !list.mDragListId.isEmpty() && list.mView.isAttachedToWindow()
          && list.mView.isShown() && list.mView.getStateWrapper() != null) {
        targets.add(list);
      }
    }
    return targets;
  }

  /*
   * TalkBack's Move to on a row: the row goes to the end of target, through the same events as a
   * drag dropped there. The target hears the landing first, then this list sends onDragEnd with
   * the target's list id, an empty key and the target's row count. TalkBack says where it went.
   */
  boolean performAccessibilityMoveToList(ShadowListCellView view, ShadowListDragController target) {
    String key = view.getRowKey();
    if (!mReorderEnabled || mDragging || key.isEmpty() || mBoard == null || target == this
        || target.mBoard != mBoard || !target.canTakeBoardRow(false)) {
      return false;
    }
    target.dispatchDragEvent(DRAG_EVENT_LAND, key, key);
    dispatchDragEvent(DRAG_EVENT_END, key, "", target.mDragListId, target.getView().getRowKeyCount());
    mView.announceForAccessibility(mView.getContext().getString(R.string.shadowlist_moved_to, target.getDragListLabel()));
    return true;
  }

  /*
   * The Move to actions of every mounted row follow the board's lists.
   */
  void updateCellAccessibilityActions() {
    ViewGroup contentView = mView.getContentView();
    for (int i = 0; i < contentView.getChildCount(); i++) {
      View child = contentView.getChildAt(i);
      if (child instanceof ShadowListCellView) {
        ((ShadowListCellView) child).updateAccessibilityActions();
      }
    }
  }

  // endregion

  private void dispatchDragEvent(int type, String sourceKey, String destinationKey) {
    dispatchDragEvent(type, sourceKey, destinationKey, "", -1);
  }

  /*
   * A drag event for JS. destinationListId and destinationIndex name the other list of the drag
   * board a row dropped in, and stay empty and -1 for every other event.
   */
  private void dispatchDragEvent(
    int type, String sourceKey, String destinationKey, String destinationListId, int destinationIndex) {
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
    mView.dispatchDragEvent(type, sourceKey, destinationKey, destinationListId, destinationIndex, sequence);
  }
}
