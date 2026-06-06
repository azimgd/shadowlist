package com.shadowlist;

import androidx.annotation.Nullable;

import com.facebook.react.bridge.Arguments;
import com.facebook.react.bridge.WritableMap;
import com.facebook.react.uimanager.events.Event;

/*
 * Pull-to-refresh event. "topRefresh" maps to the JS onRefresh handler. No payload.
 */
public class ShadowlistRefreshEvent extends Event<ShadowlistRefreshEvent> {
  public static final String EVENT_NAME = "topRefresh";

  public ShadowlistRefreshEvent(int surfaceId, int viewId) {
    super(surfaceId, viewId);
  }

  @Override
  public String getEventName() {
    return EVENT_NAME;
  }

  @Nullable
  @Override
  protected WritableMap getEventData() {
    return Arguments.createMap();
  }
}
