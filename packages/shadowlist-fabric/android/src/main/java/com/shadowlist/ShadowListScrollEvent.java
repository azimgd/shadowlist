package com.shadowlist;

import androidx.annotation.Nullable;

import com.facebook.react.bridge.Arguments;
import com.facebook.react.bridge.WritableMap;
import com.facebook.react.uimanager.events.Event;

/*
 * Drag and momentum events with the same payload as onScroll, which the core sends. Offsets
 * and sizes are in dp, velocity in dp per millisecond.
 */
public class ShadowListScrollEvent extends Event<ShadowListScrollEvent> {
  public static final String BEGIN_DRAG = "topScrollBeginDrag";
  public static final String END_DRAG = "topScrollEndDrag";
  public static final String MOMENTUM_BEGIN = "topMomentumScrollBegin";
  public static final String MOMENTUM_END = "topMomentumScrollEnd";

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
}
