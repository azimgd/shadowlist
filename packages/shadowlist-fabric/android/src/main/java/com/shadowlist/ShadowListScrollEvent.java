package com.shadowlist;

import androidx.annotation.Nullable;

import com.facebook.react.bridge.Arguments;
import com.facebook.react.bridge.WritableMap;
import com.facebook.react.uimanager.events.Event;

/*
 * onScroll and the drag and momentum events, which share one payload. Offsets and sizes are in
 * dp, velocity in dp per millisecond.
 */
public class ShadowListScrollEvent extends Event<ShadowListScrollEvent> {
  public static final String SCROLL = "topScroll";
  public static final String BEGIN_DRAG = "topScrollBeginDrag";
  public static final String END_DRAG = "topScrollEndDrag";
  public static final String MOMENTUM_BEGIN = "topMomentumScrollBegin";
  public static final String MOMENTUM_END = "topMomentumScrollEnd";

  /*
   * RawEvent::Category::Continuous.
   */
  private static final int CATEGORY_CONTINUOUS = 4;

  private final String mEventName;
  private final double mOffsetX;
  private final double mOffsetY;
  private final double mContentWidth;
  private final double mContentHeight;
  private final double mViewportWidth;
  private final double mViewportHeight;
  private final double mVelocityX;
  private final double mVelocityY;

  public ShadowListScrollEvent(
    int surfaceId,
    int viewId,
    String eventName,
    double offsetX,
    double offsetY,
    double contentWidth,
    double contentHeight,
    double viewportWidth,
    double viewportHeight,
    double velocityX,
    double velocityY) {
    super(surfaceId, viewId);
    mEventName = eventName;
    mOffsetX = offsetX;
    mOffsetY = offsetY;
    mContentWidth = contentWidth;
    mContentHeight = contentHeight;
    mViewportWidth = viewportWidth;
    mViewportHeight = viewportHeight;
    mVelocityX = velocityX;
    mVelocityY = velocityY;
  }

  @Override
  public String getEventName() {
    return mEventName;
  }

  /*
   * Each drag and momentum event is a discrete edge, like React Native's own. A merged
   * one would drop a begin or an end that JS pairs with its partner. Only the newest scroll
   * event matters, and an unread one is replaced.
   */
  @Override
  public boolean canCoalesce() {
    return SCROLL.equals(mEventName);
  }

  /*
   * A scroll event is continuous like the C++ unique events, see RawEvent::Category.
   */
  @Override
  protected int getEventCategory() {
    return SCROLL.equals(mEventName) ? CATEGORY_CONTINUOUS : super.getEventCategory();
  }

  @Nullable
  @Override
  protected WritableMap getEventData() {
    WritableMap contentOffset = Arguments.createMap();
    contentOffset.putDouble("x", mOffsetX);
    contentOffset.putDouble("y", mOffsetY);
    WritableMap contentSize = Arguments.createMap();
    contentSize.putDouble("width", mContentWidth);
    contentSize.putDouble("height", mContentHeight);
    WritableMap layoutMeasurement = Arguments.createMap();
    layoutMeasurement.putDouble("width", mViewportWidth);
    layoutMeasurement.putDouble("height", mViewportHeight);
    WritableMap velocity = Arguments.createMap();
    velocity.putDouble("x", mVelocityX);
    velocity.putDouble("y", mVelocityY);
    WritableMap contentInset = Arguments.createMap();
    contentInset.putDouble("top", 0);
    contentInset.putDouble("left", 0);
    contentInset.putDouble("bottom", 0);
    contentInset.putDouble("right", 0);

    WritableMap data = Arguments.createMap();
    data.putDouble("contentOffsetX", mOffsetX);
    data.putDouble("contentOffsetY", mOffsetY);
    data.putMap("contentOffset", contentOffset);
    data.putMap("contentSize", contentSize);
    data.putMap("layoutMeasurement", layoutMeasurement);
    data.putMap("velocity", velocity);
    data.putMap("contentInset", contentInset);
    data.putDouble("zoomScale", 1);
    return data;
  }

  /*
   * Velocity from the offsets the scroll view reports, and the scrollEventThrottle gate, like
   * ShadowListScrollTracker in C++. Offsets in dp, times in milliseconds.
   */
  static final class Tracker {
    private double mThrottleMs = 0;
    private boolean mHasPrevious = false;
    private double mPreviousX = 0;
    private double mPreviousY = 0;
    private double mPreviousTimeMs = 0;
    private boolean mHasSent = false;
    private double mSentTimeMs = 0;
    private boolean mPending = false;
    private double mPendingX = 0;
    private double mPendingY = 0;
    private double mVelocityX = 0;
    private double mVelocityY = 0;

    /*
     * Start over with a new throttle.
     */
    void reset(double throttleMs) {
      mThrottleMs = throttleMs;
      mHasPrevious = false;
      mHasSent = false;
      mPending = false;
    }

    /*
     * Record an offset. Returns whether to send the event now, with the velocity in
     * getVelocityX() and getVelocityY(). A callback that repeats the previous offset sends nothing.
     */
    boolean track(double offsetX, double offsetY) {
      if (mHasPrevious && offsetX == mPreviousX && offsetY == mPreviousY) {
        return false;
      }
      double now = System.nanoTime() / 1_000_000.0;
      mVelocityX = 0;
      mVelocityY = 0;
      if (mHasPrevious && now > mPreviousTimeMs) {
        double elapsed = now - mPreviousTimeMs;
        mVelocityX = (offsetX - mPreviousX) / elapsed;
        mVelocityY = (offsetY - mPreviousY) / elapsed;
      }
      mHasPrevious = true;
      mPreviousX = offsetX;
      mPreviousY = offsetY;
      mPreviousTimeMs = now;
      if (mThrottleMs > 0 && mHasSent && now - mSentTimeMs < mThrottleMs) {
        mPending = true;
        mPendingX = offsetX;
        mPendingY = offsetY;
        return false;
      }
      mHasSent = true;
      mSentTimeMs = now;
      mPending = false;
      return true;
    }

    /*
     * Whether the throttle held an event back. takeTrailing() then gives its offset once with
     * no velocity, in getPendingX() and getPendingY().
     */
    boolean takeTrailing() {
      if (!mPending) {
        return false;
      }
      mPending = false;
      mHasSent = true;
      mSentTimeMs = System.nanoTime() / 1_000_000.0;
      return true;
    }

    double getVelocityX() {
      return mVelocityX;
    }

    double getVelocityY() {
      return mVelocityY;
    }

    double getPendingX() {
      return mPendingX;
    }

    double getPendingY() {
      return mPendingY;
    }
  }
}
