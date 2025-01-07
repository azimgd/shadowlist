package com.shadowlist;

import androidx.annotation.Nullable;
import com.facebook.react.bridge.Arguments;
import com.facebook.react.bridge.WritableMap;
import com.facebook.react.uimanager.events.Event;

class OnScrollEvent extends Event<OnScrollEvent> {
  private static final String EVENT_NAME = "topScroll";
  private int mScrollContentWidth;
  private int mScrollContentHeight;
  private int mScrollPositionTop;
  private int mScrollPositionLeft;

  public OnScrollEvent(int surfaceId, int viewTag, int scrollContentWidth, int scrollContentHeight, int scrollPositionTop, int scrollPositionLeft) {
    super(surfaceId, viewTag);
    mScrollContentWidth = scrollContentWidth;
    mScrollContentHeight = scrollContentHeight;
    mScrollPositionTop = scrollPositionTop;
    mScrollPositionLeft = scrollPositionLeft;
  }

  @Override
  public String getEventName() {
    return EVENT_NAME;
  }

  @Nullable
  @Override
  protected WritableMap getEventData() {
    WritableMap eventData = Arguments.createMap();
    eventData.putInt("target", getViewTag());

    WritableMap contentSize = Arguments.createMap();
    contentSize.putInt("width", mScrollContentWidth);
    contentSize.putInt("height", mScrollContentHeight);
    
    WritableMap contentOffset = Arguments.createMap();
    contentOffset.putInt("x", mScrollPositionTop);
    contentOffset.putInt("y", mScrollPositionLeft);

    eventData.putMap("contentSize", contentSize);
    eventData.putMap("contentOffset", contentOffset);

    return eventData;
  }
}
