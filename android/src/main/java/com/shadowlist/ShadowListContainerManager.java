package com.shadowlist;

import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.uimanager.ReactStylesDiffMap;
import com.facebook.react.uimanager.StateWrapper;
import com.facebook.react.uimanager.ViewGroupManager;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.uimanager.ViewManagerDelegate;
import com.facebook.react.uimanager.annotations.ReactProp;
import com.facebook.react.common.mapbuffer.MapBuffer;

import com.shadowlist.ShadowListContainerManagerDelegate;
import com.shadowlist.ShadowListContainerManagerInterface;

@ReactModule(name = ShadowListContainerManager.NAME)
public class ShadowListContainerManager extends ViewGroupManager<ShadowListContainer> implements ShadowListContainerManagerInterface<ShadowListContainer> {

  private static final short CX_STATE_KEY_SCROLL_POSITION = 0;
  private static final short CX_STATE_KEY_SCROLL_CONTAINER = 1;
  private static final short CX_STATE_KEY_SCROLL_CONTENT = 1;

  private static final short CX_STATE_KEY_SCROLL_POSITION_X = 0;
  private static final short CX_STATE_KEY_SCROLL_POSITION_Y = 1;

  private static final short CX_STATE_KEY_SCROLL_CONTAINER_WIDTH = 0;
  private static final short CX_STATE_KEY_SCROLL_CONTAINER_HEIGHT = 1;

  private static final short CX_STATE_KEY_SCROLL_CONTENT_WIDTH = 0;
  private static final short CX_STATE_KEY_SCROLL_CONTENT_HEIGHT = 1;

  public static final String NAME = "ShadowListContainer";

  private final ViewManagerDelegate<ShadowListContainer> mDelegate;

  public ShadowListContainerManager() {
    mDelegate = new ShadowListContainerManagerDelegate(this);
  }

  @Nullable
  @Override
  protected ViewManagerDelegate<ShadowListContainer> getDelegate() {
    return mDelegate;
  }

  @Override
  public String getName() {
    return NAME;
  }

  @Override
  public ShadowListContainer createViewInstance(ThemedReactContext context) {
    return new ShadowListContainer(context);
  }

  @Override
  @ReactProp(name = "inverted")
  public void setInverted(ShadowListContainer view, boolean inverted) {
  }

  @Override
  public void scrollToIndex(ShadowListContainer view, int index) {
  }

  @Nullable
  @Override
  public Object updateState(@NonNull ShadowListContainer view, ReactStylesDiffMap props, StateWrapper stateWrapper) {
    MapBuffer stateMapBuffer = stateWrapper.getStateDataMapBuffer();

    if (stateMapBuffer != null) {
      return getShadowListContainerStateUpdate(view, props, stateMapBuffer);
    } else {
      return null;
    }
  }

  private Object getShadowListContainerStateUpdate(ShadowListContainer view, ReactStylesDiffMap props, MapBuffer state) {
    Boolean isActive = (
      state.contains(CX_STATE_KEY_SCROLL_POSITION) &&
      state.contains(CX_STATE_KEY_SCROLL_CONTAINER) &&
      state.contains(CX_STATE_KEY_SCROLL_CONTENT));

    if (!isActive) {
      return null;
    }

    MapBuffer scrollPosition = state.getMapBuffer(CX_STATE_KEY_SCROLL_POSITION);
    MapBuffer scrollContainer = state.getMapBuffer(CX_STATE_KEY_SCROLL_CONTAINER);
    MapBuffer scrollContent = state.getMapBuffer(CX_STATE_KEY_SCROLL_CONTENT);

    return new ShadowListContainerStateUpdate(
      (float) scrollPosition.getDouble(CX_STATE_KEY_SCROLL_POSITION_X),
      (float) scrollPosition.getDouble(CX_STATE_KEY_SCROLL_POSITION_Y),
      (float) scrollContainer.getDouble(CX_STATE_KEY_SCROLL_CONTAINER_WIDTH),
      (float) scrollContainer.getDouble(CX_STATE_KEY_SCROLL_CONTAINER_HEIGHT),
      (float) scrollContent.getDouble(CX_STATE_KEY_SCROLL_CONTENT_WIDTH),
      (float) scrollContent.getDouble(CX_STATE_KEY_SCROLL_CONTENT_HEIGHT)
    );
  }
}
