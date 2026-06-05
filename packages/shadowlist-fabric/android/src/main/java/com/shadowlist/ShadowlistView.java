package com.shadowlist;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.util.Log;
import android.view.Choreographer;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;

import com.facebook.react.bridge.ReadableArray;
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
 * for nested touch interception to resolve correctly. All scroll, sticky and
 * state-sync logic lives here; the inner subclasses just forward scroll and
 * touch-down callbacks up.
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

  private static void slLog(String message) {
    if (DEBUG_LOG) {
      Log.d(LOG_TAG, "[SL] " + message);
    }
  }

  private @Nullable StateWrapper mState = null;
  private ContentContainer mContentView;
  private ViewGroup mScrollView;

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

  /*
   * Sticky header/footer are pinned natively here (not in the core layout) so they
   * track scrolling on the UI thread without the commit-cycle latency that made the
   * core-driven version choppy. The template views keep their resting position from
   * the shadow node; we layer a translation on top each scroll frame.
   */
  private boolean mStickyHeader = false;
  private boolean mStickyFooter = false;
  private boolean mHorizontal = false;

  /*
   * Sticky section headers (SectionList). The active section header is an
   * always-mounted overlay template (templateType "sectionHeader"); the core
   * publishes the resting geometry (offset + size in DIP along the scroll axis) of
   * the in-flow section headers through state, and we pin the overlay to the
   * viewport start on each scroll tick (mirroring Container::resolveStickyHeader),
   * pushing it up as the next in-flow header arrives. The overlay never unmounts, so
   * its position has no commit-cycle latency.
   */
  private int[] mStickyHeaderIndices = new int[0];
  private double[] mStickyHeaderOffsets = new double[0];
  private double[] mStickyHeaderSizes = new double[0];

  /*
   * Drag-to-reorder. The whole gesture runs on the UI thread (like the sticky pin),
   * so it never waits on the commit cycle. A long press picks an element up; while
   * held we translate it under the finger every Choreographer frame, auto-scroll near
   * the viewport edges, and detect when its centre crosses into a neighbour's slot.
   * Each crossing is relayed to JS through state (a live array move), and the surviving
   * rows flow to their new slots via the normal layout; the picked-up view re-glues to
   * the finger against its new resting position. JS only sees onDragStart/Move/End.
   */
  private boolean mDragEnabled = false;
  private GestureDetector mDragGestureDetector;
  private Choreographer.FrameCallback mDragFrameCallback;
  private boolean mDragging = false;
  private ShadowlistElementView mDraggedView = null;
  /* The data order stays FIXED during the drag. mDragOriginIndex is where the row was
   * picked up; mDragInsertionIndex is where its centre currently sits (the gap), used
   * to shuffle the siblings between them. The single reorder is applied on drop. */
  private int mDragOriginIndex = -1;
  private int mDragInsertionIndex = -1;
  /* Size of the picked-up row along the scroll axis - the amount each shuffled sibling
   * is offset to open the gap. */
  private float mDraggedExtent = 0f;
  /* Distance along the scroll axis (content space) from the picked-up cell's leading
   * edge to the touch point, so the cell stays under the same spot of the finger. */
  private float mDragGrabOffset = 0f;
  /* Latest touch position along the scroll axis in viewport space - stable while the
   * content auto-scrolls under a still finger. */
  private float mDragTouchViewport = 0f;
  /* After a drop, hold the shuffle transforms until JS's single reorder commit lands
   * (the dropped row's index prop reaching mDropInsertionIndex), then clear them. */
  private boolean mDragDropPending = false;
  private ShadowlistElementView mDroppedView = null;
  private int mDropInsertionIndex = -1;

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
    installScrollView(false);

    // Long press picks a row up for drag-to-reorder. A quick swipe moves past the
    // touch slop and cancels the long press, so it still scrolls.
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
      applyStickyTranslations();
      // A row mounting mid-drag (auto-scroll) needs its make-room shuffle offset
      // applied at once so it appears in the right place rather than flashing in at its
      // resting slot first. The dragged row's translationZ keeps it drawn on top
      // without reordering the child array (which would desync Fabric's mounting).
      if (mDragging) {
        applyDragShuffle();
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
    updateScrollState(scrollX, scrollY);
    applyStickyTranslations();
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
    mDragEnabled = dragEnabled;
    if (!dragEnabled && mDragging) {
      teardownDrag();
    }
  }

  /*
   * Feed the long-press detector and, once a drag is in flight, steal the gesture from
   * the inner scroll view (returning true routes the rest of the sequence to our
   * onTouchEvent; Android delivers ACTION_CANCEL to the scroll view automatically).
   */
  @Override
  public boolean onInterceptTouchEvent(MotionEvent event) {
    if (mDragEnabled) {
      mDragGestureDetector.onTouchEvent(event);
    }
    if (mDragging) {
      return true;
    }
    return super.onInterceptTouchEvent(event);
  }

  @Override
  public boolean onTouchEvent(MotionEvent event) {
    if (mDragEnabled) {
      mDragGestureDetector.onTouchEvent(event);
    }
    if (mDragging) {
      switch (event.getActionMasked()) {
        case MotionEvent.ACTION_MOVE:
          mDragTouchViewport = mHorizontal ? event.getX() : event.getY();
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
    return super.onTouchEvent(event);
  }

  private void setInnerScrollEnabled(boolean enabled) {
    if (mScrollView instanceof ReactScrollView) {
      ((ReactScrollView) mScrollView).setScrollEnabled(enabled);
    } else if (mScrollView instanceof ReactHorizontalScrollView) {
      ((ReactHorizontalScrollView) mScrollView).setScrollEnabled(enabled);
    }
  }

  /* Topmost element child whose resting bounds contain the content-space point. The
   * translation a drag applies does not affect getLeft/getTop, so they give the
   * resting position even while a row is being dragged. */
  private ShadowlistElementView elementViewAtContent(float cx, float cy) {
    ShadowlistElementView result = null;
    for (int i = 0; i < mContentView.getChildCount(); i++) {
      View child = mContentView.getChildAt(i);
      if (!(child instanceof ShadowlistElementView)) {
        continue;
      }
      if (cx >= child.getLeft() && cx < child.getRight()
          && cy >= child.getTop() && cy < child.getBottom()) {
        result = (ShadowlistElementView) child;
      }
    }
    return result;
  }

  private void beginDrag(MotionEvent event) {
    float scrollX = mScrollView.getScrollX();
    float scrollY = mScrollView.getScrollY();
    float cx = event.getX() + scrollX;
    float cy = event.getY() + scrollY;

    ShadowlistElementView view = elementViewAtContent(cx, cy);
    if (view == null) {
      return;
    }
    int index = view.getElementIndex();
    if (index < 0) {
      return;
    }

    // Cancel any in-flight drop animation from a previous drag of this same view, so
    // its withEndAction (which resets translationZ) cannot fire mid-drag, and start
    // from a clean resting transform.
    view.animate().cancel();
    view.setTranslationX(0f);
    view.setTranslationY(0f);

    // Clean slate: drop any leftover post-drop transforms from a previous drag.
    mDragDropPending = false;
    mDroppedView = null;
    clearDragTransforms();

    mDragging = true;
    mDraggedView = view;
    mDragOriginIndex = index;
    mDragInsertionIndex = index;

    float restingLeading = mHorizontal ? view.getLeft() : view.getTop();
    mDraggedExtent = mHorizontal ? view.getWidth() : view.getHeight();
    float touchAxisContent = mHorizontal ? cx : cy;
    mDragGrabOffset = touchAxisContent - restingLeading;
    mDragTouchViewport = mHorizontal ? event.getX() : event.getY();

    setInnerScrollEnabled(false);

    // Lift feedback: raise the row above its siblings via Z (draw order), NOT
    // bringToFront(). bringToFront() reorders the child array, which desyncs Fabric's
    // index-based child mounting (removeContentViewAt) and corrupts the view tree -
    // breaking consecutive drags and scrambling row content. Android draws children
    // ordered by Z since API 21, so a higher translationZ lifts it without reordering.
    view.setTranslationZ(PixelUtil.toPixelFromDIP(8));

    dispatchDragEvent(1, index, index);
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
    float window = mHorizontal ? mScrollView.getWidth() : mScrollView.getHeight();
    float content = mHorizontal ? mContentView.getWidth() : mContentView.getHeight();
    float maxOffset = Math.max(0f, content - window);
    float offset = mHorizontal ? mScrollView.getScrollX() : mScrollView.getScrollY();
    float touch = mDragTouchViewport;

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

    int nx = mHorizontal ? (int) newOffset : mScrollView.getScrollX();
    int ny = mHorizontal ? mScrollView.getScrollY() : (int) newOffset;
    // Let the resulting onScrollChanged report this as a user scroll (do NOT mark it
    // programmatic): the drag owns the scroll position, so the core must virtualize at
    // this exact offset and abandon any maintain-visible-position correction. Marking
    // it programmatic makes the core keep its correction and compute the visible window
    // at a different offset than the viewport, which blanks rows mid-drag.
    mScrollView.scrollTo(nx, ny);
  }

  /*
   * Glue the picked-up row under the finger, recompute where it would insert, and
   * shuffle the siblings to open the gap. Nothing is sent to JS - the data order is
   * fixed until drop - so this never triggers a re-render.
   */
  private void updateDrag() {
    if (!mDragging || mDraggedView == null) {
      return;
    }

    float offset = mHorizontal ? mScrollView.getScrollX() : mScrollView.getScrollY();
    float touchContent = mDragTouchViewport + offset;
    float restingLeading = mHorizontal ? mDraggedView.getLeft() : mDraggedView.getTop();
    float extent = mHorizontal ? mDraggedView.getWidth() : mDraggedView.getHeight();
    float contentExtent = mHorizontal ? mContentView.getWidth() : mContentView.getHeight();

    float desiredLeading = touchContent - mDragGrabOffset;
    desiredLeading = Math.max(0f, Math.min(desiredLeading, Math.max(0f, contentExtent - extent)));

    float translation = desiredLeading - restingLeading;
    if (mHorizontal) {
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
   * The index the dragged row would insert at, by comparing its centre against each
   * neighbour's MIDPOINT relative to the (fixed) pickup index. Because the data order
   * never changes mid-drag the midpoints are stable, giving a clean half-row dead zone
   * around each one - boundary jitter cannot flip the insertion back and forth. Returns
   * the farthest neighbour whose midpoint the centre has crossed.
   */
  private int insertionIndexForCenter(float center) {
    int insertion = mDragOriginIndex;
    for (int i = 0; i < mContentView.getChildCount(); i++) {
      View child = mContentView.getChildAt(i);
      if (!(child instanceof ShadowlistElementView) || child == mDraggedView) {
        continue;
      }
      int idx = ((ShadowlistElementView) child).getElementIndex();
      if (idx < 0) {
        continue;
      }
      float lead = mHorizontal ? child.getLeft() : child.getTop();
      float extent = mHorizontal ? child.getWidth() : child.getHeight();
      float mid = lead + extent / 2f;
      if (idx > mDragOriginIndex && center > mid) {
        insertion = Math.max(insertion, idx);
      } else if (idx < mDragOriginIndex && center < mid) {
        insertion = Math.min(insertion, idx);
      }
    }
    return insertion;
  }

  /*
   * Open a one-row gap at the insertion point by translating the siblings between the
   * pickup and the insertion toward the vacated pickup slot. The shift equals the
   * picked-up row's extent, so each shuffled sibling lands exactly on its post-reorder
   * resting position - which is why clearing the transforms after the drop commit is
   * seamless (the siblings do not move, only the dragged row settles into the gap).
   */
  private void applyDragShuffle() {
    for (int i = 0; i < mContentView.getChildCount(); i++) {
      View child = mContentView.getChildAt(i);
      if (!(child instanceof ShadowlistElementView) || child == mDraggedView) {
        continue;
      }
      int idx = ((ShadowlistElementView) child).getElementIndex();
      if (idx < 0) {
        continue;
      }
      float shift = 0f;
      if (mDragOriginIndex < mDragInsertionIndex && idx > mDragOriginIndex && idx <= mDragInsertionIndex) {
        shift = -mDraggedExtent;
      } else if (mDragInsertionIndex < mDragOriginIndex && idx >= mDragInsertionIndex && idx < mDragOriginIndex) {
        shift = mDraggedExtent;
      }
      if (mHorizontal) {
        child.setTranslationX(shift);
        child.setTranslationY(0f);
      } else {
        child.setTranslationY(shift);
        child.setTranslationX(0f);
      }
    }
  }

  /* Reset every element view's drag transform and lift. */
  private void clearDragTransforms() {
    for (int i = 0; i < mContentView.getChildCount(); i++) {
      View child = mContentView.getChildAt(i);
      if (!(child instanceof ShadowlistElementView)) {
        continue;
      }
      child.setTranslationX(0f);
      child.setTranslationY(0f);
      child.setTranslationZ(0f);
    }
  }

  private void dispatchDragEvent(int type, int from, int to) {
    if (mState == null) {
      return;
    }
    double nonce = 1;
    ReadableMap currentStateData = mState.getStateData();
    if (currentStateData != null && currentStateData.hasKey("dragEventNonce")) {
      nonce = currentStateData.getDouble("dragEventNonce") + 1;
    }

    WritableMap map = new WritableNativeMap();
    map.putDouble("dragEventNonce", nonce);
    map.putDouble("dragEventType", type);
    map.putDouble("dragFromIndex", from);
    map.putDouble("dragToIndex", to);
    // Keep the core's scroll corrections off for the duration of the drag (the drag
    // owns the offset); a reorder commit must virtualize at the live offset, not snap
    // to an anchor. Cleared on the end event so normal scrolling resumes afterwards.
    map.putBoolean("userScrolled", type != 3);
    slLog("java.drag dispatch type=" + type + " from=" + from + " to=" + to);
    mState.updateState(map);
  }

  private void finishDrag() {
    if (!mDragging) {
      return;
    }
    stopDragLoop();
    setInnerScrollEnabled(true);

    int from = mDragOriginIndex;
    int to = mDragInsertionIndex;
    ShadowlistElementView view = mDraggedView;
    mDragging = false;
    mDraggedView = null;

    // Emit the single reorder. Hold the shuffle transforms (the rows already sit at
    // their post-reorder positions) until JS's reorder commit lands, then
    // clearDragTransforms runs from updateState - so nothing snaps back in between.
    dispatchDragEvent(3, from, to);

    if (from == to || view == null) {
      // No reorder (dropped where it started): no commit will move the index, so settle
      // immediately instead of waiting for a landing that never changes anything.
      clearDragTransforms();
      mDragDropPending = false;
      mDroppedView = null;
    } else {
      mDragDropPending = true;
      mDroppedView = view;
      mDropInsertionIndex = to;

      // Safety net: if the reorder never lands (e.g. a consumer that ignores
      // onReorder), clear the held transforms anyway so the gap cannot get stuck. A
      // real reorder clears earlier from updateState; a new drag clears at pickup.
      postDelayed(() -> {
        if (mDragDropPending && !mDragging) {
          clearDragTransforms();
          mDragDropPending = false;
          mDroppedView = null;
        }
      }, 300);
    }
  }

  /* Immediate teardown with no reorder (drag disabled). */
  private void teardownDrag() {
    stopDragLoop();
    mDragging = false;
    mDraggedView = null;
    mDragDropPending = false;
    mDroppedView = null;
    setInnerScrollEnabled(true);
    clearDragTransforms();
  }

  public void setStickyHeader(boolean stickyHeader) {
    mStickyHeader = stickyHeader;
    applyStickyTranslations();
  }

  public void setStickyFooter(boolean stickyFooter) {
    mStickyFooter = stickyFooter;
    applyStickyTranslations();
  }

  public void setHorizontal(boolean horizontal) {
    if (horizontal != mHorizontal) {
      mHorizontal = horizontal;
      installScrollView(horizontal);
    }
    applyStickyTranslations();
  }

  /*
   * Pin the sticky header/footer to the viewport by translating them relative to
   * their resting position. The header tracks the scroll offset (stays at the
   * start); the footer tracks offset + window - content so it stays at the viewport
   * end and lands exactly on its resting position at the scroll extreme. Runs on
   * the UI thread per scroll frame, so it stays in lockstep with the gesture.
   */
  private void applyStickyTranslations() {
    if (mContentView == null || mScrollView == null) {
      return;
    }

    int offsetX = mScrollView.getScrollX();
    int offsetY = mScrollView.getScrollY();
    int windowW = mScrollView.getWidth();
    int windowH = mScrollView.getHeight();
    int contentW = mContentView.getWidth();
    int contentH = mContentView.getHeight();

    for (int i = 0; i < mContentView.getChildCount(); i++) {
      View child = mContentView.getChildAt(i);
      if (!(child instanceof ShadowlistTemplateView)) {
        continue;
      }

      boolean isFooter = "footer".equals(((ShadowlistTemplateView) child).getTemplateType());
      boolean sticky = isFooter ? mStickyFooter : mStickyHeader;

      float translation = 0f;
      if (sticky) {
        if (isFooter) {
          translation = mHorizontal ? (offsetX + windowW - contentW) : (offsetY + windowH - contentH);
        } else {
          translation = mHorizontal ? offsetX : offsetY;
        }
      }

      if (mHorizontal) {
        child.setTranslationX(translation);
        child.setTranslationY(0f);
      } else {
        child.setTranslationY(translation);
        child.setTranslationX(0f);
      }
    }

    applyStickySectionHeaders();
    bringStickyViewsToFront();
  }

  /*
   * Pin the always-mounted section-header overlay to the viewport start, mirroring
   * Container::resolveStickyHeader. The overlay rests at the content origin and shows
   * the active section's header (its content is swapped in JS); here we only move it.
   * Walk the core-published in-flow header geometry for the active section (the last
   * header resting at/above the scroll offset) and the next one, pin the overlay to
   * the viewport start, and push it up as the next in-flow header scrolls in. When no
   * section is active (scrolled above the first header) the overlay is hidden, so the
   * regular list header shows through.
   */
  private void applyStickySectionHeaders() {
    if (mContentView == null || mScrollView == null) {
      return;
    }

    View overlay = findSectionHeaderOverlay();
    if (overlay == null) {
      return;
    }

    if (mStickyHeaderIndices.length == 0) {
      overlay.setVisibility(View.GONE);
      return;
    }

    double axisOffsetPx = mHorizontal ? mScrollView.getScrollX() : mScrollView.getScrollY();
    if (axisOffsetPx < 0.0) {
      axisOffsetPx = 0.0;
    }
    double axisOffset = PixelUtil.toDIPFromPixel((float) axisOffsetPx);

    /*
     * Headers are ascending by offset: the active one is the last resting at/above
     * the viewport start, and the first one past it is the "next" that pushes it up.
     */
    boolean hasActive = false;
    double activeSize = 0.0;
    boolean hasNext = false;
    double nextOffset = 0.0;
    for (int i = 0; i < mStickyHeaderOffsets.length; i++) {
      double headerOffset = mStickyHeaderOffsets[i];
      if (headerOffset <= axisOffset) {
        hasActive = true;
        activeSize = mStickyHeaderSizes[i];
      } else {
        nextOffset = headerOffset;
        hasNext = true;
        break;
      }
    }

    if (!hasActive) {
      overlay.setVisibility(View.GONE);
      return;
    }

    /*
     * The overlay rests at the content origin, so its translation along the scroll
     * axis is simply its displayed top: pinned to the viewport start (axisOffset),
     * pushed up to nextOffset - activeSize as the next in-flow header arrives.
     */
    double translation = axisOffset;
    if (hasNext) {
      double pushedTop = nextOffset - activeSize;
      if (pushedTop < translation) {
        translation = pushedTop;
      }
    }
    float translationPx = PixelUtil.toPixelFromDIP((float) translation);

    overlay.setVisibility(View.VISIBLE);
    if (mHorizontal) {
      overlay.setTranslationX(translationPx);
      overlay.setTranslationY(0f);
    } else {
      overlay.setTranslationY(translationPx);
      overlay.setTranslationX(0f);
    }
    overlay.bringToFront();
  }

  private @Nullable View findSectionHeaderOverlay() {
    if (mContentView == null) {
      return null;
    }
    for (int i = 0; i < mContentView.getChildCount(); i++) {
      View child = mContentView.getChildAt(i);
      if (child instanceof ShadowlistTemplateView
          && "sectionHeader".equals(((ShadowlistTemplateView) child).getTemplateType())) {
        return child;
      }
    }
    return null;
  }

  /*
   * A pinned (sticky) header/footer must stay above the scrolling elements, which
   * mount continuously and would otherwise cover it.
   */
  private void bringStickyViewsToFront() {
    if (mContentView == null || (!mStickyHeader && !mStickyFooter)) {
      return;
    }

    View headerView = null;
    View footerView = null;
    for (int i = 0; i < mContentView.getChildCount(); i++) {
      View child = mContentView.getChildAt(i);
      if (!(child instanceof ShadowlistTemplateView)) {
        continue;
      }
      if ("footer".equals(((ShadowlistTemplateView) child).getTemplateType())) {
        footerView = child;
      } else {
        headerView = child;
      }
    }

    if (mStickyHeader && headerView != null) {
      headerView.bringToFront();
    }
    if (mStickyFooter && footerView != null) {
      footerView.bringToFront();
    }
  }

  private void updateScrollState(int scrollX, int scrollY) {
    if (mState == null) {
      return;
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

    /*
     * Cache the sticky section-header geometry the core published this commit so the
     * per-scroll-tick pin (applyStickySectionHeaders) runs purely off cached values.
     */
    if (nextStateData.hasKey("stickyHeaderIndices")
        && nextStateData.hasKey("stickyHeaderOffsets")
        && nextStateData.hasKey("stickyHeaderSizes")) {
      ReadableArray indices = nextStateData.getArray("stickyHeaderIndices");
      ReadableArray offsets = nextStateData.getArray("stickyHeaderOffsets");
      ReadableArray sizes = nextStateData.getArray("stickyHeaderSizes");
      int count = indices != null ? indices.size() : 0;
      mStickyHeaderIndices = new int[count];
      mStickyHeaderOffsets = new double[count];
      mStickyHeaderSizes = new double[count];
      for (int i = 0; i < count; i++) {
        mStickyHeaderIndices[i] = indices.getInt(i);
        mStickyHeaderOffsets[i] = offsets != null ? offsets.getDouble(i) : 0.0;
        mStickyHeaderSizes[i] = sizes != null ? sizes.getDouble(i) : 0.0;
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

    // While a drag is in flight (or settling after a drop) we own the scroll offset, so
    // a core correction must not yank the content under the finger or jump the list as
    // the reorder lands.
    if (!mDragging && !mDragDropPending
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
    applyStickyTranslations();

    // Mid-drag commit (e.g. an auto-scroll re-virtualization mounted new rows): re-glue
    // the picked-up row to the finger and re-apply the make-room shuffle so freshly
    // mounted siblings are offset too.
    if (mDragging) {
      updateDrag();
    }

    // After a drop, the reorder is applied by JS on a later commit. Hold the shuffle
    // transforms until that commit lands (the dropped row's index prop reaching its
    // dropped slot) so the rows never snap back to the pre-reorder layout; the shuffled
    // positions already equal the post-reorder resting positions, so clearing then is
    // seamless.
    if (mDragDropPending && mDroppedView != null
        && mDroppedView.getElementIndex() == mDropInsertionIndex) {
      clearDragTransforms();
      mDragDropPending = false;
      mDroppedView = null;
    }
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
}
