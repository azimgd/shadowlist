package com.shadowlist;

import androidx.annotation.Nullable;
import com.facebook.react.bridge.Arguments;
import com.facebook.react.bridge.WritableMap;
import com.facebook.react.uimanager.events.Event;

class OnEndReachedEvent extends Event<OnEndReachedEvent> {
  private static final String EVENT_NAME = "topEndReached";
  private int mDistanceFromEnd;

  public OnEndReachedEvent(int surfaceId, int viewTag, int distanceFromEnd) {
    super(surfaceId, viewTag);
    mDistanceFromEnd = distanceFromEnd;
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
    eventData.putInt("distanceFromEnd", mDistanceFromEnd);
    return eventData;
  }
}
