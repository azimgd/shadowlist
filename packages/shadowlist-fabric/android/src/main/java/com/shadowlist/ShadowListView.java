package com.shadowlist;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.OverScroller;

import androidx.annotation.Nullable;
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout;

import com.facebook.react.bridge.ReactContext;
import com.facebook.react.bridge.ReadableArray;
import com.facebook.react.bridge.ReadableMap;
import com.facebook.react.bridge.WritableMap;
import com.facebook.react.common.mapbuffer.ReadableMapBuffer;
import com.facebook.react.uimanager.PixelUtil;
import com.facebook.react.uimanager.StateWrapper;
import com.facebook.react.uimanager.UIManagerHelper;
import com.facebook.react.uimanager.events.EventDispatcher;
import com.facebook.react.views.scroll.ReactHorizontalScrollView;
import com.facebook.react.views.scroll.ReactScrollView;

/*
 * Hosts the content in an inner scroll view for the chosen axis. Handles scrolling, state
 * sync and scroll commands. Sticky pinning and drag to reorder live in their own controllers,
 * which use the accessors at the bottom.
 */
public class ShadowListView extends FrameLayout {
  // Trace logging for state sync. Filter with adb logcat -s SL
  static final boolean DEBUG_LOG = false;
  private static final String LOG_TAG = "SL";

  static void slLog(String message) {
    if (DEBUG_LOG) {
      Log.d(LOG_TAG, "[SL] " + message);
    }
  }

  /*
   * Values for the scrollPhase state key, matching SCROLL_PHASE_* in host/LiveScroll.hpp.
   * Idle, finger down, or momentum running.
   */
  private static final double SCROLL_PHASE_IDLE = 0.0;
  private static final double SCROLL_PHASE_DRAGGING = 1.0;
  private static final double SCROLL_PHASE_SETTLING = 2.0;
  /*
   * The scrollToIndex index that means the end, matching SCROLL_TO_END_INDEX in Constants.hpp.
   */
  private static final double SCROLL_TO_END_INDEX = -3.0;

  /*
   * Keys of the state MapBuffer, matching ShadowListStateKey in ShadowListViewState.h.
   */
  private static final int STATE_TOTAL_WIDTH = 0;
  private static final int STATE_TOTAL_HEIGHT = 1;
  private static final int STATE_OFFSET_ENABLED = 2;
  private static final int STATE_OFFSET_X = 3;
  private static final int STATE_OFFSET_Y = 4;
  private static final int STATE_COMMIT_TOKEN = 5;
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

  private @Nullable StateWrapper mState = null;

  /*
   * The mounted list's live report handle, and the versions that tell when its sticky and
   * snap lists changed, so they are only copied then.
   */
  private long mLiveHandle = 0;
  private long mStickyVersion = -1;
  private long mSnapVersion = -1;
  private long mGeometryHandle = 0;

  /*
   * Live reports, which frames commit, the echo of core corrections and scroll commands.
   * See ShadowListScrollSync.
   */
  private final ShadowListScrollSync mSync;
  private ContentContainer mContentView;
  private ViewGroup mScrollView;
  private final ShadowListStickyController mStickyController;
  private final ShadowListDragController mDragController;
  /*
   * Grid columns from props, which drags move rows across.
   */
  private int mColumns = 1;

  /*
   * Pin sticky views again when a header or footer is laid out, so a sticky footer follows
   * its real position when the list size changes.
   */
  private final View.OnLayoutChangeListener mTemplateLayoutListener;
  // A mounted template changed type, so the sticky views must be found again.
  private final Runnable mTemplateTypeListener;

  // The scroll axis. Changing it rebuilds the inner scroll view.
  private boolean mHorizontal = false;

  /*
   * Snapping. The offsets come from the core, converted to pixels. mTouching keeps the
   * snap from fighting a finger that is still down.
   */
  private boolean mSnapToItem = false;
  private float[] mSnapOffsetsPx = new float[0];
  private boolean mTouching = false;

  /*
   * Momentum after a fling, reported to the core as settling. Android has no callback for
   * the end of a fling, so we poll the offset like ReactScrollView does and report idle once
   * it has stayed still for a few polls in a row.
   */
  private boolean mSettling = false;
  private int mSettleStableFrames = 0;
  private int mSettleLastScrollX = 0;
  private int mSettleLastScrollY = 0;
  private static final long SETTLE_POLL_DELAY_MS = 20;
  private static final int SETTLE_STABLE_FRAMES = 3;

  // Pull to refresh. Kept here so rebuilding the scroll view for a new axis restores it.
  @Nullable private SwipeRefreshLayout mRefreshLayout = null;
  private boolean mRefreshEnabled = false;
  private boolean mRefreshing = false;
  @Nullable private Integer mRefreshColor = null;
  /*
   * Set when refreshing ends. Cleared when onRefreshSettle fires, once the spinner is gone
   * and the list is at rest.
   */
  private boolean mRefreshAwaitingSettle = false;
  // The spinner takes about 200ms to hide. Check just after that, then poll until at rest.
  private static final long REFRESH_SETTLE_DELAY_MS = 250;

  /*
   * How close our own scroll must land to its target to count as reaching it. Only used to
   * spot the end of our own animated scrolls like snapping, and for the exact dp echo.
   * Core corrections are matched by cause, not by this tolerance.
   */
  private static final int PROGRAMMATIC_SCROLL_TOLERANCE_PX = 2;

  /*
   * Draws only the children that reach into the scroll viewport. Overscan rows stay mounted
   * so a fling finds them ready, but drawing them anyway made the render thread sync and draw
   * every mounted row each frame (about 2.5 ms a frame on a feed with the default overscan).
   * The host invalidates this on every scroll, which only re-records this list of children.
   */
  private static class ContentContainer extends ViewGroup {
    private int mDrawLow = Integer.MIN_VALUE;
    private int mDrawHigh = Integer.MAX_VALUE;
    private boolean mCullHorizontal = false;

    public ContentContainer(Context context) {
      super(context);
    }

    void setCullAxis(boolean horizontal) {
      mCullHorizontal = horizontal;
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    }

    @Override
    protected void dispatchDraw(Canvas canvas) {
      View scroller = getParent() instanceof View ? (View) getParent() : null;
      if (scroller != null) {
        int start = mCullHorizontal ? scroller.getScrollX() : scroller.getScrollY();
        int extent = mCullHorizontal ? scroller.getWidth() : scroller.getHeight();
        // A quarter screen of slack covers overscroll stretch and a frame of scroll.
        int slack = extent / 4;
        mDrawLow = start - slack;
        mDrawHigh = start + extent + slack;
      } else {
        mDrawLow = Integer.MIN_VALUE;
        mDrawHigh = Integer.MAX_VALUE;
      }
      super.dispatchDraw(canvas);
    }

    @Override
    protected boolean drawChild(Canvas canvas, View child, long drawingTime) {
      float low = mCullHorizontal ? child.getLeft() + child.getTranslationX() : child.getTop() + child.getTranslationY();
      float size = mCullHorizontal ? child.getWidth() : child.getHeight();
      if (low > mDrawHigh || low + size < mDrawLow) {
        return false;
      }
      return super.drawChild(canvas, child, drawingTime);
    }
  }

  /*
   * The inner scroll views pass scroll, fling and touch callbacks to the host.
   */
  private static class InnerVerticalScrollView extends ReactScrollView {
    private final ShadowListView mHost;

    InnerVerticalScrollView(Context context, ShadowListView host) {
      super(context);
      mHost = host;
    }

    @Override
    protected void onScrollChanged(int scrollX, int scrollY, int oldScrollX, int oldScrollY) {
      super.onScrollChanged(scrollX, scrollY, oldScrollX, oldScrollY);
      mHost.handleInnerScroll(scrollX, scrollY);
    }

    @Override
    public void fling(int velocityY) {
      mHost.handleInnerFling();
      if (mHost.snapFling(velocityY)) {
        return;
      }
      super.fling(velocityY);
    }

    /*
     * Track the finger here, not in onTouchEvent. A row takes the touch first, so
     * onTouchEvent misses the down event. Dispatch sees the whole gesture.
     */
    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
      int action = event.getActionMasked();
      boolean touchEnded = action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL;
      if (action == MotionEvent.ACTION_DOWN) {
        mHost.handleInnerTouchDown();
      } else if (touchEnded) {
        mHost.handleInnerTouchUp();
      }
      boolean handled = super.dispatchTouchEvent(event);
      if (touchEnded) {
        // Any fling from the lift has started by now.
        mHost.reportTouchUpPhase();
      }
      return handled;
    }
  }

  private static class InnerHorizontalScrollView extends ReactHorizontalScrollView {
    private final ShadowListView mHost;

    InnerHorizontalScrollView(Context context, ShadowListView host) {
      super(context);
      mHost = host;
    }

    @Override
    protected void onScrollChanged(int scrollX, int scrollY, int oldScrollX, int oldScrollY) {
      super.onScrollChanged(scrollX, scrollY, oldScrollX, oldScrollY);
      mHost.handleInnerScroll(scrollX, scrollY);
    }

    @Override
    public void fling(int velocityX) {
      mHost.handleInnerFling();
      if (mHost.snapFling(velocityX)) {
        return;
      }
      super.fling(velocityX);
    }

    /*
     * Track the finger here, not in onTouchEvent. A row takes the touch first, so
     * onTouchEvent misses the down event. Dispatch sees the whole gesture.
     */
    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
      int action = event.getActionMasked();
      boolean touchEnded = action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL;
      if (action == MotionEvent.ACTION_DOWN) {
        mHost.handleInnerTouchDown();
      } else if (touchEnded) {
        mHost.handleInnerTouchUp();
      }
      boolean handled = super.dispatchTouchEvent(event);
      if (touchEnded) {
        // Any fling from the lift has started by now.
        mHost.reportTouchUpPhase();
      }
      return handled;
    }
  }

  public ShadowListView(Context context) {
    super(context);
    mContentView = new ContentContainer(context);
    mStickyController = new ShadowListStickyController(this);
    mDragController = new ShadowListDragController(this, context);
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
      ? new InnerHorizontalScrollView(context, this)
      : new InnerVerticalScrollView(context, this);

    mScrollView.setVerticalScrollBarEnabled(!horizontal);
    mScrollView.setHorizontalScrollBarEnabled(horizontal);
    mScrollView.setScrollbarFadingEnabled(true);
    mScrollView.setScrollBarStyle(View.SCROLLBARS_INSIDE_OVERLAY);
    if (mScrollView instanceof ReactScrollView) {
      ((ReactScrollView) mScrollView).setFillViewport(false);
    }
    mScrollView.setClipToPadding(false);

    /*
     * The inner scroll view has no React tag, so every ScrollEvent it sends goes to tag -1
     * and logs an error, once per frame. The list reports scrolling itself, so throttle
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

    if (!horizontal) {
      // Vertical lists get wrapped for pull to refresh.
      mRefreshLayout = new SwipeRefreshLayout(context);
      mRefreshLayout.setOnRefreshListener(this::emitRefresh);
      mRefreshLayout.setEnabled(mRefreshEnabled);
      if (mRefreshColor != null) {
        mRefreshLayout.setColorSchemeColors(mRefreshColor);
      }
      mRefreshLayout.addView(mScrollView, new ViewGroup.LayoutParams(
        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
      addView(mRefreshLayout, new FrameLayout.LayoutParams(
        FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
    } else {
      addView(mScrollView, new FrameLayout.LayoutParams(
        FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
    }
  }

  public void setRefreshEnabled(boolean enabled) {
    mRefreshEnabled = enabled;
    if (mRefreshLayout != null) {
      mRefreshLayout.setEnabled(enabled);
    }
  }

  public void setRefreshing(boolean refreshing) {
    if (refreshing == mRefreshing) {
      return;
    }
    mRefreshing = refreshing;
    if (mRefreshLayout != null) {
      mRefreshLayout.setRefreshing(refreshing);
    }
    removeCallbacks(mRefreshSettleRunnable);
    mRefreshAwaitingSettle = !refreshing;
    if (!refreshing) {
      /*
       * Refresh ended. Fire onRefreshSettle once the spinner is gone and nothing moves the
       * list, so JS can add the new rows to a list at rest, like on iOS.
       * JS also has a timeout fallback.
       */
      postDelayed(mRefreshSettleRunnable, REFRESH_SETTLE_DELAY_MS);
    }
  }

  private final Runnable mRefreshSettleRunnable = new Runnable() {
    @Override
    public void run() {
      if (!mRefreshAwaitingSettle || mRefreshing) {
        return;
      }
      // Not at rest yet. A finger is down, a fling is running, or a new pull started.
      boolean pulling = mRefreshLayout != null && mRefreshLayout.isRefreshing();
      if (mTouching || mSettling || pulling) {
        postDelayed(this, REFRESH_SETTLE_DELAY_MS);
        return;
      }
      mRefreshAwaitingSettle = false;
      emitRefreshEvent(ShadowListRefreshEvent.SETTLE_EVENT_NAME);
    }
  };

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

  /*
   * Rows and templates go into the content view inside the scroll view.
   */
  public void addContentView(View child, int index) {
    if (child instanceof ShadowListElementView) {
      mContentView.addView(child, index);
      // Pin again so sticky views stay above the new row.
      mStickyController.applyStickyTransforms();
      // A row that mounts during a drag must shift right away or it flashes.
      if (mDragController.isDragging()) {
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
    /*
     * The dragged row is going away, for example its data was deleted. Cancel the drag
     * first so touches don't go to a row that is no longer there.
     */
    if (child != null && child == mDragController.getDraggedView()) {
      mDragController.teardown();
    }
    mContentView.removeViewAt(index);
  }

  private void handleInnerScroll(int scrollX, int scrollY) {
    // Rows move in and out of the drawn window, see ContentContainer.
    mContentView.invalidate();
    // Only real user scrolls move the hiding header and footer.
    boolean userScrolled = updateScrollState(scrollX, scrollY);
    mStickyController.applyStickyTransforms(userScrolled);
  }

  private void handleInnerTouchDown() {
    /*
     * The finger takes over from any scroll we started, so report the drag as the user.
     * The touch also stops any fling, so settling ends here.
     */
    mSync.disarm();
    mTouching = true;
    stopSettling();
    removeCallbacks(mSnapSettleRunnable);
  }

  /*
   * The finger lifted. If no fling follows, snap to the nearest offset.
   */
  private void handleInnerTouchUp() {
    mTouching = false;
    if (mSnapToItem && mSnapOffsetsPx.length > 0) {
      removeCallbacks(mSnapSettleRunnable);
      // Wait a bit so a real fling, which snaps on its own, can cancel this first.
      postDelayed(mSnapSettleRunnable, 40);
    }
  }

  /*
   * Tell the core the finger is gone, even if nothing scrolls after it.
   * Without a fling, an inverted list near the bottom can pin back to it.
   * A fling reports settling instead, which keeps the pin off until it ends, like on iOS.
   */
  private void reportTouchUpPhase() {
    reportScrollPhase(mSettling ? SCROLL_PHASE_SETTLING : SCROLL_PHASE_IDLE);
  }

  private void reportScrollPhase(double scrollPhase) {
    if (mState == null) {
      return;
    }
    /*
     * Going idle also ends the user scroll, like clearUserScrolled on iOS. Updates merge into
     * the last state, so otherwise the old userScrolled flag sticks around and the core
     * mistakes its own correction for the user moving the list and drops it.
     */
    if (scrollPhase == SCROLL_PHASE_IDLE) {
      if (mSync.clearUserScrolled(liveOffsetX(), liveOffsetY())) {
        mState.updateState(mSync.patchMap());
      }
      return;
    }
    mSync.livePatch(liveOffsetX(), liveOffsetY(), mSync.currentUserScrolled(), scrollPhase);
    mState.updateState(mSync.patchMap());
  }

  /*
   * A fling starts momentum. Report settling until the offset stops.
   */
  private void handleInnerFling() {
    mSettling = true;
    mSettleStableFrames = 0;
    mSettleLastScrollX = mScrollView.getScrollX();
    mSettleLastScrollY = mScrollView.getScrollY();
    removeCallbacks(mSettleRunnable);
    postOnAnimationDelayed(mSettleRunnable, SETTLE_POLL_DELAY_MS);
  }

  private void stopSettling() {
    mSettling = false;
    removeCallbacks(mSettleRunnable);
  }

  private final Runnable mSettleRunnable = new Runnable() {
    @Override
    public void run() {
      if (!mSettling) {
        return;
      }
      int scrollX = mScrollView.getScrollX();
      int scrollY = mScrollView.getScrollY();
      if (scrollX != mSettleLastScrollX || scrollY != mSettleLastScrollY) {
        mSettleLastScrollX = scrollX;
        mSettleLastScrollY = scrollY;
        mSettleStableFrames = 0;
      } else if (++mSettleStableFrames >= SETTLE_STABLE_FRAMES) {
        mSettling = false;
        reportScrollPhase(SCROLL_PHASE_IDLE);
        return;
      }
      postOnAnimationDelayed(this, SETTLE_POLL_DELAY_MS);
    }
  };

  public void setSnapToItem(boolean snapToItem) {
    mSnapToItem = snapToItem;
  }

  /*
   * Predict where the fling lands and glide to the nearest snap offset.
   */
  boolean snapFling(int velocity) {
    if (!mSnapToItem || mSnapOffsetsPx.length == 0) {
      return false;
    }
    removeCallbacks(mSnapSettleRunnable);
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

  private final Runnable mSnapSettleRunnable = new Runnable() {
    @Override
    public void run() {
      if (!mSnapToItem || mSnapOffsetsPx.length == 0 || mTouching) {
        return;
      }
      int current = mHorizontal ? mScrollView.getScrollX() : mScrollView.getScrollY();
      int target = nearestSnapOffsetPx(current);
      if (target != current) {
        smoothSnapTo(target);
      }
    }
  };

  public void setDragEnabled(boolean dragEnabled) {
    mDragController.setEnabled(dragEnabled);
  }

  public void setColumns(int columns) {
    mColumns = Math.max(1, columns);
  }

  /*
   * Reset drag and sticky state before this view is recycled.
   */
  void onDropInstance() {
    mDragController.teardown();
    mStickyController.reset();
    /*
     * Don't pass the old scroll position or content size to the next list. Sticky pinning
     * uses the content size and runs on mount, before the new list's state arrives.
     * Same as prepareForRecycle on iOS. Reset the echo state too, or the next list's first
     * real scroll gets ignored.
     */
    mSync.reset(mHorizontal);
    // The live report and list versions belong to the old list.
    mLiveHandle = 0;
    mStickyVersion = -1;
    mSnapVersion = -1;
    mGeometryHandle = 0;
    mRefreshAwaitingSettle = false;
    removeCallbacks(mRefreshSettleRunnable);
    stopSettling();
    mScrollView.scrollTo(0, 0);
    mContentView.layout(0, 0, 0, 0);
  }

  @Override
  public boolean dispatchTouchEvent(MotionEvent event) {
    mDragController.trackGesture(event);
    return super.dispatchTouchEvent(event);
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

  void setInnerScrollEnabled(boolean enabled) {
    if (mScrollView instanceof ReactScrollView) {
      ((ReactScrollView) mScrollView).setScrollEnabled(enabled);
    } else if (mScrollView instanceof ReactHorizontalScrollView) {
      ((ReactHorizontalScrollView) mScrollView).setScrollEnabled(enabled);
    }
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

  private boolean updateScrollState(int scrollX, int scrollY) {
    if (mState == null) {
      return false;
    }

    /*
     * Finger down, momentum or idle. The core keeps an inverted list from pinning to the
     * bottom until this is idle, see Container::gestureActive.
     */
    double scrollPhase = mTouching
      ? SCROLL_PHASE_DRAGGING
      : (mSettling ? SCROLL_PHASE_SETTLING : SCROLL_PHASE_IDLE);
    double offsetX = PixelUtil.toDIPFromPixel(scrollX);
    double offsetY = PixelUtil.toDIPFromPixel(scrollY);
    // Pull to refresh and drag to reorder lean on every frame.
    boolean commitEveryFrame = mRefreshing || mRefreshAwaitingSettle
      || (mRefreshLayout != null && mRefreshLayout.isRefreshing())
      || mDragController.isDragging() || mDragController.ownsScrollOffset();

    /*
     * Every frame goes into the live report. Only frames the core needs become a state
     * update, which is a full commit. See ShadowListScrollSync.
     */
    boolean needsCommit = mSync.onScroll(offsetX, offsetY, scrollPhase, commitEveryFrame);
    boolean userScrolled = mSync.frameUserScrolled();

    if (DEBUG_LOG) {
      slLog(String.format("java.onScrollChanged: offset=(%.1f,%.1f) userScrolled=%b phase=%.0f commit=%b",
        offsetX, offsetY, userScrolled, scrollPhase, needsCommit));
    }
    if (needsCommit) {
      mState.updateState(mSync.patchMap());
    }
    return userScrolled;
  }

  private double liveOffsetX() {
    return PixelUtil.toDIPFromPixel(mScrollView.getScrollX());
  }

  private double liveOffsetY() {
    return PixelUtil.toDIPFromPixel(mScrollView.getScrollY());
  }

  public void updateState(@Nullable StateWrapper stateWrapper) {
    mState = stateWrapper;

    if (mState == null) {
      return;
    }

    /*
     * Read the scalars from the state's MapBuffer. getStateData copies the whole state,
     * including the sticky and snap lists, into a map on every mount. The lists are only
     * read from it when their version moved.
     */
    ReadableMapBuffer mapBuffer = mState.getStateDataMapBuffer();
    if (mapBuffer == null) {
      return;
    }

    mLiveHandle = mapBuffer.getLong(STATE_LIVE_HANDLE);
    boolean offsetEnabled = mapBuffer.getBoolean(STATE_OFFSET_ENABLED);
    mSync.beginMount(
      mLiveHandle,
      offsetEnabled,
      mapBuffer.getDouble(STATE_OFFSET_X),
      mapBuffer.getDouble(STATE_OFFSET_Y),
      mapBuffer.getDouble(STATE_OFFSET_BASE_X),
      mapBuffer.getDouble(STATE_OFFSET_BASE_Y),
      mapBuffer.getDouble(STATE_COMMIT_TOKEN),
      mapBuffer.getBoolean(STATE_USER_SCROLLED),
      mapBuffer.getDouble(STATE_SCROLL_PHASE),
      mapBuffer.getDouble(STATE_CONCEAL_GENERATION),
      mapBuffer.getDouble(STATE_COMMAND_SEQUENCE),
      mapBuffer.getDouble(STATE_BAND_LOW),
      mapBuffer.getDouble(STATE_BAND_HIGH));
    try {
      applyMountedState(mapBuffer, offsetEnabled);
    } finally {
      mSync.endMount();
    }
  }

  private void applyMountedState(ReadableMapBuffer mapBuffer, boolean offsetEnabled) {
    long stickyVersion = mapBuffer.getLong(STATE_STICKY_VERSION);
    long snapVersion = mapBuffer.getLong(STATE_SNAP_VERSION);
    // Versions count per list, so a different list always reads its lists again.
    boolean sameList = mGeometryHandle == mLiveHandle && mLiveHandle != 0;
    boolean stickyStale = !sameList || stickyVersion != mStickyVersion;
    boolean snapStale = !sameList || snapVersion != mSnapVersion;
    if (stickyStale || snapStale) {
      ReadableMap nextStateData = mState.getStateData();
      if (nextStateData != null) {
        if (stickyStale) {
          mStickyController.cacheStickyGeometry(nextStateData);
        }
        if (snapStale && nextStateData.hasKey("snapOffsets")) {
          ReadableArray snapOffsets = nextStateData.getArray("snapOffsets");
          if (snapOffsets != null) {
            float[] offsetsPx = new float[snapOffsets.size()];
            for (int i = 0; i < snapOffsets.size(); i++) {
              offsetsPx[i] = PixelUtil.toPixelFromDIP((float) snapOffsets.getDouble(i));
            }
            mSnapOffsetsPx = offsetsPx;
          }
        }
        mGeometryHandle = mLiveHandle;
        mStickyVersion = stickyVersion;
        mSnapVersion = snapVersion;
      }
    }

    if (DEBUG_LOG) {
      slLog(String.format("java.updateState: contentSize=(%.1f,%.1f) enabled=%d offset=(%.1f,%.1f) curOffset=(%.1f,%.1f) band=(%.1f,%.1f)",
        mapBuffer.getDouble(STATE_TOTAL_WIDTH), mapBuffer.getDouble(STATE_TOTAL_HEIGHT),
        offsetEnabled ? 1 : 0,
        mapBuffer.getDouble(STATE_OFFSET_X), mapBuffer.getDouble(STATE_OFFSET_Y),
        liveOffsetX(), liveOffsetY(),
        mapBuffer.getDouble(STATE_BAND_LOW), mapBuffer.getDouble(STATE_BAND_HIGH)));
    }

    float totalContainerWidth = (float) mapBuffer.getDouble(STATE_TOTAL_WIDTH);
    float totalContainerHeight = (float) mapBuffer.getDouble(STATE_TOTAL_HEIGHT);

    int newContentWidth = (int) PixelUtil.toPixelFromDIP(totalContainerWidth);
    int newContentHeight = (int) PixelUtil.toPixelFromDIP(totalContainerHeight);

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
      PixelUtil.toDIPFromPixel(maxOffsetPx), mTouching, mTouching || mSettling,
      mDragController.ownsScrollOffset());
    if (action != ShadowListScrollSync.ACTION_WRITE) {
      return;
    }
    int appliedX = Math.round(PixelUtil.toPixelFromDIP((float) mSync.actionX()));
    int appliedY = Math.round(PixelUtil.toPixelFromDIP((float) mSync.actionY()));
    // scrollTo calls onScrollChanged right away, which must know the move was ours.
    mSync.willWrite();
    /*
     * A plain scrollTo leaves the fling running, and its next tick writes its own position over
     * ours. A prepend during a fling at the top lost its correction that way, as the bounce
     * pulled the view back to 0. Rebuild the fling from the corrected offset instead, like
     * React Native's maintainVisibleContentPosition does.
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
  void dispatchDragEvent(int type, String fromKey, String toKey, double sequence) {
    if (mState == null) {
      return;
    }
    // Keep core scroll corrections off during the drag. The end event turns them back on.
    mSync.livePatch(liveOffsetX(), liveOffsetY(), type != ShadowListDragController.DRAG_EVENT_END, mSync.currentScrollPhase());
    WritableMap map = mSync.patchMap();
    map.putDouble("dragEventSequence", sequence);
    map.putDouble("dragEventType", type);
    map.putString("dragFromKey", fromKey != null ? fromKey : "");
    map.putString("dragToKey", toKey != null ? toKey : "");
    if (DEBUG_LOG) {
      slLog("java.drag dispatch type=" + type + " from=" + fromKey + " to=" + toKey);
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

  /*
   * Stop a fling or snap and forget any scroll of ours still waiting for its echo.
   */
  private void stopMomentum() {
    if (mScrollView instanceof ReactScrollView) {
      ((ReactScrollView) mScrollView).abortAnimation();
    } else if (mScrollView instanceof ReactHorizontalScrollView) {
      ((ReactHorizontalScrollView) mScrollView).abortAnimation();
    }
    removeCallbacks(mSnapSettleRunnable);
    stopSettling();
    mSync.momentumStopped();
  }

  /*
   * A scroll command stops any fling or snap, so it can't overwrite the offset the core is
   * about to apply, and reports idle with the command. If a finger is down it stays dragging,
   * and the core lets the drag cancel the command.
   */
  private void issueScrollCommand(double index, double viewPosition) {
    boolean yielded = !mTouching;
    if (yielded) {
      stopMomentum();
    }
    mSync.issueCommand(index, viewPosition, liveOffsetX(), liveOffsetY(), yielded);
    if (DEBUG_LOG) {
      slLog("java.cmd scroll: index=" + index + " viewPosition=" + viewPosition);
    }
    mState.updateState(mSync.patchMap());
  }

  public void scrollToIndex(int index, double viewPosition) {
    if (mState == null) {
      return;
    }
    issueScrollCommand((double) index, viewPosition);
  }

  public void scrollToOffset(double offset, boolean animated) {
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
    issueScrollCommand(SCROLL_TO_END_INDEX, 0.0);
  }

  /*
   * Used by the sticky and drag controllers.
   */
  ViewGroup getContentView() {
    return mContentView;
  }

  ViewGroup getScrollView() {
    return mScrollView;
  }

  boolean isHorizontal() {
    return mHorizontal;
  }

  int getColumns() {
    return mColumns;
  }

  @Nullable StateWrapper getStateWrapper() {
    return mState;
  }
}
