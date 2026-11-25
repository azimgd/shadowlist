package com.shadowlist;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.util.Log;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.widget.FrameLayout;
import android.widget.OverScroller;

import androidx.annotation.Nullable;
import androidx.core.view.ViewCompat;
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout;

import com.facebook.react.bridge.ReactContext;
import com.facebook.react.bridge.ReadableArray;
import com.facebook.react.bridge.WritableMap;
import com.facebook.react.common.mapbuffer.ReadableMapBuffer;
import com.facebook.react.uimanager.PixelUtil;
import com.facebook.react.uimanager.StateWrapper;
import com.facebook.react.uimanager.UIManagerHelper;
import com.facebook.react.uimanager.events.EventDispatcher;
import com.facebook.react.uimanager.events.NativeGestureUtil;
import com.facebook.react.views.scroll.ReactHorizontalScrollView;
import com.facebook.react.views.scroll.ReactScrollView;

import java.util.List;

/*
 * Hosts the content in an inner scroll view for the chosen axis. Handles scrolling, state
 * sync and scroll commands. Sticky pinning, drag to reorder and accessibility live in their own
 * classes, which use the accessors at the bottom.
 */
public class ShadowListView extends FrameLayout {
  /*
   * Trace logging for state sync, on with -PshadowlistDebugLog. Filter with adb logcat -s SL
   */
  static final boolean DEBUG_LOG = BuildConfig.SHADOWLIST_DEBUG_LOG;
  private static final String LOG_TAG = "SL";

  static void slLog(String message) {
    if (DEBUG_LOG) {
      Log.d(LOG_TAG, "[SL] " + message);
    }
  }

  /*
   * Values for the scrollPhase state key and the scrollToRow rows for the end and an absolute
   * offset, read from the core through ShadowListScrollSync.
   */
  static final int SCROLL_PHASE_IDLE = ShadowListScrollSync.SCROLL_PHASE_IDLE;
  private static final int SCROLL_PHASE_DRAGGING = ShadowListScrollSync.SCROLL_PHASE_DRAGGING;
  private static final int SCROLL_PHASE_SETTLING = ShadowListScrollSync.SCROLL_PHASE_SETTLING;
  private static final double SCROLL_TO_END_SLOT = ShadowListScrollSync.SCROLL_TO_END_SLOT;
  private static final double SCROLL_TO_OFFSET_SLOT = ShadowListScrollSync.SCROLL_TO_OFFSET_SLOT;

  /*
   * Keys of the state MapBuffer, matching ShadowListStateKey in ShadowListViewState.h.
   */
  private static final int STATE_TOTAL_WIDTH = 0;
  private static final int STATE_TOTAL_HEIGHT = 1;
  private static final int STATE_OFFSET_ENABLED = 2;
  private static final int STATE_OFFSET_X = 3;
  private static final int STATE_OFFSET_Y = 4;
  private static final int STATE_COMMIT_TOKEN = 5;
  private static final int STATE_COMMIT_REVISION = 21;
  private static final int STATE_COMMAND_PENDING = 22;
  private static final int STATE_OFFSET_BASE_X = 7;
  private static final int STATE_OFFSET_BASE_Y = 8;
  private static final int STATE_USER_SCROLLED = 9;
  private static final int STATE_SCROLL_PHASE = 10;
  private static final int STATE_COMMAND_SEQUENCE = 11;
  private static final int STATE_STICKY_VERSION = 12;
  private static final int STATE_SNAP_VERSION = 13;
  private static final int STATE_BAND_LOW = 14;
  private static final int STATE_BAND_HIGH = 15;
  private static final int STATE_LIVE_HANDLE = 16;
  private static final int STATE_CONCEAL_GENERATION = 17;
  private static final int STATE_ANIMATION_SEQUENCE = 18;
  private static final int STATE_ANIMATION_OFFSET = 19;
  private static final int STATE_MOUNT_HANDLE = 20;

  private @Nullable StateWrapper mState = null;
  /*
   * getAnchorState came before the first state. It is sent once the state arrives.
   */
  private enum AnchorRequest { IDLE, PARKED }
  private enum AnchorEvent { REQUEST, READY, RESET }
  private AnchorRequest mAnchorRequest = AnchorRequest.IDLE;

  /*
   * The mounted list's live report handle, and the versions that tell when its sticky and
   * snap lists changed. They are only copied then.
   */
  private long mLiveHandle = 0;
  private long mStickyVersion = -1;
  private long mSnapVersion = -1;
  private long mGeometryHandle = 0;

  /*
   * Ticks the list's mount driver on frames, see ShadowListMountTicker.
   */
  private final ShadowListMountTicker mMountTicker = new ShadowListMountTicker();

  long getMountHandle() { return mMountTicker.getHandle(); }

  /*
   * Live reports, which frames commit, the echo of core corrections and scroll commands.
   * See ShadowListScrollSync.
   */
  private final ShadowListScrollSync mSync;
  private ShadowListInnerScrollView.ContentContainer mContentView;
  private ViewGroup mScrollView;
  private final ShadowListStickyController mStickyController;
  private final ShadowListDragController mDragController;
  /*
   * The row swiped open, or null. Only one row is open at a time.
   */
  @Nullable private ShadowListCellView mSwipedRow = null;
  /*
   * This touch closed the open row. It presses nothing but may still scroll.
   */
  private boolean mSwipeClosingTouch = false;
  /*
   * This touch stopped a fling. It shows no context menu.
   */
  private boolean mTouchStoppedFling = false;
  /*
   * A long press on a row shows its context menu, unless it belongs to drag to reorder.
   */
  private final GestureDetector mMenuGestureDetector;

  /*
   * The innermost cell with a menu that this touch came down on, the row or an ItemActionsView
   * inside it. Null when it came down on none, or on a row of a list inside this one with a menu.
   */
  @Nullable private ShadowListCellView mTouchedMenuCell = null;
  /*
   * Grid columns from props, which drags move rows across.
   */
  private int mNumberOfColumns = 1;

  /*
   * Pin sticky views again when a header or footer is laid out. A sticky footer then follows
   * its real position when the list size changes.
   */
  private final View.OnLayoutChangeListener mTemplateLayoutListener;
  /*
   * A mounted template changed type. The sticky views must be found again.
   */
  private final Runnable mTemplateTypeListener;

  /*
   * The scroll axis. Changing it rebuilds the inner scroll view.
   */
  private boolean mHorizontal = false;

  /*
   * Snap offsets come from the core. A single motion owns the finger, lift snap and frame
   * samples; samples exist only while a fling or animated command is being observed.
   */
  private boolean mSnapToItem = false;
  private float[] mSnapOffsetsPx = new float[0];
  private double[] mSnapOffsetsDip = new double[0];
  private interface ScrollMotion {}
  private enum ScrollRest implements ScrollMotion {
    IDLE, TOUCHING, TOUCHING_FLUNG, LIFTED, SNAP_PENDING, FLUNG
  }
  private enum SettleKind { FLING, TOUCHING_FLING }
  private static final class MotionSample {
    int previousX;
    int previousY;
    int stableFrames;

    MotionSample(int x, int y) {
      previousX = x;
      previousY = y;
    }
  }
  private static final class ScrollSettling implements ScrollMotion {
    final SettleKind kind;
    final MotionSample sample;

    ScrollSettling(SettleKind kind, MotionSample sample) {
      this.kind = kind;
      this.sample = sample;
    }
  }
  private static final class CommandLanding implements ScrollMotion {
    final ScrollMotion gesture;
    final MotionSample sample;

    CommandLanding(ScrollMotion gesture, MotionSample sample) {
      this.gesture = gesture;
      this.sample = sample;
    }
  }
  private enum MotionEventKind { DOWN, UP, FLING, STOP, SNAP, COMMAND, COMMAND_ENDED, FRAME, RESET }
  private ScrollMotion mMotion = ScrollRest.IDLE;
  private static final int SETTLE_STABLE_FRAMES = 6;

  /*
   * The prop and settle request share one phase. Spinner visibility comes from the widget.
   */
  @Nullable private SwipeRefreshLayout mRefreshLayout = null;
  private boolean mRefreshEnabled = false;
  @Nullable private Integer mRefreshColor = null;
  private enum RefreshPhase { REST, REFRESHING, REFRESHING_AWAITING, AWAITING, SENT }
  private enum RefreshEvent { BEGIN, END, AWAIT, CHECK, RESET }
  private RefreshPhase mRefreshPhase = RefreshPhase.REST;

  /*
   * ScrollView props. A drag turns scrolling off for its run and gives back mScrollEnabled.
   */
  private boolean mScrollEnabled = true;
  private boolean mShowsVerticalScrollIndicator = true;
  private boolean mShowsHorizontalScrollIndicator = true;
  private boolean mBounces = true;
  private double mDecelerationRate = 0;
  private boolean mNestedScrollEnabled = false;
  private double mProgressViewOffset = 0;

  /*
   * Drag and momentum events. A drag begins on the first frame a finger moves the list.
   * The fling velocity goes out with the end of the drag, in px per second.
   */
  private boolean mDragEventSent = false;
  private boolean mMomentumEventSent = false;
  private double mLandCommandSequence = 0;
  /*
   * The finger lifted from a drag that had no fling yet. A fling the inner scroll view starts
   * after the lift still sends its momentum begin.
   */
  private boolean mDragEndedAtRest = false;
  private int mFlingVelocity = 0;

  /*
   * onScroll goes out from the scroll callback while JS listens, throttled by
   * scrollEventThrottle.
   */
  private boolean mScrollEventEnabled = false;
  private double mScrollEventThrottle = 0;
  private final ShadowListScrollEvent.Tracker mScrollTracker = new ShadowListScrollEvent.Tracker();

  /*
   * The offset of the previous scroll callback. React Native's scroll view calls
   * onScrollChanged twice for some frames of its animations and flings. The repeat moves
   * nothing. Counted as a user scroll it would cancel an animated command that landed on the
   * first one.
   */
  private int mPreviousScrollX = Integer.MIN_VALUE;
  private int mPreviousScrollY = Integer.MIN_VALUE;

  /*
   * Row count, visible rows and scroll actions for accessibility services.
   */
  private final ShadowListAccessibility mAccessibility = new ShadowListAccessibility(this);

  /*
   * React Native's normal deceleration rate on Android, which a 0 prop keeps.
   */
  private static final float DEFAULT_DECELERATION_RATE = 0.985f;

  /*
   * Where the refresh spinner rests below its offset, like React Native's DEFAULT_CIRCLE_TARGET.
   */
  private static final float REFRESH_CIRCLE_TARGET_DP = 64;

  /*
   * How close our own scroll must land to its target to count as reaching it. Only used to
   * spot the end of our own animated scrolls like snapping, and for the exact dp echo.
   * Core corrections are matched by cause, not by this tolerance.
   */
  private static final int PROGRAMMATIC_SCROLL_TOLERANCE_PX = 2;

  // region Lifecycle

  public ShadowListView(Context context) {
    super(context);
    mContentView = new ShadowListInnerScrollView.ContentContainer(context);
    mStickyController = new ShadowListStickyController(this);
    mDragController = new ShadowListDragController(this);
    mMenuGestureDetector = new GestureDetector(context, new GestureDetector.SimpleOnGestureListener() {
      @Override
      public void onLongPress(MotionEvent event) {
        showRowMenu(event);
      }
    });
    mTemplateLayoutListener =
      (view, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
        if (left != oldLeft || top != oldTop || right != oldRight || bottom != oldBottom) {
          mStickyController.applyStickyTransforms();
        }
      };
    mTemplateTypeListener = () -> mStickyController.invalidateTemplates();
    mSync = new ShadowListScrollSync(PixelUtil.toDIPFromPixel(PROGRAMMATIC_SCROLL_TOLERANCE_PX));
    installScrollView(false);
  }

  /*
   * Build the inner scroll view for the axis and move the content into it.
   */
  private void installScrollView(boolean horizontal) {
    mContentView.setCullAxis(horizontal);
    mPreviousScrollX = Integer.MIN_VALUE;
    mPreviousScrollY = Integer.MIN_VALUE;
    if (mScrollView != null) {
      mScrollView.removeView(mContentView);
      if (mRefreshLayout != null) {
        mRefreshLayout.removeView(mScrollView);
        removeView(mRefreshLayout);
        mRefreshLayout = null;
      } else {
        removeView(mScrollView);
      }
    }

    Context context = getContext();
    mScrollView = horizontal
      ? new ShadowListInnerScrollView.Horizontal(context, this)
      : new ShadowListInnerScrollView.Vertical(context, this);

    mScrollView.setScrollbarFadingEnabled(true);
    mScrollView.setScrollBarStyle(View.SCROLLBARS_INSIDE_OVERLAY);
    if (mScrollView instanceof ReactScrollView) {
      ((ReactScrollView) mScrollView).setFillViewport(false);
    }
    mScrollView.setClipToPadding(false);

    /*
     * The inner scroll view has no React tag. Every ScrollEvent it sends goes to tag -1
     * and logs an error, once per frame. The list reports scrolling itself and throttles
     * them away. The last dispatch time in the future keeps even the first one back.
     */
    if (mScrollView instanceof ReactScrollView) {
      ((ReactScrollView) mScrollView).setScrollEventThrottle(Integer.MAX_VALUE);
      ((ReactScrollView) mScrollView).setLastScrollDispatchTime(Long.MAX_VALUE);
    } else if (mScrollView instanceof ReactHorizontalScrollView) {
      ((ReactHorizontalScrollView) mScrollView).setScrollEventThrottle(Integer.MAX_VALUE);
      ((ReactHorizontalScrollView) mScrollView).setLastScrollDispatchTime(Long.MAX_VALUE);
    }

    GradientDrawable scrollbarDrawable = new GradientDrawable();
    scrollbarDrawable.setShape(GradientDrawable.RECTANGLE);
    scrollbarDrawable.setColor(Color.WHITE);
    scrollbarDrawable.setCornerRadius(8);
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
      mScrollView.setVerticalScrollbarThumbDrawable(scrollbarDrawable);
      mScrollView.setHorizontalScrollbarThumbDrawable(scrollbarDrawable);
    }

    mScrollView.addView(mContentView);
    applyScrollViewProps();
    ViewCompat.setAccessibilityDelegate(mScrollView, mAccessibility);

    if (!horizontal) {
      // Vertical lists get wrapped for pull to refresh.
      mRefreshLayout = new SwipeRefreshLayout(context);
      mRefreshLayout.setOnRefreshListener(this::emitRefresh);
      mRefreshLayout.setEnabled(mRefreshEnabled);
      if (mRefreshColor != null) {
        mRefreshLayout.setColorSchemeColors(mRefreshColor);
      }
      applyProgressViewOffset();
      mRefreshLayout.addView(mScrollView, new ViewGroup.LayoutParams(
        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
      addView(mRefreshLayout, new FrameLayout.LayoutParams(
        FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
    } else {
      addView(mScrollView, new FrameLayout.LayoutParams(
        FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
    }
  }

  /*
   * Reset drag and sticky state before this view is recycled.
   */
  void onDropInstance() {
    /*
     * Fabric has destroyed the state by now. The scrollTo below reports a scroll, and later
     * frames of a list still animating out with its screen would too. Neither may reach it.
     */
    mState = null;
    transitionAnchorRequest(AnchorEvent.RESET);
    mDragController.teardown();
    mDragController.leaveDragBoard();
    mStickyController.reset();
    /*
     * Don't pass the scroll position, content size, live report or list versions to the next
     * list. Sticky pinning uses the content size and runs on mount, before the new list's
     * state arrives. Same as prepareForRecycle on iOS.
     */
    mLiveHandle = 0;
    mMountTicker.setHandle(0);
    mStickyVersion = -1;
    mSnapVersion = -1;
    mGeometryHandle = 0;
    transitionRefresh(RefreshEvent.RESET);
    transitionMotion(MotionEventKind.RESET);
    mTouchStoppedFling = false;
    mDragEventSent = false;
    mMomentumEventSent = false;
    mLandCommandSequence = 0;
    mDragEndedAtRest = false;
    mScrollEventEnabled = false;
    mScrollTracker.reset(mScrollEventThrottle);
    mScrollView.scrollTo(0, 0);
    mContentView.layout(0, 0, 0, 0);
    /*
     * Free the echo state last. A recycled view gets a fresh one on its next call. Stale echo
     * state would make the next list's first real scroll get ignored.
     */
    mSync.destroy();
  }

  /*
   * A detached list gets no frames to finish a drag or a drop, and both stop here. A running
   * drag first sends its end event. JS then lets go of the held row and the core turns its
   * scroll corrections back on.
   */
  @Override
  protected void onDetachedFromWindow() {
    mLandCommandSequence = 0;
    if (mDragController.isDragging()) {
      mDragController.cancel();
    }
    mDragController.teardown();
    super.onDetachedFromWindow();
    // A list off the window is no drag board's Move to target.
    mDragController.onWindowChanged();
  }

  @Override
  protected void onAttachedToWindow() {
    super.onAttachedToWindow();
    mDragController.onWindowChanged();
  }

  /*
   * Pin again once the scroll view has its new size. A short list whose rows all mounted
   * before this layout gets no other pass, and its sticky footer would stay where a window
   * of no size put it.
   */
  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    super.onLayout(changed, left, top, right, bottom);
    if (changed) {
      mStickyController.applyStickyTransforms();
    }
  }

  // endregion

  // region Props

  /*
   * Apply the ScrollView props to the inner scroll view. A new axis builds a new one, which
   * gets them again.
   */
  private void applyScrollViewProps() {
    mScrollView.setVerticalScrollBarEnabled(!mHorizontal && mShowsVerticalScrollIndicator);
    mScrollView.setHorizontalScrollBarEnabled(mHorizontal && mShowsHorizontalScrollIndicator);
    mScrollView.setOverScrollMode(mBounces ? View.OVER_SCROLL_IF_CONTENT_SCROLLS : View.OVER_SCROLL_NEVER);
    mScrollView.setNestedScrollingEnabled(mNestedScrollEnabled);
    float decelerationRate = mDecelerationRate > 0 ? (float) mDecelerationRate : DEFAULT_DECELERATION_RATE;
    if (mScrollView instanceof ReactScrollView) {
      ((ReactScrollView) mScrollView).setDecelerationRate(decelerationRate);
    } else if (mScrollView instanceof ReactHorizontalScrollView) {
      ((ReactHorizontalScrollView) mScrollView).setDecelerationRate(decelerationRate);
    }
    if (!mDragController.isDragging()) {
      setInnerScrollEnabled(true);
    }
  }

  public void setScrollEnabled(boolean scrollEnabled) {
    if (scrollEnabled == mScrollEnabled) {
      return;
    }
    mScrollEnabled = scrollEnabled;
    applyScrollViewProps();
  }

  public void setShowsVerticalScrollIndicator(boolean shows) {
    mShowsVerticalScrollIndicator = shows;
    applyScrollViewProps();
  }

  public void setShowsHorizontalScrollIndicator(boolean shows) {
    mShowsHorizontalScrollIndicator = shows;
    applyScrollViewProps();
  }

  public void setBounces(boolean bounces) {
    mBounces = bounces;
    applyScrollViewProps();
  }

  /*
   * 0 keeps the platform's friction, like React Native's normal.
   */
  public void setDecelerationRate(double decelerationRate) {
    mDecelerationRate = decelerationRate;
    applyScrollViewProps();
  }

  public void setNestedScrollEnabled(boolean nestedScrollEnabled) {
    mNestedScrollEnabled = nestedScrollEnabled;
    applyScrollViewProps();
  }

  public void setProgressViewOffset(double offset) {
    mProgressViewOffset = offset;
    applyProgressViewOffset();
  }

  /*
   * Start the spinner at the offset and pull it to REFRESH_CIRCLE_TARGET_DP below, like
   * React Native's progressViewOffset. 0 keeps the default.
   */
  private void applyProgressViewOffset() {
    if (mRefreshLayout == null || mProgressViewOffset == 0) {
      return;
    }
    int diameter = mRefreshLayout.getProgressCircleDiameter();
    int start = Math.round(PixelUtil.toPixelFromDIP((float) mProgressViewOffset)) - diameter;
    int end = Math.round(PixelUtil.toPixelFromDIP((float) mProgressViewOffset + REFRESH_CIRCLE_TARGET_DP));
    mRefreshLayout.setProgressViewOffset(false, start, end);
  }

  /*
   * Every row's key, which accessibility counts rows with. Only the core reads the keys
   * otherwise.
   */
  public void setItemKeys(@Nullable ReadableArray keys) {
    mAccessibility.setItemKeys(keys);
    mKeysGeneration++;
    mDragController.onKeysChanged();
  }

  /*
   * The native key store JS keeps the keys in and the version the props name, which replace
   * the rowKeys prop when set. See ShadowListKeys.
   */
  private long mKeysStore = 0;
  private double mKeysVersion = 0;

  /*
   * Counts changes of the keys. A row's index found in older keys is looked up again.
   */
  private int mKeysGeneration = 0;

  public void setKeysStore(long keysStore) {
    mKeysStore = keysStore;
    mAccessibility.setKeysStore(mKeysStore, mKeysVersion);
    mKeysGeneration++;
    mDragController.onKeysChanged();
  }

  public void setKeysVersion(double keysVersion) {
    mKeysVersion = keysVersion;
    mAccessibility.setKeysStore(mKeysStore, mKeysVersion);
    mKeysGeneration++;
    mDragController.onKeysChanged();
  }

  /*
   * A mounted row's index in this list's keys, found by its key once per keys change, or -1.
   */
  int rowIndexOf(ShadowListCellView row) {
    if (!row.hasRowIndex(mKeysGeneration)) {
      String key = row.getRowKey();
      row.cacheRowIndex(key.isEmpty() ? -1 : mAccessibility.indexOfItemKey(key), mKeysGeneration);
    }
    return row.getRowIndex();
  }

  /*
   * Whether this list's data holds a row keyed key, mounted or not.
   */
  boolean rowKeysContain(String key) {
    return mAccessibility.indexOfItemKey(key) >= 0;
  }

  /*
   * The number of rows in this list's data, or -1 when the key store no longer has them.
   */
  int getRowKeyCount() {
    return mAccessibility.getItemCountOrUnknown();
  }

  public void setScrollEventEnabled(boolean scrollEventEnabled) {
    if (scrollEventEnabled != mScrollEventEnabled) {
      mScrollEventEnabled = scrollEventEnabled;
      mScrollTracker.reset(mScrollEventThrottle);
    }
  }

  public void setScrollEventThrottle(double scrollEventThrottle) {
    if (scrollEventThrottle != mScrollEventThrottle) {
      mScrollEventThrottle = scrollEventThrottle;
      mScrollTracker.reset(scrollEventThrottle);
    }
  }

  public void setSnapToItem(boolean snapToItem) {
    mSnapToItem = snapToItem;
  }

  public void setReorderEnabled(boolean reorderEnabled) {
    boolean changed = reorderEnabled != mDragController.isEnabled();
    mDragController.setEnabled(reorderEnabled);
    // TalkBack's Move up and Move down come and go with drag.
    if (changed) {
      for (int i = 0; i < mContentView.getChildCount(); i++) {
        View child = mContentView.getChildAt(i);
        if (child instanceof ShadowListCellView) {
          ((ShadowListCellView) child).updateAccessibilityActions();
        }
      }
    }
  }

  /*
   * TalkBack's Move up or Move down on a row, see ShadowListDragController.
   */
  boolean performAccessibilityMove(ShadowListCellView row, boolean up) {
    return mDragController.performAccessibilityMove(row, up);
  }

  boolean isReorderEnabled() {
    return mDragController.isEnabled();
  }

  /*
   * Lists that share a dragBoardId form a drag board, see ShadowListDragBoard. dragListId names
   * this list to JS in a drop, and dragListLabel to TalkBack in a row's Move to action.
   */
  public void setDragBoardId(@Nullable String dragBoardId) {
    mDragController.setDragBoardId(dragBoardId);
  }

  public void setDragListId(@Nullable String dragListId) {
    mDragController.setDragListId(dragListId);
  }

  public void setDragListLabel(@Nullable String dragListLabel) {
    mDragController.setDragListLabel(dragListLabel);
  }

  /*
   * The lists of the drag board TalkBack's Move to actions on a row name, see
   * ShadowListDragController.
   */
  List<ShadowListDragController> dragBoardMoveTargets() {
    return mDragController.boardMoveTargets();
  }

  boolean performAccessibilityMoveToList(ShadowListCellView row, ShadowListDragController target) {
    return mDragController.performAccessibilityMoveToList(row, target);
  }

  public void setNumberOfColumns(int numberOfColumns) {
    mNumberOfColumns = Math.max(1, numberOfColumns);
  }

  public void setStickyHeader(boolean stickyHeader) {
    mStickyController.setStickyHeader(stickyHeader);
  }

  public void setStickyFooter(boolean stickyFooter) {
    mStickyController.setStickyFooter(stickyFooter);
  }

  public void setAutoHideHeader(boolean autoHideHeader) {
    mStickyController.setAutoHideHeader(autoHideHeader);
  }

  public void setAutoHideFooter(boolean autoHideFooter) {
    mStickyController.setAutoHideFooter(autoHideFooter);
  }

  public void setHorizontal(boolean horizontal) {
    if (horizontal != mHorizontal) {
      mHorizontal = horizontal;
      mSync.setHorizontal(horizontal);
      installScrollView(horizontal);
    }
    mStickyController.applyStickyTransforms();
  }

  // endregion

  // region Pull to refresh

  public void setRefreshEnabled(boolean enabled) {
    mRefreshEnabled = enabled;
    if (mRefreshLayout != null) {
      mRefreshLayout.setEnabled(enabled);
    }
  }

  public void setRefreshing(boolean refreshing) {
    if (refreshing == refreshInProgress()) {
      return;
    }
    transitionRefresh(refreshing ? RefreshEvent.BEGIN : RefreshEvent.END);
    if (mRefreshLayout != null) {
      mRefreshLayout.setRefreshing(refreshing);
    }
    checkRefreshSettle();
  }

  /*
   * JS holds data that came during a refresh until the refresh ended and the list is at rest.
   * It asks once refreshing turned false. A command, not the prop, since a refresh shorter
   * than a mount never reaches the view as a prop.
   */
  public void awaitRefreshSettle() {
    transitionRefresh(RefreshEvent.AWAIT);
    checkRefreshSettle();
  }

  /*
   * Fire onRefreshSettle once the refresh ended, the spinner has hidden and nothing moves the
   * list. JS then adds the new rows to a list at rest, like on iOS. SwipeRefreshLayout tells no
   * one when its hide animation ends, so while anything still moves this checks again on the
   * next frame.
   */
  private boolean refreshInProgress() {
    return mRefreshPhase == RefreshPhase.REFRESHING || mRefreshPhase == RefreshPhase.REFRESHING_AWAITING;
  }

  private boolean refreshSettleRequested() {
    return mRefreshPhase == RefreshPhase.AWAITING || mRefreshPhase == RefreshPhase.REFRESHING_AWAITING;
  }

  private void transitionRefresh(RefreshEvent event) {
    switch (event) {
      case BEGIN:
        mRefreshPhase = refreshSettleRequested() ? RefreshPhase.REFRESHING_AWAITING : RefreshPhase.REFRESHING;
        break;
      case END:
        mRefreshPhase = refreshSettleRequested() ? RefreshPhase.AWAITING : RefreshPhase.REST;
        break;
      case AWAIT:
        mRefreshPhase = refreshInProgress() ? RefreshPhase.REFRESHING_AWAITING : RefreshPhase.AWAITING;
        break;
      case CHECK:
        removeCallbacks(mRefreshSettleRunnable);
        if (mRefreshPhase != RefreshPhase.AWAITING) return;
        if (isTouching() || isSettling() || refreshSpinnerShown()) {
          postOnAnimation(mRefreshSettleRunnable);
        } else {
          mRefreshPhase = RefreshPhase.SENT;
          emitRefreshEvent(ShadowListRefreshEvent.SETTLE_EVENT_NAME);
        }
        break;
      case RESET:
        mRefreshPhase = refreshInProgress() ? RefreshPhase.REFRESHING : RefreshPhase.REST;
        removeCallbacks(mRefreshSettleRunnable);
        break;
    }
  }

  private void checkRefreshSettle() {
    transitionRefresh(RefreshEvent.CHECK);
  }

  private final Runnable mRefreshSettleRunnable = this::checkRefreshSettle;

  /*
   * SwipeRefreshLayout's spinner is its child besides the scroll view. It is gone once hidden.
   */
  private boolean refreshSpinnerShown() {
    if (mRefreshLayout == null) {
      return false;
    }
    if (mRefreshLayout.isRefreshing()) {
      return true;
    }
    for (int i = 0; i < mRefreshLayout.getChildCount(); i++) {
      View child = mRefreshLayout.getChildAt(i);
      if (child != mScrollView && child.getVisibility() == View.VISIBLE) {
        return true;
      }
    }
    return false;
  }

  public void setRefreshColor(@Nullable Integer color) {
    mRefreshColor = color;
    if (mRefreshLayout != null && color != null) {
      mRefreshLayout.setColorSchemeColors(color);
    }
  }

  private void emitRefresh() {
    emitRefreshEvent(ShadowListRefreshEvent.EVENT_NAME);
  }

  private void emitRefreshEvent(String eventName) {
    slLog("java.emitRefreshEvent: " + eventName);
    ReactContext reactContext = (ReactContext) getContext();
    EventDispatcher dispatcher =
      UIManagerHelper.getEventDispatcherForReactTag(reactContext, getId());
    if (dispatcher != null) {
      dispatcher.dispatchEvent(
        new ShadowListRefreshEvent(UIManagerHelper.getSurfaceId(this), getId(), eventName));
    }
  }

  // endregion

  // region Mounting

  /*
   * Rows and templates go into the content view inside the scroll view.
   */
  public void addContentView(View child, int index) {
    if (child instanceof ShadowListCellView) {
      mContentView.addView(child, index);
      // The row's TalkBack actions include Move up and Move down while drag is on.
      ((ShadowListCellView) child).updateAccessibilityActions();
      // Pin again so sticky views stay above the new row.
      mStickyController.applyStickyTransforms();
      // A row that mounts during a drag, or while a dragged row hovers this list, must shift right away or it flashes.
      if (mDragController.needsMountShuffle()) {
        mDragController.applyDragShuffle();
      }
      return;
    }
    if (child instanceof ShadowListTemplateView) {
      mContentView.addView(child, index);
      ((ShadowListTemplateView) child).setListListeners(mTemplateLayoutListener, mTemplateTypeListener);
      mStickyController.invalidateTemplates();
    }
  }

  public int getContentChildCount() {
    return mContentView.getChildCount();
  }

  public View getContentChildAt(int index) {
    return mContentView.getChildAt(index);
  }

  public void removeContentViewAt(int index) {
    View child = mContentView.getChildAt(index);
    if (child instanceof ShadowListTemplateView) {
      ((ShadowListTemplateView) child).setListListeners(null, null);
      mStickyController.invalidateTemplates();
    }
    // A row the drag board hid is shown again before it goes.
    if (child != null) {
      mDragController.boardViewWillUnmount(child);
    }
    /*
     * The dragged row is going away, for example its data was deleted. Cancel the drag
     * first so touches don't go to a row that is no longer there.
     */
    if (child != null && child == mDragController.getDraggedView()) {
      mDragController.cancel();
    }
    if (child != null && child == mSwipedRow) {
      mSwipedRow = null;
    }
    mContentView.removeViewAt(index);
  }

  // endregion

  // region Touch

  @Override
  public boolean dispatchTouchEvent(MotionEvent event) {
    int action = event.getActionMasked();
    if (action == MotionEvent.ACTION_DOWN) {
      // Read before the inner scroll view sees the touch and stops the fling.
      mTouchStoppedFling = isSettling();
      mTouchedMenuCell = null;
      closeSwipedRowOutside(event);
    }
    mDragController.trackGesture(event);
    if (!mDragController.isEnabled()) {
      mMenuGestureDetector.onTouchEvent(event);
    }
    boolean handled = super.dispatchTouchEvent(event);
    if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
      mTouchedMenuCell = null;
      if (mSwipeClosingTouch) {
        mSwipeClosingTouch = false;
        NativeGestureUtil.notifyNativeGestureEnded(this, event);
      }
    }
    return handled;
  }

  @Override
  public boolean onInterceptTouchEvent(MotionEvent event) {
    if (mDragController.onInterceptTouchEvent(event)) {
      return true;
    }
    return super.onInterceptTouchEvent(event);
  }

  @Override
  public boolean onTouchEvent(MotionEvent event) {
    if (mDragController.onTouchEvent(event)) {
      return true;
    }
    return super.onTouchEvent(event);
  }

  void handleInnerTouchDown() {
    emitMomentumEndAfterLand();
    /*
     * The finger takes over from any scroll we started. Report the drag as the user.
     * The touch also stops any fling and settling ends here.
     */
    mSync.disarm();
    transitionMotion(MotionEventKind.DOWN);
    mDragEventSent = false;
    mDragEndedAtRest = false;
    mFlingVelocity = 0;
    if (mMomentumEventSent) {
      // The touch stopped the fling.
      mMomentumEventSent = false;
      emitScrollEvent(ShadowListScrollEvent.MOMENTUM_END, 0, 0);
    }
  }

  /*
   * The finger lifted. If no fling follows, snap to the nearest offset.
   */
  void handleInnerTouchUp() {
    transitionMotion(MotionEventKind.UP);
  }

  /*
   * After the scroll view handled the lift. Its fling, if any, has started, and anything it
   * posted for this frame is queued ahead. Snap on the next frame unless a fling took over.
   * The motion transition cancels the pending snap when that fling starts.
   */
  private void snapAfterLift() {
    transitionMotion(MotionEventKind.SNAP);
  }

  /*
   * Tell the core the finger is gone, even if nothing scrolls after it.
   * Without a fling, an inverted list near the bottom can pin back to it.
   * A fling reports settling instead, which keeps the pin off until it ends, like on iOS.
   */
  void reportTouchUpPhase() {
    if (mDragEventSent) {
      mDragEventSent = false;
      // Velocity runs toward the end of the content, in dp per millisecond.
      double velocity = PixelUtil.toDIPFromPixel(mFlingVelocity) / 1000.0;
      emitScrollEvent(ShadowListScrollEvent.END_DRAG, mHorizontal ? velocity : 0, mHorizontal ? 0 : velocity);
      if (isSettling()) {
        mMomentumEventSent = true;
        emitScrollEvent(ShadowListScrollEvent.MOMENTUM_BEGIN, mHorizontal ? velocity : 0, mHorizontal ? 0 : velocity);
      } else {
        mDragEndedAtRest = true;
      }
    }
    reportScrollPhase(isSettling() ? SCROLL_PHASE_SETTLING : SCROLL_PHASE_IDLE);
    snapAfterLift();
  }

  /*
   * A drag turns scrolling off and back on. On never overrides the scrollEnabled prop.
   */
  void setInnerScrollEnabled(boolean enabled) {
    boolean scrollEnabled = enabled && mScrollEnabled;
    if (mScrollView instanceof ReactScrollView) {
      ((ReactScrollView) mScrollView).setScrollEnabled(scrollEnabled);
    } else if (mScrollView instanceof ReactHorizontalScrollView) {
      ((ReactHorizontalScrollView) mScrollView).setScrollEnabled(scrollEnabled);
    }
  }

  // endregion

  // region Scrolling

  void handleInnerScroll(int scrollX, int scrollY) {
    if (scrollX == mPreviousScrollX && scrollY == mPreviousScrollY) {
      return;
    }
    mPreviousScrollX = scrollX;
    mPreviousScrollY = scrollY;
    // Rows move in and out of the drawn window, see ShadowListInnerScrollView.ContentContainer.
    mContentView.invalidate();
    if (isTouching() && !mDragEventSent) {
      mDragEventSent = true;
      // A scroll closes the open row. One swiped all the way stays out while its action runs.
      if (mSwipedRow != null && !mSwipedRow.isSwipedOut()) {
        mSwipedRow.closeSwipeActions(true);
      }
      emitScrollEvent(ShadowListScrollEvent.BEGIN_DRAG, 0, 0);
    }
    // Only real user scrolls move the hiding header and footer.
    boolean userScrolled = updateScrollState(scrollX, scrollY);
    mStickyController.applyStickyTransforms(userScrolled);
  }

  private boolean updateScrollState(int scrollX, int scrollY) {
    if (mState == null) {
      return false;
    }

    /*
     * Finger down, momentum or idle. The core keeps an inverted list from pinning to the
     * bottom until this is idle, see Container::gestureActive.
     */
    int scrollPhase = isTouching()
      ? SCROLL_PHASE_DRAGGING
      : (isSettling() ? SCROLL_PHASE_SETTLING : SCROLL_PHASE_IDLE);
    double offsetX = PixelUtil.toDIPFromPixel(scrollX);
    double offsetY = PixelUtil.toDIPFromPixel(scrollY);
    // Pull to refresh and drag to reorder lean on every frame.
    boolean commitEveryFrame = refreshInProgress() || refreshSettleRequested()
      || (mRefreshLayout != null && mRefreshLayout.isRefreshing())
      || mDragController.isDragging() || mDragController.ownsScrollOffset();

    /*
     * Every frame goes into the live report. Only frames the core needs become a state
     * update, which is a full commit. See ShadowListScrollSync.
     */
    boolean needsCommit = mSync.onScroll(offsetX, offsetY, scrollPhase, commitEveryFrame);
    boolean userScrolled = mSync.isFrameUserScrolled();
    emitScroll(offsetX, offsetY, scrollPhase == SCROLL_PHASE_IDLE && !userScrolled);

    if (DEBUG_LOG) {
      slLog(String.format("java.onScrollChanged: offset=(%.1f,%.1f) userScrolled=%b phase=%d commit=%b",
        offsetX, offsetY, userScrolled, scrollPhase, needsCommit));
    }
    if (needsCommit) {
      mState.updateState(mSync.patchMap());
    }
    if (mSync.isFrameLanded() && mSync.isLanding()) {
      landAnimatedCommand();
    }
    return userScrolled;
  }

  private double liveOffsetX() {
    return PixelUtil.toDIPFromPixel(mScrollView.getScrollX());
  }

  private double liveOffsetY() {
    return PixelUtil.toDIPFromPixel(mScrollView.getScrollY());
  }

  void reportScrollPhase(int scrollPhase) {
    if (mState == null) {
      return;
    }
    /*
     * Going idle also ends the user scroll, like clearUserScrolled on iOS. Updates merge into
     * the current state. Otherwise a stale userScrolled flag sticks around and the core
     * mistakes its own correction for the user moving the list and drops it.
     */
    if (scrollPhase == SCROLL_PHASE_IDLE) {
      if (mSync.clearUserScrolled(liveOffsetX(), liveOffsetY())) {
        mState.updateState(mSync.patchMap());
      }
      emitHeldScroll();
      return;
    }
    mSync.livePatch(liveOffsetX(), liveOffsetY(), mSync.isCurrentUserScrolled(), scrollPhase);
    mState.updateState(mSync.patchMap());
  }

  /*
   * A fling starts momentum. Report settling until the offset stops.
   */
  void handleInnerFling(int velocity) {
    mFlingVelocity = velocity;
    transitionMotion(MotionEventKind.FLING);
    if (mDragEndedAtRest && !isTouching() && !mMomentumEventSent) {
      // The fling came after the lift reported its phase. JS still hears the momentum begin.
      mDragEndedAtRest = false;
      mMomentumEventSent = true;
      double dp = PixelUtil.toDIPFromPixel(velocity) / 1000.0;
      emitScrollEvent(ShadowListScrollEvent.MOMENTUM_BEGIN, mHorizontal ? dp : 0, mHorizontal ? 0 : dp);
      reportScrollPhase(SCROLL_PHASE_SETTLING);
    }
  }

  private ScrollMotion gestureMotion() {
    return mMotion instanceof CommandLanding ? ((CommandLanding) mMotion).gesture : mMotion;
  }

  private boolean isTouching() {
    ScrollMotion gesture = gestureMotion();
    return gesture == ScrollRest.TOUCHING || gesture == ScrollRest.TOUCHING_FLUNG ||
      (gesture instanceof ScrollSettling && ((ScrollSettling) gesture).kind == SettleKind.TOUCHING_FLING);
  }

  private boolean isSettling() {
    return gestureMotion() instanceof ScrollSettling;
  }

  private void stopSettling() {
    transitionMotion(MotionEventKind.STOP);
  }

  /*
   * An Animate correction can mount after a finger lifts into a fling. Landing wraps the
   * gesture instead of replacing it, and both observe the same offsets on each frame.
   */
  private void transitionMotion(MotionEventKind event) {
    ScrollMotion gesture = gestureMotion();
    boolean touching = isTouching();
    boolean landing = mMotion instanceof CommandLanding && mSync.isLanding();
    MotionSample sample = gesture instanceof ScrollSettling ? ((ScrollSettling) gesture).sample
      : mMotion instanceof CommandLanding ? ((CommandLanding) mMotion).sample : null;
    boolean finishFling = false;
    boolean finishCommand = false;
    int snapTarget = -1;
    removeCallbacks(mMotionRunnable);
    switch (event) {
      case DOWN:
        gesture = ScrollRest.TOUCHING;
        landing = false;
        break;
      case UP:
        if (gesture instanceof ScrollSettling) {
          gesture = new ScrollSettling(SettleKind.FLING, sample);
        } else {
          gesture = gesture == ScrollRest.TOUCHING_FLUNG || gesture == ScrollRest.FLUNG
            ? ScrollRest.FLUNG : ScrollRest.LIFTED;
        }
        break;
      case FLING:
        sample = new MotionSample(mScrollView.getScrollX(), mScrollView.getScrollY());
        gesture = new ScrollSettling(touching ? SettleKind.TOUCHING_FLING : SettleKind.FLING, sample);
        break;
      case STOP:
        if (gesture instanceof ScrollSettling || gesture == ScrollRest.TOUCHING_FLUNG || gesture == ScrollRest.FLUNG) {
          gesture = touching ? ScrollRest.TOUCHING_FLUNG : ScrollRest.FLUNG;
        } else {
          gesture = touching ? ScrollRest.TOUCHING : ScrollRest.IDLE;
        }
        landing = false;
        break;
      case RESET:
        gesture = ScrollRest.IDLE;
        landing = false;
        break;
      case SNAP:
        if (gesture == ScrollRest.LIFTED && mSnapToItem && mSnapOffsetsPx.length > 0) {
          gesture = ScrollRest.SNAP_PENDING;
        }
        break;
      case COMMAND:
        landing = mSync.isLanding();
        sample = new MotionSample(mScrollView.getScrollX(), mScrollView.getScrollY());
        if (gesture instanceof ScrollSettling) {
          gesture = new ScrollSettling(((ScrollSettling) gesture).kind, sample);
        }
        break;
      case COMMAND_ENDED:
        landing = false;
        break;
      case FRAME:
        if (gesture == ScrollRest.SNAP_PENDING) {
          gesture = ScrollRest.IDLE;
          if (mSnapToItem && mSnapOffsetsPx.length > 0) {
            int current = mHorizontal ? mScrollView.getScrollX() : mScrollView.getScrollY();
            int target = nearestSnapOffsetPx(current);
            if (target != current) snapTarget = target;
          }
        }
        if (!(gesture instanceof ScrollSettling) && !landing) break;
        int x = mScrollView.getScrollX();
        int y = mScrollView.getScrollY();
        if (x != sample.previousX || y != sample.previousY) {
          sample.previousX = x;
          sample.previousY = y;
          sample.stableFrames = 0;
        } else if (++sample.stableFrames >= SETTLE_STABLE_FRAMES) {
          finishFling = gesture instanceof ScrollSettling;
          finishCommand = landing;
          if (finishFling) gesture = touching ? ScrollRest.TOUCHING_FLUNG : ScrollRest.FLUNG;
          landing = false;
        }
        break;
    }
    mMotion = landing ? new CommandLanding(gesture, sample) : gesture;
    if (landing || gesture instanceof ScrollSettling || gesture == ScrollRest.SNAP_PENDING) {
      postOnAnimation(mMotionRunnable);
    }
    if (snapTarget >= 0) smoothSnapTo(snapTarget);
    if (finishFling) {
      reportScrollPhase(SCROLL_PHASE_IDLE);
      if (mMomentumEventSent) {
        mMomentumEventSent = false;
        emitScrollEvent(ShadowListScrollEvent.MOMENTUM_END, 0, 0);
      }
    }
    if (finishCommand) landAnimatedCommand();
  }

  private final Runnable mMotionRunnable = () -> transitionMotion(MotionEventKind.FRAME);

  // endregion

  // region Snapping

  /*
   * Predict where the fling lands and glide to the nearest snap offset.
   */
  boolean snapFling(int velocity) {
    if (!mSnapToItem || mSnapOffsetsPx.length == 0) {
      return false;
    }
    int start = mHorizontal ? mScrollView.getScrollX() : mScrollView.getScrollY();
    int max = mHorizontal
      ? Math.max(0, mContentView.getWidth() - mScrollView.getWidth())
      : Math.max(0, mContentView.getHeight() - mScrollView.getHeight());
    OverScroller scroller = new OverScroller(getContext());
    if (mHorizontal) {
      scroller.fling(start, 0, velocity, 0, 0, max, 0, 0);
    } else {
      scroller.fling(0, start, 0, velocity, 0, 0, 0, max);
    }
    int predicted = mHorizontal ? scroller.getFinalX() : scroller.getFinalY();
    smoothSnapTo(nearestSnapOffsetPx(predicted));
    return true;
  }

  private int nearestSnapOffsetPx(int target) {
    return ShadowListGeometry.nearestSnapOffsetPx(mSnapOffsetsPx, mSnapOffsetsPx.length, target);
  }

  private void smoothSnapTo(int target) {
    int targetX = mHorizontal ? target : mScrollView.getScrollX();
    int targetY = mHorizontal ? mScrollView.getScrollY() : target;
    mSync.arm(PixelUtil.toDIPFromPixel(targetX), PixelUtil.toDIPFromPixel(targetY), true);
    if (mScrollView instanceof ReactScrollView) {
      ((ReactScrollView) mScrollView).smoothScrollTo(targetX, targetY);
    } else if (mScrollView instanceof ReactHorizontalScrollView) {
      ((ReactHorizontalScrollView) mScrollView).smoothScrollTo(targetX, targetY);
    }
  }

  // endregion

  // region Swipe actions

  /*
   * A row starts swiping. Any other open row closes.
   */
  void swipeRowOpened(ShadowListCellView row) {
    ShadowListCellView previous = mSwipedRow;
    mSwipedRow = row;
    if (previous != null && previous != row) {
      previous.closeSwipeActions(false);
    }
  }

  void swipeRowClosed(ShadowListCellView row) {
    if (mSwipedRow == row) {
      mSwipedRow = null;
    }
  }

  /*
   * A touch that starts outside the open row closes it instead of pressing another row, like on
   * iOS. A drag from there still scrolls the list.
   */
  private void closeSwipedRowOutside(MotionEvent event) {
    mSwipeClosingTouch = false;
    ShadowListCellView row = mSwipedRow;
    if (row == null || row.isSwipedOut()) {
      return;
    }
    float contentX = event.getX() + mScrollView.getScrollX();
    float contentY = event.getY() + mScrollView.getScrollY();
    if (mDragController.cellViewAtContentPoint(contentX, contentY) == row) {
      return;
    }
    row.closeSwipeActions(true);
    mSwipeClosingTouch = true;
    NativeGestureUtil.notifyNativeGestureStarted(this, event);
  }

  boolean isReorderDragging() {
    return mDragController.isDragging();
  }

  /*
   * Slide every swiped row back.
   */
  public void closeSwipeActions() {
    for (int child = 0; child < mContentView.getChildCount(); child++) {
      View view = mContentView.getChildAt(child);
      if (view instanceof ShadowListCellView) {
        ((ShadowListCellView) view).closeSwipeActions(true);
      }
    }
  }

  // endregion

  // region Context menu

  /*
   * A touch came down on a cell with a menu inside this list. Cells are told outermost first, so
   * the innermost one is left, and a list around this one gives the long press up, as on iOS,
   * where the innermost menu wins.
   */
  void menuCellTouched(ShadowListCellView cell) {
    mTouchedMenuCell = cell;
    for (ViewParent parent = getParent(); parent != null; parent = parent.getParent()) {
      if (parent instanceof ShadowListView) {
        ((ShadowListView) parent).mTouchedMenuCell = null;
      }
    }
  }

  /*
   * Show the context menu of the cell under a long press. Drag to reorder owns the long press
   * when it is on. Its rows show no menu, and neither does a touch that stopped a fling. A cell
   * that left the window during the hold has nothing to anchor a menu to.
   */
  private void showRowMenu(MotionEvent event) {
    if (mSwipedRow != null || mSwipeClosingTouch || mTouchStoppedFling || mDragController.isEnabled()) {
      return;
    }
    ShadowListCellView cell = mTouchedMenuCell;
    if (cell != null && cell.isAttachedToWindow()) {
      cell.showRowMenu(event);
    }
  }

  // endregion

  // region State

  public void updateState(@Nullable StateWrapper stateWrapper) {
    mState = stateWrapper;

    if (mState == null) {
      return;
    }

    /*
     * Read the scalars from the state's MapBuffer. getStateData would copy the whole state,
     * including the sticky and snap lists, into a map on every mount. The lists are only
     * copied when their version moved.
     */
    ReadableMapBuffer mapBuffer = mState.getStateDataMapBuffer();
    if (mapBuffer == null) {
      return;
    }

    mLiveHandle = mapBuffer.getLong(STATE_LIVE_HANDLE);
    mMountTicker.setHandle(mapBuffer.getLong(STATE_MOUNT_HANDLE));
    boolean offsetEnabled = mapBuffer.getBoolean(STATE_OFFSET_ENABLED);
    mSync.beginMount(
      mLiveHandle,
      offsetEnabled,
      mapBuffer.getDouble(STATE_OFFSET_X),
      mapBuffer.getDouble(STATE_OFFSET_Y),
      mapBuffer.getDouble(STATE_OFFSET_BASE_X),
      mapBuffer.getDouble(STATE_OFFSET_BASE_Y),
      mapBuffer.getDouble(STATE_COMMIT_TOKEN),
      mapBuffer.getDouble(STATE_COMMIT_REVISION),
      mapBuffer.getBoolean(STATE_USER_SCROLLED),
      mapBuffer.getDouble(STATE_SCROLL_PHASE),
      mapBuffer.getDouble(STATE_CONCEAL_GENERATION),
      mapBuffer.getDouble(STATE_COMMAND_SEQUENCE),
      mapBuffer.getDouble(STATE_ANIMATION_SEQUENCE),
      mapBuffer.getDouble(STATE_ANIMATION_OFFSET),
      mapBuffer.getDouble(STATE_BAND_LOW),
      mapBuffer.getDouble(STATE_BAND_HIGH));
    try {
      applyMountedState(mapBuffer, offsetEnabled);
    } finally {
      mSync.endMount();
    }
    // The command has now mounted and applied its exact offset, not the animation estimate.
    if (mLandCommandSequence > 0 && mapBuffer.getDouble(STATE_COMMAND_SEQUENCE) >= mLandCommandSequence
        && !mapBuffer.getBoolean(STATE_COMMAND_PENDING)) {
      emitMomentumEndAfterLand();
    }
    transitionAnchorRequest(AnchorEvent.READY);
    pokeMountTicker();
  }

  /*
   * Every commit that leaves work for a frame mounts with new props or a new state: a mounted
   * range applied with more overscan to step in, or a viewability report waiting for
   * minimumViewTime.
   */
  void pokeMountTicker() {
    mMountTicker.poke();
  }

  private void applyMountedState(ReadableMapBuffer mapBuffer, boolean offsetEnabled) {
    long stickyVersion = mapBuffer.getLong(STATE_STICKY_VERSION);
    long snapVersion = mapBuffer.getLong(STATE_SNAP_VERSION);
    // Versions count per list. A different list always reads its lists again.
    boolean sameList = mGeometryHandle == mLiveHandle && mLiveHandle != 0;
    boolean stickyStale = !sameList || stickyVersion != mStickyVersion;
    boolean snapStale = !sameList || snapVersion != mSnapVersion;
    if (stickyStale || snapStale) {
      // The lists come from the state just read, through its mount driver.
      long mountHandle = mapBuffer.getLong(STATE_MOUNT_HANDLE);
      if (stickyStale) {
        mStickyController.cacheStickyGeometry(mountHandle);
      }
      if (snapStale) {
        int count = ShadowListGeometry.snapCount(mountHandle);
        if (mSnapOffsetsDip.length < count) {
          mSnapOffsetsDip = new double[count];
        }
        count = ShadowListGeometry.copySnapOffsets(mountHandle, mSnapOffsetsDip);
        if (mSnapOffsetsPx.length != count) {
          mSnapOffsetsPx = new float[count];
        }
        for (int i = 0; i < count; i++) {
          mSnapOffsetsPx[i] = PixelUtil.toPixelFromDIP((float) mSnapOffsetsDip[i]);
        }
      }
      mGeometryHandle = mLiveHandle;
      mStickyVersion = stickyVersion;
      mSnapVersion = snapVersion;
    }

    if (DEBUG_LOG) {
      slLog(String.format("java.updateState: contentSize=(%.1f,%.1f) enabled=%d offset=(%.1f,%.1f) curOffset=(%.1f,%.1f) band=(%.1f,%.1f)",
        mapBuffer.getDouble(STATE_TOTAL_WIDTH), mapBuffer.getDouble(STATE_TOTAL_HEIGHT),
        offsetEnabled ? 1 : 0,
        mapBuffer.getDouble(STATE_OFFSET_X), mapBuffer.getDouble(STATE_OFFSET_Y),
        liveOffsetX(), liveOffsetY(),
        mapBuffer.getDouble(STATE_BAND_LOW), mapBuffer.getDouble(STATE_BAND_HIGH)));
    }

    float contentWidth = (float) mapBuffer.getDouble(STATE_TOTAL_WIDTH);
    float contentHeight = (float) mapBuffer.getDouble(STATE_TOTAL_HEIGHT);

    int newContentWidth = (int) PixelUtil.toPixelFromDIP(contentWidth);
    int newContentHeight = (int) PixelUtil.toPixelFromDIP(contentHeight);

    // Skip the layout call when the size is the same, which it is on most mounts.
    if (mContentView.getLeft() != 0 || mContentView.getTop() != 0
        || mContentView.getWidth() != newContentWidth || mContentView.getHeight() != newContentHeight) {
      // Shorter content clamps the scroll position here, and that report is not the user.
      mSync.setApplyingContentSize(true);
      try {
        mContentView.layout(0, 0, newContentWidth, newContentHeight);
      } finally {
        mSync.setApplyingContentSize(false);
      }
    }

    applyCorrection();

    // Pin again, since the footer position depends on the content size.
    mStickyController.applyStickyTransforms();

    // Keep a dragged row under the finger.
    mDragController.onStateCommitted();
  }

  /*
   * Apply the mounted correction, see ScrollSync::correction. While a row is dragged the drag
   * owns the offset and corrections wait.
   */
  private void applyCorrection() {
    int beforeX = mScrollView.getScrollX();
    int beforeY = mScrollView.getScrollY();
    float maxOffsetPx = mHorizontal
      ? Math.max(0, mContentView.getWidth() - mScrollView.getWidth())
      : Math.max(0, mContentView.getHeight() - mScrollView.getHeight());
    int action = mSync.correction(
      PixelUtil.toDIPFromPixel(beforeX), PixelUtil.toDIPFromPixel(beforeY), 0.0,
      PixelUtil.toDIPFromPixel(maxOffsetPx), isTouching(), isTouching() || isSettling(),
      mDragController.ownsScrollOffset());
    if (action == ShadowListScrollSync.ACTION_ANIMATE) {
      animateCommandTo(mSync.getActionX(), mSync.getActionY());
      return;
    }
    if (action != ShadowListScrollSync.ACTION_WRITE) {
      return;
    }
    int appliedX = Math.round(PixelUtil.toPixelFromDIP((float) mSync.getActionX()));
    int appliedY = Math.round(PixelUtil.toPixelFromDIP((float) mSync.getActionY()));
    // scrollTo calls onScrollChanged right away, which must know the move was ours.
    mSync.willWrite();
    /*
     * A plain scrollTo leaves the fling running, and its next tick writes its own position over
     * ours. A prepend during a fling at the top would lose its correction as the bounce pulls
     * the view back to 0. Rebuild the fling from the corrected offset instead, like React
     * Native's maintainVisibleContentPosition does.
     */
    boolean preserveMomentum = mSync.actionPreservesMomentum();
    if (preserveMomentum && mScrollView instanceof ReactScrollView) {
      ((ReactScrollView) mScrollView).scrollToPreservingMomentum(appliedX, appliedY);
    } else if (preserveMomentum && mScrollView instanceof ReactHorizontalScrollView) {
      ((ReactHorizontalScrollView) mScrollView).scrollToPreservingMomentum(appliedX, appliedY);
    } else {
      mScrollView.scrollTo(appliedX, appliedY);
    }
    mSync.didWrite(Math.abs(mScrollView.getScrollX() - beforeX) > 1
      || Math.abs(mScrollView.getScrollY() - beforeY) > 1);
  }

  /*
   * Send a drag event with the live offset, like a scroll report.
   */
  void dispatchDragEvent(
    int type, String sourceKey, String destinationKey, String destinationListId, int destinationIndex,
    double sequence) {
    if (mState == null) {
      return;
    }
    // Keep core scroll corrections off during the drag. The end event turns them back on.
    mSync.livePatch(liveOffsetX(), liveOffsetY(), type != ShadowListDragController.DRAG_EVENT_END, mSync.getCurrentScrollPhase());
    WritableMap map = mSync.patchMap();
    map.putDouble("dragEventSequence", sequence);
    map.putDouble("dragEventType", type);
    map.putString("dragSourceKey", sourceKey != null ? sourceKey : "");
    map.putString("dragDestinationKey", destinationKey != null ? destinationKey : "");
    // The state keeps a key the update leaves out. Every event sends both.
    map.putString("dragDestinationListId", destinationListId != null ? destinationListId : "");
    map.putDouble("dragDestinationIndex", destinationIndex);
    if (DEBUG_LOG) {
      slLog("java.drag dispatch type=" + type + " from=" + sourceKey + " to=" + destinationKey
        + " list=" + destinationListId + " index=" + destinationIndex);
    }
    mState.updateState(map);
  }

  public void setStartReachedEnabled(boolean enabled) {
    if (mState == null) {
      return;
    }
    mSync.livePatch(liveOffsetX(), liveOffsetY());
    WritableMap map = mSync.patchMap();
    map.putBoolean("startReachedEnabled", enabled);
    if (DEBUG_LOG) {
      slLog("java.cmd setStartReachedEnabled: enabled=" + (enabled ? 1 : 0));
    }
    mState.updateState(map);
  }

  public void setEndReachedEnabled(boolean enabled) {
    if (mState == null) {
      return;
    }
    mSync.livePatch(liveOffsetX(), liveOffsetY());
    WritableMap map = mSync.patchMap();
    map.putBoolean("endReachedEnabled", enabled);
    if (DEBUG_LOG) {
      slLog("java.cmd setEndReachedEnabled: enabled=" + (enabled ? 1 : 0));
    }
    mState.updateState(map);
  }

  // endregion

  // region Scroll commands

  /*
   * Stop a fling or snap and forget any scroll of ours still waiting for its echo.
   */
  void stopMomentum() {
    if (mScrollView instanceof ReactScrollView) {
      ((ReactScrollView) mScrollView).abortAnimation();
    } else if (mScrollView instanceof ReactHorizontalScrollView) {
      ((ReactHorizontalScrollView) mScrollView).abortAnimation();
    }
    stopSettling();
    mSync.momentumStopped();
  }

  /*
   * A scroll command stops any fling or snap. It then can't overwrite the offset the core is
   * about to apply, and reports idle with the command. If a finger is down it stays dragging,
   * and the core lets the drag cancel the command. An animated command first gets the core's
   * estimate, see animateCommandTo.
   */
  void issueScrollCommand(double row, double viewPosition, double viewOffset, boolean animated) {
    emitMomentumEndAfterLand();
    boolean yielded = !isTouching();
    if (yielded) {
      stopMomentum();
    }
    transitionMotion(MotionEventKind.COMMAND_ENDED);
    mSync.issueCommand(row, viewPosition, viewOffset, animated, liveOffsetX(), liveOffsetY(), yielded);
    if (DEBUG_LOG) {
      slLog("java.cmd scroll: index=" + row + " viewPosition=" + viewPosition + " animated=" + animated);
    }
    mState.updateState(mSync.patchMap());
  }

  public void scrollToRow(int row, double viewPosition, double viewOffset, boolean animated) {
    if (mState == null) {
      return;
    }
    // The core rests the row viewOffset past its view position. React Native's viewOffset moves the other way.
    double coreViewOffset = Double.isNaN(viewOffset) || Double.isInfinite(viewOffset) ? 0 : -viewOffset;
    issueScrollCommand((double) row, viewPosition, coreViewOffset, animated);
  }

  public void scrollToOffset(double offset, boolean animated) {
    /*
     * An animated scroll goes through the core like scrollToRow. A smooth scroll would stop at
     * the first correction a row measured on the way sends.
     */
    if (animated && mState != null) {
      issueScrollCommand(SCROLL_TO_OFFSET_SLOT, 0.0, offset, true);
      return;
    }
    // Marked as ours so its frames don't count as user scrolls.
    int offsetPx = (int) PixelUtil.toPixelFromDIP((float) offset);
    int targetX = mHorizontal ? offsetPx : mScrollView.getScrollX();
    int targetY = mHorizontal ? mScrollView.getScrollY() : offsetPx;
    mSync.arm(PixelUtil.toDIPFromPixel(targetX), PixelUtil.toDIPFromPixel(targetY), animated);
    if (animated) {
      if (mScrollView instanceof ReactScrollView) {
        ((ReactScrollView) mScrollView).smoothScrollTo(targetX, targetY);
      } else {
        ((ReactHorizontalScrollView) mScrollView).smoothScrollTo(targetX, targetY);
      }
    } else {
      mScrollView.scrollTo(targetX, targetY);
    }
  }

  public void scrollToEnd(boolean animated) {
    if (mState == null) {
      return;
    }
    /*
     * The core keeps adjusting as rows get measured instead of jumping to an estimated bottom.
     */
    issueScrollCommand(SCROLL_TO_END_SLOT, 0.0, 0.0, animated);
  }

  /*
   * Animate to where the core estimates an animated command lands. The frames are ours, and
   * reaching the target lands the command exactly.
   */
  private void animateCommandTo(double targetDipX, double targetDipY) {
    int targetX = Math.round(PixelUtil.toPixelFromDIP((float) targetDipX));
    int targetY = Math.round(PixelUtil.toPixelFromDIP((float) targetDipY));
    int maxX = Math.max(0, mContentView.getWidth() - mScrollView.getWidth());
    int maxY = Math.max(0, mContentView.getHeight() - mScrollView.getHeight());
    targetX = Math.min(Math.max(targetX, 0), maxX);
    targetY = Math.min(Math.max(targetY, 0), maxY);
    int along = mHorizontal ? targetX - mScrollView.getScrollX() : targetY - mScrollView.getScrollY();
    if (Math.abs(along) <= PROGRAMMATIC_SCROLL_TOLERANCE_PX) {
      landAnimatedCommand();
      return;
    }
    mSync.arm(PixelUtil.toDIPFromPixel(targetX), PixelUtil.toDIPFromPixel(targetY), true);
    transitionMotion(MotionEventKind.COMMAND);
    if (mScrollView instanceof ReactScrollView) {
      ((ReactScrollView) mScrollView).smoothScrollTo(targetX, targetY);
    } else if (mScrollView instanceof ReactHorizontalScrollView) {
      ((ReactHorizontalScrollView) mScrollView).smoothScrollTo(targetX, targetY);
    }
  }

  /*
   * Send an animated command again without the animation, which lands it exactly.
   */
  private void landAnimatedCommand() {
    transitionMotion(MotionEventKind.COMMAND_ENDED);
    if (mState == null || !mSync.land(liveOffsetX(), liveOffsetY())) {
      return;
    }
    if (DEBUG_LOG) {
      slLog("java.cmd land: offset=(" + liveOffsetX() + "," + liveOffsetY() + ")");
    }
    mLandCommandSequence = mSync.getCommandSequence();
    mState.updateState(mSync.patchMap());
  }

  /*
   * Finish a command once its landing mounts, or once a finger or newer command takes over.
   */
  private void emitMomentumEndAfterLand() {
    if (mLandCommandSequence == 0) {
      return;
    }
    mLandCommandSequence = 0;
    emitScrollEvent(ShadowListScrollEvent.MOMENTUM_END, 0, 0);
  }

  public void flashScrollIndicators() {
    if (mScrollView instanceof ReactScrollView) {
      ((ReactScrollView) mScrollView).flashScrollIndicators();
    } else if (mScrollView instanceof ReactHorizontalScrollView) {
      ((ReactHorizontalScrollView) mScrollView).flashScrollIndicators();
    }
  }

  /*
   * Ask the core for the row at the viewport start. The answer comes back as onAnchorState.
   */
  public void requestAnchorState() {
    transitionAnchorRequest(AnchorEvent.REQUEST);
  }

  private void transitionAnchorRequest(AnchorEvent event) {
    if (event == AnchorEvent.RESET) {
      mAnchorRequest = AnchorRequest.IDLE;
      return;
    }
    if (event == AnchorEvent.READY && mAnchorRequest != AnchorRequest.PARKED) return;
    if (mState == null) {
      mAnchorRequest = AnchorRequest.PARKED;
      return;
    }
    mAnchorRequest = AnchorRequest.IDLE;
    mSync.requestAnchor(liveOffsetX(), liveOffsetY());
    mState.updateState(mSync.patchMap());
  }

  // endregion

  // region Scroll events

  /*
   * onScroll for one scroll callback, inside scrollEventThrottle. A frame of a list at rest, like
   * a correction or a scroll command, also sends the event the throttle held back.
   */
  private void emitScroll(double offsetX, double offsetY, boolean atRest) {
    if (!mScrollEventEnabled) {
      return;
    }
    if (mScrollTracker.track(offsetX, offsetY)) {
      dispatchScrollEvent(ShadowListScrollEvent.SCROLL, offsetX, offsetY,
        mScrollTracker.getVelocityX(), mScrollTracker.getVelocityY());
    } else if (atRest) {
      emitHeldScroll();
    }
  }

  /*
   * The list came to rest with a scroll event still held back by scrollEventThrottle. JS would
   * otherwise keep an offset from before the scroll stopped.
   */
  private void emitHeldScroll() {
    if (mScrollEventEnabled && mScrollTracker.takeTrailing()) {
      dispatchScrollEvent(ShadowListScrollEvent.SCROLL, mScrollTracker.getPendingX(), mScrollTracker.getPendingY(), 0, 0);
    }
  }

  /*
   * Send a drag or momentum event with the same payload as onScroll. Velocity is in dp per
   * millisecond.
   */
  private void emitScrollEvent(String eventName, double velocityX, double velocityY) {
    dispatchScrollEvent(eventName, liveOffsetX(), liveOffsetY(), velocityX, velocityY);
  }

  private void dispatchScrollEvent(
    String eventName, double offsetX, double offsetY, double velocityX, double velocityY) {
    if (DEBUG_LOG && ShadowListScrollEvent.MOMENTUM_END.equals(eventName)) {
      slLog("java.event " + eventName + ": time=" + android.os.SystemClock.uptimeMillis()
        + " seq=" + mSync.getCommandSequence() + " offset=(" + offsetX + "," + offsetY
        + ") physical=(" + liveOffsetX() + "," + liveOffsetY() + ")");
    }
    ReactContext reactContext = (ReactContext) getContext();
    EventDispatcher dispatcher = UIManagerHelper.getEventDispatcherForReactTag(reactContext, getId());
    if (dispatcher == null) {
      return;
    }
    dispatcher.dispatchEvent(new ShadowListScrollEvent(
      UIManagerHelper.getSurfaceId(this),
      getId(),
      eventName,
      offsetX,
      offsetY,
      PixelUtil.toDIPFromPixel(mContentView.getWidth()),
      PixelUtil.toDIPFromPixel(mContentView.getHeight()),
      PixelUtil.toDIPFromPixel(mScrollView.getWidth()),
      PixelUtil.toDIPFromPixel(mScrollView.getHeight()),
      velocityX,
      velocityY));
  }

  // endregion

  // region Accessors

  /*
   * Used by the sticky and drag controllers and the accessibility delegate.
   */
  ViewGroup getContentView() {
    return mContentView;
  }

  ViewGroup getScrollView() {
    return mScrollView;
  }

  boolean isScrollEnabled() {
    return mScrollEnabled;
  }

  boolean isHorizontal() {
    return mHorizontal;
  }

  int getNumberOfColumns() {
    return mNumberOfColumns;
  }

  @Nullable StateWrapper getStateWrapper() {
    return mState;
  }

  // endregion
}
