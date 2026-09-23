package com.shadowlist;

import android.content.Context;
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
import com.facebook.react.bridge.WritableNativeMap;
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
   * Values for the scrollPhase state key, matching ShadowListViewState.h.
   * Idle, finger down, or momentum running.
   */
  private static final double SCROLL_PHASE_IDLE = 0.0;
  private static final double SCROLL_PHASE_DRAGGING = 1.0;
  private static final double SCROLL_PHASE_SETTLING = 2.0;

  /*
   * Keys of the state MapBuffer, matching ShadowListStateKey in ShadowListViewState.h.
   */
  private static final int STATE_TOTAL_WIDTH = 0;
  private static final int STATE_TOTAL_HEIGHT = 1;
  private static final int STATE_OFFSET_ENABLED = 2;
  private static final int STATE_OFFSET_X = 3;
  private static final int STATE_OFFSET_Y = 4;
  private static final int STATE_COMMIT_TOKEN = 5;
  private static final int STATE_MOMENTUM_YIELD_TOKEN = 6;
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
   * What the mounted state says, read once per mount from its MapBuffer.
   * The band is the offset range, in dp, the view can scroll through without a state update.
   * Low above high means send every frame. The versions tell when the sticky and snap lists
   * changed, so they are only copied then. The handle finds the list's live report.
   */
  private double mBandLow = 1.0;
  private double mBandHigh = 0.0;
  private boolean mMountedOffsetEnabled = false;
  private double mMountedConcealGeneration = 0.0;
  private double mMountedCommandSequence = 0.0;
  private long mLiveHandle = 0;
  private long mStickyVersion = -1;
  private long mSnapVersion = -1;
  private long mGeometryHandle = 0;

  /*
   * The newest live report and the one the last state update carried. A change in the
   * gesture or the echoed token is always sent.
   */
  private boolean mLastLiveUserScrolled = false;
  private double mLastLivePhase = SCROLL_PHASE_IDLE;
  private boolean mHasPushedReport = false;
  private boolean mLastPushedUserScrolled = false;
  private double mLastPushedPhase = SCROLL_PHASE_IDLE;
  private long mLastPushedToken = 0;
  private double mLastPushedAck = 0.0;
  private ContentContainer mContentView;
  private ViewGroup mScrollView;
  private final ShadowListStickyController mStickyController;
  private final ShadowListDragController mDragController;

  /*
   * Pin sticky views again when a header or footer is laid out, so a sticky footer follows
   * its real position when the list size changes.
   */
  private final View.OnLayoutChangeListener mTemplateLayoutListener;

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
   * Remembers a scroll we started so its callbacks aren't reported as the user scrolling.
   * Otherwise the core drops its correction and the visible rows go blank.
   * A touch clears it so the finger wins.
   */
  private int mProgrammaticTargetX = 0;
  private int mProgrammaticTargetY = 0;
  private boolean mProgrammaticPending = false;
  private boolean mProgrammaticAnimated = false;
  /*
   * Token of the core correction being applied, sent back so the core can recognise it.
   * Zero for our own scrolls like snapping or scrollToOffset.
   */
  private long mArmedToken = 0;
  // Token of the last correction we reported back. Later reports keep sending it.
  private long mEchoedToken = 0;
  /*
   * The last correction added to the live offset and how much of it is applied, in dp.
   * The core resends the full correction each time it retargets, so only add what's left.
   */
  private long mShiftedToken = 0;
  private double mShiftedTokenDelta = 0.0;
  // The last native scroll command we stopped momentum for.
  private long mYieldedToken = 0;

  /*
   * The last scrollToIndex or scrollToEnd, sent with every state update. Updates build on
   * the last mounted state, which may not have the command yet. Without this, a scroll report
   * in that gap would overwrite the command and the core would never see it.
   * The sequence is 0 until the first command.
   */
  private double mCommandIndex = -2.0;
  private double mCommandSequence = 0.0;
  // Where scrollToIndex wants its row in the viewport.
  private double mCommandViewPosition = 0.0;

  /*
   * Only used to spot the end of our own animated scrolls like snapping.
   * Core corrections are matched by cause, not by this tolerance.
   */
  private static final int PROGRAMMATIC_SCROLL_TOLERANCE_PX = 2;

  private static class ContentContainer extends ViewGroup {
    public ContentContainer(Context context) {
      super(context);
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
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
    installScrollView(false);
  }

  /*
   * Build the inner scroll view for the axis and move the content into it.
   */
  private void installScrollView(boolean horizontal) {
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
      child.addOnLayoutChangeListener(mTemplateLayoutListener);
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
      child.removeOnLayoutChangeListener(mTemplateLayoutListener);
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
    // Only real user scrolls move the hiding header and footer.
    boolean userScrolled = updateScrollState(scrollX, scrollY);
    mStickyController.applyStickyTransforms(userScrolled);
  }

  private void handleInnerTouchDown() {
    /*
     * The finger takes over from any scroll we started, so report the drag as the user.
     * The touch also stops any fling, so settling ends here.
     */
    mProgrammaticPending = false;
    mArmedToken = 0;
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
    WritableMap map = new WritableNativeMap();
    map.putDouble("scrollPhase", scrollPhase);
    /*
     * Going idle also ends the user scroll, like clearUserScrolled on iOS. Updates merge into
     * the last state, so otherwise the old userScrolled flag sticks around and the core
     * mistakes its own correction for the user moving the list and drops it.
     */
    boolean userScrolled = mLastLiveUserScrolled;
    if (scrollPhase == SCROLL_PHASE_IDLE) {
      map.putBoolean("userScrolled", false);
      userScrolled = false;
    }
    carryLiveOffset(map, userScrolled, scrollPhase);
    carryScrollCommand(map);
    mState.updateState(map);
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
    int best = Math.round(mSnapOffsetsPx[0]);
    int bestDistance = Math.abs(best - target);
    for (float snapOffset : mSnapOffsetsPx) {
      int candidate = Math.round(snapOffset);
      int distance = Math.abs(candidate - target);
      if (distance < bestDistance) {
        bestDistance = distance;
        best = candidate;
      }
    }
    return best;
  }

  private void smoothSnapTo(int target) {
    int targetX = mHorizontal ? target : mScrollView.getScrollX();
    int targetY = mHorizontal ? mScrollView.getScrollY() : target;
    markProgrammaticScroll(targetX, targetY, true);
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

  /*
   * Our own scrolls, like snapping or scrollToOffset, have no core token.
   */
  private void markProgrammaticScroll(int targetX, int targetY, boolean animated) {
    markProgrammaticScroll(targetX, targetY, animated, 0);
  }

  private void markProgrammaticScroll(int targetX, int targetY, boolean animated, long token) {
    mProgrammaticTargetX = targetX;
    mProgrammaticTargetY = targetY;
    mProgrammaticAnimated = animated;
    mProgrammaticPending = true;
    mArmedToken = token;
  }

  public void setDragEnabled(boolean dragEnabled) {
    mDragController.setEnabled(dragEnabled);
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
    mProgrammaticPending = false;
    mProgrammaticAnimated = false;
    mArmedToken = 0;
    mEchoedToken = 0;
    mCommandIndex = -2.0;
    mCommandSequence = 0.0;
    mCommandViewPosition = 0.0;
    mShiftedToken = 0;
    mShiftedTokenDelta = 0.0;
    mYieldedToken = 0;
    // The band, live report and list versions belong to the old list.
    mBandLow = 1.0;
    mBandHigh = 0.0;
    mMountedOffsetEnabled = false;
    mMountedConcealGeneration = 0.0;
    mMountedCommandSequence = 0.0;
    mLiveHandle = 0;
    mStickyVersion = -1;
    mSnapVersion = -1;
    mGeometryHandle = 0;
    mLastLiveUserScrolled = false;
    mLastLivePhase = SCROLL_PHASE_IDLE;
    mHasPushedReport = false;
    mLastPushedUserScrolled = false;
    mLastPushedPhase = SCROLL_PHASE_IDLE;
    mLastPushedToken = 0;
    mLastPushedAck = 0.0;
    mRefreshAwaitingSettle = false;
    removeCallbacks(mRefreshSettleRunnable);
    stopSettling();
    mScrollView.scrollTo(0, 0);
    mContentView.layout(0, 0, 0, 0);
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
      installScrollView(horizontal);
    }
    mStickyController.applyStickyTransforms();
  }

  private boolean updateScrollState(int scrollX, int scrollY) {
    if (mState == null) {
      return false;
    }

    /*
     * Tell our own scrolls apart from the user's. An instant scroll from a core correction
     * causes exactly one callback, ours wherever it landed, even when clamped. An animated
     * snap spans many frames, all ours until it reaches the target. Calling ours a user
     * scroll makes the core drop its correction and freezes the visible rows.
     */
    boolean userScrolled = true;
    // Any frame of a scroll we started is sent, including the one that echoes a correction.
    boolean ourMove = mProgrammaticPending;
    if (mProgrammaticPending) {
      if (mProgrammaticAnimated) {
        // A frame of our own animation, never the user.
        userScrolled = false;
        boolean reachedTarget =
          Math.abs(scrollX - mProgrammaticTargetX) <= PROGRAMMATIC_SCROLL_TOLERANCE_PX
            && Math.abs(scrollY - mProgrammaticTargetY) <= PROGRAMMATIC_SCROLL_TOLERANCE_PX;
        if (reachedTarget) {
          mEchoedToken = mArmedToken;
          mProgrammaticPending = false;
          mArmedToken = 0;
        }
      } else {
        // Our instant scroll. This frame is ours wherever it landed.
        userScrolled = false;
        mEchoedToken = mArmedToken;
        mProgrammaticPending = false;
        mArmedToken = 0;
      }
    }
    /*
     * Keep sending the last token on later reports too. Updates get merged, so the next fling
     * frame can replace the echo before the core sees it, and the core would apply the
     * correction twice. Tokens are never reused, so an old one matches nothing.
     */
    long echoToken = mEchoedToken;

    /*
     * Finger down, momentum or idle. The core keeps an inverted list from pinning to the
     * bottom until this is idle, see Container::gestureActive.
     */
    double scrollPhase = mTouching
      ? SCROLL_PHASE_DRAGGING
      : (mSettling ? SCROLL_PHASE_SETTLING : SCROLL_PHASE_IDLE);
    double offsetX = PixelUtil.toDIPFromPixel(scrollX);
    double offsetY = PixelUtil.toDIPFromPixel(scrollY);

    /*
     * Every frame goes into the live report. Only frames the core needs become a state
     * update, which is a full commit. See liveReportNeedsCommit.
     */
    long sequence = writeLiveReport(offsetX, offsetY, userScrolled, scrollPhase, echoToken);
    boolean needsCommit = ourMove || liveReportNeedsCommit(sequence, mHorizontal ? offsetX : offsetY);

    if (DEBUG_LOG) {
      slLog(String.format("java.onScrollChanged: offset=(%.1f,%.1f) userScrolled=%b phase=%.0f seq=%d commit=%b",
        offsetX, offsetY, userScrolled, scrollPhase, sequence, needsCommit));
    }
    if (!needsCommit) {
      return userScrolled;
    }

    WritableMap map = new WritableNativeMap();

    map.putDouble("containerOffsetX", offsetX);
    map.putDouble("containerOffsetY", offsetY);
    map.putBoolean("containerOffsetEnabled", false);
    map.putBoolean("userScrolled", userScrolled);
    map.putDouble("scrollPhase", scrollPhase);
    map.putDouble("commitToken", (double) echoToken);
    map.putDouble("hostSequence", (double) sequence);
    carryScrollCommand(map);
    notePushedReport(userScrolled, scrollPhase, echoToken);

    mState.updateState(map);
    return userScrolled;
  }

  /*
   * Store a report in the list's live report and return its sequence, or 0 when there is no
   * live report, which makes every frame a state update. Android never hides rows, so the ack
   * is just the mounted generation, like iOS.
   */
  private long writeLiveReport(double offsetX, double offsetY, boolean userScrolled, double scrollPhase, long token) {
    mLastLiveUserScrolled = userScrolled;
    mLastLivePhase = scrollPhase;
    return ShadowListLiveScroll.write(
      mLiveHandle, offsetX, offsetY, userScrolled, scrollPhase, (double) token, mMountedConcealGeneration);
  }

  private void notePushedReport(boolean userScrolled, double scrollPhase, long token) {
    mHasPushedReport = true;
    mLastPushedUserScrolled = userScrolled;
    mLastPushedPhase = scrollPhase;
    mLastPushedToken = token;
    mLastPushedAck = mMountedConcealGeneration;
  }

  /*
   * Whether a scroll frame must become a state update. Conservative on purpose: anything the
   * core reacts to other than the offset moving inside the band sends it.
   */
  private boolean liveReportNeedsCommit(long sequence, double axisOffset) {
    if (sequence == 0 || !mHasPushedReport) {
      return true;
    }
    // A correction just mounted, or rows hidden until a report acknowledges them.
    if (mMountedOffsetEnabled || mMountedConcealGeneration != 0.0) {
      return true;
    }
    // The gesture changed, a correction was echoed, or a hide was acknowledged.
    if (mLastLiveUserScrolled != mLastPushedUserScrolled || mLastLivePhase != mLastPushedPhase
        || mEchoedToken != mLastPushedToken || mMountedConcealGeneration != mLastPushedAck) {
      return true;
    }
    // Pull to refresh and drag to reorder lean on every frame.
    if (mRefreshing || mRefreshAwaitingSettle || (mRefreshLayout != null && mRefreshLayout.isRefreshing())) {
      return true;
    }
    if (mDragController.isDragging() || mDragController.ownsScrollOffset()) {
      return true;
    }
    // The offset left the band the mounted layout pass published. An empty band never holds it.
    return !(mBandLow <= axisOffset && axisOffset <= mBandHigh);
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

    mBandLow = mapBuffer.getDouble(STATE_BAND_LOW);
    mBandHigh = mapBuffer.getDouble(STATE_BAND_HIGH);
    mMountedOffsetEnabled = mapBuffer.getBoolean(STATE_OFFSET_ENABLED);
    mMountedConcealGeneration = mapBuffer.getDouble(STATE_CONCEAL_GENERATION);
    mMountedCommandSequence = mapBuffer.getDouble(STATE_COMMAND_SEQUENCE);
    mLiveHandle = mapBuffer.getLong(STATE_LIVE_HANDLE);

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
        mMountedOffsetEnabled ? 1 : 0,
        mapBuffer.getDouble(STATE_OFFSET_X), mapBuffer.getDouble(STATE_OFFSET_Y),
        PixelUtil.toDIPFromPixel(mScrollView.getScrollX()), PixelUtil.toDIPFromPixel(mScrollView.getScrollY()),
        mBandLow, mBandHigh));
    }

    float totalContainerWidth = (float) mapBuffer.getDouble(STATE_TOTAL_WIDTH);
    float totalContainerHeight = (float) mapBuffer.getDouble(STATE_TOTAL_HEIGHT);

    int newContentWidth = (int) PixelUtil.toPixelFromDIP(totalContainerWidth);
    int newContentHeight = (int) PixelUtil.toPixelFromDIP(totalContainerHeight);

    // Skip the layout call when the size is the same, which it is on most mounts.
    if (mContentView.getLeft() != 0 || mContentView.getTop() != 0
        || mContentView.getWidth() != newContentWidth || mContentView.getHeight() != newContentHeight) {
      mContentView.layout(0, 0, newContentWidth, newContentHeight);
    }

    // While dragging a row, core corrections must not move the content.
    if (!mDragController.ownsScrollOffset() && mMountedOffsetEnabled) {
      float containerOffsetX = (float) mapBuffer.getDouble(STATE_OFFSET_X);
      float containerOffsetY = (float) mapBuffer.getDouble(STATE_OFFSET_Y);

      int appliedX = (int) PixelUtil.toPixelFromDIP(containerOffsetX);
      int appliedY = (int) PixelUtil.toPixelFromDIP(containerOffsetY);
      long token = (long) mapBuffer.getDouble(STATE_COMMIT_TOKEN);
      /*
       * ShadowListNative scroll commands reach the core in a commit, not through this view.
       * Stop momentum when the correction mounts, like scrollToIndex does, and write the offset.
       * If a finger is down it keeps control, and the core lets the drag cancel the command.
       */
      long yieldToken = (long) mapBuffer.getDouble(STATE_MOMENTUM_YIELD_TOKEN);
      boolean scrollCommand = yieldToken != 0 && token == yieldToken && !mTouching;
      if (scrollCommand && yieldToken != mYieldedToken) {
        mYieldedToken = yieldToken;
        stopMomentum();
      }
      int beforeX = mScrollView.getScrollX();
      int beforeY = mScrollView.getScrollY();
      /*
       * The finger or fling keeps moving while this correction is on its way, and it mounts
       * frames later. Writing the exact offset would throw that movement away and jump the
       * view back. So add the correction to the live offset instead, like iOS. The base is
       * the offset the core started from.
       * This includes corrections that keep the visible content in place. The core accepts
       * those by their echo during a gesture and never moves the view to an exact target,
       * see Container::gestureOperationId, so lost movement would never come back.
       * A correction that started during a gesture stays a shift after the motion stops,
       * and so do its retargets. Each retarget resends the full correction from the same
       * base, so only add the part not applied yet.
       */
      boolean continuesShiftedCorrection = token != 0 && token == mShiftedToken;
      boolean computedDuringGesture = token != 0
        && (mapBuffer.getBoolean(STATE_USER_SCROLLED)
          || mapBuffer.getDouble(STATE_SCROLL_PHASE) != SCROLL_PHASE_IDLE);
      boolean shiftLiveOffset = !scrollCommand
        && (mTouching || mSettling || continuesShiftedCorrection || computedDuringGesture);
      if (shiftLiveOffset) {
        double deltaX = containerOffsetX - mapBuffer.getDouble(STATE_OFFSET_BASE_X);
        double deltaY = containerOffsetY - mapBuffer.getDouble(STATE_OFFSET_BASE_Y);
        if (token != 0) {
          double shift = mHorizontal ? deltaX : deltaY;
          double unapplied = token == mShiftedToken ? shift - mShiftedTokenDelta : shift;
          mShiftedToken = token;
          mShiftedTokenDelta = shift;
          deltaX = mHorizontal ? unapplied : 0.0;
          deltaY = mHorizontal ? 0.0 : unapplied;
        }
        appliedX = beforeX + Math.round(PixelUtil.toPixelFromDIP((float) deltaX));
        appliedY = beforeY + Math.round(PixelUtil.toPixelFromDIP((float) deltaY));
      }
      /*
       * Corrections with a token and shifted corrections keep the fling going below.
       * A write with no token may carry an offset a frame old, and rebuilding the fling
       * from it would restart momentum from the past on every frame. Our own animated
       * scrolls also use the plain write. Read this before markProgrammaticScroll
       * overwrites the flags.
       */
      boolean preserveMomentum = !scrollCommand
        && (token != 0 || shiftLiveOffset) && !(mProgrammaticPending && mProgrammaticAnimated);
      /*
       * Mark the scroll before writing. scrollTo calls onScrollChanged right away, and
       * updateScrollState needs the token to send it back.
       */
      markProgrammaticScroll(appliedX, appliedY, false, token);
      /*
       * A plain scrollTo leaves the fling running, and its next tick writes its own
       * position over ours. A prepend during a fling at the top lost its correction that
       * way, as the bounce pulled the view back to 0. Rebuild the fling from the corrected
       * offset instead, like React Native's maintainVisibleContentPosition does.
       */
      if (preserveMomentum && mScrollView instanceof ReactScrollView) {
        ((ReactScrollView) mScrollView).scrollToPreservingMomentum(appliedX, appliedY);
      } else if (preserveMomentum && mScrollView instanceof ReactHorizontalScrollView) {
        ((ReactHorizontalScrollView) mScrollView).scrollToPreservingMomentum(appliedX, appliedY);
      } else {
        mScrollView.scrollTo(appliedX, appliedY);
      }
      /*
       * If the write moved nothing, no callback fires and the mark would never clear,
       * eating the next real scroll. Clear it now.
       */
      if (Math.abs(mScrollView.getScrollX() - beforeX) <= 1
          && Math.abs(mScrollView.getScrollY() - beforeY) <= 1) {
        mProgrammaticPending = false;
        mArmedToken = 0;
      }
    }

    // Pin again, since the footer position depends on the content size.
    mStickyController.applyStickyTransforms();

    // Keep a dragged row under the finger.
    mDragController.onStateCommitted();
  }

  /*
   * Updates build on the last mounted state, whose offset can be many frames old during a
   * fling. Write the live offset in, or the core lays out rows for a place the view has left,
   * which shows blank, and may later scroll back there. This update applies no correction and
   * carries the last echoed token like a scroll report. The drag controller uses it too.
   */
  void carryLiveOffset(WritableMap map) {
    carryLiveOffset(map, mLastLiveUserScrolled, mLastLivePhase);
  }

  /*
   * The update also becomes the newest live report, with the gesture state it sends, and
   * carries that report's sequence so adopt() knows the two match. See ShadowListLiveScroll.
   */
  void carryLiveOffset(WritableMap map, boolean userScrolled, double scrollPhase) {
    double offsetX = PixelUtil.toDIPFromPixel(mScrollView.getScrollX());
    double offsetY = PixelUtil.toDIPFromPixel(mScrollView.getScrollY());
    map.putDouble("containerOffsetX", offsetX);
    map.putDouble("containerOffsetY", offsetY);
    map.putBoolean("containerOffsetEnabled", false);
    map.putDouble("commitToken", (double) mEchoedToken);
    long sequence = writeLiveReport(offsetX, offsetY, userScrolled, scrollPhase, mEchoedToken);
    map.putDouble("hostSequence", (double) sequence);
    notePushedReport(userScrolled, scrollPhase, mEchoedToken);
  }

  public void setStartReachedEnabled(boolean enabled) {
    if (mState == null) {
      return;
    }

    WritableMap map = new WritableNativeMap();

    map.putBoolean("startReachedEnabled", enabled);
    carryLiveOffset(map);
    carryScrollCommand(map);

    if (DEBUG_LOG) {
      slLog("java.cmd setStartReachedEnabled: enabled=" + (enabled ? 1 : 0));
    }
    mState.updateState(map);
  }

  public void setEndReachedEnabled(boolean enabled) {
    if (mState == null) {
      return;
    }

    WritableMap map = new WritableNativeMap();

    map.putBoolean("endReachedEnabled", enabled);
    carryLiveOffset(map);
    carryScrollCommand(map);

    if (DEBUG_LOG) {
      slLog("java.cmd setEndReachedEnabled: enabled=" + (enabled ? 1 : 0));
    }
    mState.updateState(map);
  }

  /*
   * A scroll command stops any fling or snap, so it can't overwrite the offset the core is
   * about to apply, and reports idle with the command. If a finger is down it stays dragging,
   * and the core lets the drag cancel the command.
   */
  private void yieldMomentumInto(WritableMap map) {
    if (mTouching) {
      return;
    }
    stopMomentum();
    map.putBoolean("userScrolled", false);
    map.putDouble("scrollPhase", SCROLL_PHASE_IDLE);
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
    mProgrammaticPending = false;
    mProgrammaticAnimated = false;
    mArmedToken = 0;
  }

  /*
   * Save a scroll command and write it into the update. The sequence always goes past the
   * last one, so an older mounted state can't make a new command reuse a number.
   */
  private void issueScrollCommand(WritableMap map, double index, double nextSequence, double viewPosition) {
    mCommandSequence = Math.max(nextSequence, mCommandSequence + 1);
    mCommandIndex = index;
    mCommandViewPosition = viewPosition;
    map.putDouble("containerOffsetIndex", mCommandIndex);
    map.putDouble("containerOffsetIndexSequence", mCommandSequence);
    map.putDouble("containerOffsetIndexViewPosition", mCommandViewPosition);
  }

  /*
   * The drag controller's updates carry the command too.
   */
  void carryScrollCommand(WritableMap map) {
    if (mCommandSequence > 0) {
      map.putDouble("containerOffsetIndex", mCommandIndex);
      map.putDouble("containerOffsetIndexSequence", mCommandSequence);
      map.putDouble("containerOffsetIndexViewPosition", mCommandViewPosition);
    }
  }

  public void scrollToIndex(int index, double viewPosition) {
    if (mState == null) {
      return;
    }

    // A new sequence makes the core scroll again even for the same index.
    double nextSequence = 0;
    // The mounted state's command sequence, read from its MapBuffer on mount.
    nextSequence = mMountedCommandSequence + 1;

    WritableMap map = new WritableNativeMap();
    yieldMomentumInto(map);
    issueScrollCommand(map, (double) index, nextSequence, viewPosition);
    map.putBoolean("containerOffsetEnabled", true);
    if (DEBUG_LOG) {
      slLog("java.cmd scrollToIndex: index=" + index + " viewPosition=" + viewPosition
        + " sequence=" + (long) mCommandSequence);
    }
    mState.updateState(map);
  }

  public void scrollToOffset(double offset, boolean animated) {
    // Marked as ours so its frames don't count as user scrolls.
    int offsetPx = (int) PixelUtil.toPixelFromDIP((float) offset);
    int targetX = mHorizontal ? offsetPx : mScrollView.getScrollX();
    int targetY = mHorizontal ? mScrollView.getScrollY() : offsetPx;
    markProgrammaticScroll(targetX, targetY, animated);
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
     * Index -3 means the end. The core keeps adjusting as rows get measured instead of
     * jumping to an estimated bottom.
     */
    double nextSequence = 0;
    // The mounted state's command sequence, read from its MapBuffer on mount.
    nextSequence = mMountedCommandSequence + 1;

    WritableMap map = new WritableNativeMap();
    yieldMomentumInto(map);
    issueScrollCommand(map, -3.0, nextSequence, 0.0);
    map.putBoolean("containerOffsetEnabled", true);
    if (DEBUG_LOG) {
      slLog("java.cmd scrollToEnd: sequence=" + (long) mCommandSequence);
    }
    mState.updateState(map);
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

  @Nullable StateWrapper getStateWrapper() {
    return mState;
  }
}
