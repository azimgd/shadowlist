package com.shadowlist;

import androidx.annotation.Nullable;

import com.facebook.react.bridge.Arguments;
import com.facebook.react.bridge.WritableMap;
import com.facebook.react.uimanager.events.Event;

/*
 * A row's swipe action or context menu action ran. topSwipeAction fires onSwipeAction in JS
 * and topContextMenuAction fires onContextMenuAction.
 */
public class ShadowListActionEvent extends Event<ShadowListActionEvent> {
  public static final String SWIPE_ACTION = "topSwipeAction";
  public static final String CONTEXT_MENU_ACTION = "topContextMenuAction";

  private final String mEventName;
  private final int mActionIndex;
  private final boolean mLeading;
  private final boolean mFullSwipe;

  private ShadowListActionEvent(
    int surfaceId,
    int viewId,
    String eventName,
    int actionIndex,
    boolean leading,
    boolean fullSwipe) {
    super(surfaceId, viewId);
    mEventName = eventName;
    mActionIndex = actionIndex;
    mLeading = leading;
    mFullSwipe = fullSwipe;
  }

  static ShadowListActionEvent swipeAction(
    int surfaceId, int viewId, boolean leading, int actionIndex, boolean fullSwipe) {
    return new ShadowListActionEvent(surfaceId, viewId, SWIPE_ACTION, actionIndex, leading, fullSwipe);
  }

  static ShadowListActionEvent contextMenuAction(int surfaceId, int viewId, int actionIndex) {
    return new ShadowListActionEvent(surfaceId, viewId, CONTEXT_MENU_ACTION, actionIndex, false, false);
  }

  @Override
  public String getEventName() {
    return mEventName;
  }

  @Override
  public boolean canCoalesce() {
    return false;
  }

  @Nullable
  @Override
  protected WritableMap getEventData() {
    WritableMap data = Arguments.createMap();
    data.putInt("actionIndex", mActionIndex);
    if (SWIPE_ACTION.equals(mEventName)) {
      data.putBoolean("leading", mLeading);
      data.putBoolean("fullSwipe", mFullSwipe);
    }
    return data;
  }
}
