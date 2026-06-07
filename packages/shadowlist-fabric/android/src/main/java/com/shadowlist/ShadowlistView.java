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
 * prop. Keeps the scroll/content plumbing, state sync and imperative commands;
 * sticky pinning lives in ShadowlistStickyController and drag-to-reorder in
 * ShadowlistDragController, both reaching the shared views and state through the
 * package-private accessors below.
 */
public class ShadowlistView extends FrameLayout {
  // State-sync trace logging, filter with: adb logcat -s SL
  private static final boolean DEBUG_LOG = false;
  private static final String LOG_TAG = "SL";

  static void slLog(String message) {
    if (DEBUG_LOG) {
      Log.d(LOG_TAG, "[SL] " + message);
    }
  }

  private @Nullable StateWrapper mState = null;
  private ContentContainer mContentView;
  private ViewGroup mScrollView;
  private final ShadowlistStickyController mStickyController;
  private final ShadowlistDragController mDragController;

  // Re-pin sticky views once a template (header/footer) child is (re)laid out by the mounting
  // layer, so a sticky footer tracks its real resting position across list-size changes.
  private final View.OnLayoutChangeListener mTemplateLayoutListener;

  // Horizontal/vertical axis (the `horizontal` prop); a change re-installs the inner scroll view.
  private boolean mHorizontal = false;

  /*
   * View snapping. mSnapToItem enables it; mSnapOffsetsPx is the core's resting snap
   * offsets (DIP -> px) cached from state; mTouching gates the touch-up settle so it
   * never fights a finger still on screen.
   */
  private boolean mSnapToItem = false;
  private float[] mSnapOffsetsPx = new float[0];
  private boolean mTouching = false;

  // Keyboard-avoidance bottom inset (px); held to diff against the next value for the delta.
  private int mContentInsetBottom = 0;

  // Pull-to-refresh state; all held so an axis-flip re-install restores them.
  @Nullable private SwipeRefreshLayout mRefreshLayout = null;
  private boolean mRefreshEnabled = false;
  private boolean mRefreshing = false;
  @Nullable private Integer mRefreshColor = null;

  /*
   * Tracks a programmatic scroll we issued so its echoed callbacks are not reported as
   * user gestures, which would make the core abandon its in-flight correction and blank
   * the visible window. A user touch clears the flag so a finger taking over wins.
   */
  private int mProgrammaticTargetX = 0;
  private int mProgrammaticTargetY = 0;
  private boolean mProgrammaticPending = false;
  private boolean mProgrammaticAnimated = false;

  // An offset within this many pixels of the one we applied is our own echo, not a user scroll.
  private static final int PROGRAMMATIC_SCROLL_TOLERANCE_PX = 2;

  private static class ContentContainer extends ViewGroup {
    public ContentContainer(Context context) {
      super(context);
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    }
  }

  // Inner scroll views forward scroll and touch-down callbacks to the host.
  private static class InnerVerticalScrollView extends ReactScrollView {
    private final ShadowlistView mHost;

    InnerVerticalScrollView(Context context, ShadowlistView host) {
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
      if (mHost.snapFling(velocityY)) {
        return;
      }
      super.fling(velocityY);
    }

    @Override
    public boolean onTouchEvent(MotionEvent ev) {
      int action = ev.getActionMasked();
      if (action == MotionEvent.ACTION_DOWN) {
        mHost.handleInnerTouchDown();
      } else if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
        mHost.handleInnerTouchUp();
      }
      return super.onTouchEvent(ev);
    }
  }

  private static class InnerHorizontalScrollView extends ReactHorizontalScrollView {
    private final ShadowlistView mHost;

    InnerHorizontalScrollView(Context context, ShadowlistView host) {
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
      if (mHost.snapFling(velocityX)) {
        return;
      }
      super.fling(velocityX);
    }

    @Override
    public boolean onTouchEvent(MotionEvent ev) {
      int action = ev.getActionMasked();
      if (action == MotionEvent.ACTION_DOWN) {
        mHost.handleInnerTouchDown();
      } else if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
        mHost.handleInnerTouchUp();
      }
      return super.onTouchEvent(ev);
    }
  }

  public ShadowlistView(Context context) {
    super(context);
    mContentView = new ContentContainer(context);
    mStickyController = new ShadowlistStickyController(this);
    mDragController = new ShadowlistDragController(this, context);
    mTemplateLayoutListener =
      (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
        if (left != oldLeft || top != oldTop || right != oldRight || bottom != oldBottom) {
          mStickyController.applyStickyTranslations();
        }
      };
    installScrollView(false);
  }

  // Build the inner scroll view for the current axis and re-parent the content into it.
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
  }

  public void setRefreshColor(@Nullable Integer color) {
    mRefreshColor = color;
    if (mRefreshLayout != null && color != null) {
      mRefreshLayout.setColorSchemeColors(color);
    }
  }

  private void emitRefresh() {
    ReactContext reactContext = (ReactContext) getContext();
    EventDispatcher dispatcher =
      UIManagerHelper.getEventDispatcherForReactTag(reactContext, getId());
    if (dispatcher != null) {
      dispatcher.dispatchEvent(
        new ShadowlistRefreshEvent(UIManagerHelper.getSurfaceId(this), getId()));
    }
  }

  // Element/template children land in the content container inside the inner scroll view.
  public void addContentView(View child, int index) {
    if (child instanceof ShadowlistElementView) {
      mContentView.addView(child, index);
      // Re-pin so active sticky views stay on top of the newly mounted element.
      mStickyController.applyStickyTranslations();
      // A row mounting mid-drag needs its make-room shuffle applied at once to avoid a flash.
      if (mDragController.isDragging()) {
        mDragController.applyDragShuffle();
      }
      return;
    }
    if (child instanceof ShadowlistTemplateView) {
      mContentView.addView(child);
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
    if (child instanceof ShadowlistTemplateView) {
      child.removeOnLayoutChangeListener(mTemplateLayoutListener);
    }
    mContentView.removeViewAt(index);
  }

  private void handleInnerScroll(int scrollX, int scrollY) {
    // Advance the auto-hide only on genuine user scrolls (not our own echoed offset).
    boolean userScrolled = updateScrollState(scrollX, scrollY);
    mStickyController.applyStickyTranslations(userScrolled);
  }

  private void handleInnerTouchDown() {
    // A finger takes over from any in-flight programmatic scroll.
    mProgrammaticPending = false;
    mTouching = true;
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

  private void markProgrammaticScroll(int targetX, int targetY, boolean animated) {
    mProgrammaticTargetX = targetX;
    mProgrammaticTargetY = targetY;
    mProgrammaticAnimated = animated;
    mProgrammaticPending = true;
  }

  public void setDragEnabled(boolean dragEnabled) {
    mDragController.setEnabled(dragEnabled);
  }

  // Tear down any in-flight drag and sticky state before this host is recycled.
  void onDropInstance() {
    mDragController.teardown();
    mStickyController.reset();
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
    mStickyController.applyStickyTranslations();
  }

  /*
   * Keyboard avoidance (vertical only): grow the bottom padding to the inset and shift
   * the offset by the delta so rows behind the keyboard come into view. Skipped while a
   * drag owns the offset.
   */
  public void setContentInsetBottom(double insetDip) {
    int inset = Math.max(0, (int) PixelUtil.toPixelFromDIP((float) insetDip));
    if (inset == mContentInsetBottom) {
      return;
    }
    int delta = inset - mContentInsetBottom;
    mContentInsetBottom = inset;

    if (mHorizontal) {
      return;
    }

    mScrollView.setPadding(0, 0, 0, inset);

    if (!mDragController.ownsScrollOffset()) {
      int targetX = mScrollView.getScrollX();
      int targetY = Math.max(0, mScrollView.getScrollY() + delta);
      markProgrammaticScroll(targetX, targetY, true);
      if (mScrollView instanceof ReactScrollView) {
        ((ReactScrollView) mScrollView).smoothScrollTo(targetX, targetY);
      }
    }
  }

  private boolean updateScrollState(int scrollX, int scrollY) {
    if (mState == null) {
      return false;
    }

    // A callback matching the offset we just applied is our own echo, not a user gesture.
    // Mislabeling it would let the core latch and freeze the visible window.
    boolean userScrolled = true;
    if (mProgrammaticPending) {
      boolean reachedTarget =
        Math.abs(scrollX - mProgrammaticTargetX) <= PROGRAMMATIC_SCROLL_TOLERANCE_PX
          && Math.abs(scrollY - mProgrammaticTargetY) <= PROGRAMMATIC_SCROLL_TOLERANCE_PX;
      if (reachedTarget) {
        // Settled on the target: this is the echo, not a user move.
        userScrolled = false;
        mProgrammaticPending = false;
      } else if (mProgrammaticAnimated) {
        // Intermediate frame of our own animation; still not a user move.
        userScrolled = false;
      } else {
        // An instant scroll that missed the target means a real user scroll arrived; yield.
        mProgrammaticPending = false;
      }
    }

    WritableMap map = new WritableNativeMap();

    map.putDouble("containerOffsetX", PixelUtil.toDIPFromPixel(scrollX));
    map.putDouble("containerOffsetY", PixelUtil.toDIPFromPixel(scrollY));
    map.putBoolean("containerOffsetEnabled", false);
    map.putBoolean("userScrolled", userScrolled);

    if (DEBUG_LOG) {
      slLog(String.format("java.onScrollChanged: offset=(%.1f,%.1f) userScrolled=%b",
        PixelUtil.toDIPFromPixel(scrollX), PixelUtil.toDIPFromPixel(scrollY), userScrolled));
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
        markProgrammaticScroll(appliedX, appliedY, false);
        mScrollView.scrollTo(appliedX, appliedY);
      }
    }

    // Re-pin after the content size/offset changed (the footer pin depends on content size).
    mStickyController.applyStickyTranslations();

    // Mid-drag: re-glue the picked-up row to the finger and clear the shuffle once committed.
    mDragController.onStateCommitted();
  }

  public void setStartReachedEnabled(boolean enabled) {
    if (mState == null) {
      return;
    }

    WritableMap map = new WritableNativeMap();

    map.putBoolean("startReachedEnabled", enabled);

    slLog("java.cmd setStartReachedEnabled: enabled=" + (enabled ? 1 : 0));
    mState.updateState(map);
  }

  public void setEndReachedEnabled(boolean enabled) {
    if (mState == null) {
      return;
    }

    WritableMap map = new WritableNativeMap();

    map.putBoolean("endReachedEnabled", enabled);

    slLog("java.cmd setEndReachedEnabled: enabled=" + (enabled ? 1 : 0));
    mState.updateState(map);
  }

  public void scrollToIndex(int index) {
    if (mState == null) {
      return;
    }

    // Bump the nonce so the core re-scrolls even when the index is unchanged.
    double nextNonce = 0;
    ReadableMap currentStateData = mState.getStateData();
    if (currentStateData != null && currentStateData.hasKey("containerOffsetIndexNonce")) {
      nextNonce = currentStateData.getDouble("containerOffsetIndexNonce") + 1;
    }

    WritableMap map = new WritableNativeMap();
    map.putDouble("containerOffsetIndex", (double) index);
    map.putDouble("containerOffsetIndexNonce", nextNonce);
    map.putBoolean("containerOffsetEnabled", true);
    slLog("java.cmd scrollToIndex: index=" + index + " nonce=" + (long) nextNonce);
    mState.updateState(map);
  }

  public void scrollToOffset(double offset, boolean animated) {
    // Direct offset scroll along the axis; marked programmatic so its frames are not user scrolls.
    int px = (int) PixelUtil.toPixelFromDIP((float) offset);
    int targetX = mHorizontal ? px : mScrollView.getScrollX();
    int targetY = mHorizontal ? mScrollView.getScrollY() : px;
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

    // Use the SCROLL_TO_END_INDEX sentinel (-3) so the core converges on the true bottom
    // as off-screen rows are measured, instead of jumping to a stale estimated bottom.
    double nextNonce = 0;
    ReadableMap currentStateData = mState.getStateData();
    if (currentStateData != null && currentStateData.hasKey("containerOffsetIndexNonce")) {
      nextNonce = currentStateData.getDouble("containerOffsetIndexNonce") + 1;
    }

    WritableMap map = new WritableNativeMap();
    map.putDouble("containerOffsetIndex", -3.0);
    map.putDouble("containerOffsetIndexNonce", nextNonce);
    map.putBoolean("containerOffsetEnabled", true);
    slLog("java.cmd scrollToEnd: nonce=" + (long) nextNonce);
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
