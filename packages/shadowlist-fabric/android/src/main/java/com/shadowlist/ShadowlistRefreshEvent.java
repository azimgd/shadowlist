package com.shadowlist;

import androidx.annotation.Nullable;

import com.facebook.react.bridge.Arguments;
import com.facebook.react.bridge.WritableMap;
import com.facebook.react.uimanager.events.Event;

/*
 * Fabric event for pull-to-refresh. The SwipeRefreshLayout's onRefresh listener
 * dispatches this through the EventDispatcher; "topRefresh" is the registered name the
 * codegen maps back to the JS `onRefresh` handler (the same top-PascalCase convention
 * RN's own ScrollView uses). Carries no payload.
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
