package com.shadowlist;

import androidx.annotation.Nullable;
import com.facebook.react.bridge.Arguments;
import com.facebook.react.bridge.WritableMap;
import com.facebook.react.uimanager.events.Event;

class OnStartReachedEvent extends Event<OnStartReachedEvent> {
  private static final String EVENT_NAME = "topStartReached";
  private int mDistanceFromStart;

  public OnStartReachedEvent(int surfaceId, int viewTag, int distanceFromStart) {
    super(surfaceId, viewTag);
    mDistanceFromStart = distanceFromStart;
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
    eventData.putInt("distanceFromStart", mDistanceFromStart);
    return eventData;
  }
}
