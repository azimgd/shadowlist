package com.shadowlist;

import androidx.annotation.Nullable;
import com.facebook.react.bridge.Arguments;
import com.facebook.react.bridge.WritableMap;
import com.facebook.react.uimanager.events.Event;

class OnVisibleChangeEvent extends Event<OnVisibleChangeEvent> {
  private static final String EVENT_NAME = "topVisibleChange";
  private int mVisibleStartIndex;
  private int mVisibleEndIndex;

  public OnVisibleChangeEvent(int surfaceId, int viewTag, int visibleStartIndex, int visibleEndIndex) {
    super(surfaceId, viewTag);
    mVisibleStartIndex = visibleStartIndex;
    mVisibleEndIndex = visibleEndIndex;
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
    eventData.putInt("visibleStartIndex", mVisibleStartIndex);
    eventData.putInt("visibleEndIndex", mVisibleEndIndex);
    return eventData;
  }
}
