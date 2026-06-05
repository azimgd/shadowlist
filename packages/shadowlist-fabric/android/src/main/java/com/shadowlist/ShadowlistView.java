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

import androidx.annotation.Nullable;

import com.facebook.react.bridge.ReadableMap;
import com.facebook.react.bridge.WritableMap;
import com.facebook.react.bridge.WritableNativeMap;
import com.facebook.react.uimanager.PixelUtil;
import com.facebook.react.uimanager.StateWrapper;
import com.facebook.react.views.scroll.ReactHorizontalScrollView;
import com.facebook.react.views.scroll.ReactScrollView;

/*
 * Hosts the scrolling content in an inner scroll view picked by the `horizontal`
 * prop: ReactScrollView for vertical, ReactHorizontalScrollView for horizontal.
 * Android splits the two (a vertical ScrollView cannot scroll along X), so a
 * horizontal list needs the horizontal subclass for native fling/overscroll and
 * for nested touch interception to resolve correctly.
 *
 * This class keeps the scroll/content plumbing, the Fabric state sync and the
 * imperative commands; sticky pinning lives in ShadowlistStickyController and
 * drag-to-reorder in ShadowlistDragController, both reaching the shared scroll/content
 * views and state through the package-private accessors below. The inner scroll
 * subclasses just forward scroll and touch-down callbacks up.
 */
public class ShadowlistView extends FrameLayout {
  /*
   * Trace the native <-> C++ core state synchronization. Mirrors the
   * SHADOWLIST_DEBUG_LOG flag in shadowlist-core/Constants.hpp (and the iOS
   * mm.* logs) and shares the same [SL] tag so all layers interleave into one
   * stream. Filter with: adb logcat -s SL
   */
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

  /*
   * Horizontal / vertical axis (the `horizontal` prop). Shared by the sticky and drag
   * controllers, which read it through isHorizontal(); a change re-installs the inner
   * scroll view for the new axis.
   */
  private boolean mHorizontal = false;

  /*
   * A programmatic scroll we issued (a core correction, or scrollToOffset/End) is in
   * flight. onScrollChanged fires for these too, so reporting them as user gestures
   * would make the core abandon its own in-flight correction and latch/blank the
   * visible window. We track the target and whether it is animated - smoothScrollTo
   * emits many intermediate frames before reaching the target, none of which match
   * it - and a user touch (onTouchEvent) clears the flag so a finger taking over
   * mid-animation correctly wins.
   */
  private int mProgrammaticTargetX = 0;
  private int mProgrammaticTargetY = 0;
  private boolean mProgrammaticPending = false;
  private boolean mProgrammaticAnimated = false;

  /*
   * An onScrollChanged offset within this many pixels of the offset we applied is
   * its own echo, not a user scroll.
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
   * The inner scroll views forward their scroll and touch-down callbacks to the host
   * so all logic stays in one place regardless of axis.
   */
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
    public boolean onTouchEvent(MotionEvent ev) {
      if (ev.getActionMasked() == MotionEvent.ACTION_DOWN) {
        mHost.handleInnerTouchDown();
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
    public boolean onTouchEvent(MotionEvent ev) {
      if (ev.getActionMasked() == MotionEvent.ACTION_DOWN) {
        mHost.handleInnerTouchDown();
      }
      return super.onTouchEvent(ev);
    }
  }

  public ShadowlistView(Context context) {
    super(context);
    mContentView = new ContentContainer(context);
    mStickyController = new ShadowlistStickyController(this);
    mDragController = new ShadowlistDragController(this, context);
    installScrollView(false);
  }

  /*
   * Build the inner scroll view for the current axis and move the content container
   * into it. Called on construction and whenever the `horizontal` prop flips; the
   * content (and its mounted children) is re-parented, not rebuilt.
   */
  private void installScrollView(boolean horizontal) {
    if (mScrollView != null) {
      mScrollView.removeView(mContentView);
      removeView(mScrollView);
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
    addView(mScrollView, new FrameLayout.LayoutParams(
      FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
  }

  /*
   * Content child management. Fabric mounts element/template views into this host;
   * the ShadowlistViewManager routes those calls here so they land in the content
   * container inside the inner scroll view (instead of the host or scroll view).
   */
  public void addContentView(View child, int index) {
    if (child instanceof ShadowlistElementView) {
      mContentView.addView(child, index);
      // A newly mounted element can land above a pinned header/footer or section
      // header; re-pin so the active sticky views stay on top (and a freshly mounted
      // active section header gets its translation at once).
      mStickyController.applyStickyTranslations();
      // A row mounting mid-drag (auto-scroll) needs its make-room shuffle offset
      // applied at once so it appears in the right place rather than flashing in at its
      // resting slot first. The dragged row's translationZ keeps it drawn on top
      // without reordering the child array (which would desync Fabric's mounting).
      if (mDragController.isDragging()) {
        mDragController.applyDragShuffle();
      }
      return;
    }
    if (child instanceof ShadowlistTemplateView) {
      mContentView.addView(child);
    }
  }

  public int getContentChildCount() {
    return mContentView.getChildCount();
  }

  public View getContentChildAt(int index) {
    return mContentView.getChildAt(index);
  }

  public void removeContentViewAt(int index) {
    mContentView.removeViewAt(index);
  }

  private void handleInnerScroll(int scrollX, int scrollY) {
    // Advance the auto-hide only on genuine user scrolls (not our own echoed offset).
    boolean userScrolled = updateScrollState(scrollX, scrollY);
    mStickyController.applyStickyTranslations(userScrolled);
  }

  private void handleInnerTouchDown() {
    // A finger on the list takes over from any in-flight programmatic scroll, so
    // the following onScrollChanged callbacks are reported as genuine user scrolls.
    mProgrammaticPending = false;
  }

  private void markProgrammaticScroll(int targetX, int targetY, boolean animated) {
    mProgrammaticTargetX = targetX;
    mProgrammaticTargetY = targetY;
    mProgrammaticAnimated = animated;
    mProgrammaticPending = true;
  }

  public void setDragEnabled(boolean dragEnabled) {
    mDragController.setEnabled(dragEnabled);
  }

  /*
   * Called when Fabric drops/recycles this host (ShadowlistViewManager.onDropViewInstance):
   * tear down any in-flight drag so its self-reposting Choreographer callback cannot
   * keep driving a detached view and the inner scroll is restored.
   */
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

  /* Toggle the inner scroll view's gesture handling (the drag drives the offset itself). */
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

  private boolean updateScrollState(int scrollX, int scrollY) {
    if (mState == null) {
      return false;
    }

    /*
     * A callback whose offset matches the one the core just applied is the core's
     * own move, not a user gesture. Everything else (drag, fling) is a genuine
     * user scroll, which lets the core abandon an in-flight correction instead of
     * letting it latch and freeze the visible window (blank list on deep scroll).
     */
    boolean userScrolled = true;
    if (mProgrammaticPending) {
      boolean reachedTarget =
        Math.abs(scrollX - mProgrammaticTargetX) <= PROGRAMMATIC_SCROLL_TOLERANCE_PX
          && Math.abs(scrollY - mProgrammaticTargetY) <= PROGRAMMATIC_SCROLL_TOLERANCE_PX;
      if (reachedTarget) {
        // The programmatic scroll settled on its target: this is its echo, not a user move.
        userScrolled = false;
        mProgrammaticPending = false;
      } else if (mProgrammaticAnimated) {
        // An intermediate frame of our own smooth-scroll animation; still not a user move.
        userScrolled = false;
      } else {
        // An instant programmatic scroll that did not land on the target means a real
        // user scroll arrived instead, so hand control back to the user.
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

    // While a drag is in flight (or settling after a drop) the drag owns the scroll
    // offset, so a core correction must not yank the content under the finger or jump
    // the list as the reorder lands.
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

    // Re-pin after the content size / offset changed (the footer pin depends on
    // the content size) even when the list is not actively scrolling.
    mStickyController.applyStickyTranslations();

    // Mid-drag commit: re-glue the picked-up row to the finger, and once JS's reorder
    // commit lands, clear the held make-room shuffle.
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

    // Bump the nonce so the core treats this as a fresh request and re-scrolls
    // even when the index is unchanged from the previous call
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
    // Direct offset scroll along the scroll axis; the core picks up the new position
    // from the resulting onScrollChanged callback. Marked programmatic so the
    // animated path's intermediate frames are not mistaken for a user scroll.
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

    // Core-driven: ride the scrollToIndex command channel with the SCROLL_TO_END_INDEX
    // sentinel (-3, see shadowlist-core/Constants.hpp) so the core converges on the
    // true bottom as off-screen rows are measured, instead of a one-shot jump to the
    // current content size - a stale, estimate-based bottom that stops short on a
    // variable-height list. The animated flag no longer applies (the core steps to
    // the bottom).
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

  /*
   * Package-private accessors for the sticky and drag controllers: the host owns the
   * scroll/content views, the axis flag and the Fabric state; the controllers read
   * them here so all the shared mutable plumbing stays in one place.
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
