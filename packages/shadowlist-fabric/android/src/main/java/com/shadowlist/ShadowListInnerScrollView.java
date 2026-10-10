package com.shadowlist;

import android.content.Context;
import android.graphics.Canvas;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;

import com.facebook.react.views.scroll.ReactHorizontalScrollView;
import com.facebook.react.views.scroll.ReactScrollView;

/*
 * The inner scroll views of ShadowListView, one per axis, and the content they scroll. The
 * scroll views pass scroll, fling and touch callbacks to the host.
 */
final class ShadowListInnerScrollView {
  private ShadowListInnerScrollView() {}

  /*
   * Draws only the children that reach into the scroll viewport. Overscan rows stay mounted
   * for a fling to find them ready. Drawing them too would make the render thread sync and
   * draw every mounted row each frame. The host invalidates this on every scroll, which only
   * re-records this list of children.
   */
  static final class ContentContainer extends ViewGroup {
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
   * The scroll view's own dispatchTouchEvent, which dispatchTrackedTouchEvent wraps.
   */
  private interface TouchDispatch {
    boolean superDispatchTouchEvent(MotionEvent event);
  }

  /*
   * Track the finger here, not in onTouchEvent. A row takes the touch first and
   * onTouchEvent misses the down event. Dispatch sees the whole gesture.
   */
  private static boolean dispatchTrackedTouchEvent(ShadowListView host, TouchDispatch scrollView, MotionEvent event) {
    int action = event.getActionMasked();
    boolean touchEnded = action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL;
    if (action == MotionEvent.ACTION_DOWN) {
      host.handleInnerTouchDown();
    } else if (touchEnded) {
      host.handleInnerTouchUp();
    }
    boolean handled = scrollView.superDispatchTouchEvent(event);
    if (touchEnded) {
      // Any fling from the lift has started by now.
      host.reportTouchUpPhase();
    }
    return handled;
  }

  static final class Vertical extends ReactScrollView implements TouchDispatch {
    private final ShadowListView mHost;

    Vertical(Context context, ShadowListView host) {
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
      mHost.handleInnerFling(velocityY);
      if (mHost.snapFling(velocityY)) {
        return;
      }
      super.fling(velocityY);
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
      return dispatchTrackedTouchEvent(mHost, this, event);
    }

    @Override
    public boolean superDispatchTouchEvent(MotionEvent event) {
      return super.dispatchTouchEvent(event);
    }
  }

  static final class Horizontal extends ReactHorizontalScrollView implements TouchDispatch {
    private final ShadowListView mHost;

    Horizontal(Context context, ShadowListView host) {
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
      mHost.handleInnerFling(velocityX);
      if (mHost.snapFling(velocityX)) {
        return;
      }
      super.fling(velocityX);
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
      return dispatchTrackedTouchEvent(mHost, this, event);
    }

    @Override
    public boolean superDispatchTouchEvent(MotionEvent event) {
      return super.dispatchTouchEvent(event);
    }
  }
}
