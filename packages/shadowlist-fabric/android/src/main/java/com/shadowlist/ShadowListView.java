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
import com.facebook.react.uimanager.PixelUtil;
import com.facebook.react.uimanager.StateWrapper;
import com.facebook.react.uimanager.UIManagerHelper;
import com.facebook.react.uimanager.events.EventDispatcher;
import com.facebook.react.views.scroll.ReactHorizontalScrollView;
import com.facebook.react.views.scroll.ReactScrollView;

/*
 * Hosts the scrolling content in an inner scroll view picked by the `horizontal`
 * prop. Keeps the scroll/content handling, state sync and imperative commands;
 * sticky pinning lives in ShadowListStickyController and drag-to-reorder in
 * ShadowListDragController, both reaching the shared views and state through the
 * package-private accessors below.
 */
public class ShadowListView extends FrameLayout {
  // State-sync trace logging, filter with: adb logcat -s SL
  static final boolean DEBUG_LOG = false;
  private static final String LOG_TAG = "SL";

  static void slLog(String message) {
    if (DEBUG_LOG) {
      Log.d(LOG_TAG, "[SL] " + message);
    }
  }

  /*
   * Values of the "scrollPhase" state key, mirroring SCROLL_PHASE_* in ShadowListViewState.h:
   * idle, a finger is down (dragging), momentum is running (settling).
   */
  private static final double SCROLL_PHASE_IDLE = 0.0;
  private static final double SCROLL_PHASE_DRAGGING = 1.0;
  private static final double SCROLL_PHASE_SETTLING = 2.0;

  private @Nullable StateWrapper mState = null;
  private ContentContainer mContentView;
  private ViewGroup mScrollView;
  private final ShadowListStickyController mStickyController;
  private final ShadowListDragController mDragController;

  /*
   * Re-pin sticky views once a template (header/footer) child is (re)laid out by the mounting
   * layer, so a sticky footer tracks its real resting position across list-size changes.
   */
  private final View.OnLayoutChangeListener mTemplateLayoutListener;

  // Horizontal/vertical axis (the `horizontal` prop); a change reinstalls the inner scroll view.
  private boolean mHorizontal = false;

  /*
   * View snapping. mSnapToItem enables it; mSnapOffsetsPx is the core's resting snap
   * offsets (DIP -> px) cached from state; mTouching gates the touch-up settle so it
   * never fights a finger still on screen.
   */
  private boolean mSnapToItem = false;
  private float[] mSnapOffsetsPx = new float[0];
  private boolean mTouching = false;

  /*
   * Momentum after a fling, reported to the core as SCROLL_PHASE_SETTLING. Android has no
   * end-of-fling callback, so mSettleRunnable polls the offset (as ReactScrollView does for
   * its momentum events) and reports the idle phase once it has held still for
   * SETTLE_STABLE_FRAMES polls in a row.
   */
  private boolean mSettling = false;
  private int mSettleStableFrames = 0;
  private int mSettleLastScrollX = 0;
  private int mSettleLastScrollY = 0;
  private static final long SETTLE_POLL_DELAY_MS = 20;
  private static final int SETTLE_STABLE_FRAMES = 3;

  // Pull-to-refresh state; all held so an axis-flip reinstall restores them.
  @Nullable private SwipeRefreshLayout mRefreshLayout = null;
  private boolean mRefreshEnabled = false;
  private boolean mRefreshing = false;
  @Nullable private Integer mRefreshColor = null;
  /*
   * Set when refreshing ends; cleared once the spinner has retracted and the list is at rest,
   * when onRefreshSettle fires (see mRefreshSettleRunnable).
   */
  private boolean mRefreshAwaitingSettle = false;
  // SwipeRefreshLayout's retract runs 150-200ms; check just after it, then poll until at rest.
  private static final long REFRESH_SETTLE_DELAY_MS = 250;

  /*
   * Tracks a programmatic scroll we issued so its echoed callbacks are not reported as
   * user gestures, which would make the core abandon its in-flight correction and blank
   * the visible window. A user touch clears the flag so a finger taking over wins.
   */
  private int mProgrammaticTargetX = 0;
  private int mProgrammaticTargetY = 0;
  private boolean mProgrammaticPending = false;
  private boolean mProgrammaticAnimated = false;
  /*
   * Commit token of an in-flight core correction, echoed back so the core matches its
   * own write exactly. 0 for a local programmatic scroll (snap / scrollToOffset).
   */
  private long mArmedToken = 0;
  // The token of the last correction whose echo was reported, carried on later reports.
  private long mEchoedToken = 0;
  /*
   * The last operation correction shifted onto the live offset: its commit token and the
   * correction (DIP, along the scroll axis) already applied for it. The core republishes a
   * token's whole cumulative correction on each retarget; shift only the part not yet applied.
   */
  private long mShiftedToken = 0;
  private double mShiftedTokenDelta = 0.0;

  /*
   * The last scroll command this host issued (scrollToIndex / scrollToEnd), carried on every
   * state update it writes. Each update is built on the state this host last mounted, which
   * can predate a command still on its way to the core: a scroll report sent in that window
   * (the echo of a correction mounting, say) would write the previous sequence back over the
   * command, and the core would never see it. mCommandSequence is 0 until the first command.
   */
  private double mCommandIndex = -2.0;
  private double mCommandSequence = 0.0;
  // Carried with mCommandIndex: where scrollToIndex wants its row in the viewport.
  private double mCommandViewPosition = 0.0;

  /*
   * Used only to detect the end of an animated programmatic scroll (snap); the core
   * correction echo is classified by causality, not by this tolerance.
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

  // Inner scroll views forward scroll, fling and touch callbacks to the host.
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
     * Track the finger in dispatchTouchEvent, not onTouchEvent: a touch that lands on a
     * row is consumed by that child first, so this view's onTouchEvent never sees the
     * ACTION_DOWN and only starts receiving events once the scroll intercepts. Dispatch
     * sees every event of the gesture from its first DOWN to its last UP.
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
        // After super: the lift's fling (if any) has started by now.
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
     * Track the finger in dispatchTouchEvent, not onTouchEvent: a touch that lands on a
     * row is consumed by that child first, so this view's onTouchEvent never sees the
     * ACTION_DOWN and only starts receiving events once the scroll intercepts. Dispatch
     * sees every event of the gesture from its first DOWN to its last UP.
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
        // After super: the lift's fling (if any) has started by now.
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

  // Build the inner scroll view for the current axis and reparent the content into it.
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
      // Wrap the vertical list for pull-to-refresh; gated by mRefreshEnabled.
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

  // Pull-to-refresh: toggle the gesture, drive the controlled spinner, tint the indicator.
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
       * Refresh ended: fire onRefreshSettle once the spinner has retracted and no finger or
       * fling moves the list, so JS applies a held refresh-prepend on a list at rest (as iOS
       * does after its retract spring). JS keeps a timeout fallback.
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
      // Not settled yet: a finger is down, momentum is running, or a new pull already started.
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

  // Element/template children land in the content container inside the inner scroll view.
  public void addContentView(View child, int index) {
    if (child instanceof ShadowListElementView) {
      mContentView.addView(child, index);
      // Re-pin so active sticky views stay on top of the newly mounted element.
      mStickyController.applyStickyTransforms();
      // A row mounting mid-drag needs its make-room shuffle applied at once to avoid a flash.
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
     * The row being dragged is about to disappear out from under the gesture (e.g. its
     * data was deleted mid-drag): abort the drag first so the controller stops
     * intercepting touches against a now-parentless, invisible row.
     */
    if (child != null && child == mDragController.getDraggedView()) {
      mDragController.teardown();
    }
    mContentView.removeViewAt(index);
  }

  private void handleInnerScroll(int scrollX, int scrollY) {
    // Advance the auto-hide only on genuine user scrolls (not our own echoed offset).
    boolean userScrolled = updateScrollState(scrollX, scrollY);
    mStickyController.applyStickyTransforms(userScrolled);
  }

  private void handleInnerTouchDown() {
    /*
     * A finger takes over from any in-flight programmatic scroll: drop the echo arm so
     * the drag is reported as a user scroll, not mistaken for our correction's echo. It
     * also catches any running fling, so the settling phase ends here.
     */
    mProgrammaticPending = false;
    mArmedToken = 0;
    mTouching = true;
    stopSettling();
    removeCallbacks(mSnapSettleRunnable);
  }

  // The finger lifted: if no fling follows, settle to the nearest snap from rest.
  private void handleInnerTouchUp() {
    mTouching = false;
    if (mSnapToItem && mSnapOffsetsPx.length > 0) {
      removeCallbacks(mSnapSettleRunnable);
      // Short delay so a real fling, which snaps predictively, cancels this first.
      postDelayed(mSnapSettleRunnable, 40);
    }
  }

  /*
   * Tell the core the finger is gone even if no scroll follows (a nudge that stopped dead).
   * Without a fling the next frame is a non-gesture frame, so a lift inside the inverted
   * follow band lets the bottom pin take the view back. A fling reports the settling phase
   * instead, which keeps the pin off until the momentum ends, as on iOS.
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
     * The rest report also ends the user scroll, as clearUserScrolled does on iOS. A partial
     * update merges into the last state, so without this the last scroll report's userScrolled
     * outlives the gesture, and the next layout of the core's own MVCP correction reads that
     * correction as the user moving the list and drops it.
     */
    if (scrollPhase == SCROLL_PHASE_IDLE) {
      map.putBoolean("userScrolled", false);
    }
    carryLiveOffset(map);
    carryScrollCommand(map);
    mState.updateState(map);
  }

  // A fling (native or snapping) starts momentum: report settling until the offset rests.
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

  // Predict the fling landing with an OverScroller and glide to the nearest snap offset.
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

  // Local programmatic scrolls (snap / scrollToOffset) carry no core token.
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

  // Tear down any in-flight drag and sticky state before this host is recycled.
  void onDropInstance() {
    mDragController.teardown();
    mStickyController.reset();
    /*
     * A recycled host must not hand a leftover scroll position or content size to the next
     * list: the sticky/auto-hide pins derive every translation from the content size, and
     * they run on mount, before the new list's first state lands. Mirrors the iOS
     * prepareForRecycle. Reset the echo latch too, or the first genuine scroll of the next
     * list is swallowed as this one's echo.
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

  // Toggle the inner scroll view's gesture handling.
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
     * Classify our own echo vs a user gesture. An instant programmatic scroll (the core
     * correction) is classified by CAUSALITY: the onScrollChanged it triggers is our echo
     * wherever it landed, even when clamped short. An animated programmatic scroll (snap)
     * spans many frames, all ours until it settles on the target. Mislabeling an echo as a
     * user scroll would let the core abandon its correction and freeze the visible window.
     */
    boolean userScrolled = true;
    if (mProgrammaticPending) {
      if (mProgrammaticAnimated) {
        // Intermediate or final frame of our own animation; never a user move.
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
        // Instant programmatic scroll: this frame is our echo, wherever it landed.
        userScrolled = false;
        mEchoedToken = mArmedToken;
        mProgrammaticPending = false;
        mArmedToken = 0;
      }
    }
    /*
     * Keep echoing the last applied token on the reports after its echo. State updates
     * coalesce, so the echo report itself can be replaced by the next fling frame before the
     * core lays it out; the core would then read the correction as gesture travel and apply
     * it again. Operation ids are never reused, so a token the core already released matches
     * nothing.
     */
    long echoToken = mEchoedToken;

    WritableMap map = new WritableNativeMap();

    map.putDouble("containerOffsetX", PixelUtil.toDIPFromPixel(scrollX));
    map.putDouble("containerOffsetY", PixelUtil.toDIPFromPixel(scrollY));
    map.putBoolean("containerOffsetEnabled", false);
    map.putBoolean("userScrolled", userScrolled);
    /*
     * The live gesture phase (finger down, momentum, idle). The core keeps the inverted
     * bottom pin off while it is not idle (see Container::gestureActive).
     */
    double scrollPhase = mTouching
      ? SCROLL_PHASE_DRAGGING
      : (mSettling ? SCROLL_PHASE_SETTLING : SCROLL_PHASE_IDLE);
    map.putDouble("scrollPhase", scrollPhase);
    map.putDouble("commitToken", (double) echoToken);
    carryScrollCommand(map);

    if (DEBUG_LOG) {
      slLog(String.format("java.onScrollChanged: offset=(%.1f,%.1f) userScrolled=%b phase=%.0f",
        PixelUtil.toDIPFromPixel(scrollX), PixelUtil.toDIPFromPixel(scrollY), userScrolled, scrollPhase));
    }
    mState.updateState(map);
    return userScrolled;
  }

  public void updateState(@Nullable StateWrapper stateWrapper) {
    mState = stateWrapper;

    if (mState == null) {
      return;
    }

    ReadableMap nextStateData = mState.getStateData();
    if (nextStateData == null) {
      return;
    }

    mStickyController.cacheStickyGeometry(nextStateData);

    if (nextStateData.hasKey("snapOffsets")) {
      ReadableArray snapOffsets = nextStateData.getArray("snapOffsets");
      if (snapOffsets != null) {
        float[] offsetsPx = new float[snapOffsets.size()];
        for (int i = 0; i < snapOffsets.size(); i++) {
          offsetsPx[i] = PixelUtil.toPixelFromDIP((float) snapOffsets.getDouble(i));
        }
        mSnapOffsetsPx = offsetsPx;
      }
    }

    if (DEBUG_LOG) {
      slLog(String.format("java.updateState: contentSize=(%.1f,%.1f) enabled=%d offset=(%.1f,%.1f) curOffset=(%.1f,%.1f)",
        nextStateData.hasKey("totalContainerWidth") ? nextStateData.getDouble("totalContainerWidth") : 0.0,
        nextStateData.hasKey("totalContainerHeight") ? nextStateData.getDouble("totalContainerHeight") : 0.0,
        (nextStateData.hasKey("containerOffsetEnabled") && nextStateData.getBoolean("containerOffsetEnabled")) ? 1 : 0,
        nextStateData.hasKey("containerOffsetX") ? nextStateData.getDouble("containerOffsetX") : 0.0,
        nextStateData.hasKey("containerOffsetY") ? nextStateData.getDouble("containerOffsetY") : 0.0,
        PixelUtil.toDIPFromPixel(mScrollView.getScrollX()), PixelUtil.toDIPFromPixel(mScrollView.getScrollY())));
    }

    if (nextStateData.hasKey("totalContainerWidth") && nextStateData.hasKey("totalContainerHeight")) {
      float totalContainerWidth = (float) nextStateData.getDouble("totalContainerWidth");
      float totalContainerHeight = (float) nextStateData.getDouble("totalContainerHeight");

      int newContentWidth = (int) PixelUtil.toPixelFromDIP(totalContainerWidth);
      int newContentHeight = (int) PixelUtil.toPixelFromDIP(totalContainerHeight);

      mContentView.layout(0, 0, newContentWidth, newContentHeight);
    }

    // While the drag owns the offset, a core correction must not yank the content.
    if (!mDragController.ownsScrollOffset()
        && nextStateData.hasKey("containerOffsetEnabled") && nextStateData.getBoolean("containerOffsetEnabled")) {
      if (nextStateData.hasKey("containerOffsetX") && nextStateData.hasKey("containerOffsetY")) {
        float containerOffsetX = (float) nextStateData.getDouble("containerOffsetX");
        float containerOffsetY = (float) nextStateData.getDouble("containerOffsetY");

        int appliedX = (int) PixelUtil.toPixelFromDIP(containerOffsetX);
        int appliedY = (int) PixelUtil.toPixelFromDIP(containerOffsetY);
        long token = nextStateData.hasKey("commitToken") ? (long) nextStateData.getDouble("commitToken") : 0;
        int beforeX = mScrollView.getScrollX();
        int beforeY = mScrollView.getScrollY();
        /*
         * A finger or a fling keeps moving the view after the report this correction was
         * computed from, and the commit mounts frames later. Writing the absolute offset throws
         * that travel away and yanks the view back to where it was; a nudge of a fraction of a
         * point then reads as a jump of a whole screen. Shift the live offset by the correction
         * instead (containerOffsetBase is the offset the core started from), as iOS does.
         *
         * That covers operation corrections (MVCP) too: the core confirms a correction that ran
         * under a gesture by its echo, without driving the view on to an absolute target (see
         * Container::gestureOperationId), so travel dropped by an absolute write is never
         * recovered. A correction computed from a gesture report stays a shift once the motion
         * has stopped, and so does a retarget of a correction already shifted. The core
         * retargets an operation against every newer report until the host echoes its token,
         * and each retarget carries the whole correction again (the base stays where its first
         * write started), so only the part this token has not applied yet is shifted.
         */
        boolean hasBase = nextStateData.hasKey("containerOffsetBaseX") && nextStateData.hasKey("containerOffsetBaseY");
        boolean continuesShiftedCorrection = token != 0 && token == mShiftedToken;
        boolean computedDuringGesture = token != 0
          && ((nextStateData.hasKey("userScrolled") && nextStateData.getBoolean("userScrolled"))
            || (nextStateData.hasKey("scrollPhase") && nextStateData.getDouble("scrollPhase") != SCROLL_PHASE_IDLE));
        boolean shiftLiveOffset = hasBase
          && (mTouching || mSettling || continuesShiftedCorrection || computedDuringGesture);
        if (shiftLiveOffset) {
          double deltaX = containerOffsetX - nextStateData.getDouble("containerOffsetBaseX");
          double deltaY = containerOffsetY - nextStateData.getDouble("containerOffsetBaseY");
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
         * An operation-driven correction (MVCP / scrollToIndex, which carries a commit token)
         * and a correction shifted onto the live offset rebuild the momentum below. An
         * absolute token-0 write is a layout reassert or a measurement nudge that may echo an
         * offset already a frame stale; rebuilding the fling from it would restart momentum
         * from the past on every frame of a fling. Our own animated scroll (snap glide /
         * animated scrollToOffset) also keeps the plain write. Read before
         * markProgrammaticScroll overwrites the flags.
         */
        boolean preserveMomentum = (token != 0 || shiftLiveOffset) && !(mProgrammaticPending && mProgrammaticAnimated);
        /*
         * Arm before the write: scrollTo invokes onScrollChanged synchronously when it
         * moves, and updateScrollState must see the armed token to echo it back.
         */
        markProgrammaticScroll(appliedX, appliedY, false, token);
        /*
         * A plain scrollTo leaves the scroll view's OverScroller running, and its next tick
         * writes the animation's own position straight back over ours. That is how a prepend
         * landing while a fling was still settling against the top edge lost its MVCP
         * correction: the spring-back dragged the view from the anchored offset back to 0 and
         * reported it as a user scroll, so the new rows took over the viewport. Rebuild any
         * in-flight fling from the corrected offset instead (what RN's own
         * maintainVisibleContentPosition does), so momentum survives and nothing undoes the
         * write.
         */
        if (preserveMomentum && mScrollView instanceof ReactScrollView) {
          ((ReactScrollView) mScrollView).scrollToPreservingMomentum(appliedX, appliedY);
        } else if (preserveMomentum && mScrollView instanceof ReactHorizontalScrollView) {
          ((ReactHorizontalScrollView) mScrollView).scrollToPreservingMomentum(appliedX, appliedY);
        } else {
          mScrollView.scrollTo(appliedX, appliedY);
        }
        /*
         * No-op / fully-clamped write moves nothing, so no onScrollChanged fires and the
         * arm would never clear, swallowing the next genuine user scroll: disarm it now.
         */
        if (Math.abs(mScrollView.getScrollX() - beforeX) <= 1
            && Math.abs(mScrollView.getScrollY() - beforeY) <= 1) {
          mProgrammaticPending = false;
          mArmedToken = 0;
        }
      }
    }

    // Re-pin after the content size/offset changed (the footer pin depends on content size).
    mStickyController.applyStickyTransforms();

    // Mid-drag: reglue the picked-up row to the finger and clear the shuffle once committed.
    mDragController.onStateCommitted();
  }

  /*
   * A partial state update is built on the state this host last mounted, whose offset can be
   * many frames old while a fling runs. Write the live offset into it, or the update hands
   * that old offset back to the core: the core virtualizes rows for a place the view has
   * already left (a blank window), and a later offset reassert scrolls the view back there.
   * The update applies no correction, so it disables the offset write; it carries the last
   * echoed token like a scroll report (see updateScrollState).
   * Package-private so the drag controller's state updates carry it too.
   */
  void carryLiveOffset(WritableMap map) {
    map.putDouble("containerOffsetX", PixelUtil.toDIPFromPixel(mScrollView.getScrollX()));
    map.putDouble("containerOffsetY", PixelUtil.toDIPFromPixel(mScrollView.getScrollY()));
    map.putBoolean("containerOffsetEnabled", false);
    map.putDouble("commitToken", (double) mEchoedToken);
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
   * A scroll command supersedes any momentum still running: stop the fling (and a snap glide)
   * so its next tick cannot write over the offset the core is about to apply, and report the
   * idle phase with the command. A plain scrollTo leaves the OverScroller running, and a
   * token-carrying correction would even rebuild the fling from the target. A finger still on
   * the list keeps its dragging phase, so the core lets the drag cancel the command.
   */
  private void yieldMomentumInto(WritableMap map) {
    if (mTouching) {
      return;
    }
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
    map.putBoolean("userScrolled", false);
    map.putDouble("scrollPhase", SCROLL_PHASE_IDLE);
  }

  /*
   * Record a scroll command and write it into its own state update. The sequence also moves
   * past the last one issued, so a mounted state still carrying an older sequence cannot make
   * a new command reuse it.
   */
  private void issueScrollCommand(WritableMap map, double index, double nextSequence, double viewPosition) {
    mCommandSequence = Math.max(nextSequence, mCommandSequence + 1);
    mCommandIndex = index;
    mCommandViewPosition = viewPosition;
    map.putDouble("containerOffsetIndex", mCommandIndex);
    map.putDouble("containerOffsetIndexSequence", mCommandSequence);
    map.putDouble("containerOffsetIndexViewPosition", mCommandViewPosition);
  }

  // Package-private so the drag controller's state updates carry the command too.
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

    // Bump the sequence so the core rescrolls even when the index is unchanged.
    double nextSequence = 0;
    ReadableMap currentStateData = mState.getStateData();
    if (currentStateData != null && currentStateData.hasKey("containerOffsetIndexSequence")) {
      nextSequence = currentStateData.getDouble("containerOffsetIndexSequence") + 1;
    }

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
    // Direct offset scroll along the axis; marked programmatic so its frames are not user scrolls.
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
     * Use the SCROLL_TO_END_INDEX sentinel (-3) so the core converges on the true bottom
     * as off-screen rows are measured, instead of jumping to a stale estimated bottom.
     */
    double nextSequence = 0;
    ReadableMap currentStateData = mState.getStateData();
    if (currentStateData != null && currentStateData.hasKey("containerOffsetIndexSequence")) {
      nextSequence = currentStateData.getDouble("containerOffsetIndexSequence") + 1;
    }

    WritableMap map = new WritableNativeMap();
    yieldMomentumInto(map);
    issueScrollCommand(map, -3.0, nextSequence, 0.0);
    map.putBoolean("containerOffsetEnabled", true);
    if (DEBUG_LOG) {
      slLog("java.cmd scrollToEnd: sequence=" + (long) mCommandSequence);
    }
    mState.updateState(map);
  }

  // Package-private accessors to the shared views, axis flag and state for the controllers.
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
