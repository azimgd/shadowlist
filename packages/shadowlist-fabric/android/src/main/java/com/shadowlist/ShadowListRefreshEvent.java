package com.shadowlist;

import androidx.annotation.Nullable;

import com.facebook.react.bridge.Arguments;
import com.facebook.react.bridge.WritableMap;
import com.facebook.react.uimanager.events.Event;

/*
 * Pull-to-refresh events, no payload. "topRefresh" maps to the JS onRefresh handler,
 * "topRefreshSettle" (the spinner retracted after a refresh ended) to onRefreshSettle.
 */
public class ShadowListRefreshEvent extends Event<ShadowListRefreshEvent> {
  public static final String EVENT_NAME = "topRefresh";
  public static final String SETTLE_EVENT_NAME = "topRefreshSettle";

  private final String mEventName;

  public ShadowListRefreshEvent(int surfaceId, int viewId, String eventName) {
    super(surfaceId, viewId);
    mEventName = eventName;
  }

  @Override
  public String getEventName() {
    return mEventName;
  }

  @Nullable
  @Override
  protected WritableMap getEventData() {
    return Arguments.createMap();
  }
}
